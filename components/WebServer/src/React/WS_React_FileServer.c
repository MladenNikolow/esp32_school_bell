#include "WS_React_FileServer.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

#include "esp_log.h"

static const char* TAG = "WS_FILE_SERVER";

// Chunk size for file streaming. 4 KB reduces write-syscall overhead
// for large compressed JS/CSS bundles compared to the original 1 KB.
#define WS_FILE_CHUNK_SIZE 4096

// ----------------------------------------------------------------
// MIME type detection
// ----------------------------------------------------------------
const char* WS_React_FileServer_GetMime(const char* path)
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

// ----------------------------------------------------------------
// Internal helpers
// ----------------------------------------------------------------
static bool fs_file_exists(const char* path)
{
    struct stat st;
    return (stat(path, &st) == 0);
}

static bool uri_ends_with_gz(const char* uri)
{
    if (!uri) return false;
    size_t len = strlen(uri);
    return (len >= 3) && (strcmp(uri + len - 3, ".gz") == 0);
}

// ----------------------------------------------------------------
// Path resolution
// ----------------------------------------------------------------
esp_err_t WS_React_FileServer_ResolvePath(const char* uri,
                                          char*        out_path,
                                          size_t       out_len,
                                          bool*        out_gz)
{
    if (!uri || !out_path || !out_gz || (out_len == 0))
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Root: try .gz then plain index.html
    if (strcmp(uri, "/") == 0)
    {
        snprintf(out_path, out_len, "/react/index.html.gz");
        if (fs_file_exists(out_path))
        {
            *out_gz = true;
            return ESP_OK;
        }

        snprintf(out_path, out_len, "/react/index.html");
        if (fs_file_exists(out_path))
        {
            *out_gz = false;
            return ESP_OK;
        }

        ESP_LOGW(TAG, "index.html not found (tried .gz and plain)");
        return ESP_ERR_NOT_FOUND;
    }

    // Client requested pre-compressed asset: serve as-is
    if (uri_ends_with_gz(uri))
    {
        snprintf(out_path, out_len, "/react%s", uri);
        if (fs_file_exists(out_path))
        {
            *out_gz = true;
            return ESP_OK;
        }

        ESP_LOGW(TAG, "gz asset not found: %s (errno=%d)", out_path, errno);
        return ESP_ERR_NOT_FOUND;
    }

    // Try compressed version first
    snprintf(out_path, out_len, "/react%s.gz", uri);
    if (fs_file_exists(out_path))
    {
        *out_gz = true;
        ESP_LOGI(TAG, "resolved to gz: %s", out_path);
        return ESP_OK;
    }

    // Fallback to plain file
    snprintf(out_path, out_len, "/react%s", uri);
    if (fs_file_exists(out_path))
    {
        *out_gz = false;
        ESP_LOGI(TAG, "resolved to plain: %s", out_path);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "file not found for URI %s (errno=%d)", uri, errno);
    return ESP_ERR_NOT_FOUND;
}

// ----------------------------------------------------------------
// Chunked file sender
// ----------------------------------------------------------------
esp_err_t WS_React_FileServer_ServeFile(httpd_req_t* req,
                                        const char*  path,
                                        const char*  mime,
                                        bool         is_gz)
{
    assert(req && path && mime);

    FILE* f = fopen(path, "rb");
    if (!f)
    {
        ESP_LOGE(TAG, "fopen failed: %s (errno=%d)", path, errno);
        return httpd_resp_send_404(req);
    }

    (void)httpd_resp_set_type(req, mime);
    if (is_gz)
    {
        (void)httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }

    char* buf = (char*)malloc(WS_FILE_CHUNK_SIZE);
    if (!buf)
    {
        fclose(f);
        ESP_LOGE(TAG, "malloc failed for chunk buffer");
        return httpd_resp_send_404(req);
    }

    size_t r;
    while ((r = fread(buf, 1, WS_FILE_CHUNK_SIZE, f)) > 0)
    {
        if (httpd_resp_send_chunk(req, buf, (ssize_t)r) != ESP_OK)
        {
            // The socket is already broken (e.g. client disconnected or EAGAIN
            // exhausted the send_wait_timeout). Returning ESP_FAIL here would
            // cause httpd to attempt writing a 500 response into the middle of
            // a partially-sent chunked body, corrupting it and causing a white
            // screen. Log and return ESP_OK so httpd closes the connection cleanly.
            ESP_LOGW(TAG, "mid-transfer send_chunk failed for %s — client likely disconnected", path);
            free(buf);
            fclose(f);
            return ESP_OK;
        }
    }
    free(buf);
    fclose(f);

    // Terminate chunked transfer. Ignore errors here — the browser may have
    // closed the connection after receiving the last data chunk, which is normal
    // pipelining behaviour.
    esp_err_t espFinalChunk = httpd_resp_send_chunk(req, NULL, 0);
    if (ESP_OK != espFinalChunk)
    {
        ESP_LOGD(TAG, "final chunk send failed for %s (client likely closed connection)", path);
    }

    return ESP_OK;
}
