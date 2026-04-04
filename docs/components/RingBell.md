# RingBell Component

## Purpose

Hardware bell control via GPIO output. Supports timed ringing with automatic stop, and a "panic mode" for continuous ringing that persists across reboots via NVS.

## Files

```
components/RingBell/
├── CMakeLists.txt
└── src/
    ├── RingBell_API.h         # Public API
    └── RingBell_API.c         # Implementation
```

## API

```c
typedef enum {
    BELL_STATE_IDLE    = 0,    // Bell is silent
    BELL_STATE_RINGING = 1,    // Bell is ringing (timed or manual)
    BELL_STATE_PANIC   = 2,    // Bell is ringing continuously (emergency)
} BELL_STATE_E;

esp_err_t    RingBell_Init(void);                           // Initialize GPIO + restore panic state from NVS
esp_err_t    RingBell_Run(void);                            // Start ringing (manual)
esp_err_t    RingBell_Stop(void);                           // Stop ringing
esp_err_t    RingBell_RunForDuration(uint32_t ulDurationSec);  // Ring for N seconds, auto-stop
esp_err_t    RingBell_SetPanic(bool bEnable);               // Enable/disable panic mode (persisted)
BELL_STATE_E RingBell_GetState(void);                       // Get current bell state
bool         RingBell_IsPanic(void);                        // Check if panic mode is active
```

## Bell States

```
         ┌──────────┐
    ┌────│   IDLE   │────┐
    │    └──────────┘    │
    │         │          │
    │   Run() │          │ SetPanic(true)
    │         ▼          ▼
    │    ┌──────────┐  ┌──────────┐
    │    │ RINGING  │  │  PANIC   │
    │    └──────────┘  └──────────┘
    │         │          │
    │  Stop() │          │ SetPanic(false)
    │    or timer        │
    └─────────┘──────────┘
                  │
                  ▼
             ┌──────────┐
             │   IDLE   │
             └──────────┘
```

## Key Behaviors

- **Timed ringing**: `RunForDuration()` uses a one-shot FreeRTOS timer to auto-stop
- **Panic mode**: Continuous ringing, persisted in NVS namespace `"bell"` key `"panic"` — auto-restored on boot
- **State protection**: Cannot start a timed ring during panic mode

## Dependencies

- NVS (panic state persistence)
- GPIO driver (bell relay output)
