#include "WS_AccessPoint.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "React/WS_React_Routes.h"
#include "AP/RestAPI/WS_WiFiConfigAPI.h"

static const char* TAG = "WS_ACCESS_POINT";

static WIFI_CONFIG_API_H s_hWifiConfigApi = NULL;
static httpd_handle_t   s_hHttpServer     = NULL;

esp_err_t
WS_AccessPoint_Start(WIFI_MANAGER_H hWiFiManager)
{
    esp_err_t espRslt = ESP_OK;

    /* Already running — nothing to do */
    if (NULL != s_hHttpServer)
    {
        return ESP_OK;
    }

    httpd_config_t tHttpServerConfig = HTTPD_DEFAULT_CONFIG();
    tHttpServerConfig.max_uri_handlers  = 16;
    tHttpServerConfig.stack_size        = 8192;
    tHttpServerConfig.uri_match_fn      = httpd_uri_match_wildcard;
    tHttpServerConfig.send_wait_timeout = 30;
    tHttpServerConfig.recv_wait_timeout = 30;

    espRslt = httpd_start(&s_hHttpServer, &tHttpServerConfig);

    if (ESP_OK == espRslt)
    {
        espRslt = Ws_React_RegisterRoutes(s_hHttpServer);
    }

    if (ESP_OK == espRslt)
    {
        if (NULL == s_hWifiConfigApi)
        {
            WIFI_CONFIG_API_PARAMS_T tApiParams = { .hWiFiManager = hWiFiManager };
            espRslt = WiFiConfigAPI_Init(&tApiParams, &s_hWifiConfigApi);
        }
    }

    if (ESP_OK == espRslt)
    {
        espRslt = WiFiConfigAPI_Register(s_hWifiConfigApi, s_hHttpServer);
    }

    if (ESP_OK == espRslt)
    {
        ESP_LOGI(TAG, "AP HTTP server started (React + WiFi Config API)");
    }
    else
    {
        /* Clean up on failure */
        if (NULL != s_hHttpServer)
        {
            httpd_stop(s_hHttpServer);
            s_hHttpServer = NULL;
        }
    }

    return espRslt;
}

esp_err_t
WS_AccessPoint_Stop(void)
{
    if (NULL != s_hHttpServer)
    {
        httpd_stop(s_hHttpServer);
        s_hHttpServer = NULL;
        ESP_LOGI(TAG, "AP HTTP server stopped");
    }

    return ESP_OK;
}