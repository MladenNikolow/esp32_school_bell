idf_component_register(
    SRCS "FatFS/FatFS_API.c" "SPIFFS/SPIFFS_API.c"
    INCLUDE_DIRS "FatFS" "SPIFFS"
    REQUIRES fatfs vfs spiffs
)
