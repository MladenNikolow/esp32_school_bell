#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/**
 * @brief Initialize SNTP time synchronization.
 *        Loads stored timezone from NVS (with SPIFFS fallback) and
 *        starts SNTP client.  If the RTC already holds a plausible
 *        time (year >= 2024) the synced flag is pre-set so that the
 *        scheduler is not blocked after a soft reboot.
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
 * @brief Check if time has been synchronized via SNTP and is not stale.
 *        Returns false if no sync has occurred or if the last successful
 *        sync was more than TIMESYNC_STALE_THRESHOLD_SEC seconds ago.
 * @return true if time is considered valid and fresh.
 */
bool TimeSync_IsSynced(void);

/**
 * @brief Get seconds elapsed since the last successful SNTP sync.
 * @return Seconds since last sync, or UINT32_MAX if never synced.
 */
uint32_t TimeSync_GetLastSyncAgeSec(void);

/**
 * @brief Force an immediate SNTP re-synchronization attempt.
 */
void TimeSync_ForceSync(void);
