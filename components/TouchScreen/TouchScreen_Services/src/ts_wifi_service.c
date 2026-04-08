/* ================================================================== */
/* ts_wifi_service.c — WiFi scan, credential save, connection status   */
/* ================================================================== */
#include "TouchScreen_Services.h"
#include "WiFi_Manager_API.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdlib.h>
#include <string.h>

/* Forward declarations — implemented in WS_EventHandlers.c (WebServer).
   We avoid #include "WS_EventHandlers.h" to prevent a circular
   component dependency (WebServer already REQUIRES TouchScreen).        */
extern void Ws_EventHandlers_SuspendReconnect(void);
extern void Ws_EventHandlers_ResumeReconnect(void);

static const char *TAG = "TS_WIFI";

/* ------------------------------------------------------------------ */
/* Async scan state                                                    */
/* ------------------------------------------------------------------ */
static volatile bool     s_async_scan_running  = false;
static volatile bool     s_async_scan_complete = false;
static volatile esp_err_t s_async_scan_result  = ESP_FAIL;
static TS_WiFi_AP_t     s_async_ap_list[TS_WIFI_SCAN_MAX_AP];
static uint16_t          s_async_ap_count = 0;
static TaskHandle_t      s_scan_task_handle = NULL;

#define SCAN_TASK_STACK_SIZE  4096
#define SCAN_TASK_PRIORITY    2

/* ------------------------------------------------------------------ */
/* Probe scan helper — discover BSSID for a manually-entered SSID      */
/* Runs a blocking scan with show_hidden=true, matches by SSID.        */
/* Returns ESP_OK if found (BSSID written to pucBssidOut), else error.  */
/* ------------------------------------------------------------------ */
static esp_err_t
probe_hidden_bssid(const char *pcSsid, uint8_t *pucBssidOut)
{
    wifi_scan_config_t tScanCfg = {
        .show_hidden = true,
    };

    esp_err_t err = esp_wifi_scan_start(&tScanCfg, true);
    if (ESP_OK != err)
    {
        ESP_LOGW(TAG, "Probe scan failed to start: %s", esp_err_to_name(err));
        return err;
    }

    uint16_t usApCount = 0;
    esp_wifi_scan_get_ap_num(&usApCount);
    if (usApCount == 0)
    {
        esp_wifi_scan_get_ap_records(&usApCount, NULL);
        return ESP_ERR_NOT_FOUND;
    }

    wifi_ap_record_t *ptRecords = calloc(usApCount, sizeof(wifi_ap_record_t));
    if (NULL == ptRecords)
    {
        return ESP_ERR_NO_MEM;
    }

    err = esp_wifi_scan_get_ap_records(&usApCount, ptRecords);
    if (ESP_OK != err)
    {
        free(ptRecords);
        return err;
    }

    /* Find best match (strongest RSSI) for the target SSID */
    int8_t cBestRssi = -128;
    bool bFound = false;
    for (uint16_t i = 0; i < usApCount; i++)
    {
        if (strcmp((const char *)ptRecords[i].ssid, pcSsid) == 0)
        {
            if (ptRecords[i].rssi > cBestRssi)
            {
                cBestRssi = ptRecords[i].rssi;
                memcpy(pucBssidOut, ptRecords[i].bssid, 6);
                bFound = true;
            }
        }
    }

    free(ptRecords);

    if (bFound)
    {
        ESP_LOGI(TAG, "Probe found BSSID %02X:%02X:%02X:%02X:%02X:%02X for '%s' (RSSI %d)",
                 pucBssidOut[0], pucBssidOut[1], pucBssidOut[2],
                 pucBssidOut[3], pucBssidOut[4], pucBssidOut[5],
                 pcSsid, cBestRssi);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Probe scan: SSID '%s' not found", pcSsid);
    return ESP_ERR_NOT_FOUND;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t
TS_WiFi_Scan(TS_WiFi_AP_t *ptResults, uint16_t *pusCount)
{
    if (ptResults == NULL || pusCount == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    /* Blocking scan */
    esp_err_t err = esp_wifi_scan_start(NULL, true);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "WiFi scan start failed: %s", esp_err_to_name(err));
        *pusCount = 0;
        return err;
    }

    uint16_t usApCount = TS_WIFI_SCAN_MAX_AP;
    wifi_ap_record_t atRecords[TS_WIFI_SCAN_MAX_AP];
    memset(atRecords, 0, sizeof(atRecords));

    err = esp_wifi_scan_get_ap_records(&usApCount, atRecords);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Failed to get scan results: %s", esp_err_to_name(err));
        *pusCount = 0;
        return err;
    }

    /* Copy into our simplified struct */
    for (uint16_t i = 0; i < usApCount; i++)
    {
        strncpy(ptResults[i].acSsid, (const char *)atRecords[i].ssid,
                sizeof(ptResults[i].acSsid) - 1);
        ptResults[i].acSsid[sizeof(ptResults[i].acSsid) - 1] = '\0';
        memcpy(ptResults[i].abBssid, atRecords[i].bssid, 6);
        ptResults[i].cRssi    = atRecords[i].rssi;
        ptResults[i].bSecured = (atRecords[i].authmode != WIFI_AUTH_OPEN);
        ptResults[i].ucChannel = atRecords[i].primary;
    }

    *pusCount = usApCount;
    ESP_LOGI(TAG, "Scan complete: %u networks found", usApCount);
    return ESP_OK;
}

esp_err_t
TS_WiFi_SaveCredentials(const char *pcSsid, const char *pcPassword)
{
    if (pcSsid == NULL || strlen(pcSsid) == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Saving WiFi credentials for SSID: %s", pcSsid);
    return WiFi_Manager_SaveCredentials(pcSsid, pcPassword ? pcPassword : "", NULL);
}

bool
TS_WiFi_IsConnected(void)
{
    wifi_ap_record_t tApInfo;
    return (ESP_OK == esp_wifi_sta_get_ap_info(&tApInfo));
}

esp_err_t
TS_WiFi_GetConnectedSsid(char *pcOutBuf, size_t ulBufLen)
{
    if (pcOutBuf == NULL || ulBufLen == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    pcOutBuf[0] = '\0';

    wifi_ap_record_t tApInfo;
    esp_err_t err = esp_wifi_sta_get_ap_info(&tApInfo);
    if (ESP_OK != err)
    {
        return err;
    }

    strncpy(pcOutBuf, (const char *)tApInfo.ssid, ulBufLen - 1);
    pcOutBuf[ulBufLen - 1] = '\0';
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Async scan — runs blocking scan in a background FreeRTOS task       */
/* ------------------------------------------------------------------ */

static void
scan_task_func(void *pvArg)
{
    (void)pvArg;

    /* Suspend auto-reconnect so esp_wifi_connect() doesn't race the scan */
    Ws_EventHandlers_SuspendReconnect();

    /* Disconnect STA so the radio is idle — scan cannot start while connecting */
    (void)esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Clear any stuck scan state */
    (void)esp_wifi_scan_stop();

    /* Blocking scan with retry — the radio may need a moment after disconnect */
    esp_err_t err = ESP_FAIL;
    for (int retry = 0; retry < 3; retry++)
    {
        err = esp_wifi_scan_start(NULL, true);
        if (ESP_OK == err)
        {
            break;
        }
        ESP_LOGW(TAG, "Scan attempt %d failed: %s", retry + 1, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Async scan start failed after retries: %s", esp_err_to_name(err));
        s_async_ap_count = 0;
        s_async_scan_result = err;
        s_async_scan_complete = true;
        s_async_scan_running = false;
        s_scan_task_handle = NULL;
        Ws_EventHandlers_ResumeReconnect();
        vTaskDelete(NULL);
        return;
    }

    uint16_t usApCount = TS_WIFI_SCAN_MAX_AP;
    wifi_ap_record_t *ptRecords = calloc(TS_WIFI_SCAN_MAX_AP, sizeof(wifi_ap_record_t));
    if (NULL == ptRecords)
    {
        ESP_LOGE(TAG, "Failed to allocate scan result buffer");
        s_async_ap_count = 0;
        s_async_scan_result = ESP_ERR_NO_MEM;
        s_async_scan_complete = true;
        s_async_scan_running = false;
        s_scan_task_handle = NULL;
        Ws_EventHandlers_ResumeReconnect();
        vTaskDelete(NULL);
        return;
    }

    err = esp_wifi_scan_get_ap_records(&usApCount, ptRecords);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Async scan get results failed: %s", esp_err_to_name(err));
        free(ptRecords);
        s_async_ap_count = 0;
        s_async_scan_result = err;
        s_async_scan_complete = true;
        s_async_scan_running = false;
        s_scan_task_handle = NULL;
        Ws_EventHandlers_ResumeReconnect();
        vTaskDelete(NULL);
        return;
    }

    /* Copy into shared buffer */
    for (uint16_t i = 0; i < usApCount; i++)
    {
        strncpy(s_async_ap_list[i].acSsid, (const char *)ptRecords[i].ssid,
                sizeof(s_async_ap_list[i].acSsid) - 1);
        s_async_ap_list[i].acSsid[sizeof(s_async_ap_list[i].acSsid) - 1] = '\0';
        memcpy(s_async_ap_list[i].abBssid, ptRecords[i].bssid, 6);
        s_async_ap_list[i].cRssi    = ptRecords[i].rssi;
        s_async_ap_list[i].bSecured = (ptRecords[i].authmode != WIFI_AUTH_OPEN);
        s_async_ap_list[i].ucChannel = ptRecords[i].primary;
    }

    free(ptRecords);

    s_async_ap_count = usApCount;
    s_async_scan_result = ESP_OK;
    s_async_scan_complete = true;
    s_async_scan_running = false;
    s_scan_task_handle = NULL;

    /* Resume auto-reconnect — the STA_DISCONNECTED event from our
       earlier esp_wifi_disconnect() will trigger reconnect scheduling. */
    Ws_EventHandlers_ResumeReconnect();

    ESP_LOGI(TAG, "Async scan complete: %u networks found", usApCount);
    vTaskDelete(NULL);
}

esp_err_t
TS_WiFi_ScanAsync(void)
{
    if (s_async_scan_running)
    {
        ESP_LOGW(TAG, "Async scan already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    s_async_scan_complete = false;
    s_async_scan_result = ESP_FAIL;
    s_async_ap_count = 0;
    s_async_scan_running = true;

    BaseType_t ret = xTaskCreate(scan_task_func, "wifi_scan", SCAN_TASK_STACK_SIZE,
                                 NULL, SCAN_TASK_PRIORITY, &s_scan_task_handle);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create scan task");
        s_async_scan_running = false;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

bool
TS_WiFi_IsScanComplete(void)
{
    return s_async_scan_complete;
}

esp_err_t
TS_WiFi_ScanGetResults(TS_WiFi_AP_t *ptResults, uint16_t *pusCount)
{
    if (ptResults == NULL || pusCount == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_async_scan_complete)
    {
        *pusCount = 0;
        return ESP_ERR_INVALID_STATE;
    }

    if (s_async_scan_result != ESP_OK)
    {
        *pusCount = 0;
        return s_async_scan_result;
    }

    memcpy(ptResults, s_async_ap_list, sizeof(TS_WiFi_AP_t) * s_async_ap_count);
    *pusCount = s_async_ap_count;
    return ESP_OK;
}

void
TS_WiFi_ScanAbort(void)
{
    if (s_async_scan_running)
    {
        (void)esp_wifi_scan_stop();
        /* The task will fail and clean up on its own */
    }
    s_async_scan_running = false;
    s_async_scan_complete = false;
}

esp_err_t
TS_WiFi_Connect(const char *pcSsid, const char *pcPassword, const uint8_t *pucBssid)
{
    if (pcSsid == NULL || strlen(pcSsid) == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const char *pcPw = pcPassword ? pcPassword : "";

    /* If no BSSID provided, attempt a probe scan to discover it
       (handles hidden networks entered manually via SSID).         */
    uint8_t abProbedBssid[6] = {0};
    if (pucBssid == NULL || memcmp(pucBssid, abProbedBssid, 6) == 0)
    {
        if (probe_hidden_bssid(pcSsid, abProbedBssid) == ESP_OK)
        {
            pucBssid = abProbedBssid;
        }
    }

    /* 1. Persist credentials to NVS so they survive reboot */
    esp_err_t err = WiFi_Manager_SaveCredentials(pcSsid, pcPw, pucBssid);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Failed to save credentials: %s", esp_err_to_name(err));
        return err;
    }

    /* 2. Update running STA config without rebooting */
    wifi_config_t tStaCfg = {0};
    strncpy((char *)tStaCfg.sta.ssid, pcSsid, sizeof(tStaCfg.sta.ssid) - 1);
    strncpy((char *)tStaCfg.sta.password, pcPw, sizeof(tStaCfg.sta.password) - 1);

    /* Pin to specific AP if BSSID is provided and non-zero */
    if (pucBssid != NULL)
    {
        static const uint8_t abZero[6] = {0};
        if (memcmp(pucBssid, abZero, 6) != 0)
        {
            memcpy(tStaCfg.sta.bssid, pucBssid, 6);
            tStaCfg.sta.bssid_set = true;
            ESP_LOGI(TAG, "BSSID pinning: %02X:%02X:%02X:%02X:%02X:%02X",
                     pucBssid[0], pucBssid[1], pucBssid[2],
                     pucBssid[3], pucBssid[4], pucBssid[5]);
        }
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &tStaCfg);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Failed to set STA config: %s", esp_err_to_name(err));
        return err;
    }

    /* 3. Disconnect current session (triggers DISCONNECTED event which
     *    feeds into the existing retry mechanism in WS_EventHandlers).
     *    Then initiate a fresh connection attempt. */
    (void)esp_wifi_disconnect();
    err = esp_wifi_connect();

    ESP_LOGI(TAG, "WiFi reconnecting to '%s'...", pcSsid);
    return err;
}

esp_err_t
TS_WiFi_GetIpAddress(char *pcOutBuf, size_t ulBufLen)
{
    if (pcOutBuf == NULL || ulBufLen < 16)
    {
        return ESP_ERR_INVALID_ARG;
    }

    pcOutBuf[0] = '\0';

    esp_netif_t *pNetif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (pNetif == NULL)
    {
        return ESP_FAIL;
    }

    esp_netif_ip_info_t tIpInfo;
    esp_err_t err = esp_netif_get_ip_info(pNetif, &tIpInfo);
    if (ESP_OK != err || tIpInfo.ip.addr == 0)
    {
        return ESP_FAIL;
    }

    snprintf(pcOutBuf, ulBufLen, IPSTR, IP2STR(&tIpInfo.ip));
    return ESP_OK;
}
