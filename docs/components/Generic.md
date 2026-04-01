# Generic Component

## Purpose

Shared error codes and type definitions used across all components. Provides a unified error-handling vocabulary for the entire firmware.

## Files

```
components/Generic/
├── CMakeLists.txt
└── Definitions/
    └── AppErrors.h            # Error code constants
```

## Error Codes

| Constant | Value | Description |
|----------|-------|-------------|
| `APP_SUCCESS` | `0x00000000` | Operation completed successfully |
| `APP_ERROR` | `-1` | Generic error |
| `APP_ERROR_INVALID_PARAM` | `0x01000001` | Invalid parameter passed to function |
| `APP_ERROR_NOT_FOUND` | `0x01000002` | Requested resource not found |
| `APP_ERROR_OUT_OF_MEMORY` | `0x01000003` | Memory allocation failed |
| `APP_ERROR_UNEXPECTED` | `0x01000004` | Unexpected internal error |
| `APP_ERROR_TASK_CREATE_FAILED` | `0x01000005` | FreeRTOS task creation failed |
| `APP_ERROR_QUEUE_CREATE_FAILED` | `0x01000006` | FreeRTOS queue creation failed |
| `APP_ERROR_BUFFER_TOO_SMALL` | `0x01000007` | Output buffer too small for data |
| `APP_ERROR_QUEUE_SEND_FAILED` | `0x01000008` | Failed to send to FreeRTOS queue |
| `APP_ERROR_INIT_FAILED` | `0x01000009` | Component initialization failed |

## Usage Pattern

All components return `int32_t` or `esp_err_t` status codes. The convention is:
- Return `APP_SUCCESS` on success
- Return the specific `APP_ERROR_*` code on failure
- Check with `if (APP_SUCCESS == lResult)`

## Dependencies

None — this is a leaf dependency used by all other components.
