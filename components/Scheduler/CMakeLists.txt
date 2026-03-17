idf_component_register(
    SRCS "src/Schedule_Data.c" "src/Scheduler_API.c"
    INCLUDE_DIRS "src"
    REQUIRES json esp_timer freertos FileSystem TimeSync RingBell Generic NVS
)
