idf_component_register(
    SRCS "src/TimeSync_API.c"
    INCLUDE_DIRS "src"
    REQUIRES esp_event esp_netif esp_timer nvs_flash NVS FileSystem json
)
