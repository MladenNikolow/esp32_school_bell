#include "WS_Station.h"
#include "esp_http_server.h"
#include "Pages/Schedule/WS_SchedulePage.h"
#include "React/Ws_React.h"

httpd_uri_t schedule_config_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = Ws_SchedulePage_Get
};

httpd_uri_t schedule_config_post = {
    .uri = "/schedule",
    .method = HTTP_POST,
    .handler = Ws_SchedulePage_Post
};  

esp_err_t
WS_Station_Start(void)
{
    esp_err_t espRslt = ESP_OK;
    httpd_config_t tHttpServerConfig = HTTPD_DEFAULT_CONFIG();
    tHttpServerConfig.max_uri_handlers = 64;
    tHttpServerConfig.stack_size = 16384;
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

    // if (ESP_OK == espErr) 
    // {
    //     espErr = httpd_register_uri_handler(server, &schedule_config_get);
    // }

    // if(ESP_OK == espErr)
    // {
    //     espErr =  httpd_register_uri_handler(server, &schedule_config_post);
    // } 

    return espRslt;
}