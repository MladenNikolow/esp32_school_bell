#include "WS_Station.h"
#include "esp_http_server.h"
#include "React/Ws_React.h"
#include "React/WS_React_Routes.h"
#include "React/RestAPI/Example/ExampleAPI.h"
#include "React/RestAPI/Schedule/ScheduleAPI.h"

static EXAMPLE_API_H s_hExampleApi = NULL;
static SCHEDULE_API_H s_hScheduleApi = NULL;

/* Minimal /api/wifi/status for STA mode so the React frontend can
   distinguish AP from STA without hitting the catch-all file server. */
static esp_err_t
ws_Station_WifiStatusHandler(httpd_req_t* ptReq)
{
    httpd_resp_set_type(ptReq, "application/json");
    httpd_resp_sendstr(ptReq, "{\"mode\":\"STA\"}");
    return ESP_OK;
}

esp_err_t
WS_Station_Start(SCHEDULER_H hScheduler)
{
    esp_err_t espRslt = ESP_OK;
    httpd_config_t tHttpServerConfig = HTTPD_DEFAULT_CONFIG();
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

    /* Register the catch-all wildcard LAST so it does not shadow API routes */
    if (ESP_OK == espRslt)
    {
        espRslt = Ws_React_RegisterCatchAll(hHttpServer);
    }

    return espRslt;
}