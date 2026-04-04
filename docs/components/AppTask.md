# AppTask Component

## Purpose

Main application orchestrator. The sole task created by `app_main()` — it initializes all subsystems in a strict 4-phase sequence, then enters an event-processing loop. All other components are created and managed by AppTask.

## Files

```
components/AppTask/
├── CMakeLists.txt
└── src/
    ├── AppTask_API.h          # Public API (Create)
    ├── AppTask_API.c          # Implementation (init phases, event loop)
    └── AppTask_Public.h       # Shared types (params, events)
```

## API

```c
typedef struct _APP_TASK_RSC_T*  APP_TASK_H;    // Opaque handle

typedef struct {
    uint32_t ulTaskPriority;                     // FreeRTOS task priority
} APP_TASK_PARAMS_T;

int32_t AppTask_Create(APP_TASK_PARAMS_T* ptParams, APP_TASK_H* phAppTask);
```

## Initialization Phases

| Phase | Description | Components Initialized |
|-------|-------------|----------------------|
| **1. Hardware & Storage** | Low-level hardware and persistent storage | NVS, FatFS, SPIFFS, Display hardware |
| **2. Asset Verification** | Verify React SPA assets exist in `/react/` | FatFS file checks |
| **3. Connectivity & Services** | Network and application services | WiFi_Manager, RingBell, Scheduler, WebServer, TimeSync |
| **4. UI Initialization** | Display and user interface | TouchScreen, Splash screen, Setup wizard or Dashboard |

## Event System

- **Queue**: 4-element FreeRTOS queue of `APP_TASK_EVENT_T`
- **Event loop**: Blocks on `xQueueReceive(portMAX_DELAY)` in `appTask_Process()`
- **Event type**: `APP_TASK_EVENT_T { ulEvent, pvData, ulDataLen }`

## FreeRTOS Usage

| Resource | Value |
|----------|-------|
| Task name | `"APP_TASK"` |
| Stack size | 4096 bytes |
| Priority | Configurable (default 3) |
| Queue depth | 4 events |

## Behavior

- If WiFi is already configured → displays Dashboard after splash
- If WiFi is not configured → launches WiFi Setup screen or Setup Wizard
- Setup wizard callback: marks setup complete, saves WiFi credentials, triggers restart
- WiFi setup callback: saves credentials and triggers restart

## Dependencies

All other components: NVS, FileSystem, WiFi_Manager, RingBell, Scheduler, WebServer, TimeSync, TouchScreen

## Internal Resource Structure

The `APP_TASK_RSC_T` struct holds handles to all subsystems:
- `WIFI_MANAGER_H` — WiFi credential manager
- `WEB_SERVER_H` — HTTP server
- `TOUCHSCREEN_H` — Display UI
- `SCHEDULER_H` — Bell scheduler
