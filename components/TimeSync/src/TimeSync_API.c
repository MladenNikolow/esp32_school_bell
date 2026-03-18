#include "TimeSync_API.h"
#include "NVS_API.h"
#include "SPIFFS_API.h"
#include "cJSON.h"
#include "esp_sntp.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
#include <stdatomic.h>

static const char* TAG = "timesync";

#define TIMESYNC_NVS_NAMESPACE      "timesync"
#define TIMESYNC_NVS_KEY_TZ         "tz"
#define TIMESYNC_TZ_MAX_LEN         64
#define TIMESYNC_DEFAULT_TZ         "UTC0"

/** Year threshold: anything before 2024 is considered invalid */
#define TIMESYNC_MIN_VALID_YEAR     124   /* struct tm year since 1900 */

/** If the last successful SNTP sync is older than this, IsSynced()
 *  returns false.  At ~40 ppm crystal drift, 24 h produces ~3.5 s
 *  of error — well within acceptable range for bell scheduling. */
#define TIMESYNC_STALE_THRESHOLD_SEC  86400

/** Path to SPIFFS settings.json (timezone fallback after NVS erase) */
#define TIMESYNC_SPIFFS_SETTINGS    "/storage/settings.json"

static atomic_bool  s_bSynced          = false;
static bool         s_bStaleWarned     = false;  /* one-shot stale warning */
static int64_t      s_llLastSyncTimeUs = 0;     /* esp_timer_get_time() */

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static bool
timeSync_IsSystemTimeValid(void)
{
    time_t tNow;
    time(&tNow);
    struct tm tTm;
    gmtime_r(&tNow, &tTm);
    return (tTm.tm_year >= TIMESYNC_MIN_VALID_YEAR);
}

/* ------------------------------------------------------------------ */
/* SNTP callback                                                       */
/* ------------------------------------------------------------------ */
static void
timeSync_SntpCallback(struct timeval* ptTv)
{
    if (timeSync_IsSystemTimeValid())
    {
        s_llLastSyncTimeUs = esp_timer_get_time();
        s_bStaleWarned = false;
        atomic_store(&s_bSynced, true);
        ESP_LOGI(TAG, "SNTP time synchronized");
    }
    else
    {
        ESP_LOGW(TAG, "SNTP callback but system time still invalid (year < 2024)");
    }
}

/* ------------------------------------------------------------------ */
/* Restart SNTP when WiFi obtains an IP address                        */
/* ------------------------------------------------------------------ */
static void
timeSync_IpEventHandler(void* pvArg, esp_event_base_t tEventBase,
                        int32_t lEventId, void* pvEventData)
{
    (void)pvArg;
    (void)tEventBase;
    (void)lEventId;
    (void)pvEventData;

    ESP_LOGI(TAG, "Got IP — restarting SNTP");
    esp_sntp_restart();
}

/* ------------------------------------------------------------------ */
/* Load timezone: NVS first, then SPIFFS settings.json fallback        */
/* ------------------------------------------------------------------ */
static void
timeSync_ApplyStoredTimezone(void)
{
    nvs_handle_t hNvs;
    char acTz[TIMESYNC_TZ_MAX_LEN] = { 0 };

    /* --- Try NVS first --- */
    if (NVS_Open(TIMESYNC_NVS_NAMESPACE, NVS_READONLY, &hNvs) == ESP_OK)
    {
        size_t ulLen = sizeof(acTz);
        if (NVS_ReadString(hNvs, TIMESYNC_NVS_KEY_TZ, acTz, &ulLen) == ESP_OK)
        {
            ESP_LOGI(TAG, "Loaded timezone from NVS: %s", acTz);
        }
        NVS_Close(hNvs);
    }

    /* --- SPIFFS fallback (covers erase-flash scenario) --- */
    if (acTz[0] == '\0' && SPIFFS_FileExists(TIMESYNC_SPIFFS_SETTINGS))
    {
        ESP_LOGI(TAG, "NVS timezone empty — trying SPIFFS settings.json");
        FILE* f = fopen(TIMESYNC_SPIFFS_SETTINGS, "r");
        if (f)
        {
            fseek(f, 0, SEEK_END);
            long lSize = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (lSize > 0 && lSize < 4096)
            {
                char* pcBuf = (char*)malloc((size_t)lSize + 1);
                if (pcBuf)
                {
                    size_t ulRead = fread(pcBuf, 1, (size_t)lSize, f);
                    pcBuf[ulRead] = '\0';
                    cJSON* ptRoot = cJSON_Parse(pcBuf);
                    if (ptRoot)
                    {
                        cJSON* ptTz = cJSON_GetObjectItem(ptRoot, "timezone");
                        if (ptTz && cJSON_IsString(ptTz) && ptTz->valuestring[0] != '\0')
                        {
                            strncpy(acTz, ptTz->valuestring, sizeof(acTz) - 1);
                            ESP_LOGI(TAG, "Loaded timezone from SPIFFS: %s", acTz);
                        }
                        cJSON_Delete(ptRoot);
                    }
                    free(pcBuf);
                }
            }
            fclose(f);
        }
    }

    if (acTz[0] == '\0')
    {
        strncpy(acTz, TIMESYNC_DEFAULT_TZ, sizeof(acTz) - 1);
        ESP_LOGI(TAG, "Using default timezone: %s", acTz);
    }

    setenv("TZ", acTz, 1);
    tzset();
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
esp_err_t
TimeSync_Init(void)
{
    timeSync_ApplyStoredTimezone();

    /* RTC fast-path: after a soft reboot the internal RTC may still
       hold a valid time.  Pre-set the synced flag so the scheduler
       is not blocked waiting for NTP. */
    if (timeSync_IsSystemTimeValid())
    {
        atomic_store(&s_bSynced, true);
        s_llLastSyncTimeUs = esp_timer_get_time();
        ESP_LOGI(TAG, "RTC time already valid (year >= 2024) — pre-synced");
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_setservername(2, "time.cloudflare.com");
    esp_sntp_set_time_sync_notification_cb(timeSync_SntpCallback);
    esp_sntp_init();

    /* Restart SNTP immediately whenever WiFi gets an IP */
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               timeSync_IpEventHandler, NULL);

    ESP_LOGI(TAG, "SNTP initialised (3 servers, restart-on-IP enabled, "
             "stale threshold %d s)", TIMESYNC_STALE_THRESHOLD_SEC);
    return ESP_OK;
}

esp_err_t
TimeSync_SetTimezone(const char* pcTzPosix)
{
    if (NULL == pcTzPosix)
    {
        return ESP_ERR_INVALID_ARG;
    }

    /* Persist to NVS */
    nvs_handle_t hNvs;
    esp_err_t espRslt = NVS_Open(TIMESYNC_NVS_NAMESPACE, NVS_READWRITE, &hNvs);
    if (ESP_OK == espRslt)
    {
        espRslt = NVS_WriteString(hNvs, TIMESYNC_NVS_KEY_TZ, pcTzPosix);
        if (ESP_OK == espRslt)
        {
            espRslt = NVS_Commit(hNvs);
        }
        NVS_Close(hNvs);
    }

    /* Apply immediately */
    setenv("TZ", pcTzPosix, 1);
    tzset();

    ESP_LOGI(TAG, "Timezone set to: %s", pcTzPosix);
    return espRslt;
}

esp_err_t
TimeSync_GetTimezone(char* pcOutBuf, size_t ulBufLen)
{
    if ((NULL == pcOutBuf) || (0 == ulBufLen))
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t hNvs;
    esp_err_t espRslt = NVS_Open(TIMESYNC_NVS_NAMESPACE, NVS_READONLY, &hNvs);
    if (ESP_OK == espRslt)
    {
        size_t ulLen = ulBufLen;
        espRslt = NVS_ReadString(hNvs, TIMESYNC_NVS_KEY_TZ, pcOutBuf, &ulLen);
        NVS_Close(hNvs);
    }

    if (espRslt != ESP_OK)
    {
        strncpy(pcOutBuf, TIMESYNC_DEFAULT_TZ, ulBufLen - 1);
        pcOutBuf[ulBufLen - 1] = '\0';
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

esp_err_t
TimeSync_GetLocalTime(struct tm* ptTimeInfo)
{
    if (NULL == ptTimeInfo)
    {
        return ESP_ERR_INVALID_ARG;
    }

    time_t tNow;
    time(&tNow);
    localtime_r(&tNow, ptTimeInfo);

    return ESP_OK;
}

bool
TimeSync_IsSynced(void)
{
    if (!atomic_load(&s_bSynced))
    {
        return false;
    }

    /* Check staleness: if last sync is too old, consider unsynced */
    int64_t llAge = esp_timer_get_time() - s_llLastSyncTimeUs;
    if (llAge > (int64_t)TIMESYNC_STALE_THRESHOLD_SEC * 1000000LL)
    {
        if (!s_bStaleWarned)
        {
            ESP_LOGW(TAG, "SNTP sync stale (last sync %lld s ago, threshold %d s) "
                     "— bell scheduling paused until re-sync",
                     (long long)(llAge / 1000000LL), TIMESYNC_STALE_THRESHOLD_SEC);
            s_bStaleWarned = true;
        }
        return false;
    }

    s_bStaleWarned = false;
    return true;
}

uint32_t
TimeSync_GetLastSyncAgeSec(void)
{
    if (s_llLastSyncTimeUs == 0)
    {
        return UINT32_MAX;
    }

    int64_t llAge = esp_timer_get_time() - s_llLastSyncTimeUs;
    if (llAge < 0) llAge = 0;

    return (uint32_t)(llAge / 1000000LL);
}

void
TimeSync_ForceSync(void)
{
    ESP_LOGI(TAG, "Forcing SNTP re-sync");
    esp_sntp_restart();
}
