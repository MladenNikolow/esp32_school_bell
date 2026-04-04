#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

typedef struct _CREDENTIAL_API_RSC_T *CREDENTIAL_API_H;

typedef struct
{
    int reserved;
} CREDENTIAL_API_PARAMS_T;

/**
 * @brief Initialize Credential API resource.
 */
esp_err_t CredentialAPI_Init(const CREDENTIAL_API_PARAMS_T *ptParams, CREDENTIAL_API_H *phApi);

/**
 * @brief Register GET/POST/DELETE /api/system/credentials handlers.
 */
esp_err_t CredentialAPI_Register(CREDENTIAL_API_H hApi, httpd_handle_t hHttpServer);
