set(DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR})

fatfs_create_spiflash_image(
    fatfs-react
    ${DATA_DIR}
    FLASH_IN_PROJECT
)
