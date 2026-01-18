idf_component_register(
    SRCS "src/WiFi_Manager_API.c"
    INCLUDE_DIRS "src"
    REQUIRES esp_wifi esp_event nvs_flash Generic RingBell NVS
)
