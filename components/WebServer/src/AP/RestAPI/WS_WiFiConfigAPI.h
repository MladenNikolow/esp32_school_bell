#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "WiFi_Manager_API.h"

typedef struct _WIFI_CONFIG_API_PARAMS_T
{
    WIFI_MANAGER_H hWiFiManager;
} WIFI_CONFIG_API_PARAMS_T;

typedef struct _WIFI_CONFIG_API_RSC_T* WIFI_CONFIG_API_H;

/**
 * @brief Allocate and initialise a WiFiConfigAPI resource.
 */
esp_err_t WiFiConfigAPI_Init(const WIFI_CONFIG_API_PARAMS_T* ptParams, WIFI_CONFIG_API_H* phApi);

/**
 * @brief Register AP WiFi config API endpoints with the HTTP server.
 *
 *  GET  /api/wifi/status   — returns current mode and AP SSID
 *  POST /api/wifi/config   — accepts { "ssid": "...", "password": "..." }, saves to NVS and restarts
 *  GET  /api/wifi/networks — returns JSON array of nearby networks (blocking WiFi scan)
 */
esp_err_t WiFiConfigAPI_Register(WIFI_CONFIG_API_H hApi, httpd_handle_t hHttpServer);
