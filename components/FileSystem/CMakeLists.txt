idf_component_register(
    SRCS "FatFS/FatFS_API.c"
    INCLUDE_DIRS "FatFS"
    REQUIRES fatfs vfs
)
