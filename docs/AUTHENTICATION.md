# Authentication System Documentation

## Architecture Overview

The authentication system uses **HttpOnly session cookies** managed entirely server-side. The React frontend never sees or stores the actual session token — the browser handles cookie transmission automatically. The system provides CSRF protection via mandatory request headers on state-changing endpoints.

| Layer | File | Responsibility |
|-------|------|----------------|
| **Session Management** | `components/WebServer/src/Auth/WS_Auth.c` | Token generation, session store, login/logout/validate handlers |
| **CSRF Protection** | `components/WebServer/src/Auth/WS_Auth.c` | Content-Type + X-Requested-With enforcement |
| **Security Headers** | `components/WebServer/src/Auth/WS_Auth.c` | X-Content-Type-Options, X-Frame-Options, Cache-Control |
| **Endpoint Guards** | `components/WebServer/src/React/RestAPI/` | `auth_require_session()` + `auth_csrf_check()` per handler |
| **PIN Authentication** | `components/TouchScreen/TouchScreen_Services/` | PIN storage, validation, lockout (NVS) |

---

## Cookie-Based Auth Model

The session credential is an **HttpOnly cookie** set by the server on successful login:

```
Set-Cookie: session=<opaque_token>; HttpOnly; SameSite=Strict; Path=/
```

| Attribute | Value | Purpose |
|-----------|-------|---------|
| `HttpOnly` | *(flag)* | JavaScript cannot read the cookie — XSS cannot steal the credential |
| `SameSite` | `Strict` | Cookie only sent on same-site requests — primary CSRF defense |
| `Path` | `/` | Cookie available to all API paths |
| `Expires/Max-Age` | *omitted* | Session cookie — cleared when browser closes |

The browser sends this cookie automatically on every same-origin request. The React frontend uses `credentials: 'same-origin'` on all `fetch()` calls.

---

## Session Management

### Token Generation
- 32-character hex string from `esp_random()` (hardware RNG)
- Cryptographically random, unpredictable

### Session Store
- Fixed-size array in RAM: `session_t[MAX_SESSIONS]` (MAX_SESSIONS = 3)
- Each session: `{ token[65], username[32], role[16], created_at, active }`
- **Eviction**: When all slots are full, the oldest session is evicted
- **Expiration**: 24-hour maximum age (`SESSION_MAX_AGE_S = 86400`)
- **Reboot behavior**: All sessions lost (RAM-only) = automatic logout on power cycle

### Session Lookup
- `auth_require_session()` extracts `session=<token>` from the `Cookie` header
- Looks up token in the session array
- Verifies session is active and not expired (24h)
- Returns username + role via output parameters

---

## CSRF Protection

State-changing requests (POST/PUT/DELETE) to protected endpoints require two mandatory headers:

### 1. Content-Type Enforcement
```
Content-Type: application/json
```
**Why:** HTML forms can only submit `application/x-www-form-urlencoded`, `multipart/form-data`, or `text/plain`. They cannot send `application/json`. Rejecting other Content-Types blocks form-based CSRF.

**Error:** `415 Unsupported Media Type`

### 2. X-Requested-With Header
```
X-Requested-With: XMLHttpRequest
```
**Why:** Custom headers cannot be set by HTML forms and trigger CORS preflight for cross-origin requests. This provides a second layer of CSRF defense.

**Error:** `403 Forbidden`

### Validation Pseudocode (actual implementation in WS_Auth.c)
```c
bool auth_csrf_check(httpd_req_t* req) {
    if (method == GET || method == OPTIONS || method == HEAD) return true;  // safe methods
    
    // Check Content-Type: application/json
    // Check X-Requested-With: XMLHttpRequest
    // Reject with 415 or 403 if missing
}
```

---

## Login Flow

```
Client: POST /api/login
  Headers: Content-Type: application/json, X-Requested-With: XMLHttpRequest
  Body: { "username": "admin", "password": "password123" }
  │
  ▼
Server: auth_csrf_check() → rate_limit_check()
  │
  ├─ Rate limited → 429 Too Many Requests
  │
  ├─ Bad credentials → 401 Unauthorized
  │
  └─ Success:
       1. Generate 32-char hex token via esp_random()
       2. Allocate session slot (evict oldest if full)
       3. Store { token, username, role, created_at }
       │
       ▼
  Response (200):
    Set-Cookie: session=<token>; HttpOnly; SameSite=Strict; Path=/
    Body: { "user": { "username": "admin", "role": "admin" }, "message": "Login successful" }
```

### Rate Limiting
- **Window**: 60 seconds
- **Max attempts**: 5
- **Scope**: Global (all clients combined)
- **Response**: `429 Too Many Requests`

---

## Session Validation

```
Client: GET /api/validate-token
  Cookie: session=<token>  (sent automatically by browser)
  │
  ▼
Server: auth_require_session(req, &user, &role)
  │
  ├─ No cookie → 401 Unauthorized
  ├─ Invalid/expired token → 401 Unauthorized
  │
  └─ Valid:
       Response (200): { "valid": true, "user": { "username": "admin", "role": "admin" } }
```

---

## Logout Flow

```
Client: POST /api/logout
  Headers: Content-Type: application/json, X-Requested-With: XMLHttpRequest
  Cookie: session=<token>
  │
  ▼
Server:
  1. auth_csrf_check() → pass
  2. auth_require_session() → find session
  3. Mark session slot as inactive
  │
  ▼
Response (200):
  Set-Cookie: session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0
  Body: { "success": true, "message": "Logged out successfully" }
```

---

## Protected API Request Flow

```
Client: GET /api/schedule/settings
  Cookie: session=<token>  (automatic)
  │
  ▼
Handler:
  1. auth_set_security_headers(req)      ← Always first
  2. auth_require_session(req, &user, &role)
     ├─ 401 → return immediately
     └─ OK → continue
  3. [For POST/PUT/DELETE only]: auth_csrf_check(req)
     ├─ 415/403 → return immediately
     └─ OK → continue
  4. Process request, return response
```

---

## Security Headers

Set on **every** response via `auth_set_security_headers()`:

| Header | Value | Purpose |
|--------|-------|---------|
| `X-Content-Type-Options` | `nosniff` | Prevents MIME-sniffing |
| `X-Frame-Options` | `DENY` | Prevents clickjacking (iframe embedding) |
| `Cache-Control` | `no-store` | Prevents caching of authenticated responses |
| `Content-Type` | `application/json` | Explicit content type for API responses |

---

## Public vs Protected Endpoints

| Public (no auth) | Protected (session required) |
|---|---|
| `POST /api/login` (rate-limited) | `POST /api/logout` |
| `GET /api/health` | `GET /api/validate-token` |
| `GET /api/status` | `GET/POST /api/schedule/*` |
| `GET /api/wifi/status` | `GET/POST /api/bell/*` |
| `GET /api/wifi/networks`* | `GET/POST /api/system/*` |
| `POST /api/wifi/config`* | `GET/POST /api/mode` |

\*WiFi endpoints are public on soft-AP, protected when STA is connected.

---

## PIN System (TouchScreen)

Separate from the web session auth — used for local touchscreen access control.

| Feature | Details |
|---------|---------|
| Length | 4–6 digits |
| Storage | NVS namespace `"touchscreen"` key `"pin"` |
| Lockout | After failed attempts, with countdown timer |
| WebServer access | `GET/POST /api/system/pin` (session-protected) |
| Factory reset | Resets PIN to default |

---

## Kconfig Options

```kconfig
menu "WebServer Auth"
    config WS_AUTH_USERNAME
        string "Admin username"
        default "admin"
    config WS_AUTH_PASSWORD
        string "Admin password"
        default "password123"
endmenu
```

---

## Testing Endpoints

```bash
# Health check (public)
curl http://ringy.local/api/health

# Login — save session cookie
curl -X POST http://ringy.local/api/login \
  -H "Content-Type: application/json" \
  -H "X-Requested-With: XMLHttpRequest" \
  -c cookies.txt \
  -d '{"username":"admin","password":"password123"}'

# Validate session
curl http://ringy.local/api/validate-token -b cookies.txt

# Get schedule settings (authenticated)
curl http://ringy.local/api/schedule/settings -b cookies.txt

# Update settings (authenticated + CSRF headers)
curl -X POST http://ringy.local/api/schedule/settings \
  -H "Content-Type: application/json" \
  -H "X-Requested-With: XMLHttpRequest" \
  -b cookies.txt \
  -d '{"timezone":"EET-2EEST,M3.5.0/3,M10.5.0/4","workingDays":[1,2,3,4,5]}'

# Logout
curl -X POST http://ringy.local/api/logout \
  -H "Content-Type: application/json" \
  -H "X-Requested-With: XMLHttpRequest" \
  -b cookies.txt -c cookies.txt \
  -d '{}'
```
