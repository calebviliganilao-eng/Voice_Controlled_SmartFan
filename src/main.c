/* ESP32 DevKit V1 / ESP-IDF / Sinric Pro / two 2-channel relay boards.
   Voice via Alexa -> Sinric cloud -> ESP32, buttons work locally.
   WARNING: for one TWO-WIRE DC motor the relay can switch ON/OFF only. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sinricpro.h"
#include "sinricpro_fan.h"
#include "capabilities/range_controller.h"
#include "core/sinricpro_device_internal.h"
#include "app_secrets.h"
#include "fan_logic.h"
#include "fan_hardware.h"

#ifndef FAN_DC_ONOFF
#define FAN_DC_ONOFF 0
#endif
/*
 * SinricPro has TWO distinct fan-speed wire commands:
 *   - setPowerLevel: 0..100 percentage
 *   - setRangeValue: often a fan speed range 1..3
 *
 * The ESP-IDF 1.3.0 Fan driver only routes PowerLevel speed commands.
 * Attach the SDK's RangeController to the same Fan's request handler
 * before SinricPro starts, so the existing power handling still works.
 */
#define WIFI_CONNECTED_BIT BIT0
static const char *TAG = "SMARTFAN";
static EventGroupHandle_t wifi_bits;
static SemaphoreHandle_t level_mutex;
static sinricpro_device_handle_t fan_device = NULL;
static sinricpro_range_controller_handle_t fan_range_controller;
static sinricpro_device_request_handler_t original_fan_request_handler;
static int selected_level = 0;
static bool sntp_started = false;

static void wifi_events(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg; (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_bits, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "Wi-Fi disconnected; retrying");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_bits, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi connected / IP assigned");
        if (!sntp_started) {
            esp_sntp_config_t clock_config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
            esp_err_t result = esp_netif_sntp_init(&clock_config);
            if (result == ESP_OK) sntp_started = true;
            else ESP_LOGW(TAG, "SNTP setup: %s", esp_err_to_name(result));
        }
    }
}

static void initialize_wifi(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_bits = xEventGroupCreate();
    configASSERT(wifi_bits != NULL);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_events, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               wifi_events, NULL));
    wifi_config_t wifi = {0};
    /* Credentials never printed in logs. */
    strncpy((char *)wifi.sta.ssid, WIFI_SSID, sizeof(wifi.sta.ssid) - 1);
    strncpy((char *)wifi.sta.password, WIFI_PASSWORD,
            sizeof(wifi.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Connecting to 2.4 GHz Wi-Fi");
}

static void apply_level_under_lock(int level) {
    int rounded = fan_normalize(level);
    if (selected_level == rounded) return;
    fan_hardware_set_level(rounded, FAN_DC_ONOFF != 0);
    selected_level = rounded;
}

/* Standard Fan / FanUS RangeController (1,2,3). Also tolerates percentage input. */
static bool on_range_value(const char *device_id, int *range_value, void *user_data) {
    (void)device_id; (void)user_data;
    if (!range_value || xSemaphoreTake(level_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return false;
    const int requested = *range_value;
    apply_level_under_lock(fan_from_sinric_request(requested));
    /* RangeController expects the discrete step as the response. */
    *range_value = fan_step_from_percent(selected_level);
    ESP_LOGI(TAG, "REMOTE RANGE setRangeValue requested=%d -> step=%d/3 (%d%%)",
             requested, *range_value, selected_level);
    xSemaphoreGive(level_mutex);
    return true;
}

static bool on_fan_request(const char *device_id, const char *action, const char *instance_id,
                           cJSON *request_value, cJSON *response_value, void *user_data) {
    if (strcmp(action, SINRICPRO_ACTION_SET_RANGE_VALUE) == 0) {
        return sinricpro_range_controller_handle_request(
            fan_range_controller, device_id, action, request_value, response_value);
    }
    return original_fan_request_handler(
        device_id, action, instance_id, request_value, response_value, user_data);
}

static bool on_power_state(const char *device_id, bool *state, void *user_data) {
    (void)device_id; (void)user_data;
    if (!state || xSemaphoreTake(level_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return false;
    if (!*state) {
        apply_level_under_lock(0);
    } else if (selected_level == 0) {
        apply_level_under_lock(fan_from_sinric_request(1));
    }
    ESP_LOGI(TAG, "Sinric/Alexa POWER %s -> selected speed %d/3 (%d%%)",
             *state ? "ON" : "OFF", fan_step_from_percent(selected_level), selected_level);
    xSemaphoreGive(level_mutex);
    return true;
}

static bool on_power_level(const char *device_id, int *power, void *user_data) {
    (void)device_id; (void)user_data;
    if (!power || xSemaphoreTake(level_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return false;
    const int requested = *power;
    apply_level_under_lock(fan_from_sinric_request(requested));
    /* The setPowerLevel response is always a percentage 0..100. */
    *power = selected_level;
    ESP_LOGI(TAG, "REMOTE POWERLEVEL setPowerLevel requested=%d -> step=%d/3 (%d%%), relay output selected",
             requested, fan_step_from_percent(selected_level), selected_level);
    xSemaphoreGive(level_mutex);
    return true;
}

static bool on_adjust_power_level(const char *device_id, int *delta, void *user_data) {
    (void)device_id; (void)user_data;
    if (!delta || xSemaphoreTake(level_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return false;
    const int requested_delta = *delta;
    apply_level_under_lock(fan_adjust_from_sinric(selected_level, requested_delta));
    /* API expects the final absolute level, not the delta. */
    *delta = selected_level; /* Absolute percent, not the requested delta. */
    ESP_LOGI(TAG, "REMOTE ADJUST adjustPowerLevel %+d -> step=%d/3 (%d%%)",
             requested_delta, fan_step_from_percent(selected_level), selected_level);
    xSemaphoreGive(level_mutex);
    return true;
}

static void buttons_task(void *arg) {
    (void)arg;
    for (;;) {
        int button_level;
        if (fan_hardware_poll_button(&button_level)) {
            int level;
            if (xSemaphoreTake(level_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                apply_level_under_lock(button_level);
                level = selected_level;
                xSemaphoreGive(level_mutex);
                ESP_LOGI(TAG, "Physical button -> speed %d/3 (%d%%)",
                         fan_step_from_percent(level), level);
                if (fan_device != NULL) {
                    /* The library queues events when connected; when offline,
                       manual button control still works independently. */
                    (void)sinricpro_fan_send_power_state_event(
                        fan_device, level != 0, SINRICPRO_CAUSE_PHYSICAL_INTERACTION);
                    if (level != 0) {
                        /* Keep the existing percentage report for PowerLevel devices. */
                        (void)sinricpro_fan_send_power_level_event(
                            fan_device, level, SINRICPRO_CAUSE_PHYSICAL_INTERACTION);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs);
    fan_hardware_init();
    level_mutex = xSemaphoreCreateMutex();
    configASSERT(level_mutex != NULL);
    BaseType_t made = xTaskCreate(buttons_task, "fan_buttons", 4096, NULL, 5, NULL);
    configASSERT(made == pdPASS);
    ESP_LOGI(TAG, "Fan controller ready: A1=GPIO32 A2=GPIO33 B1=GPIO25");
#if FAN_DC_ONOFF
    ESP_LOGW(TAG, "Two-wire DC fan: all nonzero presets = physical ON (one speed)");
#endif
    initialize_wifi();
    xEventGroupWaitBits(wifi_bits, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    /* TLS validation normally requires an accurate clock. Let SNTP update it. */
    for (int retry = 0; retry < 40 && time(NULL) < 1735689600; ++retry) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (time(NULL) < 1735689600) {
        ESP_LOGW(TAG, "Clock not synchronized; TLS may fail until NTP is reachable");
    }

    sinricpro_config_t sinric = {
        .app_key = SINRIC_APP_KEY,
        .app_secret = SINRIC_APP_SECRET,
        .auto_reconnect = true
    };
    ESP_ERROR_CHECK(sinricpro_init(&sinric));
    fan_device = sinricpro_fan_create(SINRIC_DEVICE_ID);
    if (!fan_device) {
        ESP_LOGE(TAG, "Sinric Fan creation failed; check Device ID");
        return;
    }
    /* Register PowerLevel commands and attach RangeController for discrete speeds. */
    (void)sinricpro_fan_on_power_state(fan_device, on_power_state, NULL);
    (void)sinricpro_fan_on_power_level(fan_device, on_power_level, NULL);
    (void)sinricpro_fan_on_adjust_power_level(fan_device, on_adjust_power_level, NULL);
    fan_range_controller = sinricpro_range_controller_create();
    ESP_ERROR_CHECK(sinricpro_range_controller_set_callback(fan_range_controller, on_range_value, NULL));
    sinricpro_device_t *device = fan_device;
    original_fan_request_handler = device->request_handler;
    device->request_handler = on_fan_request;
    ESP_LOGI(TAG, "Remote setRangeValue (1/2/3) callback registered");
    ESP_LOGI(TAG, "Supported power-level requests: 1/2/3 or 33/66/100 percent");
    ESP_ERROR_CHECK(sinricpro_start());
    ESP_LOGI(TAG, "Sinric started; link its skill with Alexa and discover Smart Fan");
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
