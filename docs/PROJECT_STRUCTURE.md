# ESP32 School Bell — Project Structure

## 📁 Directory Layout

```
esp32_school_bell/
├── main/
│   ├── main.c                         # Entry point — creates AppTask, deletes self
│   ├── CMakeLists.txt                 # Main component build config
│   ├── Kconfig.projbuild              # BSP config (IO expander I2C address)
│   └── idf_component.yml             # Component manager dependencies
│
├── components/
│   ├── AppTask/                       # 🎯 Orchestrator — 4-phase boot sequence
│   ├── FileSystem/                    # 💾 FatFS + SPIFFS dual filesystem
│   ├── Generic/                       # 📦 Shared error codes & types
│   ├── NVS/                           # 🔑 Non-Volatile Storage wrapper
│   ├── RingBell/                      # 🔔 GPIO bell control + panic mode
│   ├── Scheduler/                     # 📅 Bell scheduling engine
│   ├── TimeSync/                      # ⏰ NTP time synchronization
│   ├── TouchScreen/                   # 📱 LVGL UI framework (display + touch)
│   ├── WebServer/                     # 🌐 HTTP server + REST API + React SPA
│   └── WiFi_Manager/                  # 📶 WiFi credential management
│
├── data/
│   ├── CMakeLists.txt                 # FatFS image build
│   ├── default_schedule.json          # Factory default schedule (Bulgarian)
│   └── assets/                        # Static assets for FatFS partition
│
├── esp32_school_bell_web/             # 🖥️ React frontend (git submodule)
│
├── managed_components/                # ESP-IDF component manager packages
│   ├── espressif__mdns/               # mDNS service
│   ├── espressif__esp_lvgl_port/      # LVGL port for ESP-IDF
│   ├── lvgl__lvgl/                    # LVGL graphics library
│   ├── waveshare__esp32_s3_touch_lcd_4/  # BSP for Waveshare board
│   └── ...                            # LCD drivers, touch drivers, etc.
│
├── scripts/
│   └── erase_wifi_credentials.py      # Utility to wipe NVS WiFi credentials
│
├── CMakeLists.txt                     # Top-level project CMake (patches, overrides)
├── partitions.csv                     # Flash partition table
├── sdkconfig.defaults                 # Default build configuration
└── sdkconfig                          # Active build configuration
```

## 🔄 Boot Sequence & Data Flow

```
app_main()
  │  Creates AppTask (priority 3), deletes self
  ▼
AppTask — 4-Phase Initialization
  │
  ├─ Phase 1: Hardware & Storage
  │     NVS_Init() → FatFS_Init() → SPIFFS_Init() → DisplayInit()
  │
  ├─ Phase 2: Asset Verification
  │     Verify React assets exist in /react/ (FatFS)
  │
  ├─ Phase 3: Connectivity & Services
  │     WiFi_Manager_Init() → RingBell_Init() → Scheduler_Init()
  │     → Ws_Init() (WebServer) → TimeSync_Init()
  │
  └─ Phase 4: UI Initialization
        TouchScreen_Init() → Splash → Setup Wizard or Dashboard
```

### Component Dependencies

```
AppTask (Orchestrator)
├── NVS
├── FileSystem
│   ├── FatFS (/react — web assets)
│   └── SPIFFS (/storage — JSON configs)
├── WiFi_Manager → NVS
├── RingBell → NVS
├── Scheduler
│   ├── FileSystem (SPIFFS)
│   ├── TimeSync
│   ├── RingBell
│   └── cJSON
├── WebServer
│   ├── WiFi_Manager
│   ├── Scheduler
│   ├── RingBell (via ScheduleAPI)
│   ├── TouchScreen Services (PIN)
│   ├── FileSystem (FatFS — React SPA)
│   └── Auth (session management)
├── TimeSync
│   ├── NVS
│   └── FileSystem (SPIFFS — settings fallback)
└── TouchScreen
    ├── LVGL + BSP
    ├── RingBell (via Services)
    ├── Scheduler (via Services)
    ├── WiFi_Manager (via Services)
    ├── TimeSync (via Services)
    └── NVS (via Services)
```

## 🎯 Component Responsibilities

Each component is documented in its own file under `docs/components/`:

| Component | Doc File | Purpose |
|-----------|----------|---------|
| **AppTask** | [AppTask.md](components/AppTask.md) | Main orchestrator — 4-phase boot, event queue |
| **FileSystem** | [FileSystem.md](components/FileSystem.md) | Dual filesystem: FatFS + SPIFFS |
| **Generic** | [Generic.md](components/Generic.md) | Shared error codes and types |
| **NVS** | [NVS.md](components/NVS.md) | Non-Volatile Storage wrapper |
| **RingBell** | [RingBell.md](components/RingBell.md) | GPIO bell control, panic mode, timed ringing |
| **Scheduler** | [Scheduler.md](components/Scheduler.md) | Bell scheduling engine with shifts, holidays, exceptions |
| **TimeSync** | [TimeSync.md](components/TimeSync.md) | NTP time synchronization with timezone support |
| **TouchScreen** | [TouchScreen.md](components/TouchScreen.md) | LVGL-based UI: screens, navigation, services, i18n |
| **WebServer** | [WebServer.md](components/WebServer.md) | HTTP server, REST API, React SPA hosting, auth |
| **WiFi_Manager** | [WiFi_Manager.md](components/WiFi_Manager.md) | WiFi credential storage and state management |

## 💾 Storage Architecture

### Flash Partition Table

| Name | Type | SubType | Offset | Size | Purpose |
|------|------|---------|--------|------|---------|
| `nvs` | data | nvs | 0x9000 | 24KB | Key-value config storage |
| `phy_init` | data | phy | 0xF000 | 4KB | WiFi PHY calibration data |
| `factory` | app | factory | 0x10000 | 8MB | Application firmware |
| `fatfs-react` | data | fat | 0x810000 | 3MB | React web interface (gzipped) |
| `storage` | data | spiffs | 0xB10000 | 4MB | Schedule/settings JSON files |

### Filesystem Mount Points

| Mount Point | Type | Contents |
|-------------|------|----------|
| `/react/` | FatFS | React SPA build (HTML, JS, CSS — gzipped) |
| `/storage/` | SPIFFS | `settings.json`, `schedule.json`, `calendar.json`, `templates.json` |

### NVS Namespaces

| Namespace | Used By | Keys |
|-----------|---------|------|
| `wifi_manager` | WiFi_Manager | `ssid`, `password`, `config_state` |
| `bell` | RingBell | `panic` (panic mode flag) |
| `timesync` | TimeSync | `tz_posix` (timezone string) |
| `touchscreen` | TouchScreen Services | `pin`, `setup_complete`, `language` |
| `auth` | WebServer Auth | `svc_salt`, `svc_hash`, `cli_user`, `cli_salt`, `cli_hash`, `cli_exists` |

## ⚡ FreeRTOS Tasks

| Task | Stack | Priority | Component |
|------|-------|----------|-----------|
| APP_TASK | 4096B | 3 (configurable) | AppTask |
| Scheduler | 8192B | 2 | Scheduler |
| TouchScreen | 8192B | 2 | TouchScreen |
| LVGL Rendering | 10240B | 4 | LVGL Port |
| Touch Input | — | 5 | LVGL Port |

## 🔧 Key Design Patterns

| Pattern | Description |
|---------|-------------|
| **Opaque Handles** | Every component uses `typedef struct _X_RSC_T* X_H` — internals hidden from consumers |
| **4-Phase Init** | Hardware/Storage → Asset Verification → Connectivity → UI |
| **Service Layer** | TouchScreen Services bridge UI ↔ backend, preventing direct coupling |
| **Dual Filesystem** | SPIFFS for user data (JSON), FatFS for web assets (gzipped) |
| **NVS Namespace Isolation** | Each subsystem uses its own NVS namespace |
| **Event-Driven** | AppTask uses FreeRTOS queue; TouchScreen uses screen event system |
| **Mutex Protection** | Scheduler uses semaphore for thread-safe data access |
| **Fallback Mechanisms** | TimeSync: NVS → SPIFFS; WiFi: STA with always-available soft-AP |
| **cJSON Serialization** | Schedule data uses cJSON for JSON ↔ struct conversion |
