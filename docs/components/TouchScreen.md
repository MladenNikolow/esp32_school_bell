# TouchScreen Component

## Purpose

LVGL-based user interface framework for the 480×480 capacitive touch display. Provides screen navigation, a service layer bridging UI to backend components, bilingual i18n (Bulgarian/English), and a theming system.

## Directory Structure

```
components/TouchScreen/
├── CMakeLists.txt
├── TouchScreenAPI/                    # Main entry point
│   ├── include/
│   │   └── TouchScreen_API.h         # Public API (Init, Show*)
│   └── src/
│       └── TouchScreen_API.c
│
├── TouchScreen_Services/              # Backend service bridges
│   ├── include/
│   │   └── TouchScreen_Services.h    # PIN, Setup, Bell, Schedule, WiFi services
│   └── src/
│       ├── TS_Pin_Service.c           # PIN management (NVS)
│       ├── TS_Setup_Service.c         # Setup wizard state (NVS)
│       ├── TS_Bell_Service.c          # Bell control (→ RingBell)
│       ├── TS_Schedule_Service.c      # Schedule access (→ Scheduler)
│       └── TS_WiFi_Service.c          # WiFi scanning & config
│
└── TouchScreen_UI_Manager/            # UI framework
    ├── include/
    │   ├── TouchScreen_UI_Manager.h   # Navigation API
    │   └── TouchScreen_UI_Types.h     # Screen enum, Screen_Def_t
    └── src/
        ├── TouchScreen_UI_Manager.c   # Screen lifecycle management
        ├── ui_theme.h                 # Material Design theme + fonts
        ├── ui_config.h                # Display config, timeouts
        ├── ui_statusbar.h/c           # Status bar (clock, WiFi, bell, NTP)
        ├── ui_navbar.h/c              # Tab navigation bar
        ├── ui_strings.h/c             # i18n string table (BG/EN)
        ├── components/                # Reusable UI components
        │   ├── button/
        │   ├── card/
        │   ├── input_field/
        │   └── wifi_list_item/
        └── screens/                   # Screen implementations
            ├── splash/
            ├── wifi_setup/
            ├── dashboard/
            ├── schedule_view/
            ├── settings/
            ├── info/
            ├── pin_entry/
            └── setup_wizard/
```

## TouchScreenAPI

```c
typedef void* TOUCHSCREEN_H;    // Opaque handle

typedef struct {
    uint32_t ulTaskPriority;
} TOUCHSCREEN_PARAMS_T;

int32_t TouchScreen_DisplayInit(void);       // Initialize BSP display hardware
int32_t TouchScreen_Init(TOUCHSCREEN_PARAMS_T* ptParams, TOUCHSCREEN_H* phTouchScreen);
int32_t TouchScreen_Deinit(TOUCHSCREEN_H hTouchScreen);
int32_t TouchScreen_ShowSplash(TOUCHSCREEN_H h, uint32_t duration_ms);
int32_t TouchScreen_ShowWiFiSetup(TOUCHSCREEN_H h, void (*callback)(...));
int32_t TouchScreen_ShowDashboard(TOUCHSCREEN_H h);
int32_t TouchScreen_ShowSetupWizard(TOUCHSCREEN_H h, void (*callback)(...));
```

## UI Manager (Navigation)

```c
bool TouchScreen_UI_ManagerInit(void);
void TouchScreen_UI_NavigateTo(TouchScreen_UI_Screen_t screen);
void TouchScreen_UI_PushOverlay(TouchScreen_UI_Screen_t screen);   // PIN entry
void TouchScreen_UI_PopOverlay(void);
TouchScreen_UI_Screen_t TouchScreen_UI_GetCurrentScreen(void);
void TouchScreen_UI_Update(void);
```

## Screens

| Screen | Enum | Description |
|--------|------|-------------|
| Splash | `SCREEN_SPLASH` | Boot logo with loading animation |
| WiFi Setup | `SCREEN_WIFI_SETUP` | SSID selection + password entry |
| Dashboard | `SCREEN_DASHBOARD` | Main screen: clock, next bell, bell state |
| Schedule View | `SCREEN_SCHEDULE_VIEW` | Today's bell schedule display |
| Settings | `SCREEN_SETTINGS` | WiFi, PIN, language, system settings |
| Info | `SCREEN_INFO` | System information, version, memory |
| PIN Entry | `SCREEN_PIN_ENTRY` | Overlay for admin authentication |
| Setup Wizard | `SCREEN_SETUP_WIZARD` | First-boot guided setup |

### Screen Lifecycle

Each screen implements `TouchScreen_Screen_Def_t`:
```c
typedef struct {
    void (*create)(lv_obj_t* parent);   // Build LVGL objects
    void (*destroy)(void);              // Cleanup
    void (*update)(void);               // Periodic refresh (1s interval)
    bool  show_chrome;                  // Show status bar + navbar
} TouchScreen_Screen_Def_t;
```

## Display Layout

```
┌────────────────────────────────────┐
│         Status Bar (40px)          │  Clock, WiFi, NTP, Bell indicators
├────────────────────────────────────┤
│                                    │
│                                    │
│        Content Area (384px)        │  Active screen content
│                                    │
│                                    │
├────────────────────────────────────┤
│       Navigation Bar (56px)        │  Dashboard, Schedule, Settings, Info
└────────────────────────────────────┘
               480×480
```

## TouchScreen Services

Business-logic bridge between UI and backend — prevents direct coupling.

### PIN Service
```c
esp_err_t TS_Pin_Init(void);
bool      TS_Pin_Validate(const char* pcPin);
esp_err_t TS_Pin_Set(const char* pcNewPin);        // 4–6 digits
esp_err_t TS_Pin_Get(char* pcOutBuf, size_t len);
uint8_t   TS_Pin_GetLength(void);
bool      TS_Pin_IsConfigured(void);
esp_err_t TS_Pin_Reset(void);
bool      TS_Pin_IsLockedOut(void);                // After too many failed attempts
uint32_t  TS_Pin_GetLockoutRemaining(void);
void      TS_Pin_ResetAttempts(void);
```

### Setup Wizard Service
```c
esp_err_t TS_Setup_Init(void);
bool      TS_Setup_IsComplete(void);
esp_err_t TS_Setup_MarkComplete(void);
esp_err_t TS_Setup_Reset(void);
bool      TS_Setup_CheckMigration(void);
```

### Bell Service
```c
BELL_STATE_E TS_Bell_GetState(void);
bool         TS_Bell_IsPanic(void);
esp_err_t    TS_Bell_SetPanic(bool bEnable);
esp_err_t    TS_Bell_TestRing(uint32_t ulDurationSec);
```

### Schedule Service
```c
esp_err_t TS_Schedule_Init(SCHEDULER_H hScheduler);
esp_err_t TS_Schedule_GetStatus(SCHEDULER_STATUS_T* ptStatus);
esp_err_t TS_Schedule_GetNextBell(NEXT_BELL_INFO_T* ptInfo);
esp_err_t TS_Schedule_GetShiftBells(uint8_t ucShift, BELL_ENTRY_T* ptBells, uint32_t* pulCount, bool* pbEnabled);
esp_err_t TS_Schedule_GetSettings(SCHEDULE_SETTINGS_T* ptSettings);
esp_err_t TS_Schedule_SetTodayOverride(EXCEPTION_ACTION_E eAction);
esp_err_t TS_Schedule_CancelTodayOverride(void);
int       TS_Schedule_GetTodayOverrideAction(void);
```

### WiFi Service
```c
esp_err_t TS_WiFi_Scan(TS_WiFi_AP_t* ptResults, uint16_t* pusCount);
esp_err_t TS_WiFi_ScanAsync(void);
bool      TS_WiFi_IsScanComplete(void);
esp_err_t TS_WiFi_ScanGetResults(TS_WiFi_AP_t* ptResults, uint16_t* pusCount);
void      TS_WiFi_ScanAbort(void);
esp_err_t TS_WiFi_SaveCredentials(const char* pcSsid, const char* pcPassword);
bool      TS_WiFi_IsConnected(void);
esp_err_t TS_WiFi_GetConnectedSsid(char* pcOutBuf, size_t ulBufLen);
esp_err_t TS_WiFi_Connect(const char* pcSsid, const char* pcPassword);
esp_err_t TS_WiFi_GetIpAddress(char* pcOutBuf, size_t ulBufLen);
```

## i18n (Internationalization)

- **Languages**: Bulgarian (default), English
- **String count**: 120+ string IDs
- **Lookup**: `ui_str(STRING_ID)` returns the active language's string
- **Persistence**: Language selection stored in NVS namespace `"touchscreen"` key `"language"`
- **Coverage**: All UI labels, messages, status text, button captions

## Theme

- Material Design color palette
- Custom Montserrat fonts (12, 14, 16, 20, 24, 28px) with Latin + Cyrillic glyphs
- Card and button style helpers for consistent look

## Configuration (`ui_config.h`)

| Setting | Value |
|---------|-------|
| Screen size | 480×480 |
| Update interval | 1 second |
| Dim timeout | 30 seconds |
| Brightness | Configurable |

## FreeRTOS Usage

| Resource | Value |
|----------|-------|
| Task stack | 8192 bytes |
| Task priority | AppTask priority − 1 (default 2) |
| LVGL stack | 10240 bytes (custom, larger than default) |
| LVGL priority | 4 |
| Touch input priority | 5 (highest for responsiveness) |

## Dependencies

- LVGL graphics library
- Waveshare BSP (display + touch drivers)
- RingBell (via Bell Service)
- Scheduler (via Schedule Service)
- WiFi_Manager (via WiFi Service)
- TimeSync (via Schedule Service status)
- NVS (PIN, setup state, language persistence)
