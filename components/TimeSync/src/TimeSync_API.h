#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <time.h>

/**
 * @brief Initialize SNTP time synchronization.
 *        Loads stored timezone from NVS and starts SNTP client.
 * @return ESP_OK on success.
 */
esp_err_t TimeSync_Init(void);

/**
 * @brief Set timezone using a POSIX TZ string and persist to NVS.
 * @param pcTzPosix  POSIX timezone string, e.g. "EET-2EEST,M3.5.0/3,M10.5.0/4"
 * @return ESP_OK on success.
 */
esp_err_t TimeSync_SetTimezone(const char* pcTzPosix);

/**
 * @brief Get the currently configured timezone string.
 * @param pcOutBuf   Output buffer.
 * @param ulBufLen   Size of output buffer.
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if not set.
 */
esp_err_t TimeSync_GetTimezone(char* pcOutBuf, size_t ulBufLen);

/**
 * @brief Get current local time.
 * @param ptTimeInfo  Output struct tm.
 * @return ESP_OK on success.
 */
esp_err_t TimeSync_GetLocalTime(struct tm* ptTimeInfo);

/**
 * @brief Check if time has been synchronized via SNTP.
 * @return true if time is valid.
 */
bool TimeSync_IsSynced(void);
