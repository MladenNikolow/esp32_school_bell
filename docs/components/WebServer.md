# WebServer Component

## Purpose

HTTP server providing a RESTful API, React SPA hosting, session-based authentication with CSRF protection, and a WiFi configuration portal. Operates in dual mode: AP (captive portal for initial setup) and STA (full application server).

## Directory Structure

```
components/WebServer/
├── CMakeLists.txt
├── Kconfig.projbuild              # Auth credentials config
└── src/
    ├── WS_API.h                   # Public API (Ws_Init)
    ├── WS_API.c                   # Top-level init
    ├── WS_Public.h                # Shared types (params, handles)
    ├── WS_EventHandlers.h/c       # WiFi event handlers (STA/AP/IP)
    │
    ├── Auth/
    │   ├── WS_Auth.h              # Auth API (register, require_session, csrf_check)
    │   └── WS_Auth.c              # Session management, login/logout/validate
    │
    ├── STA/
    │   ├── WS_Station.h           # Station-mode server start
    │   └── WS_Station.c           # Main HTTP server setup + endpoint registration
    │
    ├── AP/
    │   ├── WS_AccessPoint.h       # AP-mode server start
    │   ├── WS_AccessPoint.c
    │   └── RestAPI/
    │       ├── WS_WiFiConfigAPI.h # WiFi config API (AP mode)
    │       └── WS_WiFiConfigAPI.c
    │
    ├── React/
    │   ├── Ws_React.h             # SPA hosting registration
    │   ├── Ws_React.c
    │   ├── WS_React_Routes.h      # Route handlers (/, /assets/*, catch-all)
    │   ├── WS_React_Routes.c
    │   ├── WS_React_FileServer.h  # Gzip-aware static file server
    │   ├── WS_React_FileServer.c
    │   └── RestAPI/
    │       ├── Schedule/
    │       │   ├── ScheduleAPI.h  # Schedule/bell/system endpoints
    │       │   └── ScheduleAPI.c
    │       ├── Pin/
    │       │   ├── PinAPI.h       # PIN management endpoints
    │       │   └── PinAPI.c
    │       └── Example/
    │           ├── ExampleAPI.h   # Mode toggle (dev/demo)
    │           └── ExampleAPI.c
    │
    └── Pages/
        └── WiFiConfig/
            ├── WS_WifiConfigPage.h  # Legacy HTML-form WiFi config
            └── WS_WifiConfigPage.c
```

## Top-Level API

```c
typedef struct _WEB_SERVER_RSC_T* WEB_SERVER_H;

typedef struct {
    WIFI_MANAGER_H hWiFiManager;
    SCHEDULER_H    hScheduler;
} WEB_SERVER_PARAMS_T;

esp_err_t Ws_Init(WEB_SERVER_PARAMS_T* ptParams, WEB_SERVER_H* phWebServer);
```

## Dual-Mode Operation

### Station Mode (STA)
- Full application server at device IP or `ringy.local` (mDNS)
- React SPA hosting from FatFS `/react/`
- Session-based authentication
- All REST API endpoints active

### Access Point Mode (AP)
- Captive portal at `192.168.4.1`
- WiFi configuration-only endpoints (no auth required)
- Used for initial network setup

### APSTA Mode
Both AP and STA interfaces active simultaneously — STA with automatic reconnect, soft-AP always available as fallback.

## Authentication System

Documented in detail in [AUTHENTICATION.md](../AUTHENTICATION.md).

### Auth API
```c
esp_err_t auth_register_endpoints(httpd_handle_t server);
void      auth_set_security_headers(httpd_req_t* req);
bool      auth_csrf_check(httpd_req_t* req);
esp_err_t auth_require_session(httpd_req_t* req, const char** out_user, const char** out_role);
int       auth_active_session_count(void);
```

### Session Management
- **Token**: 32-character hex string from `esp_random()`
- **Max sessions**: 3 concurrent
- **Session lifetime**: 24 hours
- **Storage**: RAM (cleared on reboot = automatic logout)
- **Cookie**: `session=<token>; HttpOnly; SameSite=Strict; Path=/`
- **Eviction**: Oldest session evicted when all slots full

### CSRF Protection
Required on all POST/PUT/DELETE to protected endpoints:
- `Content-Type: application/json` (blocks HTML form submissions)
- `X-Requested-With: XMLHttpRequest` (blocks cross-origin requests)

### Security Headers (all responses)
```
X-Content-Type-Options: nosniff
X-Frame-Options: DENY
Cache-Control: no-store
Content-Type: application/json
```

### Rate Limiting
- Login: 5 attempts per 60-second window (global)

## REST API Endpoints

### Authentication (WS_Auth.c)

| Method | URI | Auth | Description |
|--------|-----|------|-------------|
| POST | `/api/login` | Rate-limited | Authenticate with username/password, receive session cookie |
| POST | `/api/logout` | Session+CSRF | Invalidate session, clear cookie |
| GET | `/api/validate-token` | Session | Validate current session cookie |

### Schedule & Bells (ScheduleAPI.c)

| Method | URI | Auth | Description |
|--------|-----|------|-------------|
| GET | `/api/schedule/settings` | Session | Get timezone + working days |
| POST | `/api/schedule/settings` | Session+CSRF | Update timezone + working days |
| GET | `/api/schedule/bells` | Session | Get first/second shift bell arrays |
| POST | `/api/schedule/bells` | Session+CSRF | Update bell definitions (max 8KB body) |
| GET | `/api/schedule/holidays` | Session | Get holidays array |
| POST | `/api/schedule/holidays` | Session+CSRF | Update holidays |
| GET | `/api/schedule/exceptions` | Session | Get exceptions + custom bell sets |
| POST | `/api/schedule/exceptions` | Session+CSRF | Update exceptions (actions: `day-off`, `normal`, `first-shift`, `second-shift`, `template`, `custom`) |
| GET | `/api/schedule/templates` | Session | Get bell templates |
| POST | `/api/schedule/templates` | Session+CSRF | Update bell templates |
| GET | `/api/schedule/defaults` | Session | Get factory default schedule |

### Bell Control (ScheduleAPI.c)

| Method | URI | Auth | Description |
|--------|-----|------|-------------|
| GET | `/api/bell/status` | Session | Bell state, panic mode, day type, time sync, next bell |
| POST | `/api/bell/panic` | Session+CSRF | `{enabled: bool}` — toggle panic mode |
| POST | `/api/bell/test` | Session+CSRF | `{durationSec: 1-30}` — test ring (blocked during panic) |

### System (ScheduleAPI.c)

| Method | URI | Auth | Description |
|--------|-----|------|-------------|
| GET | `/api/system/time` | Session | Current time, date, sync status, timezone |
| GET | `/api/system/info` | Session | Uptime, heap, chip info, IDF version |
| POST | `/api/system/reboot` | Session+CSRF | Reboot after 1-second delay |
| POST | `/api/system/factory-reset` | Session+CSRF | Delete configs, recreate defaults, reset PIN + setup |
| POST | `/api/system/sync-time` | Session+CSRF | Force NTP resync |

### PIN Management (PinAPI.c)

| Method | URI | Auth | Description |
|--------|-----|------|-------------|
| GET | `/api/system/pin` | Session | Get current PIN |
| POST | `/api/system/pin` | Session+CSRF | Set new 4–6 digit PIN |

### System Status (WS_Station.c — inline)

| Method | URI | Auth | Description |
|--------|-----|------|-------------|
| GET | `/api/health` | None | `{status, timestamp, uptime, memory}` |
| GET | `/api/status` | None | `{device, version, wifi, auth}` |

### WiFi Configuration (WS_Station.c / WS_WiFiConfigAPI.c)

| Method | URI | Auth | Description |
|--------|-----|------|-------------|
| GET | `/api/wifi/status` | None | `{mode, ap_ssid}` |
| POST | `/api/wifi/config` | Conditional* | Save SSID/password, restart |
| GET | `/api/wifi/networks` | Conditional* | WiFi scan results |

\*WiFi endpoints require session auth only when STA is connected; unauthenticated on soft-AP.

### Mode (ExampleAPI.c)

| Method | URI | Auth | Description |
|--------|-----|------|-------------|
| GET | `/api/mode` | Session | Get current device mode |
| POST | `/api/mode` | Session+CSRF | Set device mode |

## React SPA Hosting

- Static files served from FatFS `/react/` partition
- **Gzip-aware**: Serves `.gz` files with `Content-Encoding: gzip` header
- **MIME detection**: Automatic Content-Type based on file extension
- **SPA catch-all**: Unmatched routes redirect to `index.html` (registered last)
- **Route priority**: Static files → API routes → SPA catch-all

## Kconfig Options

```kconfig
menu "WebServer Auth"
    config WS_AUTH_USERNAME
        string "Admin username"
        default "admin"
    config WS_AUTH_PASSWORD
        string "Admin password"
        default "password123"
endmenu
```

## HTTP Server Configuration

| Setting | Value |
|---------|-------|
| Max URI handlers | 64 |
| Stack size | 16384 bytes |
| Send timeout | 30 seconds |
| Receive timeout | 30 seconds |
| Wildcard URI | Enabled |

## Event Handlers

```c
void Ws_EventHandler_StaIP(void* pvArg, esp_event_base_t, int32_t, void*);    // STA got IP
void Ws_EventHandler_StaWiFi(void* pvArg, esp_event_base_t, int32_t, void*);  // STA WiFi events
void Ws_EventHandler_ApWiFi(void* pvArg, esp_event_base_t, int32_t, void*);   // AP WiFi events
void Ws_EventHandlers_SuspendReconnect(void);   // Pause STA reconnect during WiFi scan
void Ws_EventHandlers_ResumeReconnect(void);    // Resume STA reconnect after WiFi scan
```

## Dependencies

- WiFi_Manager (credentials, config state)
- Scheduler (schedule data, bell control)
- TouchScreen Services (PIN management)
- FileSystem (FatFS for React assets)
- cJSON (request/response parsing)
- ESP HTTP Server (`esp_http_server`)
- mDNS (`ringy.local`)
