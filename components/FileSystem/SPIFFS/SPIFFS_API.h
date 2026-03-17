#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>

#define SPIFFS_MOUNT_POINT "/storage"

/**
 * @brief Initialize and mount the SPIFFS partition.
 * @return ESP_OK on success.
 */
esp_err_t SPIFFS_Init(void);

/**
 * @brief Read entire file contents into buffer.
 * @param pcPath     Full path (e.g. "/storage/schedule.json").
 * @param pcOutBuf   Output buffer.
 * @param ulBufSize  Size of output buffer.
 * @param pulBytesRead  If non-NULL, receives actual bytes read.
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if file missing.
 */
esp_err_t SPIFFS_ReadFile(const char* pcPath, char* pcOutBuf, size_t ulBufSize, size_t* pulBytesRead);

/**
 * @brief Write data to a file (creates or overwrites).
 * @param pcPath   Full path.
 * @param pcData   Data to write.
 * @param ulDataLen Length of data.
 * @return ESP_OK on success.
 */
esp_err_t SPIFFS_WriteFile(const char* pcPath, const char* pcData, size_t ulDataLen);

/**
 * @brief Check whether a file exists.
 * @param pcPath Full path.
 * @return true if file exists.
 */
bool SPIFFS_FileExists(const char* pcPath);
