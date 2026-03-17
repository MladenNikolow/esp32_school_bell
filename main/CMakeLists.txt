idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS .
    REQUIRES AppTask Generic NVS WebServer WiFi_Manager lvgl__lvgl waveshare__esp32_s3_touch_lcd_4 espressif__esp_lvgl_port TouchScreen
)