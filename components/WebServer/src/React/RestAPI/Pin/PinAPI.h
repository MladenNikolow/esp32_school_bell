#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

typedef struct _PIN_API_RSC_T *PIN_API_H;

typedef struct
{
    int reserved;   /* No external handles needed — uses NVS directly */
} PIN_API_PARAMS_T;

/**
 * @brief Initialize PIN API resource (also calls TS_Pin_Init).
 */
esp_err_t PinAPI_Init(const PIN_API_PARAMS_T *ptParams, PIN_API_H *phApi);

/**
 * @brief Register GET/POST /api/system/pin handlers.
 */
esp_err_t PinAPI_Register(PIN_API_H hApi, httpd_handle_t hHttpServer);
