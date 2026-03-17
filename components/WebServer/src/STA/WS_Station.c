#include "WS_Station.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "React/Ws_React.h"
#include "React/WS_React_Routes.h"
#include "React/RestAPI/Example/ExampleAPI.h"
#include "React/RestAPI/Schedule/ScheduleAPI.h"
#include "Auth/WS_Auth.h"

static const char* TAG = "WS_STATION";

#define WIFI_SCAN_MAX_AP    20

static EXAMPLE_API_H s_hExampleApi = NULL;
static SCHEDULE_API_H s_hScheduleApi = NULL;

/* ----------------------------------------------------------------
   Helper: check whether the STA interface has an active connection
   to an external access-point.
   ---------------------------------------------------------------- */
static bool
ws_Station_IsStaConnected(void)
{
    wifi_ap_record_t tApInfo;
    return (ESP_OK == esp_wifi_sta_get_ap_info(&tApInfo));
}

/* /api/wifi/status — returns current mode based on actual STA state.
   When STA is connected the frontend shows the normal app;
   when STA is NOT connected the frontend shows the WiFi config page
   so the user can enter new credentials via the fallback soft-AP.   */
static esp_err_t
ws_Station_WifiStatusHandler(httpd_req_t* ptReq)
{
    httpd_resp_set_type(ptReq, "application/json");

    if (ws_Station_IsStaConnected())
    {
        httpd_resp_sendstr(ptReq, "{\"mode\":\"STA\"}");
    }
    else
    {
        httpd_resp_sendstr(ptReq, "{\"mode\":\"AP\",\"ap_ssid\":\"ESP32_Setup\"}");
    }

    return ESP_OK;
}

// ----------------------------------------------------------------
// POST /api/wifi/config  (STA mode — auth required)
// Body:     { "ssid": "MyNetwork", "password": "secret" }
// Response: { "status": "ok", "message": "Credentials saved. Restarting..." }
// ----------------------------------------------------------------
static esp_err_t
ws_Station_WifiConfigHandler(httpd_req_t* ptReq)
{
    /* When the STA is connected the user is on the normal network
       and must be authenticated.  When STA is NOT connected the
       user is on the fallback soft-AP — skip auth (same policy as
       the dedicated AP-mode WiFi config server).                  */
    if (ws_Station_IsStaConnected())
    {
        esp_err_t espAuth = auth_require_bearer(ptReq, NULL, NULL);
        if (ESP_OK != espAuth) return espAuth;
    }

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

    httpd_resp_set_type(ptReq, "application/json");
    httpd_resp_sendstr(ptReq, pcJson);

    cJSON_Delete(ptResp);
    free((void*)pcJson);

    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

    return ESP_OK; /* unreachable */
}

// ----------------------------------------------------------------
// GET /api/wifi/networks  (STA mode — auth required)
// ----------------------------------------------------------------
static const char*
ws_Station_AuthmodeToStr(wifi_auth_mode_t eAuthmode)
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
ws_Station_WifiNetworksHandler(httpd_req_t* ptReq)
{
    /* Require auth only when STA is connected (see config handler). */
    if (ws_Station_IsStaConnected())
    {
        esp_err_t espErr = auth_require_bearer(ptReq, NULL, NULL);
        if (ESP_OK != espErr) return espErr;
    }

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
        char acBssid[18];
        snprintf(acBssid, sizeof(acBssid), "%02X:%02X:%02X:%02X:%02X:%02X",
                 ptApRecords[i].bssid[0], ptApRecords[i].bssid[1],
                 ptApRecords[i].bssid[2], ptApRecords[i].bssid[3],
                 ptApRecords[i].bssid[4], ptApRecords[i].bssid[5]);

        cJSON* ptEntry = cJSON_CreateObject();
        cJSON_AddStringToObject(ptEntry, "ssid",     (char*)ptApRecords[i].ssid);
        cJSON_AddStringToObject(ptEntry, "bssid",    acBssid);
        cJSON_AddNumberToObject(ptEntry, "rssi",     ptApRecords[i].rssi);
        cJSON_AddNumberToObject(ptEntry, "channel",  ptApRecords[i].primary);
        cJSON_AddStringToObject(ptEntry, "authmode", ws_Station_AuthmodeToStr(ptApRecords[i].authmode));
        cJSON_AddBoolToObject(ptEntry,   "secured",  ptApRecords[i].authmode != WIFI_AUTH_OPEN);
        cJSON_AddItemToArray(ptNetworks, ptEntry);
    }

    free(ptApRecords);

    const char* pcJson = cJSON_PrintUnformatted(ptRoot);
    httpd_resp_set_type(ptReq, "application/json");
    httpd_resp_sendstr(ptReq, pcJson);

    cJSON_Delete(ptRoot);
    free((void*)pcJson);

    return ESP_OK;
}

esp_err_t
WS_Station_Start(SCHEDULER_H hScheduler, WIFI_MANAGER_H hWiFiManager)
{
    esp_err_t espRslt = ESP_OK;
    httpd_config_t tHttpServerConfig = HTTPD_DEFAULT_CONFIG();

    (void)hWiFiManager; /* WiFi_Manager_SaveCredentials is a free function */
    tHttpServerConfig.max_uri_handlers = 64;
    tHttpServerConfig.stack_size = 16384;
    tHttpServerConfig.uri_match_fn = httpd_uri_match_wildcard;
    // Large JS bundles can exceed the default 5-second socket send timeout;
    // raise both directions to prevent EAGAIN drops mid-transfer.
    tHttpServerConfig.send_wait_timeout = 30;
    tHttpServerConfig.recv_wait_timeout = 30;
    httpd_handle_t hHttpServer = NULL;

    espRslt = httpd_start(&hHttpServer, &tHttpServerConfig);

    if (ESP_OK == espRslt)
    {
        espRslt = Ws_React_RegisterStaticFiles(hHttpServer);
    }

    if (ESP_OK == espRslt)
    {
        espRslt = Ws_React_RegisterApiHandlers(hHttpServer);
    }

    if (ESP_OK == espRslt)
    {
        EXAMPLE_API_PARAMS_T tExampleParams = {0};
        espRslt = ExampleAPI_Init(&tExampleParams, &s_hExampleApi);
    }

    if (ESP_OK == espRslt)
    {
        espRslt = ExampleAPI_Register(s_hExampleApi, hHttpServer);
    }

    if (ESP_OK == espRslt)
    {
        SCHEDULE_API_PARAMS_T tScheduleParams = { .hScheduler = hScheduler };
        espRslt = ScheduleAPI_Init(&tScheduleParams, &s_hScheduleApi);
    }

    if (ESP_OK == espRslt)
    {
        espRslt = ScheduleAPI_Register(s_hScheduleApi, hHttpServer);
    }

    /* WiFi status endpoint — lets the frontend detect STA mode */
    if (ESP_OK == espRslt)
    {
        httpd_uri_t tWifiStatus = {
            .uri      = "/api/wifi/status",
            .method   = HTTP_GET,
            .handler  = ws_Station_WifiStatusHandler,
            .user_ctx = NULL,
        };
        espRslt = httpd_register_uri_handler(hHttpServer, &tWifiStatus);
    }

    /* WiFi config endpoint — save new credentials and restart */
    if (ESP_OK == espRslt)
    {
        httpd_uri_t tWifiConfig = {
            .uri      = "/api/wifi/config",
            .method   = HTTP_POST,
            .handler  = ws_Station_WifiConfigHandler,
            .user_ctx = NULL,
        };
        espRslt = httpd_register_uri_handler(hHttpServer, &tWifiConfig);
    }

    /* WiFi networks endpoint — scan for available networks */
    if (ESP_OK == espRslt)
    {
        httpd_uri_t tWifiNetworks = {
            .uri      = "/api/wifi/networks",
            .method   = HTTP_GET,
            .handler  = ws_Station_WifiNetworksHandler,
            .user_ctx = NULL,
        };
        espRslt = httpd_register_uri_handler(hHttpServer, &tWifiNetworks);
    }

    /* Register the catch-all wildcard LAST so it does not shadow API routes */
    if (ESP_OK == espRslt)
    {
        espRslt = Ws_React_RegisterCatchAll(hHttpServer);
    }

    return espRslt;
}