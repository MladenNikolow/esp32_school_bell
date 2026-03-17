idf_component_register(
    SRCS "src/TimeSync_API.c"
    INCLUDE_DIRS "src"
    REQUIRES esp_event esp_netif nvs_flash NVS
)
