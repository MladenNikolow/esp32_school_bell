/* ================================================================== */
/* CredentialAPI.c — GET/POST/DELETE /api/system/credentials            */
/* Service-role only endpoints for managing the client account.        */
/* ================================================================== */
#include "CredentialAPI.h"
#include "Auth/WS_Auth.h"
#include "Auth/WS_AuthStore.h"
#include "cJSON.h"
#include "esp_log.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "CRED_API";

/* ------------------------------------------------------------------ */
/* Resource struct                                                     */
/* ------------------------------------------------------------------ */
typedef struct _CREDENTIAL_API_RSC_T
{
    int reserved;
} CREDENTIAL_API_RSC_T;

/* ------------------------------------------------------------------ */
/* Forward declarations                                                */
/* ------------------------------------------------------------------ */
static esp_err_t handler_GetCredentials(httpd_req_t *ptReq);
static esp_err_t handler_PostCredentials(httpd_req_t *ptReq);
static esp_err_t handler_DeleteCredentials(httpd_req_t *ptReq);

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static esp_err_t
sendJson(httpd_req_t *ptReq, cJSON *ptRoot)
{
    const char *pcJson = cJSON_PrintUnformatted(ptRoot);
    auth_set_security_headers(ptReq);
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
    auth_set_security_headers(ptReq);
    httpd_resp_set_status(ptReq, pcStatus);
    httpd_resp_sendstr(ptReq, pcJson);
    free((void *)pcJson);
    cJSON_Delete(ptRoot);
    return ESP_OK;
}

/**
 * @brief Require service role + CSRF check for mutating requests.
 */
static bool
requireServiceAccess(httpd_req_t *ptReq)
{
    auth_set_security_headers(ptReq);

    if ((ptReq->method == HTTP_POST || ptReq->method == HTTP_PUT || ptReq->method == HTTP_DELETE)
        && !auth_csrf_check(ptReq)) {
        return false;
    }

    return (auth_require_role(ptReq, "service", NULL, NULL) == ESP_OK);
}

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */
esp_err_t
CredentialAPI_Init(const CREDENTIAL_API_PARAMS_T *ptParams, CREDENTIAL_API_H *phApi)
{
    (void)ptParams;

    if (phApi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    CREDENTIAL_API_RSC_T *ptRsc = (CREDENTIAL_API_RSC_T *)calloc(1, sizeof(CREDENTIAL_API_RSC_T));
    if (ptRsc == NULL) {
        return ESP_ERR_NO_MEM;
    }

    *phApi = ptRsc;
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Register                                                            */
/* ------------------------------------------------------------------ */
esp_err_t
CredentialAPI_Register(CREDENTIAL_API_H hApi, httpd_handle_t hHttpServer)
{
    if (hApi == NULL || hHttpServer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    CREDENTIAL_API_RSC_T *ptRsc = (CREDENTIAL_API_RSC_T *)hApi;

    httpd_uri_t tGetUri = {
        .uri      = "/api/system/credentials",
        .method   = HTTP_GET,
        .handler  = handler_GetCredentials,
        .user_ctx = ptRsc,
    };
    esp_err_t err = httpd_register_uri_handler(hHttpServer, &tGetUri);

    if (ESP_OK == err) {
        httpd_uri_t tPostUri = {
            .uri      = "/api/system/credentials",
            .method   = HTTP_POST,
            .handler  = handler_PostCredentials,
            .user_ctx = ptRsc,
        };
        err = httpd_register_uri_handler(hHttpServer, &tPostUri);
    }

    if (ESP_OK == err) {
        httpd_uri_t tDeleteUri = {
            .uri      = "/api/system/credentials",
            .method   = HTTP_DELETE,
            .handler  = handler_DeleteCredentials,
            .user_ctx = ptRsc,
        };
        err = httpd_register_uri_handler(hHttpServer, &tDeleteUri);
    }

    if (ESP_OK == err) {
        ESP_LOGI(TAG, "Credential API registered: GET/POST/DELETE /api/system/credentials");
    }

    return err;
}

/* ================================================================== */
/* GET /api/system/credentials                                         */
/* Returns: { "clientExists": bool, "clientUsername": "..." }          */
/* ================================================================== */
static esp_err_t
handler_GetCredentials(httpd_req_t *ptReq)
{
    if (!requireServiceAccess(ptReq)) {
        return ESP_OK;
    }

    cJSON *ptRoot = cJSON_CreateObject();
    bool exists = auth_store_client_exists();
    cJSON_AddBoolToObject(ptRoot, "clientExists", exists);

    if (exists) {
        char username[32] = {0};
        if (ESP_OK == auth_store_get_client_username(username, sizeof(username))) {
            cJSON_AddStringToObject(ptRoot, "clientUsername", username);
        } else {
            cJSON_AddStringToObject(ptRoot, "clientUsername", "");
        }
    } else {
        cJSON_AddStringToObject(ptRoot, "clientUsername", "");
    }

    return sendJson(ptReq, ptRoot);
}

/* ================================================================== */
/* POST /api/system/credentials                                        */
/* Body: { "username": "...", "password": "..." }                      */
/* Creates or updates the client account.                              */
/* ================================================================== */
static esp_err_t
handler_PostCredentials(httpd_req_t *ptReq)
{
    if (!requireServiceAccess(ptReq)) {
        return ESP_OK;
    }

    char acBuf[256];
    int iLen = httpd_req_recv(ptReq, acBuf, sizeof(acBuf) - 1);
    if (iLen <= 0) {
        return sendError(ptReq, "400 Bad Request", "Empty body");
    }
    acBuf[iLen] = '\0';

    cJSON *ptRoot = cJSON_Parse(acBuf);
    if (ptRoot == NULL) {
        return sendError(ptReq, "400 Bad Request", "Invalid JSON");
    }

    cJSON *ptUser = cJSON_GetObjectItem(ptRoot, "username");
    cJSON *ptPass = cJSON_GetObjectItem(ptRoot, "password");

    if (!cJSON_IsString(ptUser) || !cJSON_IsString(ptPass)) {
        cJSON_Delete(ptRoot);
        return sendError(ptReq, "400 Bad Request", "Missing 'username' or 'password' field");
    }

    const char *username = ptUser->valuestring;
    const char *password = ptPass->valuestring;

    if (strlen(username) == 0 || strlen(username) > 31) {
        cJSON_Delete(ptRoot);
        return sendError(ptReq, "400 Bad Request", "Username must be 1-31 characters");
    }

    if (strlen(password) < 8) {
        cJSON_Delete(ptRoot);
        return sendError(ptReq, "400 Bad Request", "Password must be at least 8 characters");
    }

    esp_err_t err = auth_store_set_client(username, password);
    cJSON_Delete(ptRoot);

    if (ESP_OK != err) {
        return sendError(ptReq, "500 Internal Server Error", "Failed to save credentials");
    }

    /* Invalidate all active sessions — force re-login */
    auth_invalidate_all_sessions();

    cJSON *ptResp = cJSON_CreateObject();
    cJSON_AddBoolToObject(ptResp, "success", true);
    cJSON_AddStringToObject(ptResp, "message", "Client account saved");
    return sendJson(ptReq, ptResp);
}

/* ================================================================== */
/* DELETE /api/system/credentials                                      */
/* Deletes the client account.                                         */
/* ================================================================== */
static esp_err_t
handler_DeleteCredentials(httpd_req_t *ptReq)
{
    if (!requireServiceAccess(ptReq)) {
        return ESP_OK;
    }

    if (!auth_store_client_exists()) {
        return sendError(ptReq, "404 Not Found", "No client account exists");
    }

    esp_err_t err = auth_store_delete_client();
    if (ESP_OK != err) {
        return sendError(ptReq, "500 Internal Server Error", "Failed to delete credentials");
    }

    /* Invalidate all active sessions — force re-login */
    auth_invalidate_all_sessions();

    cJSON *ptResp = cJSON_CreateObject();
    cJSON_AddBoolToObject(ptResp, "success", true);
    cJSON_AddStringToObject(ptResp, "message", "Client account deleted");
    return sendJson(ptReq, ptResp);
}
