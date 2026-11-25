#include "fan_hardware.h"
#include "fan_logic.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#ifndef RELAY_ACTIVE_LOW
#define RELAY_ACTIVE_LOW 1
#endif
#define RELAY_OFF_LEVEL (RELAY_ACTIVE_LOW ? 1 : 0)
#define RELAY_ON_LEVEL  (RELAY_ACTIVE_LOW ? 0 : 1)

#define PIN_OFF GPIO_NUM_19
#define PIN_LOW GPIO_NUM_18
#define PIN_MED GPIO_NUM_5
#define PIN_HIGH GPIO_NUM_17
#define PIN_A1 GPIO_NUM_32
#define PIN_A2 GPIO_NUM_33
#define PIN_B1 GPIO_NUM_25

static const char *TAG = "FAN_HW";
static const gpio_num_t BUTTONS[4] = { PIN_OFF, PIN_LOW, PIN_MED, PIN_HIGH };
static const int LEVELS[4] = {0, 33, 66, 100};
static const gpio_num_t RELAYS[3] = { PIN_A1, PIN_A2, PIN_B1 };
static int last_reading[4] = {1, 1, 1, 1};
static int stable_reading[4] = {1, 1, 1, 1};
static TickType_t last_change[4];
static unsigned last_mask = 0u;

static void output_off(void) {
    for (int i = 0; i < 3; ++i) {
        ESP_ERROR_CHECK(gpio_set_level(RELAYS[i], RELAY_OFF_LEVEL));
    }
}

void fan_hardware_init(void) {
    /* Preload OFF before changing GPIOs to outputs. Boot-time floating
       cannot be entirely prevented in software; verify safe hardware OFF. */
    output_off();
    gpio_config_t out = {
        .pin_bit_mask = (1ULL << PIN_A1) | (1ULL << PIN_A2) | (1ULL << PIN_B1),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&out));
    output_off();
    gpio_config_t in = {
        .pin_bit_mask = (1ULL << PIN_OFF) | (1ULL << PIN_LOW) |
                        (1ULL << PIN_MED) | (1ULL << PIN_HIGH),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&in));
    for (int i = 0; i < 4; ++i) {
        last_reading[i] = stable_reading[i] = gpio_get_level(BUTTONS[i]);
        last_change[i] = xTaskGetTickCount();
    }
    ESP_LOGI(TAG, "Buttons OFF19 LOW18 MED5 HIGH17, relay A1=32 A2=33 B1=25");
}

void fan_hardware_set_level(int level, bool dc_mode) {
    unsigned mask = fan_relay_mask(level, dc_mode);
    if (!fan_mask_is_safe(mask)) {
        ESP_LOGE(TAG, "Unsafe relay mask rejected");
        return;
    }
    if (mask == last_mask) return;
    output_off();
    if (last_mask != 0 && mask != 0) {
        vTaskDelay(pdMS_TO_TICKS(80)); /* never intentionally overlap relays */
    }
    for (int i = 0; i < 3; ++i) {
        if (mask & (1u << i)) {
            ESP_ERROR_CHECK(gpio_set_level(RELAYS[i], RELAY_ON_LEVEL));
        }
    }
    last_mask = mask;
    ESP_LOGI(TAG, "Requested %d%%; relay mask 0x%x", fan_normalize(level),mask);
}

bool fan_hardware_poll_button(int *requested_level) {
    if (!requested_level) return false;
    TickType_t now = xTaskGetTickCount();
    TickType_t debounce = pdMS_TO_TICKS(50);
    if (debounce == 0) debounce = 1;
    int pressed = -1;
    for (int i = 0; i < 4; ++i) {
        int reading = gpio_get_level(BUTTONS[i]);
        if (reading != last_reading[i]) {
            last_reading[i] = reading;
            last_change[i] = now;
        }
        if (reading != stable_reading[i] &&
            (now - last_change[i]) >= debounce) {
            stable_reading[i] = reading;
            if (reading == 0 && pressed == -1) pressed = i;
        }
    }
    if (pressed < 0) return false;
    /* OFF has highest priority when held with another button. */
    if (gpio_get_level(PIN_OFF) == 0) pressed = 0;
    *requested_level = LEVELS[pressed];
    return true;
}
