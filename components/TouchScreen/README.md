# TouchScreen Component Implementation

## Overview
A new **TouchScreen** component has been created to manage the touch screen display and UI logic. It replaces the previous `UI_TASK` with a more robust, persistent task-based component following the same architectural pattern as other components in the project.

## Architecture

### Component Structure
The TouchScreen component is organized into two independent modules:

```
components/TouchScreen/
├── CMakeLists.txt                          (Build configuration)
├── README.md                               (Documentation)
│
├── TouchScreenAPI/                         (Main API Module)
│   ├── include/
│   │   └── TouchScreen_API.h               (Public API)
│   └── src/
│       └── TouchScreen_API.c               (Implementation)
│
└── TouchScreen_UI_Manager/                 (UI Management Module)
    ├── include/
    │   ├── TouchScreen_UI_Manager.h        (Public UI API)
    │   └── TouchScreen_UI_Types.h          (Public type definitions)
    ├── src/
    │   ├── ui_manager.c                    (Core UI state machine)
    │   ├── ui_manager_internal.h           (Internal UI interface)
    │   ├── ui_config.h                     (Configuration constants)
    │   └── ui_types.h                      (Internal type definitions)
    ├── assets/
    │   ├── ringy_logo.h                    (Logo image descriptor)
    │   └── ringy_logo.c                    (256x256 RGB565 logo data)
    ├── screens/
    │   ├── splash/
    │   │   ├── splash_screen.c             (Splash screen implementation)
    │   │   └── splash_screen_internal.h    (Splash screen interface)
    │   └── wifi_setup/
    │       ├── wifi_setup_screen.c         (WiFi setup form implementation)
    │       └── wifi_setup_screen_internal.h (WiFi setup interface)
    └── components/
        ├── button/
        │   ├── button_component.c          (Reusable button widget)
        │   └── button_component.h          (Button interface)
        └── input_field/
            ├── input_field_component.c     (Reusable input field widget)
            └── input_field_component.h     (Input field interface)
```

### Initialization Order
```
1. Display/BSP (bsp_display_start)  - Called in app_main
2. APP_TASK (Priority 3)            - Manages core functionality
   ├── NVS Initialization
   ├── WiFi Manager Initialization
   ├── File System Initialization
   ├── Web Server Initialization
   └── TouchScreen Initialization
3. TouchScreen (Priority 2)         - Manages UI and display updates
   ├── UI Manager Initialization
   ├── Splash Screen Display
   └── WiFi Setup Screen Display
4. LVGL Task (Priority 4)           - Display rendering (managed component)
5. Touch HID Task (Priority 5)      - Touch input (managed component)
```

## Task Priorities

| Task | Priority | Purpose |
|------|----------|---------|
| **Touch HID Input** | 5 | Highest - Responsive touch handling |
| **LVGL Rendering** | 4 | High - Display updates |
| **APP_TASK** | 3 | Medium - Core application logic |
| **TouchScreen** | 2 | Lower - UI screen management |
| **FreeRTOS Timer** | 1 | System |
| **IDLE Task** | 0 | System |

## Module Responsibilities

### TouchScreenAPI Module
Provides the main interface to the TouchScreen component:
- **File**: `TouchScreenAPI/src/TouchScreen_API.c`
- **Header**: `TouchScreenAPI/include/TouchScreen_API.h`
- **Purpose**: 
  - Manages the persistent FreeRTOS task
  - Handles event-based communication via queue
  - Provides high-level API functions
  - Bridges application layer with UI Manager

### TouchScreen_UI_Manager Module
Manages internal UI state, screens, and display components:
- **Main Implementation**: `TouchScreen_UI_Manager/src/ui_manager.c`
- **Public Headers**: 
  - `TouchScreen_UI_Manager/include/TouchScreen_UI_Manager.h`
  - `TouchScreen_UI_Manager/include/TouchScreen_UI_Types.h`
- **Internal Implementation**: 
  - UI state machine and screen transitions
  - Screen definitions (Splash, WiFi Setup)
  - Reusable UI components (Button, Input Field)
  - Asset management (Logo)
- **Purpose**:
  - Encapsulates all UI logic and rendering
  - Provides internal API to TouchScreenAPI
  - Manages screen lifecycle
  - Handles LVGL integration

## Internal Architecture

### 1. **Event-Based Communication (TouchScreenAPI)**
- TouchScreenAPI task communicates via a FreeRTOS queue
- Events are processed in TouchScreenAPI task loop
- Supports multiple event types for screen transitions
- Non-blocking event posting with timeout handling

### 2. **UI State Machine (TouchScreen_UI_Manager)**
- Maintains current screen state and transitions
- TouchScreen_UI_Manager handles internal state transitions
- Screens manage LVGL objects and lifecycle
- Callback mechanism for screen completion events

### 3. **Modular UI Components**
- **Button Component**: Reusable button widget with callbacks
- **Input Field Component**: Text input with validation support
- **Screen System**: Base functionality for creating new screens
- Components are self-contained and can be extended

### 4. **Asset Management**
- Logo stored as binary image data (256x256 RGB565)
- LVGL format for efficient rendering
- Embedded in application image

### 5. **Separation of Concerns**
- **TouchScreenAPI**: External interface and task management
- **TouchScreen_UI_Manager**: Internal UI logic and rendering
- Clear boundary between public and private implementations
- Easy to extend with new screens and components

## Public API

### TouchScreenAPI Functions

#### `TouchScreen_Init()`
Initialize the TouchScreen component and start the management task.

```c
TOUCHSCREEN_PARAMS_T params = {
    .ulTaskPriority = APP_TASK_PRIORITY - 1  // Priority 2
};
TouchScreen_Init(&params, &hTouchScreen);
```
**Module**: TouchScreenAPI  
**File**: `TouchScreenAPI/src/TouchScreen_API.c`

#### `TouchScreen_DisplayInit()`
Initialize the BSP display and set up LVGL.

```c
TouchScreen_DisplayInit();
```
**Module**: TouchScreenAPI  
**File**: `TouchScreenAPI/src/TouchScreen_API.c`

### TouchScreen_UI_Manager Functions

#### `TouchScreen_ShowSplash()`
Display the splash screen for a specified duration.

```c
TouchScreen_ShowSplash(hTouchScreen, 5000);  // 5 seconds
```
**Module**: TouchScreenAPI (exposed)  
**Implementation**: TouchScreen_UI_Manager  
**File**: `TouchScreen_UI_Manager/src/ui_manager.c`

#### `TouchScreen_ShowWiFiSetup()`
Display the WiFi setup screen with optional callback.

```c
TouchScreen_ShowWiFiSetup(hTouchScreen, wifi_callback_function);
```
**Module**: TouchScreenAPI (exposed)  
**Implementation**: TouchScreen_UI_Manager  
**File**: `TouchScreen_UI_Manager/src/ui_manager.c`

#### `TouchScreen_Deinit()`
Clean up and stop the TouchScreen component.

```c
TouchScreen_Deinit(hTouchScreen);
```
**Module**: TouchScreenAPI  
**File**: `TouchScreenAPI/src/TouchScreen_API.c`

### Switching Screens
Screens are managed through `TouchScreen_UI_Manager`:
- **Splash Screen**: `TouchScreen_UI_Manager/screens/splash/`
- **WiFi Setup Screen**: `TouchScreen_UI_Manager/screens/wifi_setup/`
- Each screen has internal state and LVGL object management

## Internal API (TouchScreen_UI_Manager)

### Screen Interface
```c
// Create new screen
void touchscreen_splash_screen_create(uint32_t duration_ms);
void touchscreen_wifi_setup_screen_create(TouchScreen_WiFi_Setup_Callback_t callback);

// UI component functions
lv_obj_t *touchscreen_button_create(lv_obj_t *parent, const char *text, 
                                    touchscreen_button_callback_t callback);
lv_obj_t *touchscreen_input_field_create(lv_obj_t *parent, const char *placeholder);
```

**Location**: `TouchScreen_UI_Manager/src/`

## Integration Points

### AppTask Integration
The TouchScreen is initialized within `AppTask_Init()`:

```c
// In AppTask_Init() after WebServer initialization:
TOUCHSCREEN_PARAMS_T tTouchScreenParams = {
    .ulTaskPriority = ptAppTaskRsc->tParams.ulTaskPriority - 1,
};
lResult = TouchScreen_Init(&tTouchScreenParams, &ptAppTaskRsc->hTouchScreen);

// Show UI sequences
TouchScreen_ShowSplash(ptAppTaskRsc->hTouchScreen, 5000);
TouchScreen_ShowWiFiSetup(ptAppTaskRsc->hTouchScreen, NULL);
```

### Main Application
`main.c` is simplified:
1. Initialize BSP display
2. Create APP_TASK
3. APP_TASK handles all initialization including TouchScreen

## Advantages Over Previous Implementation

| Aspect | Previous UI_TASK | New TouchScreen |
|--------|-----------------|-----------------|
| **Lifetime** | One-shot, self-deleting | Persistent, running |
| **Responsiveness** | Not available after init | Always responsive |
| **Extensibility** | Limited | Full event support |
| **Resource Management** | Manual task deletion | Proper cleanup |
| **Error Handling** | Basic | Comprehensive |
| **Testability** | Difficult | Easy with events |

## Important Notes

### Display Initialization
- Display must be initialized via `bsp_display_start()` **before** TouchScreen_Init()
- This ensures LVGL and touch driver are ready

### Priority Selection
- TouchScreen priority is `APP_TASK_PRIORITY - 1` (Priority 2)
- This keeps it lower than APP_TASK but allows UI updates
- LVGL task (Priority 4) is higher to ensure smooth rendering

### Event Queue
- Size: 8 events
- Timeout: 100ms per send operation
- Loop timeout: 10ms (allows LVGL task to run)

### Callback Handling
- WiFi callback is stored in the component resource
- Can be extended for other UI interactions

## Extending the Component

### Adding a New Screen

1. Create a new directory under `TouchScreen_UI_Manager/screens/`:
```
TouchScreen_UI_Manager/screens/new_screen/
├── new_screen.c             (Implementation)
└── new_screen_internal.h    (Internal interface)
```

2. Implement screen creation function:
```c
void touchscreen_new_screen_create(void) {
    // Create LVGL objects
    // Register callbacks
}
```

3. Add to ui_manager.c event handler and expose via public API

4. Update CMakeLists.txt with new source files

### Adding a New UI Component

1. Create component directory:
```
TouchScreen_UI_Manager/components/new_component/
├── new_component.c
└── new_component.h
```

2. Implement component creation and callback functions:
```c
lv_obj_t *touchscreen_new_component_create(lv_obj_t *parent, /* params */);
void touchscreen_new_component_set_callback(/* callback params */);
```

3. Use from screens via relative includes
4. Update CMakeLists.txt

## Compilation and Build

The TouchScreen component is built as a single unit but with separated modules:

**CMakeLists.txt** registers all source files:
```cmake
SRCS
    TouchScreenAPI/src/TouchScreen_API.c
    TouchScreen_UI_Manager/src/ui_manager.c
    TouchScreen_UI_Manager/screens/splash/splash_screen.c
    TouchScreen_UI_Manager/screens/wifi_setup/wifi_setup_screen.c
    TouchScreen_UI_Manager/components/button/button_component.c
    TouchScreen_UI_Manager/components/input_field/input_field_component.c
    TouchScreen_UI_Manager/assets/ringy_logo.c

INCLUDE_DIRS
    TouchScreenAPI/include
    TouchScreen_UI_Manager/include
    TouchScreen_UI_Manager/src
    TouchScreen_UI_Manager/assets
```

**Include Paths**:
- Public headers: `TouchScreenAPI/include/` and `TouchScreen_UI_Manager/include/`
- Internal implementation: `TouchScreen_UI_Manager/src/`
- Assets: `TouchScreen_UI_Manager/assets/`

## Error Codes

Added to `AppErrors.h`:
- `APP_ERROR_QUEUE_SEND_FAILED` (0x01000008) - Event posting failed

## Dependencies

- **FreeRTOS**: Task and queue management
- **LVGL**: Display rendering
- **ESP-IDF**: Logging and system APIs
- **UI Manager**: Screen display logic

## Testing Checklist

- [ ] Project builds without errors
- [ ] Splash screen displays for 5 seconds
- [ ] WiFi setup screen appears after splash
- [ ] Touch input is responsive (Priority 5 task)
- [ ] Display renders smoothly (Priority 4 task)
- [ ] Web server responds to HTTP requests
- [ ] No memory leaks or crashes during operation
