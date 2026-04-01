# NVS Component

## Purpose

Thin wrapper around ESP-IDF Non-Volatile Storage (NVS) APIs. Provides namespace-based key-value storage for persistent configuration data across reboots.

## Files

```
components/NVS/
├── CMakeLists.txt
└── src/
    ├── NVS_API.h              # Public API
    └── NVS_API.c              # Implementation
```

## API

```c
esp_err_t NVS_Init(void);
esp_err_t NVS_Deinit(void);

// Namespace operations
esp_err_t NVS_Open(const char* pcNamespace, uint8_t usOpenMode, nvs_handle_t* phNvsHandle);
esp_err_t NVS_Close(nvs_handle_t hNvsHandle);
esp_err_t NVS_Commit(nvs_handle_t hNvsHandle);

// String operations (null-terminated)
esp_err_t NVS_ReadString(nvs_handle_t h, const char* pcKey, void* pvOutBuffer, size_t* pLen);
esp_err_t NVS_WriteString(nvs_handle_t h, const char* pcKey, const void* pvInBuffer);

// Blob operations (arbitrary binary data)
esp_err_t NVS_Read(nvs_handle_t h, const char* pcKey, void* pvOutBuffer, size_t* pLen);
esp_err_t NVS_Write(nvs_handle_t h, const char* pcKey, const void* pvInBuffer, size_t usLen);
```

## NVS Namespaces

| Namespace | Component | Keys |
|-----------|-----------|------|
| `wifi_manager` | WiFi_Manager | `ssid`, `password`, `config_state` |
| `bell` | RingBell | `panic` (panic mode persistence) |
| `timesync` | TimeSync | `tz_posix` (timezone string) |
| `touchscreen` | TouchScreen Services | `pin`, `setup_complete`, `language` |

## Usage Pattern

```c
nvs_handle_t hNvs;
NVS_Open("wifi_manager", NVS_READWRITE, &hNvs);
NVS_WriteString(hNvs, "ssid", "MyNetwork");
NVS_Commit(hNvs);
NVS_Close(hNvs);
```

## Dependencies

- ESP-IDF `nvs_flash` / `nvs` APIs
