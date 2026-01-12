#include "WS_Station.h"
#include "esp_http_server.h"
#include "Pages/Schedule/WS_SchedulePage.h"

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
    esp_err_t espErr = ESP_OK;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    espErr = httpd_start(&server, &config);

    if (ESP_OK == espErr) 
    {
        espErr = httpd_register_uri_handler(server, &schedule_config_get);
    }

    if(ESP_OK == espErr)
    {
        espErr =  httpd_register_uri_handler(server, &schedule_config_post);
    } 

    return espErr;
}