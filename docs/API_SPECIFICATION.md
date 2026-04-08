# ESP32 School Bell — REST API Specification

Complete reference for all HTTP endpoints served by the ESP32 firmware. Authentication uses **HttpOnly session cookies** with CSRF protection and role-based access control (service/client). See [AUTHENTICATION.md](AUTHENTICATION.md) for full auth system documentation.

---

## Authentication Endpoints

### POST /api/login
**Access**: Public (rate-limited: 5 attempts / 60s)

**Request:**
```
Content-Type: application/json
X-Requested-With: XMLHttpRequest
```
```json
{ "username": "admin", "password": "password123" }
```

**Response (200):**
```
Set-Cookie: session=<token>; HttpOnly; SameSite=Strict; Path=/
```
```json
{ "user": { "username": "admin", "role": "service" }, "message": "Login successful" }
```

`role` is `"service"` or `"client"` depending on which account matched.

**Errors:** 400 (bad JSON), 401 (invalid credentials), 429 (rate limited)

---

### POST /api/logout
**Access**: Session + CSRF

**Request:** `{}` (empty JSON body)

**Response (200):**
```
Set-Cookie: session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0
```
```json
{ "success": true, "message": "Logged out successfully" }
```

---

### GET /api/validate-token
**Access**: Session (cookie sent automatically)

**Response (200):**
```json
{ "valid": true, "user": { "username": "admin", "role": "service" } }
```

`role` is `"service"` or `"client"`.

**Errors:** 401 (no cookie or expired)

---

## Schedule Endpoints

### GET /api/schedule/settings
**Access**: Session

**Response (200):**
```json
{
  "timezone": "EET-2EEST,M3.5.0/3,M10.5.0/4",
  "workingDays": [1, 2, 3, 4, 5]
}
```

### POST /api/schedule/settings
**Access**: Session + CSRF

**Request:**
```json
{
  "timezone": "EET-2EEST,M3.5.0/3,M10.5.0/4",
  "workingDays": [1, 2, 3, 4, 5]
}
```
`workingDays`: array of day indices (0=Sunday, 6=Saturday)

---

### GET /api/schedule/bells
**Access**: Session

**Response (200):**
```json
{
  "firstShift": {
    "enabled": true,
    "bells": [
      { "hour": 8, "minute": 0, "durationSec": 3, "label": "Class 1 start" },
      { "hour": 8, "minute": 45, "durationSec": 3, "label": "Class 1 end" }
    ]
  },
  "secondShift": {
    "enabled": true,
    "bells": [
      { "hour": 14, "minute": 0, "durationSec": 3, "label": "Class 7 start" }
    ]
  }
}
```

### POST /api/schedule/bells
**Access**: Session + CSRF — **Max body: 8KB**

**Request:** Same format as GET response. Max 50 bells per shift, 100 total.

---

### GET /api/schedule/holidays
**Access**: Session

**Response (200):**
```json
{
  "holidays": [
    { "startDate": "2025-12-24", "endDate": "2026-01-02", "label": "Winter break" }
  ]
}
```

### POST /api/schedule/holidays
**Access**: Session + CSRF

**Request:** Same format. Max 50 holidays.

---

### GET /api/schedule/exceptions
**Access**: Session

**Response (200):**
```json
{
  "exceptions": [
    {
      "startDate": "2026-03-03",
      "endDate": "2026-03-03",
      "label": "Liberation Day",
      "action": "day-off",
      "timeOffsetMin": 0,
      "templateIdx": -1,
      "customBellsIdx": -1
    }
  ],
  "customBellSets": [
    {
      "bells": [
        { "hour": 9, "minute": 0, "durationSec": 5, "label": "Special bell" }
      ]
    }
  ]
}
```

### POST /api/schedule/exceptions
**Access**: Session + CSRF

**Exception actions:** `day-off` (default), `normal`, `first-shift`, `second-shift`, `template`, `custom`

| Field | Description |
|-------|-------------|
| `timeOffsetMin` | ±120 minutes — shifts all bell times for the day |
| `templateIdx` | Index into templates array (for `template` action) |
| `customBellsIdx` | Index into `customBellSets` (for `custom` action) |

Max: 40 exceptions, 5 custom bell sets (30 bells each).

---

### GET /api/schedule/templates
**Access**: Session

**Response (200):**
```json
{
  "templates": [
    {
      "name": "Short Day",
      "bells": [
        { "hour": 8, "minute": 0, "durationSec": 3, "label": "Start" }
      ]
    }
  ]
}
```

### POST /api/schedule/templates
**Access**: Session + CSRF

Max 5 templates.

---

### GET /api/schedule/defaults
**Access**: Session

Returns the factory `default_schedule.json` content (read-only).

---

## Bell Control Endpoints

### GET /api/bell/status
**Access**: Session

**Response (200):**
```json
{
  "bellState": "idle",
  "panicMode": false,
  "dayType": "working",
  "timeSynced": true,
  "lastSyncAgeSec": 3600,
  "currentTime": "10:30:00",
  "currentDate": "2026-04-02",
  "nextBell": {
    "valid": true,
    "hour": 10,
    "minute": 45,
    "durationSec": 3,
    "label": "Class 3 end"
  }
}
```

`bellState`: `"idle"` | `"ringing"` | `"panic"`
`dayType`: `"off"` | `"working"` | `"holiday"` | `"exception_working"` | `"exception_holiday"`

---

### POST /api/bell/panic
**Access**: Session + CSRF

**Request:**
```json
{ "enabled": true }
```

---

### POST /api/bell/test
**Access**: Session + CSRF — blocked during panic mode

**Request:**
```json
{ "durationSec": 3 }
```
`durationSec`: 1–30 (optional, default 3)

---

## System Endpoints

### GET /api/system/time
**Access**: Session

**Response (200):**
```json
{
  "time": "10:30:00",
  "date": "2026-04-02",
  "synced": true,
  "lastSyncAgeSec": 3600,
  "timezone": "EET-2EEST,M3.5.0/3,M10.5.0/4"
}
```

---

### GET /api/system/info
**Access**: Session

**Response (200):**
```json
{
  "uptimeSec": 86400,
  "freeHeap": 120000,
  "minFreeHeap": 85000,
  "chipCores": 2,
  "idfVersion": "5.4.0",
  "time": "10:30:00",
  "date": "2026-04-02",
  "timeSynced": true,
  "lastSyncAgeSec": 3600,
  "timezone": "EET-2EEST,M3.5.0/3,M10.5.0/4"
}
```

---

### POST /api/system/reboot
**Access**: Session + CSRF

**Request:** `{}` — Reboots after 1-second timer.

**Response (200):**
```json
{ "status": "ok", "message": "Device will reboot in 1 second" }
```

---

### POST /api/system/factory-reset
**Access**: Session + CSRF

**Request:** `{}`

Deletes config files from SPIFFS, recreates from `default_schedule.json`, resets PIN, resets setup wizard state, reloads scheduler.

---

### POST /api/system/sync-time
**Access**: Session + CSRF

**Request:** `{}` — Forces NTP resync.

**Response (200):**
```json
{ "status": "ok", "timeSynced": true, "lastSyncAgeSec": 0 }
```

---

### GET /api/system/pin
**Access**: Session

**Response (200):**
```json
{ "pin": "1234" }
```

### POST /api/system/pin
**Access**: Session + CSRF

**Request:**
```json
{ "pin": "5678" }
```
PIN must be 4–6 digits.

---

## Credential Management Endpoints

These endpoints manage client account credentials. **Service role only** — returns `403 Forbidden` for client-role sessions.

### GET /api/system/credentials
**Access**: Session (service role)

**Response (200):**
```json
{ "clientExists": true, "clientUsername": "teacher" }
```
If no client account exists: `{ "clientExists": false }`

---

### POST /api/system/credentials
**Access**: Session + CSRF (service role)

**Request:**
```json
{ "username": "teacher", "password": "securepass" }
```
- `username`: 1–31 characters
- `password`: minimum 8 characters

**Response (200):**
```json
{ "status": "ok", "message": "Client credentials saved" }
```

All active sessions are invalidated after this operation.

**Errors:** 400 (invalid input), 403 (not service role), 500 (storage error)

---

### DELETE /api/system/credentials
**Access**: Session + CSRF (service role)

**Response (200):**
```json
{ "status": "ok", "message": "Client credentials deleted" }
```

All active sessions are invalidated after this operation.

**Errors:** 403 (not service role), 404 (no client account), 500 (storage error)

---

## Public System Endpoints

### GET /api/health
**Access**: Public

**Response (200):**
```json
{
  "status": "healthy",
  "timestamp": 1640995200,
  "uptime": 3600,
  "memory": { "free": 120000, "total": 520000 }
}
```

---

### GET /api/status
**Access**: Public

**Response (200):**
```json
{
  "device": "ESP32-S3",
  "version": "1.0.0",
  "wifi": { "connected": true, "ssid": "MyNetwork", "rssi": -45 },
  "auth": { "enabled": true, "sessions": 1 }
}
```

---

## WiFi Endpoints

### GET /api/wifi/status
**Access**: Public

**Response (200):**
```json
{ "mode": "STA", "ap_ssid": "ESP32_Setup" }
```

---

### POST /api/wifi/config
**Access**: Session (when STA connected) / Public (on soft-AP)

**Request:**
```json
{ "ssid": "MyNetwork", "password": "password123", "bssid": "AA:BB:CC:DD:EE:FF" }
```

- `ssid` (string, required): Network SSID
- `password` (string, optional): Network password (empty string for open networks)
- `bssid` (string, optional): AP MAC address in `"XX:XX:XX:XX:XX:XX"` format — used to pin connection to a specific AP. Omit for manual SSID entry (a probe scan will attempt to discover it on connect).

Saves credentials and triggers device restart.

---

### GET /api/wifi/networks
**Access**: Session (when STA connected) / Public (on soft-AP)

**Response (200):**
```json
{
  "networks": [
    { "ssid": "MyNetwork", "bssid": "AA:BB:CC:DD:EE:FF", "rssi": -45, "channel": 6, "authmode": 3, "secured": true }
  ]
}
```

---

## Mode Endpoint

### GET /api/mode
**Access**: Session

### POST /api/mode
**Access**: Session + CSRF

**Request:**
```json
{ "mode": "manual" }
```

---

## Error Response Format

All error responses follow this pattern:
```json
{ "error": "Error description" }
```

## Common HTTP Status Codes

| Code | Meaning |
|------|---------|
| 200 | Success |
| 400 | Bad Request (invalid JSON, missing fields) |
| 401 | Unauthorized (no session / expired) |
| 403 | Forbidden (missing CSRF headers) |
| 415 | Unsupported Media Type (wrong Content-Type on POST) |
| 429 | Too Many Requests (login rate limit) |
| 500 | Internal Server Error |
