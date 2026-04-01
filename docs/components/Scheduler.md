# Scheduler Component

## Purpose

Core bell scheduling engine. Manages two shifts of bell times, holidays, exceptions (date-based overrides), custom bell sets, and reusable bell templates. Runs a background task that fires bells at scheduled times based on the current day type.

## Files

```
components/Scheduler/
├── CMakeLists.txt
└── src/
    ├── Scheduler_API.h        # Public API (init, reload, status, next bell)
    ├── Scheduler_API.c        # Background task, day-type logic, bell firing
    ├── Schedule_Data.h        # Data structures + persistence layer
    └── Schedule_Data.c        # JSON ↔ struct conversion, SPIFFS read/write
```

## Scheduler API

```c
typedef struct _SCHEDULER_RSC_T* SCHEDULER_H;   // Opaque handle

esp_err_t Scheduler_Init(SCHEDULER_H* phScheduler);
esp_err_t Scheduler_ReloadSchedule(SCHEDULER_H h);
esp_err_t Scheduler_GetNextBell(SCHEDULER_H h, NEXT_BELL_INFO_T* ptInfo);
esp_err_t Scheduler_GetStatus(SCHEDULER_H h, SCHEDULER_STATUS_T* ptStatus);
```

## Data Structures

### Bell Entry
```c
typedef struct {
    uint8_t  ucHour;           // 0–23
    uint8_t  ucMinute;         // 0–59
    uint16_t usDurationSec;    // Ring duration in seconds
    char     acLabel[48];      // Human-readable label
} BELL_ENTRY_T;
```

### Shift
```c
#define SCHEDULE_MAX_BELLS_PER_SHIFT  50

typedef struct {
    bool          bEnabled;
    uint32_t      ulBellCount;
    BELL_ENTRY_T  atBells[SCHEDULE_MAX_BELLS_PER_SHIFT];
} SCHEDULE_SHIFT_T;
```

### Holiday
```c
typedef struct {
    char acStartDate[11];      // "YYYY-MM-DD"
    char acEndDate[11];        // "YYYY-MM-DD"
    char acLabel[48];
} HOLIDAY_T;
```

### Exception Entry
```c
typedef enum {
    EXCEPTION_ACTION_DAY_OFF = 0,
    EXCEPTION_ACTION_NORMAL,           // Use normal schedule
    EXCEPTION_ACTION_FIRST_SHIFT,      // Force first shift
    EXCEPTION_ACTION_SECOND_SHIFT,     // Force second shift
    EXCEPTION_ACTION_TEMPLATE,         // Use a bell template
    EXCEPTION_ACTION_CUSTOM,           // Use a custom bell set
} EXCEPTION_ACTION_E;

typedef struct {
    char                acStartDate[11];
    char                acEndDate[11];
    char                acLabel[48];
    EXCEPTION_ACTION_E  eAction;
    int16_t             sTimeOffsetMin;    // ±120 minutes time shift
    int8_t              cTemplateIdx;      // Index into templates array
    int8_t              cCustomBellsIdx;   // Index into custom bell sets array
} EXCEPTION_ENTRY_T;
```

### Complete Schedule Data
```c
typedef struct {
    SCHEDULE_SETTINGS_T       tSettings;               // Timezone + working days
    SCHEDULE_SHIFT_T          tFirstShift;              // Up to 50 bells
    SCHEDULE_SHIFT_T          tSecondShift;             // Up to 50 bells
    HOLIDAY_T                 atHolidays[50];
    uint32_t                  ulHolidayCount;
    EXCEPTION_ENTRY_T         atExceptions[40];
    uint32_t                  ulExceptionCount;
    EXCEPTION_CUSTOM_BELLS_T  atCustomBellSets[5];     // Up to 30 bells each
    uint32_t                  ulCustomBellSetCount;
    BELL_TEMPLATE_T           atTemplates[5];
    uint32_t                  ulTemplateCount;
} SCHEDULE_DATA_T;
```

### Status & Next Bell
```c
typedef enum {
    DAY_TYPE_OFF = 0,
    DAY_TYPE_WORKING,
    DAY_TYPE_HOLIDAY,
    DAY_TYPE_EXCEPTION_WORKING,
    DAY_TYPE_EXCEPTION_HOLIDAY,
} DAY_TYPE_E;

typedef struct {
    bool     bValid;
    uint8_t  ucHour;
    uint8_t  ucMinute;
    uint16_t usDurationSec;
    char     acLabel[48];
} NEXT_BELL_INFO_T;

typedef struct {
    bool              bRunning;
    bool              bTimeSynced;
    uint32_t          ulLastSyncAgeSec;
    DAY_TYPE_E        eDayType;
    NEXT_BELL_INFO_T  tNextBell;
    char              acCurrentTime[9];    // "HH:MM:SS"
    char              acCurrentDate[11];   // "YYYY-MM-DD"
} SCHEDULER_STATUS_T;
```

## Schedule_Data API (Persistence Layer)

```c
// Load/Save from SPIFFS JSON files
esp_err_t Schedule_Data_LoadSettings(SCHEDULE_SETTINGS_T* ptSettings);
esp_err_t Schedule_Data_SaveSettings(const SCHEDULE_SETTINGS_T* ptSettings);
esp_err_t Schedule_Data_LoadBells(SCHEDULE_SHIFT_T* ptFirst, SCHEDULE_SHIFT_T* ptSecond);
esp_err_t Schedule_Data_SaveBells(const SCHEDULE_SHIFT_T* ptFirst, const SCHEDULE_SHIFT_T* ptSecond);
esp_err_t Schedule_Data_LoadCalendar(SCHEDULE_DATA_T* ptData);
esp_err_t Schedule_Data_SaveCalendar(const SCHEDULE_DATA_T* ptData);
esp_err_t Schedule_Data_LoadTemplates(SCHEDULE_DATA_T* ptData);
esp_err_t Schedule_Data_SaveTemplates(const SCHEDULE_DATA_T* ptData);
esp_err_t Schedule_Data_CreateDefaults(void);
esp_err_t Schedule_Data_CleanupExpiredExceptions(void);

// JSON serializers (caller must cJSON_Delete the result)
cJSON* Schedule_Data_SettingsToJson(const SCHEDULE_SETTINGS_T* ptSettings);
cJSON* Schedule_Data_BellsToJson(const SCHEDULE_SHIFT_T* ptFirst, const SCHEDULE_SHIFT_T* ptSecond);
cJSON* Schedule_Data_HolidaysToJson(const HOLIDAY_T* ptHolidays, uint32_t ulCount);
cJSON* Schedule_Data_ExceptionsToJson(const EXCEPTION_ENTRY_T* ptExceptions, uint32_t ulCount, ...);
cJSON* Schedule_Data_TemplatesToJson(const BELL_TEMPLATE_T* ptTemplates, uint32_t ulCount);
cJSON* Schedule_Data_ReadDefaultsJson(void);
```

## SPIFFS File Paths

| Constant | Path | Contents |
|----------|------|----------|
| `SCHEDULE_FILE_SETTINGS` | `/storage/settings.json` | `{ timezone, workingDays }` |
| `SCHEDULE_FILE_BELLS` | `/storage/schedule.json` | `{ firstShift, secondShift }` |
| `SCHEDULE_FILE_CALENDAR` | `/storage/calendar.json` | `{ holidays, exceptions, customBellSets }` |
| `SCHEDULE_FILE_TEMPLATES` | `/storage/templates.json` | `{ templates }` |
| `SCHEDULE_FILE_DEFAULTS` | `/react/default_schedule.json` | Factory defaults (read-only) |

## Limits

| Resource | Maximum |
|----------|---------|
| Total bells (both shifts) | 100 |
| Bells per shift | 50 |
| Holidays | 50 |
| Exceptions | 40 |
| Custom bells per set | 30 |
| Custom bell sets | 5 |
| Bell templates | 5 |
| Time offset | ±120 minutes |

## Day Type Resolution

Priority-based evaluation (highest to lowest):
1. **Exception** → check exceptions array for today's date
2. **Holiday** → check holiday date ranges
3. **Working day** → check `workingDays` array (0=Sun, 6=Sat)
4. **Off** → default if none match

Day type is cached once per day and refreshed at midnight.

## Background Task

- **Stack**: 8192 bytes, priority 2
- **Behavior**: Checks the current time every second against today's bell list
- **Firing**: Calls `RingBell_RunForDuration()` when a bell time matches
- **Time sync**: Only fires bells when `TimeSync_IsSynced()` is true
- **Cleanup**: Auto-removes expired exceptions daily
- **Thread safety**: Mutex-protected access to schedule data

## Dependencies

- FileSystem (SPIFFS — schedule persistence)
- TimeSync (current time, sync status)
- RingBell (bell firing)
- cJSON (JSON serialization/deserialization)
