idf_component_register(
    SRCS "main.c" "fan_hardware.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_wifi esp_event esp_netif nvs_flash sinricpro__esp-idf
)

idf_component_get_property(sinricpro_dir sinricpro__esp-idf COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${sinricpro_dir}/src")
