# Authentication System Documentation

## Architecture Overview

The authentication system uses **HttpOnly session cookies** managed entirely server-side. The React frontend never sees or stores the actual session token — the browser handles cookie transmission automatically. The system provides CSRF protection via mandatory request headers on state-changing endpoints.

| Layer | File | Responsibility |
|-------|------|----------------|
| **Session Management** | `components/WebServer/src/Auth/WS_Auth.c` | Token generation, session store, login/logout/validate, role-based guards |
| **Password Hashing** | `components/WebServer/src/Auth/WS_AuthCrypto.c` | Salted SHA-256 hashing, constant-time verification |
| **Credential Storage** | `components/WebServer/src/Auth/WS_AuthStore.c` | NVS-backed credential store for service & client accounts |
| **Credential Management** | `components/WebServer/src/React/RestAPI/Credential/CredentialAPI.c` | REST API for client account CRUD (service-role only) |
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
- Fixed-size array in RAM: `session_t[MAX_SESSIONS]` (MAX_SESSIONS = 1)
- Each session: `{ token[33], username[32], role[16], created_at, active }`
- **Eviction**: When the slot is full, the existing session is evicted
- **Expiration**: 1-hour maximum age (`SESSION_MAX_AGE_S = 3600`)
- **Reboot behavior**: All sessions lost (RAM-only) = automatic logout on power cycle

### Session Lookup
- `auth_require_session()` extracts `session=<token>` from the `Cookie` header
- Looks up token in the session array
- Verifies session is active and not expired (1h)
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
  ├─ Bad credentials (neither service nor client match) → 401 Unauthorized
  │
  └─ Success:
       1. Verify credentials: try service account first (`auth_store_verify_service()`),
          then client account (`auth_store_verify_client()`)
       2. Determine role: `"service"` or `"client"`
       3. Generate 32-char hex token via esp_random()
       4. Allocate session slot (evict oldest if full)
       5. Store { token, username, role, created_at }
       │
       ▼
  Response (200):
    Set-Cookie: session=<token>; HttpOnly; SameSite=Strict; Path=/
    Body: { "user": { "username": "<name>", "role": "service|client" }, "message": "Login successful" }
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
       Response (200): { "valid": true, "user": { "username": "<name>", "role": "service|client" } }
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

| Public (no auth) | Protected (session required) | Protected (service role only) |
|---|---|---|
| `POST /api/login` (rate-limited) | `POST /api/logout` | `GET /api/system/credentials` |
| `GET /api/health` | `GET /api/validate-token` | `POST /api/system/credentials` |
| `GET /api/status` | `GET/POST /api/schedule/*` | `DELETE /api/system/credentials` |
| `GET /api/wifi/status` | `GET/POST /api/bell/*` | |
| `GET /api/wifi/networks`* | `GET/POST /api/system/*` | |
| `POST /api/wifi/config`* | `GET/POST /api/mode` | |

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

## Dual-Account Model

The system supports two account types with role-based access:

| Account | Role | Credential Source | Can Manage Client? | Can Be Changed? |
|---------|------|-------------------|--------------------|-----------------|
| **Service** | `"service"` | Kconfig → NVS (first-boot hash) | Yes | No (compile-time) |
| **Client** | `"client"` | Created by service via REST API | No | Yes (via service) |

### Service Account
- Username: compile-time constant from `CONFIG_WS_AUTH_USERNAME`
- Password: Kconfig `CONFIG_WS_AUTH_PASSWORD` hashed to NVS on first boot only
- Full system access + credential management endpoints
- Cannot be modified at runtime

### Client Account
- Optional — no client account exists by default
- Created/updated/deleted by the service account via `POST/DELETE /api/system/credentials`
- Full system access except credential management
- Stored in NVS namespace `"auth"` (keys: `cli_user`, `cli_salt`, `cli_hash`, `cli_exists`)

---

## Password Hashing

All passwords are stored as **salted SHA-256** hashes in NVS. Plaintext passwords are never persisted.

### Algorithm
1. Generate 16-byte random salt via `esp_fill_random()`
2. Compute `SHA-256(salt || password)` using `mbedtls_sha256()`
3. Store salt (32 hex chars) and hash (64 hex chars) as NVS strings

### Verification
1. Read stored salt + hash from NVS
2. Recompute `SHA-256(stored_salt || input_password)`
3. **Constant-time comparison** via `mbedtls_ct_memcmp()` — prevents timing attacks

### NVS Storage Layout (namespace: `"auth"`)

| Key | Type | Description |
|-----|------|-------------|
| `svc_salt` | string (32 hex) | Service account password salt |
| `svc_hash` | string (64 hex) | Service account password SHA-256 hash |
| `cli_user` | string (1–31 chars) | Client account username |
| `cli_salt` | string (32 hex) | Client account password salt |
| `cli_hash` | string (64 hex) | Client account password SHA-256 hash |
| `cli_exists` | uint8 (0/1) | Whether a client account exists |

### Implementation Files
- `WS_AuthCrypto.c` — `auth_crypto_hash_password()`, `auth_crypto_verify_password()`, hex conversion helpers
- `WS_AuthStore.c` — `auth_store_init()`, `auth_store_verify_service()`, `auth_store_verify_client()`, `auth_store_set_client()`, `auth_store_delete_client()`

---

## Credential Management API

Service-role accounts can manage client credentials via REST endpoints:

| Method | URI | Auth | Description |
|--------|-----|------|-------------|
| GET | `/api/system/credentials` | Session (service only) | Check if client exists + get username |
| POST | `/api/system/credentials` | Session+CSRF (service only) | Create or update client account |
| DELETE | `/api/system/credentials` | Session+CSRF (service only) | Delete client account |

All credential changes invalidate all active sessions (`auth_invalidate_all_sessions()`).

See [API_SPECIFICATION.md](API_SPECIFICATION.md) for request/response formats.

---

## Kconfig Options

```kconfig
menu "WebServer Auth"
    config WS_AUTH_USERNAME
        string "Service account username"
        default "admin"
        help
            Compile-time service account username (cannot be changed at runtime).

    config WS_AUTH_PASSWORD
        string "Service account initial password"
        default "password123"
        help
            Hashed into NVS on first boot only. Changing this
            value only takes effect after NVS erase.
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
