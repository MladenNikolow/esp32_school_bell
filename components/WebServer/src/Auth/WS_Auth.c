#include "WS_Auth.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_random.h"
#include "cJSON.h"

static const char *TAG = "AUTH";

// ---------------------- Config ----------------------
#define TOKEN_MAX_LEN 64
#define USER_MAX_LEN  31
#define ROLE_MAX_LEN  15

// demo credentials
static const char* DEMO_USER = "admin";
static const char* DEMO_PASS = "password123";
static const char* DEMO_ROLE = "admin";

// token lifetime (seconds). set 0 to disable expiry.
#define TOKEN_TTL_SEC (24 * 60 * 60)

// login rate limit: 5 attempts / 60 seconds (global, simple)
#define LOGIN_WINDOW_SEC 60
#define LOGIN_MAX_ATTEMPTS 5

#define ITRUE  1
#define IFALSE 0

// ---------------------- State ----------------------
static char g_token[TOKEN_MAX_LEN + 1] = {0};
static char g_user[USER_MAX_LEN + 1] = {0};
static char g_role[ROLE_MAX_LEN + 1] = {0};
static long g_exp = 0;

static int  g_login_count = 0;
static long g_login_window_start = 0;

// ---------------------- Helpers ----------------------
static long now_sec(void)
{
    return (long)time(NULL);
}

static int rate_limited(void)
{
    long now = now_sec();
    if ((0 == g_login_window_start) || ((now - g_login_window_start) >= LOGIN_WINDOW_SEC)) {
        g_login_window_start = now;
        g_login_count = 0;
    }

    g_login_count++;
    return (g_login_count > LOGIN_MAX_ATTEMPTS) ? ITRUE : IFALSE;
}

static void make_token(char out[TOKEN_MAX_LEN + 1])
{
    // 16 random bytes -> 32 hex chars (safe + easy)
    uint8_t r[16];

    for (int i = 0; i < 16; i += 4) {
        uint32_t v = esp_random();
        r[i + 0] = (v >>  0) & 0xFF;
        r[i + 1] = (v >>  8) & 0xFF;
        r[i + 2] = (v >> 16) & 0xFF;
        r[i + 3] = (v >> 24) & 0xFF;
    }
    for (int i = 0; i < 16; i++) {
        sprintf(out + i*2, "%02x", r[i]);
    }
    out[32] = 0;
}

static esp_err_t send_json(httpd_req_t* req, const char* status, const char* json)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

static int read_body(httpd_req_t* req, char* out, size_t outlen)
{
    int total = req->content_len;
    if ((total <= 0) || (total >= (int)outlen)) {
        return IFALSE;
    }

    int got = 0;
    while (got < total) {
        int r = httpd_req_recv(req, out + got, total - got);
        if (r <= 0) {
            return IFALSE;
        }
        got += r;
    }
    out[got] = 0;
    return ITRUE;
}

static int get_bearer_token(httpd_req_t* req, char* out, size_t outlen)
{
    char auth[256];
    int len = httpd_req_get_hdr_value_str(req, "Authorization", auth, sizeof(auth));
    if (len <= 0) {
        return IFALSE;
    }

    const char* prefix = "Bearer ";
    if (0 != strncmp(auth, prefix, strlen(prefix))) {
        return IFALSE;
    }

    const char* tok = auth + strlen(prefix);
    size_t tlen = strlen(tok);
    if ((0 == tlen) || ((tlen + 1) > outlen)) {
        return IFALSE;
    }

    strcpy(out, tok);
    return ITRUE;
}

static int token_valid(void)
{
    if (0 == g_token[0]) {
        return IFALSE;
    }

    if (TOKEN_TTL_SEC > 0) {
        long now = now_sec();
        if ((0 == g_exp) || (now >= g_exp)) {
            return IFALSE;
        }
    }

    return ITRUE;
}

static void token_invalidate(void)
{
    g_token[0] = 0;
    g_user[0] = 0;
    g_role[0] = 0;
    g_exp = 0;
}

// ---------------------- Public guard ----------------------
esp_err_t auth_require_bearer(httpd_req_t* req, const char** out_user, const char** out_role)
{
    if (ITRUE != token_valid()) {
        (void)send_json(req, "401 Unauthorized", "{\"error\":\"not logged in\"}");
        return ESP_FAIL;
    }

    char token[TOKEN_MAX_LEN + 1];
    if (ITRUE != get_bearer_token(req, token, sizeof(token))) {
        (void)send_json(req, "401 Unauthorized", "{\"error\":\"missing Authorization Bearer token\"}");
        return ESP_FAIL;
    }

    if (0 != strcmp(token, g_token)) {
        (void)send_json(req, "401 Unauthorized", "{\"error\":\"invalid or expired token\"}");
        return ESP_FAIL;
    }

    if (NULL != out_user) { *out_user = g_user; }
    if (NULL != out_role) { *out_role = g_role; }

    return ESP_OK;
}

// ---------------------- Endpoints ----------------------
static esp_err_t api_login(httpd_req_t* req)
{
    if (ITRUE == rate_limited()) {
        return send_json(req, "429 Too Many Requests", "{\"error\":\"rate limited\"}");
    }

    char body[512];
    if (ITRUE != read_body(req, body, sizeof(body))) {
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

    int ok = ((0 == strcmp(username, DEMO_USER)) && (0 == strcmp(password, DEMO_PASS))) ? ITRUE : IFALSE;
    cJSON_Delete(root);

    if (ITRUE != ok) {
        return send_json(req, "401 Unauthorized", "{\"error\":\"invalid credentials\"}");
    }

    // Issue new token (single active session)
    make_token(g_token);
    snprintf(g_user, sizeof(g_user), "%s", username);
    snprintf(g_role, sizeof(g_role), "%s", DEMO_ROLE);
    g_exp = (TOKEN_TTL_SEC > 0) ? (now_sec() + TOKEN_TTL_SEC) : 0;

    char resp[512];
    snprintf(resp, sizeof(resp),
        "{"
          "\"token\":\"%s\","
          "\"user\":{\"username\":\"%s\",\"role\":\"%s\"},"
          "\"message\":\"Login successful\""
        "}",
        g_token, g_user, g_role);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, resp);
}

static esp_err_t api_logout(httpd_req_t* req)
{
    const char* user = NULL;
    const char* role = NULL;

    esp_err_t espErr = auth_require_bearer(req, &user, &role);
    if (ESP_OK == espErr) {
        token_invalidate();
        return send_json(req, "200 OK", "{\"success\":true,\"message\":\"Logged out successfully\"}");
    }

    return espErr; // auth_require_bearer already sent response
}

static esp_err_t api_validate(httpd_req_t* req)
{
    const char* user = NULL;
    const char* role = NULL;

    esp_err_t espErr = auth_require_bearer(req, &user, &role);
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

    return espErr; // auth_require_bearer already sent response
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
