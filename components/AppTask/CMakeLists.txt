file(GLOB_RECURSE SOURCES "src/*.c" "src/*.cpp")

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "src"
    REQUIRES    
            freertos
            esp_common
            esp_timer
            esp_event
            esp_wifi
            nvs_flash
            Generic
            NVS
            WiFi_Manager
            WebServer
            FileSystem
)
