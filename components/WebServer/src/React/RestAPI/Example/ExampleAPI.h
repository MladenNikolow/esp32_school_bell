#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

typedef struct _EXAMPLE_API_PARAMS_T
{
    int reserved; /* no external dependencies currently */
} EXAMPLE_API_PARAMS_T;

typedef struct _EXAMPLE_API_RSC_T* EXAMPLE_API_H;

/**
 * @brief Allocate and initialise an ExampleAPI resource.
 *
 * @param ptParams  Pointer to init params (may be zeroed; reserved for future use).
 * @param phExampleApi  Output handle set on success.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t ExampleAPI_Init(const EXAMPLE_API_PARAMS_T* ptParams, EXAMPLE_API_H* phExampleApi);

/**
 * @brief Register GET /api/mode and POST /api/mode handlers with the HTTP server.
 *
 * The handle stores mode state internally; each handler accesses it via
 * httpd_req_t::user_ctx.
 *
 * @param hExampleApi  Handle returned by ExampleAPI_Init.
 * @param hHttpServer  Running HTTP server handle.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t ExampleAPI_Register(EXAMPLE_API_H hExampleApi, httpd_handle_t hHttpServer);
