#include "WS_Station.h"
#include "esp_http_server.h"
#include "React/Ws_React.h"
#include "React/RestAPI/Example/ExampleAPI.h"

static EXAMPLE_API_H s_hExampleApi = NULL;

esp_err_t
WS_Station_Start(void)
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

    // if (ESP_OK == espErr)
    // {
    //     espErr = httpd_register_uri_handler(server, &schedule_config_get);
    // }

    // if(ESP_OK == espErr)
    // {
    //     espErr =  httpd_register_uri_handler(server, &schedule_config_post);
    //}

    return espRslt;
}