/* ================================================================== */
/* PinAPI.c — GET/POST /api/system/pin  (Bearer-protected)             */
/* ================================================================== */
#include "PinAPI.h"
#include "TouchScreen_Services.h"
#include "Auth/WS_Auth.h"
#include "cJSON.h"
#include "esp_log.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "PIN_API";

/* ------------------------------------------------------------------ */
/* Resource struct                                                     */
/* ------------------------------------------------------------------ */
typedef struct _PIN_API_RSC_T
{
    int reserved;
} PIN_API_RSC_T;

/* ------------------------------------------------------------------ */
/* Forward declarations                                                */
/* ------------------------------------------------------------------ */
static esp_err_t handler_GetPin(httpd_req_t *ptReq);
static esp_err_t handler_PostPin(httpd_req_t *ptReq);

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static esp_err_t
sendJson(httpd_req_t *ptReq, cJSON *ptRoot)
{
    const char *pcJson = cJSON_PrintUnformatted(ptRoot);
    httpd_resp_set_type(ptReq, "application/json");
    httpd_resp_sendstr(ptReq, pcJson);
    free((void *)pcJson);
    cJSON_Delete(ptRoot);
    return ESP_OK;
}

static esp_err_t
sendError(httpd_req_t *ptReq, const char *pcStatus, const char *pcMsg)
{
    cJSON *ptRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(ptRoot, "error", pcMsg);
    const char *pcJson = cJSON_PrintUnformatted(ptRoot);
    httpd_resp_set_status(ptReq, pcStatus);
    httpd_resp_set_type(ptReq, "application/json");
    httpd_resp_sendstr(ptReq, pcJson);
    free((void *)pcJson);
    cJSON_Delete(ptRoot);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */
esp_err_t
PinAPI_Init(const PIN_API_PARAMS_T *ptParams, PIN_API_H *phApi)
{
    (void)ptParams;

    if (phApi == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    /* Ensure PIN service is initialized (writes default if absent) */
    esp_err_t err = TS_Pin_Init();
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "TS_Pin_Init failed: %s", esp_err_to_name(err));
        return err;
    }

    PIN_API_RSC_T *ptRsc = (PIN_API_RSC_T *)calloc(1, sizeof(PIN_API_RSC_T));
    if (ptRsc == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    *phApi = ptRsc;
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Register                                                            */
/* ------------------------------------------------------------------ */
esp_err_t
PinAPI_Register(PIN_API_H hApi, httpd_handle_t hHttpServer)
{
    if (hApi == NULL || hHttpServer == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    PIN_API_RSC_T *ptRsc = (PIN_API_RSC_T *)hApi;

    httpd_uri_t tGetUri = {
        .uri      = "/api/system/pin",
        .method   = HTTP_GET,
        .handler  = handler_GetPin,
        .user_ctx = ptRsc,
    };
    esp_err_t err = httpd_register_uri_handler(hHttpServer, &tGetUri);

    if (ESP_OK == err)
    {
        httpd_uri_t tPostUri = {
            .uri      = "/api/system/pin",
            .method   = HTTP_POST,
            .handler  = handler_PostPin,
            .user_ctx = ptRsc,
        };
        err = httpd_register_uri_handler(hHttpServer, &tPostUri);
    }

    if (ESP_OK == err)
    {
        ESP_LOGI(TAG, "PIN API registered: GET/POST /api/system/pin");
    }

    return err;
}

/* ================================================================== */
/* GET /api/system/pin                                                 */
/* Returns: { "pin": "1234" }                                          */
/* ================================================================== */
static esp_err_t
handler_GetPin(httpd_req_t *ptReq)
{
    const char *pcUser;
    const char *pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK)
    {
        return ESP_OK;
    }

    char acPin[7] = {0};
    esp_err_t err = TS_Pin_Get(acPin, sizeof(acPin));
    if (ESP_OK != err)
    {
        return sendError(ptReq, "500 Internal Server Error", "Failed to read PIN");
    }

    cJSON *ptRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(ptRoot, "pin", acPin);
    return sendJson(ptReq, ptRoot);
}

/* ================================================================== */
/* POST /api/system/pin                                                */
/* Body: { "pin": "5678" }                                             */
/* Returns: { "status": "ok" }                                         */
/* ================================================================== */
static esp_err_t
handler_PostPin(httpd_req_t *ptReq)
{
    const char *pcUser;
    const char *pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK)
    {
        return ESP_OK;
    }

    char acBuf[128];
    int iLen = httpd_req_recv(ptReq, acBuf, sizeof(acBuf) - 1);
    if (iLen <= 0)
    {
        return sendError(ptReq, "400 Bad Request", "Empty body");
    }
    acBuf[iLen] = '\0';

    cJSON *ptRoot = cJSON_Parse(acBuf);
    if (ptRoot == NULL)
    {
        return sendError(ptReq, "400 Bad Request", "Invalid JSON");
    }

    cJSON *ptPin = cJSON_GetObjectItem(ptRoot, "pin");
    if (ptPin == NULL || !cJSON_IsString(ptPin))
    {
        cJSON_Delete(ptRoot);
        return sendError(ptReq, "400 Bad Request", "Missing 'pin' field");
    }

    esp_err_t err = TS_Pin_Set(ptPin->valuestring);
    cJSON_Delete(ptRoot);

    if (ESP_OK != err)
    {
        return sendError(ptReq, "400 Bad Request",
                         "Invalid PIN \xe2\x80\x94 must be 4-6 digits");
    }

    cJSON *ptResp = cJSON_CreateObject();
    cJSON_AddStringToObject(ptResp, "status", "ok");
    return sendJson(ptReq, ptResp);
}
