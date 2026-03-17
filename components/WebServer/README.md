# WebServer Component

Dual-mode HTTP server for the ESP32-S3 Ringy project. Serves a React single-page application (SPA) and exposes REST APIs for WiFi configuration, authentication, and application control.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Directory Structure](#directory-structure)
3. [Initialization Flow](#initialization-flow)
4. [Operating Modes](#operating-modes)
   - [AP Mode (Access Point)](#ap-mode-access-point)
   - [STA Mode (Station)](#sta-mode-station)
5. [WiFi Event Handling & Fallback AP](#wifi-event-handling--fallback-ap)
6. [REST API Reference](#rest-api-reference)
   - [WiFi Configuration API (AP Mode)](#wifi-configuration-api-ap-mode)
   - [Authentication API (STA Mode)](#authentication-api-sta-mode)
7. [React SPA File Serving](#react-spa-file-serving)
8. [Configuration (Kconfig)](#configuration-kconfig)
9. [Dependencies](#dependencies)
10. [Adding a New REST API Endpoint](#adding-a-new-rest-api-endpoint)

---

## Architecture Overview

The WebServer runs one HTTP server at a time on port 80. Which server starts depends on whether WiFi credentials are stored in NVS:

```
                  ┌─────────────────────────┐
                  │       Ws_Init()          │
                  │    (WS_API.c:31)         │
                  └────────┬────────────────┘
                           │
              WiFi_Manager_GetConfigurationState()
                           │
            ┌──────────────┴──────────────┐
            │                             │
   WAITING_CONFIGURATION             CONFIGURED
            │                             │
    ws_ConfigureAp()              ws_ConfigureSta()
    (WS_API.c:111)                (WS_API.c:193)
            │                             │
    WS_AccessPoint_Start()        WS_Station_Start()
            │                             │
   ┌────────┴────────┐          ┌────────┴────────┐
   │  AP HTTP Server  │          │  STA HTTP Server │
   │  (port 80)       │          │  (port 80)       │
   │                  │          │                   │
   │ • React routes   │          │ • React routes    │
   │ • WiFi Config API│          │ • Auth endpoints  │
   │   /api/wifi/*    │          │   /api/login      │
   └──────────────────┘          │   /api/logout     │
                                 └───────────────────┘
```

When the STA loses its WiFi connection, a **Fallback AP** is spun up automatically so the user can reconfigure WiFi credentials without physically accessing the device.

---

## Directory Structure

```
components/WebServer/
├── CMakeLists.txt                              # Component registration & dependencies
├── Kconfig.projbuild                           # Menuconfig: auth username/password
└── src/
    ├── WS_Public.h                             # Public types (WEB_SERVER_PARAMS_T)
    ├── WS_API.h                                # Ws_Init() declaration
    ├── WS_API.c                                # Initialization, WiFi AP/STA setup
    ├── WS_EventHandlers.h                      # Event handler + suspend/resume API
    ├── WS_EventHandlers.c                      # WiFi events, reconnect, fallback AP
    ├── AP/
    │   ├── WS_AccessPoint.h                    # WS_AccessPoint_Start/Stop
    │   ├── WS_AccessPoint.c                    # AP HTTP server lifecycle
    │   └── RestAPI/
    │       ├── WS_WiFiConfigAPI.h              # WiFiConfigAPI_Init/Register
    │       └── WS_WiFiConfigAPI.c              # /api/wifi/* handlers
    ├── STA/
    │   ├── WS_Station.h                        # WS_Station_Start/Stop
    │   └── WS_Station.c                        # STA HTTP server lifecycle
    ├── Auth/
    │   ├── WS_Auth.h                           # auth_register_endpoints, auth_require_bearer
    │   └── WS_Auth.c                           # Login/logout/validate, rate limiting
    └── React/
        ├── Ws_React.h                          # RegisterStaticFiles/RegisterApiHandlers
        ├── Ws_React.c                          # Delegates to Routes + Auth
        ├── WS_React_FileServer.h               # ResolvePath, ServeFile, GetMime
        ├── WS_React_FileServer.c               # Compressed file serving from FatFS
        ├── WS_React_Routes.h                   # RegisterRoutes (/, /assets/*, /favicon.ico)
        ├── WS_React_Routes.c                   # URI handlers for React SPA
        └── RestAPI/
```

---

## Initialization Flow

The WebServer is initialised during Phase 3 of `AppTask_Init()` in `components/AppTask/src/AppTask_API.c`:

```c
/* Phase 3: Connectivity & Services */
WEB_SERVER_PARAMS_T tWebServerParams = {
    .hWiFiManager = ptAppTaskRsc->hWiFiManager,       /* ← from Phase 1 */
};
lResult = Ws_Init(&tWebServerParams, &ptAppTaskRsc->hWebServer);
```

**`Ws_Init()`** (defined in `WS_API.c:31`) performs these steps:

| Step | Action | Code Reference |
|------|--------|----------------|
| 1 | Allocate `WEB_SERVER_RSC_T` | `WS_API.c:49` |
| 2 | Query WiFi config state from NVS | `WiFi_Manager_GetConfigurationState()` |
| 3a | **If unconfigured:** `ws_ConfigureAp()` → `WS_AccessPoint_Start()` | `WS_API.c:61–76` |
| 3b | **If configured:** `ws_ConfigureSta()` → `WS_Station_Start()` | `WS_API.c:81–90` |

### `ws_ConfigureAp()` — AP Mode WiFi Setup (`WS_API.c:111`)

1. Initializes `esp_netif` and creates AP + STA netifs (STA needed for scanning)
2. Reads default SSID/password from WiFi Manager (`"ESP32_Setup"` / `"12345678"`)
3. Sets WiFi to `WIFI_MODE_APSTA` (AP visible, STA available for scanning)
4. Calls `esp_wifi_start()`

### `ws_ConfigureSta()` — STA Mode WiFi Setup (`WS_API.c:193`)

1. Initializes `esp_netif` and creates STA netif
2. Registers event handlers for `IP_EVENT_STA_GOT_IP` and `WIFI_EVENT_STA_DISCONNECTED`
3. Passes WiFi Manager handle to event handlers via `Ws_EventHandlers_SetWiFiManager()`
4. Reads stored SSID/password from NVS
5. Sets WiFi to `WIFI_MODE_STA`, configures STA, starts WiFi, and calls `esp_wifi_connect()`

---

## Operating Modes

### AP Mode (Access Point)

**When:** No WiFi credentials in NVS (first boot or after credential wipe).

**Server:** `WS_AccessPoint_Start()` in `WS_AccessPoint.c`

```c
httpd_config_t tHttpServerConfig = HTTPD_DEFAULT_CONFIG();
tHttpServerConfig.max_uri_handlers  = 16;
tHttpServerConfig.stack_size        = 8192;
tHttpServerConfig.uri_match_fn      = httpd_uri_match_wildcard;
tHttpServerConfig.send_wait_timeout = 30;
tHttpServerConfig.recv_wait_timeout = 30;
```

**Registered endpoints:**

| Method | URI | Handler | Source |
|--------|-----|---------|--------|
| GET | `/` | `ws_react_index_handler` | `WS_React_Routes.c` |
| GET | `/assets/*` | `ws_react_assets_handler` | `WS_React_Routes.c` |
| GET | `/favicon.ico` | `ws_react_favicon_handler` | `WS_React_Routes.c` |
| GET | `/api/wifi/status` | `wifiConfigApi_GetStatus` | `WS_WiFiConfigAPI.c` |
| POST | `/api/wifi/config` | `wifiConfigApi_PostConfig` | `WS_WiFiConfigAPI.c` |
| GET | `/api/wifi/networks` | `wifiConfigApi_GetNetworks` | `WS_WiFiConfigAPI.c` |

**Network:** Device broadcasts SSID `"ESP32_Setup"` with password `"12345678"`, IP `192.168.4.1`.

### STA Mode (Station)

**When:** WiFi credentials exist in NVS.

**Server:** `WS_Station_Start()` in `WS_Station.c`

```c
httpd_config_t tHttpServerConfig = HTTPD_DEFAULT_CONFIG();
tHttpServerConfig.max_uri_handlers = 64;        /* more endpoints */
tHttpServerConfig.stack_size       = 16384;      /* larger for React assets */
tHttpServerConfig.uri_match_fn     = httpd_uri_match_wildcard;
tHttpServerConfig.send_wait_timeout = 30;
tHttpServerConfig.recv_wait_timeout = 30;
```

**Registered endpoints:**

| Method | URI | Handler | Source |
|--------|-----|---------|--------|
| GET | `/` | `ws_react_index_handler` | `WS_React_Routes.c` |
| GET | `/assets/*` | `ws_react_assets_handler` | `WS_React_Routes.c` |
| GET | `/favicon.ico` | `ws_react_favicon_handler` | `WS_React_Routes.c` |
| POST | `/api/login` | `api_login` | `WS_Auth.c` |
| POST | `/api/logout` | `api_logout` | `WS_Auth.c` |
| GET | `/api/validate-token` | `api_validate` | `WS_Auth.c` |


---

## WiFi Event Handling & Fallback AP

Defined in `WS_EventHandlers.c`. Handles automatic recovery when WiFi connection drops.

### Event Flow

```
WiFi connected (STA mode, normal operation)
         │
    Router goes down / signal lost
         │
    WIFI_EVENT_STA_DISCONNECTED
         │
    ┌────┴───────────────────────────────────────┐
    │ Is it a credential error?                   │
    │ (4WAY_HANDSHAKE_TIMEOUT, AUTH_FAIL,         │
    │  HANDSHAKE_TIMEOUT)                         │
    ├─── YES ─── Fallback AP active? ────────────┤
    │            │                                │
    │        YES: retry     NO: clear NVS         │
    │                           + esp_restart()   │
    ├─── NO ─────────────────────────────────────┤
    │    ws_ScheduleReconnectIfAllowed()           │
    │    ws_StartFallbackAp()                      │
    └─────────────────────────────────────────────┘
         │
    Fallback AP starts ("ESP32_Setup")
    STA keeps retrying in background (APSTA mode)
         │
    ┌────┴──────────────────┐
    │ IP_EVENT_STA_GOT_IP   │ ← Router comes back
    │ ws_StopFallbackAp()   │
    │ WS_Station_Start()    │
    └───────────────────────┘
```

### Reconnect Timer — Exponential Back-off

The reconnect timer doubles the delay after each failed attempt, capped at 60 seconds:

| Attempt | Delay |
|---------|-------|
| 0 | 5 s |
| 1 | 10 s |
| 2 | 20 s |
| 3 | 40 s |
| 4+ | 60 s (capped) |

Defined by `WS_RECONNECT_DELAY_MIN_MS` (5000) and `WS_RECONNECT_DELAY_MAX_MS` (60000).

### Fallback AP Lifecycle

**Start** (`ws_StartFallbackAp()`, `WS_EventHandlers.c`):

1. Creates AP netif (once, reused)
2. Configures AP with hardcoded SSID `"ESP32_Setup"` / password `"12345678"`
3. Stops STA HTTP server (port 80 exclusivity)
4. Switches to `WIFI_MODE_APSTA`
5. Starts AP HTTP server with WiFi Config API endpoints

**Stop** (`ws_StopFallbackAp()`, `WS_EventHandlers.c`):

1. Stops AP HTTP server
2. Reverts to `WIFI_MODE_STA`
3. Restarts STA HTTP server

### Suspend/Resume Reconnect

WiFi scanning requires exclusive radio access. The reconnect timer would otherwise call `esp_wifi_connect()` mid-scan, causing `ESP_ERR_WIFI_STATE`. Two functions coordinate this:

```c
/* WS_EventHandlers.h */
void Ws_EventHandlers_SuspendReconnect(void);   /* stop timer, block rescheduling */
void Ws_EventHandlers_ResumeReconnect(void);    /* re-arm timer */
```

**Usage in `WS_WiFiConfigAPI.c` scan handler:**

```c
static esp_err_t wifiConfigApi_GetNetworks(httpd_req_t* ptReq)
{
    Ws_EventHandlers_SuspendReconnect();          /* ← pause reconnect */
    (void)esp_wifi_disconnect();                  /* ← free the radio  */
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_err_t espErr = esp_wifi_scan_start(NULL, true);   /* blocking scan */
    /* ... build JSON response ... */

    Ws_EventHandlers_ResumeReconnect();           /* ← resume reconnect */
    return ESP_OK;
}
```

---

## REST API Reference

### WiFi Configuration API (AP Mode)

Registered by `WiFiConfigAPI_Register()` in `WS_WiFiConfigAPI.c`. Only available when the AP HTTP server is running (initial setup or fallback AP).

#### `GET /api/wifi/status`

Returns the current WiFi mode and AP SSID.

**Response:**
```json
{
  "mode": "AP",
  "ap_ssid": "ESP32_Setup"
}
```

The React frontend uses this response to determine whether to show the WiFi configuration page or the main application. If this endpoint returns a 404 (not registered in STA mode), the frontend knows it's in STA mode.

#### `GET /api/wifi/networks`

Performs a blocking WiFi scan (~1–2 seconds) and returns nearby access points.

**Response:**
```json
{
  "networks": [
    {
      "ssid": "MyHomeWiFi",
      "rssi": -42,
      "channel": 6,
      "authmode": "WPA2",
      "secured": true
    },
    {
      "ssid": "OpenNetwork",
      "rssi": -68,
      "channel": 11,
      "authmode": "OPEN",
      "secured": false
    }
  ]
}
```

**Auth mode values:** `"OPEN"`, `"WEP"`, `"WPA"`, `"WPA2"`, `"WPA/WPA2"`, `"WPA3"`, `"WPA2/WPA3"`, `"UNKNOWN"`

**Max results:** 20 (defined by `WIFI_SCAN_MAX_AP`).

#### `POST /api/wifi/config`

Saves WiFi credentials to NVS and restarts the device.

**Request body:**
```json
{
  "ssid": "MyHomeWiFi",
  "password": "my_secure_password"
}
```

**Response:**
```json
{
  "status": "ok",
  "message": "Credentials saved. Restarting..."
}
```

**Behaviour:** The response is sent, then after a 500 ms delay `esp_restart()` is called. On reboot, `Ws_Init()` finds credentials in NVS and starts in STA mode.

### Authentication API (STA Mode)

Registered by `auth_register_endpoints()` in `WS_Auth.c`. Credentials are configured via menuconfig (see [Configuration](#configuration-kconfig)).

#### `POST /api/login`

**Request body:**
```json
{
  "username": "admin",
  "password": "password123"
}
```

**Success response:**
```json
{
  "token": "a1b2c3d4e5f6...",
  "user": {
    "username": "admin",
    "role": "admin"
  },
  "message": "Login successful"
}
```

**Error responses:**
- `401`: `{"error": "invalid credentials"}`
- `429`: `{"error": "rate limited"}` (after 5 attempts in 60 seconds)
- `400`: `{"error": "invalid json"}` or `{"error": "missing fields"}`

**Token format:** 32-character hex string from 16 random bytes (`esp_random()`).

**Session model:** Single active session. A new login invalidates the previous token.

#### `POST /api/logout`

Requires `Authorization: Bearer <token>` header.

**Response:**
```json
{
  "success": true,
  "message": "Logged out successfully"
}
```

#### `GET /api/validate-token`

Requires `Authorization: Bearer <token>` header.

**Response:**
```json
{
  "valid": true,
  "user": {
    "username": "admin",
    "role": "admin"
  }
}
```

#### Using `auth_require_bearer()` in Custom Endpoints

To protect your own endpoints with authentication, use the guard function:

```c
#include "Auth/WS_Auth.h"

static esp_err_t my_protected_handler(httpd_req_t* req)
{
    const char* user = NULL;
    const char* role = NULL;

    /* Returns ESP_OK if valid Bearer token is present.
       On failure, sends 401 JSON response automatically. */
    if (ESP_OK != auth_require_bearer(req, &user, &role))
    {
        return ESP_FAIL;    /* response already sent by auth */
    }

    /* user and role pointers valid for this request */
    ESP_LOGI(TAG, "Authenticated user: %s (role: %s)", user, role);

    /* ... handle request ... */
    return ESP_OK;
}
```

---

## React SPA File Serving

React assets are stored on a FatFS partition mounted at `/react/`. The file server supports transparent gzip decompression.

### File Resolution Logic (`WS_React_FileServer.c`)

| Request URI | Resolution order |
|------------|-----------------|
| `/` | `/react/index.html.gz` → `/react/index.html` |
| `/assets/index-Cizzs6XD.js` | `/react/assets/index-Cizzs6XD.js.gz` → `/react/assets/index-Cizzs6XD.js` |
| `/assets/index-Cizzs6XD.js.gz` | `/react/assets/index-Cizzs6XD.js.gz` (direct) |
| `/favicon.ico` | `/react/favicon.ico.gz` → `/react/favicon.ico` → 204 No Content |

### Compression Support

When a `.gz` file is resolved, the server sets `Content-Encoding: gzip` and streams the compressed file directly. The browser handles decompression. This reduces flash storage usage and transfer time significantly.

### MIME Type Detection

Based on file extension (checked against the resolved path before `.gz` stripping):

| Extension | MIME Type |
|-----------|-----------|
| `.html` | `text/html` |
| `.js` | `application/javascript` |
| `.css` | `text/css` |
| `.json` | `application/json` |
| `.svg` | `image/svg+xml` |
| `.png` | `image/png` |
| `.jpg`, `.jpeg` | `image/jpeg` |
| `.ico` | `image/x-icon` |
| `.wasm` | `application/wasm` |
| other | `text/plain` |

### Chunked Transfer

Files are streamed in **4 KB chunks** (`WS_FILE_CHUNK_SIZE`). If a chunk send fails mid-transfer (client disconnect), the handler logs a warning and returns `ESP_OK` to avoid corrupting the chunked body with an error response.

### Caching Headers

| Route | Cache-Control | Reason |
|-------|---------------|--------|
| `/` (index.html) | `no-cache, no-store, must-revalidate` | SPA shell must always be fresh to pick up new asset hashes |
| `/assets/*` | `no-store` (dev) / `public, max-age=31536000, immutable` (prod) | Vite hashes filenames — safe to cache forever in production |
| `/favicon.ico` | none | Small file, infrequent |

---

## Configuration (Kconfig)

The `Kconfig.projbuild` file adds a menu under **Component config → WebServer Auth**:

| Config Key | Default | Description |
|-----------|---------|-------------|
| `CONFIG_WS_AUTH_USERNAME` | `"admin"` | Web interface admin username |
| `CONFIG_WS_AUTH_PASSWORD` | `"password123"` | Web interface admin password |

Change these via `idf.py menuconfig` before deploying to production.

---

## Dependencies

Declared in `CMakeLists.txt`:

| Dependency | Usage |
|-----------|-------|
| `esp_http_server` | HTTP server framework (`httpd_start`, URI handlers) |
| `esp_wifi` | WiFi AP/STA configuration, scanning, connect/disconnect |
| `esp_event` | Event loop for WiFi/IP events |
| `esp_timer` | Reconnect timer with exponential back-off |
| `nvs_flash` | NVS read/write for WiFi credentials |
| `json` | cJSON library for REST API JSON serialization |
| `WiFi_Manager` | Credential storage/retrieval, configuration state |
| `Generic` | Common application error codes |

---

## Adding a New REST API Endpoint

### 1. Create header (`MyAPI.h`)

```c
#pragma once
#include "esp_err.h"
#include "esp_http_server.h"

typedef struct _MY_API_PARAMS_T {
    /* dependencies your API needs */
} MY_API_PARAMS_T;

typedef struct _MY_API_RSC_T* MY_API_H;

esp_err_t MyAPI_Init(const MY_API_PARAMS_T* ptParams, MY_API_H* phApi);
esp_err_t MyAPI_Register(MY_API_H hApi, httpd_handle_t hHttpServer);
```

### 2. Create implementation (`MyAPI.c`)

```c
#include "MyAPI.h"
#include <stdlib.h>
#include "cJSON.h"
#include "Auth/WS_Auth.h"    /* if you need authentication */

typedef struct _MY_API_RSC_T {
    /* internal state */
    int value;
} MY_API_RSC_T;

static esp_err_t myApi_GetHandler(httpd_req_t* ptReq);

esp_err_t MyAPI_Init(const MY_API_PARAMS_T* ptParams, MY_API_H* phApi)
{
    MY_API_RSC_T* ptRsc = (MY_API_RSC_T*)calloc(1, sizeof(MY_API_RSC_T));
    if (NULL == ptRsc) return ESP_ERR_NO_MEM;

    *phApi = (MY_API_H)ptRsc;
    return ESP_OK;
}

esp_err_t MyAPI_Register(MY_API_H hApi, httpd_handle_t hHttpServer)
{
    httpd_uri_t tUri = {
        .uri      = "/api/my-endpoint",
        .method   = HTTP_GET,
        .handler  = myApi_GetHandler,
        .user_ctx = hApi,
    };
    return httpd_register_uri_handler(hHttpServer, &tUri);
}

static esp_err_t myApi_GetHandler(httpd_req_t* ptReq)
{
    /* Optional: require authentication */
    if (ESP_OK != auth_require_bearer(ptReq, NULL, NULL))
        return ESP_FAIL;

    MY_API_RSC_T* ptRsc = (MY_API_RSC_T*)ptReq->user_ctx;

    cJSON* ptRoot = cJSON_CreateObject();
    cJSON_AddNumberToObject(ptRoot, "value", ptRsc->value);
    const char* pcJson = cJSON_PrintUnformatted(ptRoot);

    httpd_resp_set_type(ptReq, "application/json");
    httpd_resp_sendstr(ptReq, pcJson);

    cJSON_Delete(ptRoot);
    free((void*)pcJson);
    return ESP_OK;
}
```

### 3. Register in `WS_Station.c`

Add initialisation and registration in `WS_Station_Start()`:

```c
/* In WS_Station_Start(): */
if (ESP_OK == espRslt)
{
    if (NULL == s_hMyApi)
    {
        MY_API_PARAMS_T tParams = {0};
        espRslt = MyAPI_Init(&tParams, &s_hMyApi);
    }
}

if (ESP_OK == espRslt)
{
    espRslt = MyAPI_Register(s_hMyApi, s_hHttpServer);
}
```

### 4. Add source file to `CMakeLists.txt`

```cmake
idf_component_register(
    SRCS
        ...
        "src/React/RestAPI/MyAPI/MyAPI.c"    # ← add here
    ...
)
```

### 5. Increase URI handler limit if needed

If you exceed the configured maximum, bump `max_uri_handlers` in `WS_Station.c`:

```c
tHttpServerConfig.max_uri_handlers = 64;  /* increase if needed */
```
