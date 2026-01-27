#include <stdio.h>
#include <string.h>
#include "Ws_React.h"
#include "Ws_React_Static.h"
#include "esp_log.h"
#include "RestAPI/Example/ExampleAPI.h"
#include "esp_http_server.h"
#include "Auth/WS_Auth.h"

static const char *TAG = "WS_REACT";

esp_err_t
Ws_React_RegisterStaticFiles(httpd_handle_t hHttpServer)
{
    esp_err_t espRslt = ESP_OK;

    if(espRslt == ESP_OK)
    {
        // Serve index.html (and SPA entry point)
        httpd_uri_t index_uri =
        {
            .uri = "/",
            .method = HTTP_GET,
            .handler = Ws_React_StaticFileHandler
        };

        espRslt = httpd_register_uri_handler(hHttpServer, &index_uri);

        if(espRslt == ESP_OK)
        {
            httpd_uri_t assets_uri =
            {
                .uri = "/assets/*",
                .method = HTTP_GET,
                .handler = Ws_React_StaticFileHandler
            };

            espRslt = httpd_register_uri_handler(hHttpServer, &assets_uri);
            ESP_LOGI(TAG, "register assets handler -> %s (uri=%s)", espRslt == ESP_OK ? "OK" : "FAIL", assets_uri.uri);
        }
    }

    ESP_LOGI(TAG, "Static file handlers registered.");

    return espRslt;
}

esp_err_t
Ws_React_RegisterApiHandlers(httpd_handle_t hHttpServer)
{
    esp_err_t espRslt = auth_register_endpoints(hHttpServer);
    
    if(ESP_OK == espRslt)
    {
        espRslt = Ws_React_RegisterExampleAPI(hHttpServer);
        if(ESP_OK == espRslt)
        {
            ESP_LOGI(TAG, "Example API registered.");
        }
    }
    
    return espRslt;
}
