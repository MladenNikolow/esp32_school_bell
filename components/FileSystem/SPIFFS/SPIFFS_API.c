#include "SPIFFS_API.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char* TAG = "spiffs";

esp_err_t
SPIFFS_Init(void)
{
    esp_vfs_spiffs_conf_t tConf = {
        .base_path       = SPIFFS_MOUNT_POINT,
        .partition_label = "storage",
        .max_files       = 8,
        .format_if_mount_failed = true
    };

    esp_err_t espRslt = esp_vfs_spiffs_register(&tConf);

    if (espRslt != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(espRslt));
    }
    else
    {
        ESP_LOGI(TAG, "SPIFFS mounted at %s", SPIFFS_MOUNT_POINT);

        size_t ulTotal = 0, ulUsed = 0;
        if (esp_spiffs_info("storage", &ulTotal, &ulUsed) == ESP_OK)
        {
            ESP_LOGI(TAG, "SPIFFS: total=%zu, used=%zu", ulTotal, ulUsed);
        }
    }

    return espRslt;
}

esp_err_t
SPIFFS_ReadFile(const char* pcPath, char* pcOutBuf, size_t ulBufSize, size_t* pulBytesRead)
{
    if ((NULL == pcPath) || (NULL == pcOutBuf) || (0 == ulBufSize))
    {
        return ESP_ERR_INVALID_ARG;
    }

    FILE* pFile = fopen(pcPath, "r");
    if (NULL == pFile)
    {
        return ESP_ERR_NOT_FOUND;
    }

    size_t ulRead = fread(pcOutBuf, 1, ulBufSize - 1, pFile);
    pcOutBuf[ulRead] = '\0';
    fclose(pFile);

    if (pulBytesRead != NULL)
    {
        *pulBytesRead = ulRead;
    }

    return ESP_OK;
}

esp_err_t
SPIFFS_WriteFile(const char* pcPath, const char* pcData, size_t ulDataLen)
{
    if ((NULL == pcPath) || (NULL == pcData))
    {
        return ESP_ERR_INVALID_ARG;
    }

    FILE* pFile = fopen(pcPath, "w");
    if (NULL == pFile)
    {
        ESP_LOGE(TAG, "Failed to open %s for writing", pcPath);
        return ESP_FAIL;
    }

    size_t ulWritten = fwrite(pcData, 1, ulDataLen, pFile);
    fclose(pFile);

    if (ulWritten != ulDataLen)
    {
        ESP_LOGE(TAG, "Write incomplete: %zu of %zu bytes", ulWritten, ulDataLen);
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool
SPIFFS_FileExists(const char* pcPath)
{
    if (NULL == pcPath)
    {
        return false;
    }

    struct stat tStat;
    return (stat(pcPath, &tStat) == 0);
}
