#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

#include "Ws_React_Static.h"
#include "esp_log.h"

static const char *TAG = "WS_REACT";

/* ---------------------------------------------------------
 * Helper: determine MIME type from file extension
 * --------------------------------------------------------- */
static const char* ws_react_get_mime_type(const char* path)
{
    assert(path);

    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".js"))   return "application/javascript";
    if (strstr(path, ".css"))  return "text/css";
    if (strstr(path, ".json")) return "application/json";
    if (strstr(path, ".svg"))  return "image/svg+xml";
    if (strstr(path, ".png"))  return "image/png";
    if (strstr(path, ".jpg"))  return "image/jpeg";
    if (strstr(path, ".jpeg")) return "image/jpeg";
    if (strstr(path, ".ico"))  return "image/x-icon";
    if (strstr(path, ".wasm")) return "application/wasm";

    return "text/plain";
}

/* Utility: ends_with */
static int ends_with(const char *s, const char *suffix)
{
    if (!s || !suffix) return 0;
    size_t ls = strlen(s), lsu = strlen(suffix);
    if (ls < lsu) return 0;
    return strcmp(s + ls - lsu, suffix) == 0;
}

/* Utility: strip query string in-place */
static void strip_query(char *p)
{
    if (!p) return;
    char *q = strchr(p, '?');
    if (q) *q = '\0';
}

/* ---------------------------------------------------------
 * Serve a (gzip) file from filesystem as chunked response
 * --------------------------------------------------------- */
static esp_err_t ws_react_send_file_chunked(httpd_req_t* req,
                                           const char* file_path,
                                           const char* content_type,
                                           bool is_gz)
{
    assert(req && file_path && content_type);

    FILE* f = fopen(file_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "File not found: %s", file_path);
        return httpd_resp_send_404(req);
    }

    (void)httpd_resp_set_type(req, content_type);
    if (is_gz) {
        (void)httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }

    char buf[1024];
    size_t r;
    while ((r = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, r) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }
    fclose(f);

    return httpd_resp_send_chunk(req, NULL, 0);
}

static bool file_exists(const char* path)
{
    struct stat st;
    return (stat(path, &st) == 0);
}

/* ---------------------------------------------------------
 * Public handler
 * --------------------------------------------------------- */
esp_err_t Ws_React_StaticFileHandler(httpd_req_t* req)
{
    if (!req) return ESP_FAIL;

    if (req->uri[0] == '\0') {
        ESP_LOGE(TAG, "Empty request URI");
        return httpd_resp_send_404(req);
    }

    size_t uri_len = strlen(req->uri);
    ESP_LOGI(TAG, "Incoming request URI (len=%u): %s", (unsigned)uri_len, req->uri);

    if (uri_len > 2048) {
        ESP_LOGE(TAG, "URI too long: %u", (unsigned)uri_len);
        return httpd_resp_send_404(req);
    }

    /* Copy URI and strip query string */
    char uri_small[256];
    char *uri_ptr = NULL;
    char *uri_heap = NULL;

    if (uri_len < sizeof(uri_small)) {
        strncpy(uri_small, req->uri, sizeof(uri_small));
        uri_small[sizeof(uri_small)-1] = '\0';
        strip_query(uri_small);
        uri_ptr = uri_small;
    } else {
        uri_heap = malloc(uri_len + 1);
        if (!uri_heap) {
            ESP_LOGE(TAG, "malloc failed for uri_heap");
            return httpd_resp_send_404(req);
        }
        strncpy(uri_heap, req->uri, uri_len + 1);
        strip_query(uri_heap);
        uri_ptr = uri_heap;
    }

    /* Handle root -> index */
    if (strcmp(uri_ptr, "/") == 0) {
        if (file_exists("/react/index.html.gz")) {
            esp_err_t r = ws_react_send_file_chunked(req, "/react/index.html.gz", "text/html", true);
            if (uri_heap) free(uri_heap);
            return r;
        }

        if (file_exists("/react/index.html")) {
            esp_err_t r = ws_react_send_file_chunked(req, "/react/index.html", "text/html", false);
            if (uri_heap) free(uri_heap);
            return r;
        }

        if (uri_heap) free(uri_heap);
        return httpd_resp_send_404(req);
    }

    /* Build filesystem path(s) on heap to avoid large stack usage */
    size_t need = strlen("/react") + strlen(uri_ptr) + strlen(".gz") + 1;
    char *fs_path = malloc(need);
    if (!fs_path) {
        ESP_LOGE(TAG, "malloc failed for fs_path");
        if (uri_heap) free(uri_heap);
        return httpd_resp_send_404(req);
    }

    /* If client requested .gz explicitly, map directly to /react<uri> */
    if (ends_with(uri_ptr, ".gz")) {
        snprintf(fs_path, need, "/react%s", uri_ptr);
        ESP_LOGI(TAG, "Trying exact gz path: %s", fs_path);

        if (file_exists(fs_path)) {
            const char *mime = ws_react_get_mime_type(fs_path);
            esp_err_t r = ws_react_send_file_chunked(req, fs_path, mime, true);
            free(fs_path);
            if (uri_heap) free(uri_heap);
            return r;
        }

        ESP_LOGW(TAG, "File not found: %s (errno=%d %s)", fs_path, errno, strerror(errno));
    } else {
        /* Try gzipped file first: /react<uri>.gz */
        snprintf(fs_path, need, "/react%s.gz", uri_ptr);
        ESP_LOGI(TAG, "Trying gz path: %s", fs_path);

        if (file_exists(fs_path)) {
            const char *mime = ws_react_get_mime_type(fs_path);
            esp_err_t r = ws_react_send_file_chunked(req, fs_path, mime, true);
            free(fs_path);
            if (uri_heap) free(uri_heap);
            return r;
        }

        ESP_LOGW(TAG, "File not found: %s (errno=%d %s)", fs_path, errno, strerror(errno));
    }

    /* Fallback: try plain file /react<uri> */
    snprintf(fs_path, need, "/react%s", uri_ptr);
    ESP_LOGI(TAG, "Trying plain path: %s", fs_path);

    if (file_exists(fs_path)) {
        const char *mime = ws_react_get_mime_type(fs_path);
        esp_err_t r = ws_react_send_file_chunked(req, fs_path, mime, false);
        free(fs_path);
        if (uri_heap) free(uri_heap);
        return r;
    }

    ESP_LOGW(TAG, "File not found: %s (errno=%d %s)", fs_path, errno, strerror(errno));

    free(fs_path);
    if (uri_heap) free(uri_heap);
    ESP_LOGE(TAG, "File not found for URI %s (tried gz and plain)", uri_ptr);
    return httpd_resp_send_404(req);
}
