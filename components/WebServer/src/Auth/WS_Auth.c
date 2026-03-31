#include "WS_Auth.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_random.h"
#include "cJSON.h"
#include "sdkconfig.h"

static const char *TAG = "AUTH";

// ---------------------- Config ----------------------
#define SESSION_TOKEN_LEN 32
#define USER_MAX_LEN  31
#define ROLE_MAX_LEN  15

#define MAX_SESSIONS 3
#define SESSION_MAX_AGE_S 86400

// credentials configured via menuconfig (Component config -> WebServer Auth)
#define AUTH_USERNAME CONFIG_WS_AUTH_USERNAME
#define AUTH_PASSWORD CONFIG_WS_AUTH_PASSWORD

static const char* DEMO_ROLE = "admin";

// login rate limit: 5 attempts / 60 seconds (global, simple)
#define LOGIN_WINDOW_SEC 60
#define LOGIN_MAX_ATTEMPTS 5

// ---------------------- State ----------------------
typedef struct {
    char token[SESSION_TOKEN_LEN + 1];
    char username[USER_MAX_LEN + 1];
    char role[ROLE_MAX_LEN + 1];
    time_t created_at;
    bool active;
} session_t;

static session_t g_sessions[MAX_SESSIONS] = {0};

static int  g_login_count = 0;
static long g_login_window_start = 0;

// ---------------------- Helpers ----------------------
static esp_err_t rate_limited(void)
{
    long now = (long)time(NULL);
    if ((0 == g_login_window_start) || ((now - g_login_window_start) >= LOGIN_WINDOW_SEC)) {
        g_login_window_start = now;
        g_login_count = 0;
    }

    g_login_count++;
    return (g_login_count > LOGIN_MAX_ATTEMPTS) ? ESP_FAIL : ESP_OK;
}

static void make_token(char out[SESSION_TOKEN_LEN + 1])
{
    for (size_t i = 0; i < SESSION_TOKEN_LEN; i += 2) {
        uint8_t byte = (uint8_t)(esp_random() & 0xFF);
        sprintf(out + i, "%02x", byte);
    }
    out[SESSION_TOKEN_LEN] = '\0';
}

void auth_set_security_headers(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json");
}

static esp_err_t send_json(httpd_req_t* req, const char* status, const char* json)
{
    auth_set_security_headers(req);
    httpd_resp_set_status(req, status);
    return httpd_resp_sendstr(req, json);
}

static esp_err_t read_body(httpd_req_t* req, char* out, size_t outlen)
{
    int total = req->content_len;
    if ((total <= 0) || (total >= (int)outlen)) {
        return ESP_ERR_INVALID_SIZE;
    }

    int got = 0;
    while (got < total) {
        int r = httpd_req_recv(req, out + got, total - got);
        if (r <= 0) {
            return ESP_FAIL;
        }
        got += r;
    }
    out[got] = 0;
    return ESP_OK;
}

static const char* extract_session_cookie(httpd_req_t* req)
{
    static char cookie_buf[256];
    if (httpd_req_get_hdr_value_str(req, "Cookie", cookie_buf, sizeof(cookie_buf)) != ESP_OK) {
        return NULL;
    }

    char* start = strstr(cookie_buf, "session=");
    if (NULL == start) {
        return NULL;
    }

    start += 8;
    char* end = strchr(start, ';');
    if (NULL != end) {
        *end = '\0';
    }

    if (strlen(start) == 0) {
        return NULL;
    }

    return start;
}

static session_t* find_session(const char* token)
{
    if (NULL == token) {
        return NULL;
    }

    time_t now = time(NULL);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (g_sessions[i].active && (strcmp(g_sessions[i].token, token) == 0)) {
            if (difftime(now, g_sessions[i].created_at) > SESSION_MAX_AGE_S) {
                g_sessions[i].active = false;
                return NULL;
            }
            return &g_sessions[i];
        }
    }

    return NULL;
}

static session_t* allocate_session_slot(void)
{
    int oldest_idx = 0;
    time_t oldest = g_sessions[0].created_at;

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!g_sessions[i].active) {
            return &g_sessions[i];
        }

        if (i == 0 || g_sessions[i].created_at < oldest) {
            oldest = g_sessions[i].created_at;
            oldest_idx = i;
        }
    }

    return &g_sessions[oldest_idx];
}

static void invalidate_session_by_token(const char* token)
{
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (g_sessions[i].active && (strcmp(g_sessions[i].token, token) == 0)) {
            g_sessions[i].active = false;
            g_sessions[i].token[0] = '\0';
            g_sessions[i].username[0] = '\0';
            g_sessions[i].role[0] = '\0';
            g_sessions[i].created_at = 0;
            return;
        }
    }
}

int auth_active_session_count(void)
{
    int count = 0;
    time_t now = time(NULL);

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!g_sessions[i].active) {
            continue;
        }

        if (difftime(now, g_sessions[i].created_at) > SESSION_MAX_AGE_S) {
            g_sessions[i].active = false;
            continue;
        }

        count++;
    }

    return count;
}

bool auth_csrf_check(httpd_req_t* req)
{
    if ((req->method == HTTP_GET) || (req->method == HTTP_OPTIONS) || (req->method == HTTP_HEAD)) {
        return true;
    }

    char ct[64] = {0};
    if (httpd_req_get_hdr_value_str(req, "Content-Type", ct, sizeof(ct)) != ESP_OK
        || strncmp(ct, "application/json", 16) != 0) {
        (void)send_json(req, "415 Unsupported Media Type", "{\"error\":\"Content-Type must be application/json\"}");
        return false;
    }

    char xrw[32] = {0};
    if (httpd_req_get_hdr_value_str(req, "X-Requested-With", xrw, sizeof(xrw)) != ESP_OK
        || strcmp(xrw, "XMLHttpRequest") != 0) {
        (void)send_json(req, "403 Forbidden", "{\"error\":\"Missing X-Requested-With header\"}");
        return false;
    }

    return true;
}

// ---------------------- Public guard ----------------------
esp_err_t auth_require_session(httpd_req_t* req, const char** out_user, const char** out_role)
{
    const char* token = extract_session_cookie(req);
    if (NULL == token) {
        (void)send_json(req, "401 Unauthorized", "{\"error\":\"Authentication required\"}");
        return ESP_FAIL;
    }

    session_t* s = find_session(token);
    if (NULL == s) {
        (void)send_json(req, "401 Unauthorized", "{\"error\":\"Invalid or expired session\"}");
        return ESP_FAIL;
    }

    if (NULL != out_user) { *out_user = s->username; }
    if (NULL != out_role) { *out_role = s->role; }

    return ESP_OK;
}

// ---------------------- Endpoints ----------------------
static esp_err_t api_login(httpd_req_t* req)
{
    if (!auth_csrf_check(req)) {
        return ESP_OK;
    }

    if (ESP_OK != rate_limited()) {
        return send_json(req, "429 Too Many Requests", "{\"error\":\"Too many login attempts. Try again later.\"}");
    }

    char body[512];
    if (ESP_OK != read_body(req, body, sizeof(body))) {
        return send_json(req, "400 Bad Request", "{\"error\":\"invalid body\"}");
    }

    cJSON* root = cJSON_Parse(body);
    if (NULL == root) {
        return send_json(req, "400 Bad Request", "{\"error\":\"invalid json\"}");
    }

    cJSON* u = cJSON_GetObjectItem(root, "username");
    cJSON* p = cJSON_GetObjectItem(root, "password");
    if ((0 == cJSON_IsString(u)) || (0 == cJSON_IsString(p))) {
        cJSON_Delete(root);
        return send_json(req, "400 Bad Request", "{\"error\":\"missing fields\"}");
    }

    const char* username = u->valuestring;
    const char* password = p->valuestring;

    esp_err_t credsOk = ((0 == strcmp(username, AUTH_USERNAME)) && (0 == strcmp(password, AUTH_PASSWORD))) ? ESP_OK : ESP_FAIL;

    if (ESP_OK != credsOk) {
        cJSON_Delete(root);
        return send_json(req, "401 Unauthorized", "{\"error\":\"invalid credentials\"}");
    }

    session_t* session = allocate_session_slot();
    memset(session, 0, sizeof(*session));
    make_token(session->token);
    snprintf(session->username, sizeof(session->username), "%s", username);
    snprintf(session->role, sizeof(session->role), "%s", DEMO_ROLE);
    session->created_at = time(NULL);
    session->active = true;

    cJSON_Delete(root);

    char cookie[128];
    snprintf(cookie, sizeof(cookie), "session=%s; HttpOnly; SameSite=Strict; Path=/", session->token);
    httpd_resp_set_hdr(req, "Set-Cookie", cookie);

    char resp[256];
    snprintf(resp, sizeof(resp),
        "{"
          "\"user\":{\"username\":\"%s\",\"role\":\"%s\"},"
          "\"message\":\"Login successful\""
        "}",
        session->username, session->role);

    return send_json(req, "200 OK", resp);
}

static esp_err_t api_logout(httpd_req_t* req)
{
    auth_set_security_headers(req);

    if (!auth_csrf_check(req)) {
        return ESP_OK;
    }

    if (ESP_OK != auth_require_session(req, NULL, NULL)) {
        return ESP_OK;
    }

    const char* token = extract_session_cookie(req);
    if (NULL != token) {
        invalidate_session_by_token(token);
    }

    httpd_resp_set_hdr(req, "Set-Cookie", "session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0");
    return send_json(req, "200 OK", "{\"success\":true,\"message\":\"Logged out successfully\"}");
}

static esp_err_t api_validate(httpd_req_t* req)
{
    auth_set_security_headers(req);

    const char* user = NULL;
    const char* role = NULL;

    esp_err_t espErr = auth_require_session(req, &user, &role);
    if (ESP_OK == espErr) {
        char resp[256];
        snprintf(resp, sizeof(resp),
            "{"
              "\"valid\":true,"
              "\"user\":{\"username\":\"%s\",\"role\":\"%s\"}"
            "}",
            user, role);

        return send_json(req, "200 OK", resp);
    }

    return ESP_OK;
}

esp_err_t auth_register_endpoints(httpd_handle_t server)
{
    httpd_uri_t login = {
        .uri = "/api/login",
        .method = HTTP_POST,
        .handler = api_login,
    };

    httpd_uri_t logout = {
        .uri = "/api/logout",
        .method = HTTP_POST,
        .handler = api_logout,
    };

    httpd_uri_t validate = {
        .uri = "/api/validate-token",
        .method = HTTP_GET,
        .handler = api_validate,
    };

    esp_err_t espErr = ESP_OK;

    espErr = httpd_register_uri_handler(server, &login);
    if(ESP_OK == espErr) 
    {
       espErr = httpd_register_uri_handler(server, &logout);

       if(ESP_OK == espErr)
       {
           espErr = httpd_register_uri_handler(server, &validate);
       }
    }

    ESP_LOGI(TAG, "Auth endpoints registered: /api/login, /api/logout, /api/validate-token");

    return espErr;
}
