#include "WS_WiFiConfigAPI.h"

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "cJSON.h"
#include "Auth/WS_Auth.h"

static const char* TAG = "WIFI_CONFIG_API";

#define WIFI_SCAN_MAX_AP    20

typedef struct _WIFI_CONFIG_API_RSC_T
{
    WIFI_MANAGER_H hWiFiManager;
} WIFI_CONFIG_API_RSC_T;

// ----------------------------------------------------------------
// Forward declarations
// ----------------------------------------------------------------
static esp_err_t wifiConfigApi_GetStatus  (httpd_req_t* ptReq);
static esp_err_t wifiConfigApi_PostConfig (httpd_req_t* ptReq);
static esp_err_t wifiConfigApi_GetNetworks(httpd_req_t* ptReq);

// ----------------------------------------------------------------
// Init
// ----------------------------------------------------------------
esp_err_t
WiFiConfigAPI_Init(const WIFI_CONFIG_API_PARAMS_T* ptParams, WIFI_CONFIG_API_H* phApi)
{
    if ((NULL == ptParams) || (NULL == phApi))
    {
        return ESP_ERR_INVALID_ARG;
    }

    WIFI_CONFIG_API_RSC_T* ptRsc = (WIFI_CONFIG_API_RSC_T*)calloc(1, sizeof(WIFI_CONFIG_API_RSC_T));
    if (NULL == ptRsc)
    {
        return ESP_ERR_NO_MEM;
    }

    ptRsc->hWiFiManager = ptParams->hWiFiManager;

    *phApi = (WIFI_CONFIG_API_H)ptRsc;
    return ESP_OK;
}

// ----------------------------------------------------------------
// Register URI handlers
// ----------------------------------------------------------------
esp_err_t
WiFiConfigAPI_Register(WIFI_CONFIG_API_H hApi, httpd_handle_t hHttpServer)
{
    if ((NULL == hApi) || (NULL == hHttpServer))
    {
        return ESP_ERR_INVALID_ARG;
    }

    WIFI_CONFIG_API_RSC_T* ptRsc = (WIFI_CONFIG_API_RSC_T*)hApi;

    httpd_uri_t tStatusUri = {
        .uri      = "/api/wifi/status",
        .method   = HTTP_GET,
        .handler  = wifiConfigApi_GetStatus,
        .user_ctx = ptRsc,
    };

    esp_err_t espErr = httpd_register_uri_handler(hHttpServer, &tStatusUri);

    if (ESP_OK == espErr)
    {
        httpd_uri_t tConfigUri = {
            .uri      = "/api/wifi/config",
            .method   = HTTP_POST,
            .handler  = wifiConfigApi_PostConfig,
            .user_ctx = ptRsc,
        };
        espErr = httpd_register_uri_handler(hHttpServer, &tConfigUri);
    }

    if (ESP_OK == espErr)
    {
        httpd_uri_t tNetworksUri = {
            .uri      = "/api/wifi/networks",
            .method   = HTTP_GET,
            .handler  = wifiConfigApi_GetNetworks,
            .user_ctx = ptRsc,
        };
        espErr = httpd_register_uri_handler(hHttpServer, &tNetworksUri);
    }

    if (ESP_OK == espErr)
    {
        ESP_LOGI(TAG, "WiFi Config API registered: GET /api/wifi/status, POST /api/wifi/config, GET /api/wifi/networks");
    }

    return espErr;
}

// ----------------------------------------------------------------
// GET /api/wifi/status
// Response: { "mode": "AP", "ap_ssid": "<ssid>" }
// ----------------------------------------------------------------
static esp_err_t
wifiConfigApi_GetStatus(httpd_req_t* ptReq)
{
    auth_set_security_headers(ptReq);

    WIFI_CONFIG_API_RSC_T* ptRsc = (WIFI_CONFIG_API_RSC_T*)ptReq->user_ctx;

    uint8_t abSsid[WIFI_MANAGER_MAX_SSID_LENGTH + 1] = {0};
    size_t  szSsidLen = sizeof(abSsid);

    (void)WiFi_Manager_GetSsid(ptRsc->hWiFiManager, abSsid, &szSsidLen);

    cJSON* ptRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(ptRoot, "mode",    "AP");
    cJSON_AddStringToObject(ptRoot, "ap_ssid", (char*)abSsid);

    const char* pcJson = cJSON_PrintUnformatted(ptRoot);

    httpd_resp_sendstr(ptReq, pcJson);

    cJSON_Delete(ptRoot);
    free((void*)pcJson);

    return ESP_OK;
}

// ----------------------------------------------------------------
// POST /api/wifi/config
// Body:     { "ssid": "MyNetwork", "password": "secret" }
// Response: { "status": "ok", "message": "Credentials saved. Restarting..." }
// ----------------------------------------------------------------
static esp_err_t
wifiConfigApi_PostConfig(httpd_req_t* ptReq)
{
    auth_set_security_headers(ptReq);

    if (!auth_csrf_check(ptReq))
    {
        return ESP_OK;
    }

    (void)ptReq->user_ctx; /* WiFi_Manager_SaveCredentials is a free function */

    char acBuf[384];
    int  iLen = httpd_req_recv(ptReq, acBuf, sizeof(acBuf) - 1);

    if (iLen <= 0)
    {
        httpd_resp_send_err(ptReq, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
    acBuf[iLen] = '\0';

    cJSON* ptRoot = cJSON_Parse(acBuf);
    if (NULL == ptRoot)
    {
        httpd_resp_send_err(ptReq, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON* ptSsid = cJSON_GetObjectItem(ptRoot, "ssid");
    cJSON* ptPass = cJSON_GetObjectItem(ptRoot, "password");

    if (!ptSsid || !cJSON_IsString(ptSsid) || (0 == strlen(ptSsid->valuestring)))
    {
        cJSON_Delete(ptRoot);
        httpd_resp_send_err(ptReq, HTTPD_400_BAD_REQUEST, "Missing or empty 'ssid'");
        return ESP_FAIL;
    }

    const char* pcSsid = ptSsid->valuestring;
    const char* pcPass = (ptPass && cJSON_IsString(ptPass)) ? ptPass->valuestring : "";

    esp_err_t espErr = WiFi_Manager_SaveCredentials(pcSsid, pcPass);
    cJSON_Delete(ptRoot);

    if (ESP_OK != espErr)
    {
        ESP_LOGE(TAG, "Failed to save credentials: %s", esp_err_to_name(espErr));
        httpd_resp_send_err(ptReq, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save credentials");
        return espErr;
    }

    cJSON* ptResp = cJSON_CreateObject();
    cJSON_AddStringToObject(ptResp, "status",  "ok");
    cJSON_AddStringToObject(ptResp, "message", "Credentials saved. Restarting...");

    const char* pcJson = cJSON_PrintUnformatted(ptResp);

    httpd_resp_sendstr(ptReq, pcJson);

    cJSON_Delete(ptResp);
    free((void*)pcJson);

    /* Small delay so the HTTP response is flushed before the reset */
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

    return ESP_OK; /* unreachable */
}

// ----------------------------------------------------------------
// GET /api/wifi/networks
// Performs a blocking scan and returns:
// { "networks": [ { "ssid", "rssi", "channel", "authmode", "secured" }, ... ] }
// ----------------------------------------------------------------
static const char*
wifiConfigApi_AuthmodeToStr(wifi_auth_mode_t eAuthmode)
{
    switch (eAuthmode)
    {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
        default:                        return "UNKNOWN";
    }
}

static esp_err_t
wifiConfigApi_GetNetworks(httpd_req_t* ptReq)
{
    auth_set_security_headers(ptReq);

    (void)ptReq->user_ctx;

    /* Blocking scan — runs inside the httpd task, typically takes 1–2 s */
    esp_err_t espErr = esp_wifi_scan_start(NULL, true);
    if (ESP_OK != espErr)
    {
        ESP_LOGE(TAG, "WiFi scan start failed: %s", esp_err_to_name(espErr));
        httpd_resp_send_err(ptReq, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
        return espErr;
    }

    uint16_t usApCount = WIFI_SCAN_MAX_AP;
    wifi_ap_record_t* ptApRecords = (wifi_ap_record_t*)calloc(WIFI_SCAN_MAX_AP, sizeof(wifi_ap_record_t));
    if (NULL == ptApRecords)
    {
        httpd_resp_send_err(ptReq, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_ERR_NO_MEM;
    }

    espErr = esp_wifi_scan_get_ap_records(&usApCount, ptApRecords);
    if (ESP_OK != espErr)
    {
        free(ptApRecords);
        ESP_LOGE(TAG, "Failed to get scan results: %s", esp_err_to_name(espErr));
        httpd_resp_send_err(ptReq, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get scan results");
        return espErr;
    }

    cJSON* ptRoot     = cJSON_CreateObject();
    cJSON* ptNetworks = cJSON_AddArrayToObject(ptRoot, "networks");

    for (uint16_t i = 0; i < usApCount; i++)
    {
        char acBssid[18]; /* "AA:BB:CC:DD:EE:FF" + '\0' */
        snprintf(acBssid, sizeof(acBssid), "%02X:%02X:%02X:%02X:%02X:%02X",
                 ptApRecords[i].bssid[0], ptApRecords[i].bssid[1],
                 ptApRecords[i].bssid[2], ptApRecords[i].bssid[3],
                 ptApRecords[i].bssid[4], ptApRecords[i].bssid[5]);

        cJSON* ptEntry = cJSON_CreateObject();
        cJSON_AddStringToObject(ptEntry, "ssid",     (char*)ptApRecords[i].ssid);
        cJSON_AddStringToObject(ptEntry, "bssid",    acBssid);
        cJSON_AddNumberToObject(ptEntry, "rssi",     ptApRecords[i].rssi);
        cJSON_AddNumberToObject(ptEntry, "channel",  ptApRecords[i].primary);
        cJSON_AddStringToObject(ptEntry, "authmode", wifiConfigApi_AuthmodeToStr(ptApRecords[i].authmode));
        cJSON_AddBoolToObject  (ptEntry, "secured",  ptApRecords[i].authmode != WIFI_AUTH_OPEN);
        cJSON_AddItemToArray(ptNetworks, ptEntry);
    }

    free(ptApRecords);

    const char* pcJson = cJSON_PrintUnformatted(ptRoot);

    httpd_resp_sendstr(ptReq, pcJson);

    cJSON_Delete(ptRoot);
    free((void*)pcJson);

    return ESP_OK;
}
