#include "TimeSync_API.h"
#include "NVS_API.h"
#include "esp_sntp.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "timesync";

#define TIMESYNC_NVS_NAMESPACE  "timesync"
#define TIMESYNC_NVS_KEY_TZ     "tz"
#define TIMESYNC_TZ_MAX_LEN     64
#define TIMESYNC_DEFAULT_TZ     "UTC0"

static bool s_bSynced = false;

/* ------------------------------------------------------------------ */
/* SNTP callback                                                       */
/* ------------------------------------------------------------------ */
static void
timeSync_SntpCallback(struct timeval* ptTv)
{
    s_bSynced = true;
    ESP_LOGI(TAG, "SNTP time synchronized");
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
/* Load timezone from NVS and apply                                    */
/* ------------------------------------------------------------------ */
static void
timeSync_ApplyStoredTimezone(void)
{
    nvs_handle_t hNvs;
    char acTz[TIMESYNC_TZ_MAX_LEN] = { 0 };

    if (NVS_Open(TIMESYNC_NVS_NAMESPACE, NVS_READONLY, &hNvs) == ESP_OK)
    {
        size_t ulLen = sizeof(acTz);
        if (NVS_ReadString(hNvs, TIMESYNC_NVS_KEY_TZ, acTz, &ulLen) == ESP_OK)
        {
            ESP_LOGI(TAG, "Loaded timezone: %s", acTz);
        }
        NVS_Close(hNvs);
    }

    if (acTz[0] == '\0')
    {
        strncpy(acTz, TIMESYNC_DEFAULT_TZ, sizeof(acTz) - 1);
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

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_setservername(2, "time.cloudflare.com");
    esp_sntp_set_time_sync_notification_cb(timeSync_SntpCallback);
    esp_sntp_init();

    /* Restart SNTP immediately whenever WiFi gets an IP */
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               timeSync_IpEventHandler, NULL);

    ESP_LOGI(TAG, "SNTP initialised (3 servers, restart-on-IP enabled)");
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
    return s_bSynced;
}
