#include "WS_WifiConfigPage.h"
#include "Definitions/AppErrors.h"
#include "WiFi_Manager_API.h"

#define WEBI_SERVER_MAX_FORM_BUFFER_SIZE    256

static const char* WIFI_CONFIG_PAGE =
    "<!DOCTYPE html>"
    "<html>"
    "<body>"
    "<h2>WiFi Configuration</h2>"
    "<form action=\"/wifi_config\" method=\"post\">"
    "SSID:<br>"
    "<input type=\"text\" name=\"ssid\"><br>"
    "Password:<br>"
    "<input type=\"password\" name=\"password\"><br><br>"
    "<input type=\"submit\" value=\"Save\">"
    "</form>"
    "</body>"
    "</html>";

esp_err_t
Ws_WifiConfigPage_Get(httpd_req_t *req)
{
    esp_err_t espErr = ESP_OK;

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, WIFI_CONFIG_PAGE, HTTPD_RESP_USE_STRLEN);

    return espErr;
}

esp_err_t
Ws_WifiConfigPage_Post(httpd_req_t *req)
{
    esp_err_t espErr = ESP_OK;

    char buf[WEBI_SERVER_MAX_FORM_BUFFER_SIZE];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (len <= 0) 
    {
        espErr = ESP_ERR_INVALID_RESPONSE;
    }
    else
    {
        buf[len] = '\0';

        // Parse form data: ssid=XXX&password=YYY
        char ssid[WIFI_MANAGER_MAX_SSID_LENGTH + 1] = {0};
        char pass[WIFI_MANAGER_MAX_PASS_LENGTH + 1] = {0};

        espErr = httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid));

        if(ESP_OK == espErr)
        {
            espErr = httpd_query_key_value(buf, "password", pass, sizeof(pass));
        }

        if(ESP_OK == espErr)
        {
            espErr = httpd_query_key_value(buf, "password", pass, sizeof(pass));
        }

        if(ESP_OK == espErr)
        {
            espErr = WiFi_Manager_SaveCredentials(ssid, pass);
        }

        if(ESP_OK == espErr)
        {
            const char* resp = "Credentials saved. Restarting...";
            httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

            // Delay so response is sent
            vTaskDelay(1000 / portTICK_PERIOD_MS);

            esp_restart();
        }
    }
    
    return espErr;
}