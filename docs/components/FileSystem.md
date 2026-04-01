# FileSystem Component

## Purpose

Dual filesystem abstraction layer. Manages two separate filesystems:
- **FatFS** — mounted at `/react/` for React web application assets (gzipped HTML, JS, CSS)
- **SPIFFS** — mounted at `/storage/` for schedule configuration JSON files

## Files

```
components/FileSystem/
├── CMakeLists.txt
├── FatFS/
│   ├── FatFS_API.h            # FatFS init + debug listing
│   └── FatFS_API.c
└── SPIFFS/
    ├── SPIFFS_API.h           # SPIFFS init + file read/write/exists
    └── SPIFFS_API.c
```

## FatFS API

```c
esp_err_t FatFS_Init(void);                // Mount FatFS partition at /react/
void      debug_list_react_assets(void);   // Log all files in /react/ (debug)
```

### FatFS Configuration
- **Mount point**: `/react/`
- **Partition label**: `fatfs-react`
- **Allocation unit**: 4096 bytes
- **Contents**: React SPA build output (gzipped static files)

## SPIFFS API

```c
#define SPIFFS_MOUNT_POINT "/storage"

esp_err_t SPIFFS_Init(void);
esp_err_t SPIFFS_ReadFile(const char* pcPath, char* pcOutBuf, size_t ulBufSize, size_t* pulBytesRead);
esp_err_t SPIFFS_WriteFile(const char* pcPath, const char* pcData, size_t ulDataLen);
bool      SPIFFS_FileExists(const char* pcPath);
```

### SPIFFS Configuration
- **Mount point**: `/storage/`
- **Max open files**: 8
- **Contents**: Schedule, settings, calendar, and template JSON files

### SPIFFS File Inventory

| File | Purpose | Used By |
|------|---------|---------|
| `/storage/settings.json` | Timezone + working days | Scheduler, TimeSync |
| `/storage/schedule.json` | First/second shift bell definitions | Scheduler |
| `/storage/calendar.json` | Holidays + exceptions + custom bell sets | Scheduler |
| `/storage/templates.json` | Reusable bell templates | Scheduler |

## Dependencies

- ESP-IDF VFS subsystem
- `esp_spiffs` (SPIFFS driver)
- `esp_vfs_fat` (FatFS driver)
