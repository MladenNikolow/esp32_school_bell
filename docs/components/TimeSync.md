# TimeSync Component

## Purpose

NTP-based time synchronization with timezone support. Uses SNTP to keep the system clock accurate, with persistence of timezone settings in NVS (SPIFFS fallback).

## Files

```
components/TimeSync/
├── CMakeLists.txt
└── src/
    ├── TimeSync_API.h         # Public API
    └── TimeSync_API.c         # Implementation
```

## API

```c
esp_err_t TimeSync_Init(void);                                    // Initialize SNTP + load timezone
esp_err_t TimeSync_SetTimezone(const char* pcTzPosix);            // Set and persist timezone
esp_err_t TimeSync_GetTimezone(char* pcOutBuf, size_t ulBufLen);  // Get current timezone string
esp_err_t TimeSync_GetLocalTime(struct tm* ptTimeInfo);           // Get current local time
bool      TimeSync_IsSynced(void);                                // Check if time is valid
uint32_t  TimeSync_GetLastSyncAgeSec(void);                       // Seconds since last NTP sync
void      TimeSync_ForceSync(void);                               // Trigger immediate NTP resync
```

## NTP Servers

Configured in sdkconfig (3 servers for redundancy):
1. `pool.ntp.org`
2. `time.google.com`
3. `time.cloudflare.com`

## Timezone

- Format: POSIX TZ string (e.g., `"EET-2EEST,M3.5.0/3,M10.5.0/4"` for Eastern European Time)
- **Load order**: NVS namespace `"timesync"` key `"tz_posix"` → SPIFFS `/storage/settings.json` fallback
- **Persistence**: Saved to NVS on `SetTimezone()` call

## Sync Behavior

- **Staleness threshold**: 86400 seconds (24 hours) — `IsSynced()` returns `false` if sync is too old
- **Pre-sync on RTC**: If RTC holds plausible time (year ≥ 2024 / `tm_year >= 124`) after soft reboot, marks as pre-synced
- **Auto-restart**: Triggers resync on WiFi IP acquisition event
- **Year validation**: Rejects any time with year < 2024 as invalid

## Dependencies

- NVS (timezone persistence)
- FileSystem / SPIFFS (timezone fallback from `settings.json`)
- ESP-IDF SNTP subsystem
- WiFi event system (IP_EVENT for auto-resync)
