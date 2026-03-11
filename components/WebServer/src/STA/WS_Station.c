#include "WS_Station.h"
#include "esp_http_server.h"
#include "React/Ws_React.h"

#include "esp_log.h"

static const char* TAG = "WS_STATION";

static httpd_handle_t s_hHttpServer = NULL;

esp_err_t
WS_Station_Start(void)
{
    esp_err_t espRslt = ESP_OK;

    /* Already running — nothing to do */
    if (NULL != s_hHttpServer)
    {
        return ESP_OK;
    }

    httpd_config_t tHttpServerConfig = HTTPD_DEFAULT_CONFIG();
    tHttpServerConfig.max_uri_handlers = 64;
    tHttpServerConfig.stack_size = 16384;
    tHttpServerConfig.uri_match_fn = httpd_uri_match_wildcard;
    // Large JS bundles can exceed the default 5-second socket send timeout;
    // raise both directions to prevent EAGAIN drops mid-transfer.
    tHttpServerConfig.send_wait_timeout = 30;
    tHttpServerConfig.recv_wait_timeout = 30;

    espRslt = httpd_start(&s_hHttpServer, &tHttpServerConfig);

    if (ESP_OK == espRslt)
    {
        espRslt = Ws_React_RegisterStaticFiles(s_hHttpServer);
    }

    if (ESP_OK == espRslt)
    {
        espRslt = Ws_React_RegisterApiHandlers(s_hHttpServer);
    }

    // if (ESP_OK == espErr)
    // {
    //     espErr = httpd_register_uri_handler(server, &schedule_config_get);
    // }

    // if(ESP_OK == espErr)
    // {
    //     espErr =  httpd_register_uri_handler(server, &schedule_config_post);
    //}

    if (ESP_OK != espRslt)
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
WS_Station_Stop(void)
{
    if (NULL != s_hHttpServer)
    {
        httpd_stop(s_hHttpServer);
        s_hHttpServer = NULL;
        ESP_LOGI(TAG, "STA HTTP server stopped");
    }

    return ESP_OK;
}