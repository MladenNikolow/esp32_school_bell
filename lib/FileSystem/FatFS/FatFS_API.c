#include "FatFS_API.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "esp_log.h"
#include "wear_levelling.h"

static const char *TAG = "fatfs";
static wl_handle_t hWearLevelling = WL_INVALID_HANDLE;

esp_err_t
FatFS_Init()
{
    esp_err_t espRslt = ESP_OK;
    const char* pcBasePath = "/react";

    esp_vfs_fat_mount_config_t tFatFsConfig =
    {
        .format_if_mount_failed = true,
        .max_files = 10,
        .allocation_unit_size = 4096
    };

    espRslt = esp_vfs_fat_spiflash_mount_rw_wl(
        pcBasePath,          // Mount point
        "fatfs-react",       // Partition label
        &tFatFsConfig,       // FATFS config
        &hWearLevelling      // Wear leveling handle
    );

    if (espRslt != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(espRslt));
    }
    else
    {
        ESP_LOGI(TAG, "FATFS mounted at %s", pcBasePath);
    }

    return espRslt;
}