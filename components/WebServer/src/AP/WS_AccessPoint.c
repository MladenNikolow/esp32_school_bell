#include "WS_AccessPoint.h"
#include "esp_http_server.h"
#include "Pages/WiFiConfig/WS_WifiConfigPage.h"

httpd_uri_t wifi_config_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = Ws_WifiConfigPage_Get
};

httpd_uri_t wifi_config_post = {
    .uri = "/wifi_config",
    .method = HTTP_POST,
    .handler = Ws_WifiConfigPage_Post
};

esp_err_t
WS_AccessPoint_Start(void)
{
    esp_err_t espErr = ESP_OK;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    espErr = httpd_start(&server, &config);

    if (ESP_OK == espErr) 
    {
        espErr = httpd_register_uri_handler(server, &wifi_config_get);
    }

    if(ESP_OK == espErr)
    {
        espErr =  httpd_register_uri_handler(server, &wifi_config_post);
    } 

    return espErr;
}