#include "WS_React_Routes.h"
#include "WS_React_FileServer.h"

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"

static const char* TAG = "WS_REACT_ROUTES";

// ----------------------------------------------------------------
// Forward declarations of dedicated per-route handlers
// ----------------------------------------------------------------
static esp_err_t ws_react_index_handler(httpd_req_t* req);
static esp_err_t ws_react_assets_handler(httpd_req_t* req);
static esp_err_t ws_react_favicon_handler(httpd_req_t* req);
static esp_err_t ws_react_root_file_handler(httpd_req_t* req);

// Named URI descriptor structs — one per explicit route
static const httpd_uri_t s_index_uri =
{
    .uri     = "/",
    .method  = HTTP_GET,
    .handler = ws_react_index_handler,
};

static const httpd_uri_t s_assets_uri =
{
    .uri     = "/assets/*",
    .method  = HTTP_GET,
    .handler = ws_react_assets_handler,
};

static const httpd_uri_t s_favicon_uri =
{
    .uri     = "/favicon.ico",
    .method  = HTTP_GET,
    .handler = ws_react_favicon_handler,
};

static const httpd_uri_t s_root_file_uri =
{
    .uri     = "/*",
    .method  = HTTP_GET,
    .handler = ws_react_root_file_handler,
};

// ----------------------------------------------------------------
// Route registration
// ----------------------------------------------------------------
esp_err_t Ws_React_RegisterRoutes(httpd_handle_t hHttpServer)
{
    esp_err_t espErr = httpd_register_uri_handler(hHttpServer, &s_index_uri);

    if (ESP_OK == espErr)
    {
        espErr = httpd_register_uri_handler(hHttpServer, &s_assets_uri);
    }

    if (ESP_OK == espErr)
    {
        espErr = httpd_register_uri_handler(hHttpServer, &s_favicon_uri);
    }

    if (ESP_OK == espErr)
    {
        ESP_LOGI(TAG, "React routes registered: GET /, GET /assets/*, GET /favicon.ico");
    }

    return espErr;
}

esp_err_t Ws_React_RegisterCatchAll(httpd_handle_t hHttpServer)
{
    esp_err_t espErr = httpd_register_uri_handler(hHttpServer, &s_root_file_uri);

    if (ESP_OK == espErr)
    {
        ESP_LOGI(TAG, "React catch-all route registered: GET /*");
    }

    return espErr;
}

// ----------------------------------------------------------------
// GET "/" — serve React entry point (index.html[.gz])
// ----------------------------------------------------------------
static esp_err_t ws_react_index_handler(httpd_req_t* req)
{
    char path[64];
    bool is_gz = false;

    esp_err_t espErr = WS_React_FileServer_ResolvePath("/", path, sizeof(path), &is_gz);
    if (ESP_OK != espErr)
    {
        return httpd_resp_send_404(req);
    }

    // SPA entry point — must not be cached, otherwise an outdated shell will
    // prevent the browser from picking up new hashed asset filenames.
    (void)httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");

    return WS_React_FileServer_ServeFile(req, path, "text/html", is_gz);
}

// ----------------------------------------------------------------
// GET "/favicon.ico" — serve from FS if present, otherwise 204 No Content
// ----------------------------------------------------------------
static esp_err_t ws_react_favicon_handler(httpd_req_t* req)
{
    char  path[64];
    bool  is_gz = false;

    esp_err_t espErr = WS_React_FileServer_ResolvePath("/favicon.ico", path, sizeof(path), &is_gz);
    if (ESP_OK == espErr)
    {
        return WS_React_FileServer_ServeFile(req, path, "image/x-icon", is_gz);
    }

    // Not on the filesystem — return 204 so the browser stops retrying
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

// ----------------------------------------------------------------
// GET "/assets/*" — serve hashed JS/CSS/asset files
// ----------------------------------------------------------------
static esp_err_t ws_react_assets_handler(httpd_req_t* req)
{
    // URI can be up to CONFIG_HTTPD_MAX_URI_LEN (512); use heap to keep stack lean
    size_t uri_len = strlen(req->uri);
    char*  uri     = (char*)malloc(uri_len + 1);
    if (!uri)
    {
        return httpd_resp_send_404(req);
    }

    memcpy(uri, req->uri, uri_len + 1);

    // Strip query string in-place
    char* q = strchr(uri, '?');
    if (q) { *q = '\0'; }

    // Resolve to a concrete FS path (max 256 — /react + /assets/... + .gz + NUL)
    char  path[256];
    bool  is_gz = false;
    esp_err_t espErr = WS_React_FileServer_ResolvePath(uri, path, sizeof(path), &is_gz);
    free(uri);

    if (ESP_OK != espErr)
    {
        return httpd_resp_send_404(req);
    }

    const char* mime = WS_React_FileServer_GetMime(path);

    // Vite hashes asset filenames on every build — they are safe to cache
    // indefinitely in the browser. This eliminates redundant re-downloads.
    //(void)httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000, immutable");

    // However, during development we want to disable caching of assets, otherwise the browser may fail to pick up new hashed filenames after a rebuild, resulting in a broken app until
    (void)httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    return WS_React_FileServer_ServeFile(req, path, mime, is_gz);
}

// ----------------------------------------------------------------
// GET "/*" — catch-all for root-level static files (logo, favicon.svg, etc.)
//             Falls back to index.html for SPA client-side routing.
// ----------------------------------------------------------------
static esp_err_t ws_react_root_file_handler(httpd_req_t* req)
{
    size_t uri_len = strlen(req->uri);
    char*  uri     = (char*)malloc(uri_len + 1);
    if (!uri)
    {
        return httpd_resp_send_404(req);
    }

    memcpy(uri, req->uri, uri_len + 1);

    // Strip query string in-place
    char* q = strchr(uri, '?');
    if (q) { *q = '\0'; }

    char  path[256];
    bool  is_gz = false;
    esp_err_t espErr = WS_React_FileServer_ResolvePath(uri, path, sizeof(path), &is_gz);
    free(uri);

    if (ESP_OK != espErr)
    {
        // Not a real file — serve index.html for SPA client-side routing
        bool idx_gz = false;
        espErr = WS_React_FileServer_ResolvePath("/", path, sizeof(path), &idx_gz);
        if (ESP_OK != espErr)
        {
            return httpd_resp_send_404(req);
        }
        (void)httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        return WS_React_FileServer_ServeFile(req, path, "text/html", idx_gz);
    }

    const char* mime = WS_React_FileServer_GetMime(path);
    (void)httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return WS_React_FileServer_ServeFile(req, path, mime, is_gz);
}
