#include "ExampleAPI.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "cJSON.h"

static const char* TAG = "EXAMPLE_API";

#define MODE_MAX_LEN 31

// ----------------------------------------------------------------
// Resource struct
// ----------------------------------------------------------------
typedef struct _EXAMPLE_API_RSC_T
{
    char mode_value[MODE_MAX_LEN + 1];
} EXAMPLE_API_RSC_T;

// ----------------------------------------------------------------
// Forward declarations
// ----------------------------------------------------------------
static esp_err_t ws_React_GetMode(httpd_req_t* ptReq);
static esp_err_t ws_React_PostMode(httpd_req_t* ptReq);

// ----------------------------------------------------------------
// Init
// ----------------------------------------------------------------
esp_err_t ExampleAPI_Init(const EXAMPLE_API_PARAMS_T* ptParams, EXAMPLE_API_H* phExampleApi)
{
    (void)ptParams; /* unused — reserved for future dependencies */

    if (NULL == phExampleApi)
    {
        return ESP_ERR_INVALID_ARG;
    }

    EXAMPLE_API_RSC_T* ptRsc = (EXAMPLE_API_RSC_T*)calloc(1, sizeof(EXAMPLE_API_RSC_T));
    if (NULL == ptRsc)
    {
        return ESP_ERR_NO_MEM;
    }

    snprintf(ptRsc->mode_value, sizeof(ptRsc->mode_value), "AUTO");

    *phExampleApi = ptRsc;
    return ESP_OK;
}

// ----------------------------------------------------------------
// Register URI handlers
// ----------------------------------------------------------------
esp_err_t ExampleAPI_Register(EXAMPLE_API_H hExampleApi, httpd_handle_t hHttpServer)
{
    if ((NULL == hExampleApi) || (NULL == hHttpServer))
    {
        return ESP_ERR_INVALID_ARG;
    }

    EXAMPLE_API_RSC_T* ptRsc = (EXAMPLE_API_RSC_T*)hExampleApi;

    httpd_uri_t tGetModeUri =
    {
        .uri      = "/api/mode",
        .method   = HTTP_GET,
        .handler  = ws_React_GetMode,
        .user_ctx = ptRsc
    };

    esp_err_t espErr = httpd_register_uri_handler(hHttpServer, &tGetModeUri);

    if (ESP_OK == espErr)
    {
        httpd_uri_t tPostModeUri =
        {
            .uri      = "/api/mode",
            .method   = HTTP_POST,
            .handler  = ws_React_PostMode,
            .user_ctx = ptRsc
        };

        espErr = httpd_register_uri_handler(hHttpServer, &tPostModeUri);
    }

    if (ESP_OK == espErr)
    {
        ESP_LOGI(TAG, "Example API registered: GET /api/mode, POST /api/mode");
    }

    return espErr;
}

// ----------------------------------------------------------------
// GET /api/mode
// ----------------------------------------------------------------
static esp_err_t ws_React_GetMode(httpd_req_t* ptReq)
{
    EXAMPLE_API_RSC_T* ptRsc = (EXAMPLE_API_RSC_T*)ptReq->user_ctx;

    cJSON* ptJsonRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(ptJsonRoot, "mode", ptRsc->mode_value);

    const char* json = cJSON_PrintUnformatted(ptJsonRoot);

    httpd_resp_set_type(ptReq, "application/json");
    httpd_resp_sendstr(ptReq, json);

    cJSON_Delete(ptJsonRoot);
    free((void*)json);

    return ESP_OK;
}

// ----------------------------------------------------------------
// POST /api/mode
// ----------------------------------------------------------------
static esp_err_t ws_React_PostMode(httpd_req_t* ptReq)
{
    EXAMPLE_API_RSC_T* ptRsc = (EXAMPLE_API_RSC_T*)ptReq->user_ctx;

    char buf[128];
    int  len = httpd_req_recv(ptReq, buf, sizeof(buf) - 1);

    if (len <= 0)
    {
        return ESP_FAIL;
    }

    buf[len] = '\0';

    cJSON* root = cJSON_Parse(buf);
    if (!root)
    {
        return ESP_FAIL;
    }

    cJSON* mode_item = cJSON_GetObjectItem(root, "mode");
    if (mode_item && cJSON_IsString(mode_item))
    {
        strncpy(ptRsc->mode_value, mode_item->valuestring, MODE_MAX_LEN);
        ptRsc->mode_value[MODE_MAX_LEN] = '\0';
    }

    cJSON_Delete(root);

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    cJSON_AddStringToObject(resp, "mode", ptRsc->mode_value);

    const char* json = cJSON_PrintUnformatted(resp);

    httpd_resp_set_type(ptReq, "application/json");
    httpd_resp_sendstr(ptReq, json);

    cJSON_Delete(resp);
    free((void*)json);

    return ESP_OK;
}
