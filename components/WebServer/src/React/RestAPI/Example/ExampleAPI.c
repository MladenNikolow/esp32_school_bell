#include <string.h>
#include "ExampleAPI.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "cJSON.h"
//#include "Auth/WS_Auth.h"
 
static char mode_value[32] = "AUTO";   // default mode

// -------------------------
// GET /api/mode
// -------------------------
static esp_err_t 
ws_React_GetMode(httpd_req_t* ptReq);

// -------------------------
// POST /api/mode
// -------------------------
static esp_err_t 
ws_React_PostMode(httpd_req_t* ptReq);

esp_err_t
Ws_React_RegisterExampleAPI(httpd_handle_t hHttpServer)
{
    esp_err_t espRslt = ESP_OK;

    httpd_uri_t tGetModeUri = 
    {
        .uri = "/api/mode",
        .method = HTTP_GET,
        .handler = ws_React_GetMode
    };

    espRslt = httpd_register_uri_handler(hHttpServer, &tGetModeUri);
 
    if(ESP_OK == espRslt)
    {
        httpd_uri_t tPostModeUri = 
        {
            .uri = "/api/mode",
            .method = HTTP_POST,
            .handler = ws_React_PostMode
        };

        espRslt = httpd_register_uri_handler(hHttpServer, &tPostModeUri);
    }

    return espRslt;
}

static esp_err_t 
ws_React_GetMode(httpd_req_t* ptReq)
{
    //char** out_user;
    //char** out_role;
    //auth_require_bearer(ptReq, out_user, out_role); // Ensure auth system is initialized
    //if(out_role == NULL || strcmp(*out_role, "admin" != 0)
    //{
    //    return ESP_FAIL; // Only admin can get mode
    //}

    cJSON* ptJsonRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(ptJsonRoot, "mode", mode_value);
 
    const char *json = cJSON_PrintUnformatted(ptJsonRoot);
 
    httpd_resp_set_type(ptReq, "application/json");
    httpd_resp_sendstr(ptReq, json);
 
    cJSON_Delete(ptJsonRoot);
    free((void*)json);
 
    return ESP_OK;
}
 
static esp_err_t 
ws_React_PostMode(httpd_req_t* ptReq)
{
    char buf[128];
    int len = httpd_req_recv(ptReq, buf, sizeof(buf) - 1);
 
    if (len <= 0) {
        return ESP_FAIL;
    }
 
    buf[len] = '\0';
 
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return ESP_FAIL;
    }
 
    const char *new_mode = cJSON_GetObjectItem(root, "mode")->valuestring;
    if (new_mode) {
        strncpy(mode_value, new_mode, sizeof(mode_value));
        mode_value[sizeof(mode_value) - 1] = '\0';
    }
 
    cJSON_Delete(root);
 
    // Response
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    cJSON_AddStringToObject(resp, "mode", mode_value);
 
    const char *json = cJSON_PrintUnformatted(resp);
 
    httpd_resp_set_type(ptReq, "application/json");
    httpd_resp_sendstr(ptReq, json);
 
    cJSON_Delete(resp);
    free((void*)json);
 
    return ESP_OK;
}