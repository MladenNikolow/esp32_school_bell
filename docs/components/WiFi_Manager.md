# WiFi_Manager Component

## Purpose

Persistent WiFi credential management. Stores SSID and password in NVS, tracks configuration state, and provides factory defaults for the initial soft-AP network.

## Files

```
components/WiFi_Manager/
├── CMakeLists.txt
└── src/
    ├── WiFi_Manager_API.h     # Public API
    ├── WiFi_Manager_API.c     # Implementation
    └── WiFI_Manager_Public.h  # Shared types (handle, params, states)
```

## API

```c
typedef struct _WIFI_MANAGER_RSC_T* WIFI_MANAGER_H;    // Opaque handle

int32_t   WiFi_Manager_Init(WIFI_MANAGER_H* phWifiManager);
int32_t   WiFi_Manager_GetSsid(WIFI_MANAGER_H h, uint8_t* pbSsid, size_t* pSsidLen);
int32_t   WiFi_Manager_GetPassword(WIFI_MANAGER_H h, uint8_t* pbPass, size_t* pPassLen);
esp_err_t WiFi_Manager_GetConfigurationState(WIFI_MANAGER_H h, uint32_t* pulConfigurationState);
esp_err_t WiFi_Manager_SaveCredentials(const char* ssid, const char* pass);
esp_err_t WiFi_Manager_ClearCredentials(void);
```

## Configuration States

```c
typedef enum {
    WIFI_MANAGER_CONFIGURATION_STATE_NOT_CONFIGURED       = 0,   // No credentials stored
    WIFI_MANAGER_CONFIGURATION_STATE_WAITING_CONFIGURATION = 1,   // Waiting for user input
    WIFI_MANAGER_CONFIGURATION_STATE_CONFIGURED            = 2,   // Credentials stored, ready to connect
} WIFI_MANAGER_CONFIGURATION_STATE_E;
```

## Constants

| Constant | Value |
|----------|-------|
| `WIFI_MANAGER_MAX_SSID_LENGTH` | 32 |
| `WIFI_MANAGER_MAX_PASS_LENGTH` | 63 |
| Default AP SSID | `"ESP32_Setup"` |
| Default AP Password | `"12345678"` |

## NVS Configuration

- **Namespace**: `"wifi_manager"`
- **Keys**: `ssid`, `password`, `config_state`

## State Transitions

```
                    ┌──────────────────────┐
                    │   NOT_CONFIGURED     │  ← Factory default / ClearCredentials()
                    └──────────┬───────────┘
                               │
                        Init() │
                               ▼
                    ┌──────────────────────┐
                    │ WAITING_CONFIGURATION│  ← No credentials in NVS
                    └──────────┬───────────┘
                               │
               SaveCredentials()│
                               ▼
                    ┌──────────────────────┐
                    │     CONFIGURED       │  ← Credentials stored in NVS
                    └──────────────────────┘
```

## Dependencies

- NVS (credential persistence)
- ESP-IDF WiFi driver (used by WebServer for actual WiFi operations)
