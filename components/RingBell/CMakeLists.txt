idf_component_register(
    SRCS "src/RingBell_API.c"
    INCLUDE_DIRS "src"
    REQUIRES driver esp_timer nvs_flash waveshare__esp32_s3_touch_lcd_4
)
