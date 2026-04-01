# Copilot Project Instructions

## Project Overview

This is the **ESP32 School Bell** firmware — an automated school bell system built on ESP32-S3 with a 4-inch touchscreen UI and a web-based configuration interface. It controls bell schedules with shifts, holidays, exceptions, and provides both local (touch) and remote (web) administration.

## Tech Stack

- **ESP-IDF** v5.4.0 (Espressif IoT Development Framework)
- **FreeRTOS** as the underlying RTOS
- **C** (C11) — all firmware code
- **LVGL** v9 for touchscreen UI
- **cJSON** for JSON serialization
- **ESP HTTP Server** for REST API
- **React** web frontend (separate submodule, built to FatFS partition)

## Hardware Target

- **Board**: Waveshare ESP32-S3-Touch-LCD-4
- **MCU**: ESP32-S3-WROOM-1 (16MB Flash, 8MB PSRAM)
- **Display**: 4" IPS LCD, 480×480, ST7701 driver, GT911 capacitive touch
- **IO Expander**: CH32V003 (I2C address 0x20)

## Project Structure

Modular component-based architecture under `components/`:

- `AppTask/` — Main orchestrator (4-phase boot, event loop)
- `FileSystem/` — Dual filesystem (FatFS `/react/` + SPIFFS `/storage/`)
- `Generic/` — Shared error codes (`APP_SUCCESS`, `APP_ERROR_*`)
- `NVS/` — Non-Volatile Storage wrapper (namespace-based key-value)
- `RingBell/` — GPIO bell control (timed ringing, panic mode)
- `Scheduler/` — Bell scheduling engine (2 shifts, holidays, exceptions, templates)
- `TimeSync/` — NTP time synchronization with timezone support
- `TouchScreen/` — LVGL UI framework (screens, services, i18n BG/EN)
- `WebServer/` — HTTP server + REST API + React SPA hosting + auth
- `WiFi_Manager/` — WiFi credential management (NVS persistence)

Other directories:

- `main/` — Entry point (`app_main()` → `AppTask_Create()`)
- `data/` — Default schedule JSON, FatFS image build
- `esp32_school_bell_web/` — React frontend (git submodule)
- `managed_components/` — ESP-IDF component manager packages

## Architecture Patterns

- **Opaque handles**: Every component uses `typedef struct _X_RSC_T* X_H`
- **4-phase init**: Hardware/Storage → Asset Verification → Connectivity → UI
- **Service layer**: TouchScreen Services bridge UI ↔ backend (no direct coupling)
- **Dual filesystem**: SPIFFS for config data, FatFS for web assets
- **NVS namespace isolation**: Each subsystem uses its own NVS namespace
- **cJSON serialization**: Schedule data uses cJSON for JSON ↔ struct
- **Event-driven**: FreeRTOS queues for AppTask, screen events for UI

## Authentication System

The web interface uses **HttpOnly session cookies** — documented in `docs/AUTHENTICATION.md`. Key points:

- Server sets `Set-Cookie: session=<token>; HttpOnly; SameSite=Strict; Path=/` on login
- Max 3 concurrent sessions, 24h expiry, stored in RAM (lost on reboot)
- Token: 32-char hex from `esp_random()`
- CSRF: POST/PUT/DELETE require `Content-Type: application/json` + `X-Requested-With: XMLHttpRequest`
- Guard pattern: `auth_require_session()` + `auth_csrf_check()` per handler
- Security headers on all responses: `X-Content-Type-Options`, `X-Frame-Options`, `Cache-Control`
- Rate limiting: 5 login attempts per 60-second window
- Credentials: Kconfig `WS_AUTH_USERNAME` / `WS_AUTH_PASSWORD`

## API Communication

- All REST endpoints documented in `docs/API_SPECIFICATION.md`
- Public endpoints: `/api/login`, `/api/health`, `/api/status`, `/api/wifi/*`
- Protected endpoints require valid session cookie
- CSRF on mutating requests: `Content-Type: application/json` + `X-Requested-With: XMLHttpRequest`
- React SPA served from FatFS with gzip support, SPA catch-all for client routing

## Conventions

- Return `APP_SUCCESS` / `APP_ERROR_*` for component APIs, `esp_err_t` for ESP-IDF wrappers
- Opaque handle pattern — never access `_RSC_T` internals from outside
- cJSON objects returned by serializers must be freed by caller (`cJSON_Delete()`)
- All web handlers call `auth_set_security_headers()` first
- SPIFFS files at `/storage/*.json`, FatFS assets at `/react/*`
- TouchScreen Services as bridge layer — UI never calls backend directly
- Bilingual i18n: Bulgarian (default) + English, `ui_str(STRING_ID)` lookup

## Storage

| Location | Type | Mount | Contents |
|----------|------|-------|----------|
| NVS | flash | — | WiFi credentials, PIN, timezone, panic state, setup flags, language |
| SPIFFS | flash | `/storage/` | `settings.json`, `schedule.json`, `calendar.json`, `templates.json` |
| FatFS | flash | `/react/` | React SPA build (gzipped HTML/JS/CSS), `default_schedule.json` |

## Documentation

- `docs/PROJECT_STRUCTURE.md` — File organization, boot sequence, dependencies
- `docs/AUTHENTICATION.md` — Full authentication system documentation
- `docs/API_SPECIFICATION.md` — Complete REST API endpoint reference
- `docs/components/*.md` — Individual component documentation (AppTask, FileSystem, Generic, NVS, RingBell, Scheduler, TimeSync, TouchScreen, WebServer, WiFi_Manager)
- `docs/.instructions.md` — Copilot context for source files

## Post-Task Rule

After completing any task, **always ask the user** whether the project documentation and Copilot instructions should be updated to reflect the changes made. Specifically check if any of the following need updates:

- `docs/AUTHENTICATION.md` — if auth, session, CSRF, or security logic changed
- `docs/.instructions.md` — if key files, patterns, or conventions changed
- `.github/copilot-instructions.md` — if project structure, tech stack, or conventions changed
- `docs/PROJECT_STRUCTURE.md` — if files were added, moved, or removed
- `docs/API_SPECIFICATION.md` — if API endpoints or request/response formats changed
- `docs/components/*.md` — if a specific component's API or behavior changed
