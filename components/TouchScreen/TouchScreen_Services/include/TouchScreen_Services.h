#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* PIN Service                                                         */
/* ================================================================== */

/** @brief Initialize PIN service (opens NVS, writes default if needed). */
esp_err_t TS_Pin_Init(void);

/** @brief Validate a PIN string against stored value.
 *  @return true if match, false otherwise (increments fail counter). */
bool TS_Pin_Validate(const char *pcPin);

/** @brief Change the stored PIN. PIN must be 4-6 digits. */
esp_err_t TS_Pin_Set(const char *pcNewPin);

/** @brief Read the current PIN into buffer (must be >= 7 bytes). */
esp_err_t TS_Pin_Get(char *pcOutBuf, size_t ulBufLen);

/** @brief Get length of the currently stored PIN (4-6). */
uint8_t TS_Pin_GetLength(void);

/** @brief Check if PIN was explicitly configured by user (not default). */
bool TS_Pin_IsConfigured(void);

/** @brief Erase the stored PIN and configured flag from NVS. */
esp_err_t TS_Pin_Reset(void);

/** @brief Check if locked out due to too many failed attempts. */
bool TS_Pin_IsLockedOut(void);

/** @brief Get remaining lockout seconds (0 if not locked). */
uint32_t TS_Pin_GetLockoutRemaining(void);

/** @brief Reset the failed attempt counter (called after successful PIN). */
void TS_Pin_ResetAttempts(void);

/* ================================================================== */
/* Setup Wizard Service                                                */
/* ================================================================== */

/** @brief Initialize setup service (reads first-boot flag from NVS). */
esp_err_t TS_Setup_Init(void);

/** @brief Check if the initial setup wizard has been completed. */
bool TS_Setup_IsComplete(void);

/** @brief Mark the initial setup as complete (writes to NVS). */
esp_err_t TS_Setup_MarkComplete(void);

/** @brief Reset the setup flag so wizard shows again on next boot. */
esp_err_t TS_Setup_Reset(void);

/** @brief Auto-complete setup for existing devices being upgraded.
 *  @return true if migration was applied, false otherwise. */
bool TS_Setup_CheckMigration(void);

/* ================================================================== */
/* Bell Service                                                        */
/* ================================================================== */

#include "RingBell_API.h"

/** @brief Get current bell state (IDLE, RINGING, PANIC). */
BELL_STATE_E TS_Bell_GetState(void);

/** @brief Check if panic mode is active. */
bool TS_Bell_IsPanic(void);

/** @brief Toggle panic mode (PIN-gated at UI level). */
esp_err_t TS_Bell_SetPanic(bool bEnable);

/** @brief Ring bell for a test duration (PIN-gated at UI level). */
esp_err_t TS_Bell_TestRing(uint32_t ulDurationSec);

/* ================================================================== */
/* Schedule Service                                                    */
/* ================================================================== */

#include "Scheduler_API.h"

/** @brief Initialize schedule service with scheduler handle. */
esp_err_t TS_Schedule_Init(SCHEDULER_H hScheduler);

/** @brief Get full scheduler status (day type, next bell, time sync). */
esp_err_t TS_Schedule_GetStatus(SCHEDULER_STATUS_T *ptStatus);

/** @brief Get next bell info. */
esp_err_t TS_Schedule_GetNextBell(NEXT_BELL_INFO_T *ptInfo);

/** @brief Get bell entries for a shift (0=first, 1=second).
 *  @param ucShift 0 or 1
 *  @param ptBells Output array (caller provides SCHEDULE_MAX_BELLS_PER_SHIFT slots)
 *  @param pulCount Output: number of bells filled
 *  @param pbEnabled Output: whether shift is enabled
 *  @return ESP_OK on success */
esp_err_t TS_Schedule_GetShiftBells(uint8_t ucShift, BELL_ENTRY_T *ptBells,
                                     uint32_t *pulCount, bool *pbEnabled);

/** @brief Load schedule settings (timezone, working days) from SPIFFS.
 *  @param ptSettings Output: settings struct */
esp_err_t TS_Schedule_GetSettings(SCHEDULE_SETTINGS_T *ptSettings);

/** @brief Override today's day type by adding a temporary exception.
 *  Creates or replaces a single-day exception for today's date.
 *  @param eAction EXCEPTION_ACTION_DAY_OFF or EXCEPTION_ACTION_NORMAL
 *  @return ESP_OK on success */
esp_err_t TS_Schedule_SetTodayOverride(EXCEPTION_ACTION_E eAction);

/** @brief Cancel today's manual override (remove the "Manual Override" exception).
 *  @return ESP_OK on success or if no override was found */
esp_err_t TS_Schedule_CancelTodayOverride(void);

/** @brief Query today's manual override action.
 *  @return EXCEPTION_ACTION_DAY_OFF (0) or EXCEPTION_ACTION_NORMAL (1) if
 *          a manual override exists, or -1 if none. */
int TS_Schedule_GetTodayOverrideAction(void);

/* ================================================================== */
/* WiFi Service                                                        */
/* ================================================================== */

/** WiFi scan result entry for the touchscreen UI */
typedef struct {
    char     acSsid[33];    /**< SSID (null-terminated) */
    uint8_t  abBssid[6];    /**< AP BSSID (MAC address) */
    int8_t   cRssi;         /**< Signal strength in dBm */
    bool     bSecured;      /**< true if network requires password */
    uint8_t  ucChannel;     /**< WiFi channel */
} TS_WiFi_AP_t;

#define TS_WIFI_SCAN_MAX_AP  20

/** @brief Start a blocking WiFi scan and populate results.
 *  @param ptResults Output array (caller provides TS_WIFI_SCAN_MAX_AP slots)
 *  @param pusCount Output: number of APs found
 *  @return ESP_OK on success */
esp_err_t TS_WiFi_Scan(TS_WiFi_AP_t *ptResults, uint16_t *pusCount);

/** @brief Start a non-blocking WiFi scan in a background task.
 *  Poll TS_WiFi_IsScanComplete() then call TS_WiFi_ScanGetResults().
 *  @return ESP_OK if scan started, ESP_ERR_INVALID_STATE if already running */
esp_err_t TS_WiFi_ScanAsync(void);

/** @brief Check if an async scan has completed.
 *  @return true if scan finished (success or error), false if still running */
bool TS_WiFi_IsScanComplete(void);

/** @brief Retrieve results from a completed async scan.
 *  @param ptResults Output array (caller provides TS_WIFI_SCAN_MAX_AP slots)
 *  @param pusCount Output: number of APs found
 *  @return ESP_OK on success, scan error code on failure, ESP_ERR_INVALID_STATE if not done */
esp_err_t TS_WiFi_ScanGetResults(TS_WiFi_AP_t *ptResults, uint16_t *pusCount);

/** @brief Abort any in-progress async scan. Safe to call if no scan is running. */
void TS_WiFi_ScanAbort(void);

/** @brief Save WiFi credentials and request reconnect. */
esp_err_t TS_WiFi_SaveCredentials(const char *pcSsid, const char *pcPassword);

/** @brief Check if device is connected to an AP. */
bool TS_WiFi_IsConnected(void);

/** @brief Get currently connected SSID (empty string if not connected).
 *  @param pcOutBuf Buffer for SSID (must be >= 33 bytes)
 *  @param ulBufLen Buffer size */
esp_err_t TS_WiFi_GetConnectedSsid(char *pcOutBuf, size_t ulBufLen);

/** @brief Save new WiFi credentials and reconnect in-place (no reboot).
 *  Saves to NVS, reconfigures STA, disconnects and reconnects.
 *  The existing retry mechanism handles failures automatically.
 *  @param pcSsid     Network SSID
 *  @param pcPassword Network password (empty string for open networks)
 *  @param pucBssid   AP BSSID (6 bytes), or NULL if not known */
esp_err_t TS_WiFi_Connect(const char *pcSsid, const char *pcPassword,
                          const uint8_t *pucBssid);

/** @brief Get the device's current STA IP address as a string.
 *  @param pcOutBuf Buffer for "x.x.x.x" string (must be >= 16 bytes)
 *  @param ulBufLen Buffer size
 *  @return ESP_OK on success, ESP_FAIL if no IP available */
esp_err_t TS_WiFi_GetIpAddress(char *pcOutBuf, size_t ulBufLen);

#ifdef __cplusplus
}
#endif
