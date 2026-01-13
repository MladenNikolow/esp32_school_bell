#include <stdio.h>
#include <string.h>
#include "Ws_React.h"
#include "esp_log.h"
#include "RestAPI/Example/ExampleAPI.h"
#include <sys/stat.h>
#include <errno.h>
#include "esp_http_server.h"

#define WS_REACT_FILE_PATH_SIZE             524

static const char *TAG = "WS_REACT";

static esp_err_t 
ws_React_ServeGzipFile(httpd_req_t* ptHttpReq, 
                       const char* pcFilePath, 
                       const char* pcContentType);
static esp_err_t 
ws_React_StaticFileHandler(httpd_req_t* ptHttpReq);

static const char*
ws_React_GetMimeType(const char* pcPath);

esp_err_t
Ws_React_RegisterStaticFiles(httpd_handle_t hHttpServer)
{
    esp_err_t espRslt = ESP_OK;

    // Serve index.html
    httpd_uri_t index_uri = 
    {
        .uri = "/",
        .method = HTTP_GET,
        .handler = ws_React_StaticFileHandler
    };

    espRslt = httpd_register_uri_handler(hHttpServer, &index_uri);

        if(espRslt == ESP_OK)
    {
        httpd_uri_t assets_uri =
        {
            .uri = "/assets/*",
            .method = HTTP_GET,
            .handler = ws_React_StaticFileHandler
        };

        espRslt = httpd_register_uri_handler(hHttpServer, &assets_uri);
        ESP_LOGI(TAG, "register assets handler -> %s (uri=%s)", espRslt == ESP_OK ? "OK" : "FAIL", assets_uri.uri);
    }

    // // Serve JS files
    // if(espRslt == ESP_OK)
    // {
    //     httpd_uri_t js_uri = 
    //     {
    //         .uri = "/assets/*.js*",
    //         .method = HTTP_GET,
    //         .handler = ws_React_StaticFileHandler
    //     };

    //     espRslt = httpd_register_uri_handler(hHttpServer, &js_uri);
    // }

    // // Serve CSS files
    // if(espRslt == ESP_OK)
    // {
    //     httpd_uri_t css_uri = 
    //     {
    //         .uri = "/assets/*.css*",
    //         .method = HTTP_GET,
    //         .handler = ws_React_StaticFileHandler
    //     };

    //     espRslt = httpd_register_uri_handler(hHttpServer, &css_uri);
    // }
 
    // // Serve images
    // if(espRslt == ESP_OK)
    // {
    //     httpd_uri_t png_uri = 
    //     {
    //         .uri = "/assets/*.png*",
    //         .method = HTTP_GET,
    //         .handler = ws_React_StaticFileHandler
    //     };

    //     espRslt = httpd_register_uri_handler(hHttpServer, &png_uri);
    // }
    
    // if(espRslt == ESP_OK)
    // {
    //     httpd_uri_t jpg_uri = 
    //     {
    //         .uri = "/assets/*.jpg*",
    //         .method = HTTP_GET,
    //         .handler = ws_React_StaticFileHandler
    //     };

    //     espRslt = httpd_register_uri_handler(hHttpServer, &jpg_uri);
    // }
 
    // if(espRslt == ESP_OK)
    // {
    //     httpd_uri_t svg_uri = 
    //     {
    //         .uri = "/assets/*.svg*",
    //         .method = HTTP_GET,
    //         .handler = ws_React_StaticFileHandler
    //     };

    //     espRslt = httpd_register_uri_handler(hHttpServer, &svg_uri);
    // }
 
    // // Serve JSON files
    // if(espRslt == ESP_OK)
    // {
    //     httpd_uri_t json_uri = 
    //     {
    //         .uri = "/assets/*.json*",
    //         .method = HTTP_GET,
    //         .handler = ws_React_StaticFileHandler
    //     };

    //     espRslt = httpd_register_uri_handler(hHttpServer, &json_uri);
    // }
 
    // // Serve WASM
    // if(espRslt == ESP_OK)
    // {
    //     httpd_uri_t wasm_uri = 
    //     {
    //         .uri = "/assets/*.wasm*",
    //         .method = HTTP_GET,
    //         .handler = ws_React_StaticFileHandler
    //     };

    //     espRslt = httpd_register_uri_handler(hHttpServer, &wasm_uri);
    // }
 
    ESP_LOGI(TAG, "Static file handlers registered.");

    return espRslt;
}


// ---------------------------------------------------------
// Serve a gzip-compressed file from FatFS
// ---------------------------------------------------------
static esp_err_t 
ws_React_ServeGzipFile(httpd_req_t* ptHttpReq, 
                       const char* pcFilePath, 
                       const char* pcContentType)
{
    char acBuffer[1024];
    size_t readBytes = 0;

    assert((NULL != ptHttpReq)      &&
           (NULL != pcFilePath)     &&
           (NULL != pcContentType));

    FILE* ptFile = fopen(pcFilePath, "rb");

    if (NULL == ptFile) 
    {
        ESP_LOGE(TAG, "File not found: %s", pcFilePath);
        return httpd_resp_send_404(ptHttpReq);
    }
 
    (void)httpd_resp_set_type(ptHttpReq, pcContentType);
    (void)httpd_resp_set_hdr(ptHttpReq, "Content-Encoding", "gzip");
 
    while ((readBytes = fread(acBuffer, 1, sizeof(acBuffer), ptFile)) > 0) 
    {
        (void)httpd_resp_send_chunk(ptHttpReq, acBuffer, readBytes);
    }
 
    fclose(ptFile);

    return httpd_resp_send_chunk(ptHttpReq, NULL, 0);
}
 
// ---------------------------------------------------------
// Helper: determine MIME type from file extension
// ---------------------------------------------------------
static const char*
ws_React_GetMimeType(const char* pcPath)
{
    assert(NULL != pcPath);

    if (strstr(pcPath, ".html")) return "text/html";
    if (strstr(pcPath, ".js"))   return "application/javascript";
    if (strstr(pcPath, ".css"))  return "text/css";
    if (strstr(pcPath, ".json")) return "application/json";
    if (strstr(pcPath, ".svg"))  return "image/svg+xml";
    if (strstr(pcPath, ".png"))  return "image/png";
    if (strstr(pcPath, ".jpg"))  return "image/jpeg";
    if (strstr(pcPath, ".jpeg")) return "image/jpeg";
    if (strstr(pcPath, ".ico"))  return "image/x-icon";
    if (strstr(pcPath, ".wasm")) return "application/wasm";

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

/* Main handler */
static esp_err_t ws_React_StaticFileHandler(httpd_req_t* req)
{
    if (!req) return ESP_FAIL;

    /* req->uri is an array in esp_http_server; check contents */
    if (req->uri[0] == '\0') {
        ESP_LOGE(TAG, "Empty request URI");
        return httpd_resp_send_404(req);
    }

    size_t uri_len = strlen(req->uri);
    ESP_LOGI(TAG, "Incoming request URI (len=%u): %s", (unsigned)uri_len, req->uri);

    /* Reject absurdly long URIs to avoid resource issues */
    if (uri_len > 2048) {
        ESP_LOGE(TAG, "URI too long: %u", (unsigned)uri_len);
        return httpd_resp_send_404(req);
    }

    /* Copy and strip query */
    char uri_small[256];
    char *uri_ptr = NULL;
    char *uri_heap = NULL;

    if (uri_len < sizeof(uri_small)) {
        strncpy(uri_small, req->uri, sizeof(uri_small));
        uri_small[sizeof(uri_small)-1] = '\0';
        strip_query(uri_small);
        uri_ptr = uri_small;
    } else {
        /* allocate on heap for longer but acceptable URIs */
        uri_heap = malloc(uri_len + 1);
        if (!uri_heap) {
            ESP_LOGE(TAG, "malloc failed for uri_heap");
            return httpd_resp_send_404(req);
        }
        strncpy(uri_heap, req->uri, uri_len + 1);
        strip_query(uri_heap);
        uri_ptr = uri_heap;
    }

    /* Handle root */
    if (strcmp(uri_ptr, "/") == 0) {
        struct stat st;
        if (stat("/react/index.html.gz", &st) == 0) {
            if (uri_heap) free(uri_heap);
            return ws_React_ServeGzipFile(req, "/react/index.html.gz", "text/html");
        }
        if (stat("/react/index.html", &st) == 0) {
            FILE *f = fopen("/react/index.html", "rb");
            if (!f) {
                if (uri_heap) free(uri_heap);
                return httpd_resp_send_404(req);
            }
            httpd_resp_set_type(req, "text/html");
            char buf[1024];
            size_t r;
            while ((r = fread(buf,1,sizeof(buf),f))>0) {
                if (httpd_resp_send_chunk(req, buf, r) != ESP_OK) {
                    fclose(f);
                    if (uri_heap) free(uri_heap);
                    return ESP_FAIL;
                }
            }
            fclose(f);
            if (uri_heap) free(uri_heap);
            return httpd_resp_send_chunk(req, NULL, 0);
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

    struct stat st;

    /* If client requested .gz explicitly, map directly to /react<uri> */
    if (ends_with(uri_ptr, ".gz")) {
        snprintf(fs_path, need, "/react%s", uri_ptr); /* uri_ptr already contains .gz */
        ESP_LOGI(TAG, "Trying exact gz path: %s", fs_path);
        if (stat(fs_path, &st) == 0) {
            const char *mime = ws_React_GetMimeType(fs_path);
            esp_err_t r = ws_React_ServeGzipFile(req, fs_path, mime);
            free(fs_path);
            if (uri_heap) free(uri_heap);
            return r;
        } else {
            ESP_LOGW(TAG, "stat failed for %s: errno=%d (%s)", fs_path, errno, strerror(errno));
        }
    } else {
        /* Try gzipped file first: /react<uri>.gz */
        snprintf(fs_path, need, "/react%s.gz", uri_ptr);
        ESP_LOGI(TAG, "Trying gz path: %s", fs_path);
        if (stat(fs_path, &st) == 0) {
            const char *mime = ws_React_GetMimeType(fs_path);
            esp_err_t r = ws_React_ServeGzipFile(req, fs_path, mime);
            free(fs_path);
            if (uri_heap) free(uri_heap);
            return r;
        } else {
            ESP_LOGW(TAG, "stat failed for %s: errno=%d (%s)", fs_path, errno, strerror(errno));
        }
    }

    /* Fallback: try plain file /react<uri> */
    snprintf(fs_path, need, "/react%s", uri_ptr);
    ESP_LOGI(TAG, "Trying plain path: %s", fs_path);
    if (stat(fs_path, &st) == 0) {
        const char *mime = ws_React_GetMimeType(fs_path);
        FILE *f = fopen(fs_path, "rb");
        if (!f) {
            ESP_LOGE(TAG, "fopen failed for %s: %s", fs_path, strerror(errno));
            free(fs_path);
            if (uri_heap) free(uri_heap);
            return httpd_resp_send_404(req);
        }
        httpd_resp_set_type(req, mime);
        char buf[1024];
        size_t r;
        while ((r = fread(buf,1,sizeof(buf),f))>0) {
            if (httpd_resp_send_chunk(req, buf, r) != ESP_OK) {
                fclose(f);
                free(fs_path);
                if (uri_heap) free(uri_heap);
                return ESP_FAIL;
            }
        }
        fclose(f);
        free(fs_path);
        if (uri_heap) free(uri_heap);
        return httpd_resp_send_chunk(req, NULL, 0);
    } else {
        ESP_LOGW(TAG, "stat failed for %s: errno=%d (%s)", fs_path, errno, strerror(errno));
    }

    free(fs_path);
    if (uri_heap) free(uri_heap);
    ESP_LOGE(TAG, "File not found for URI %s (tried gz and plain)", uri_ptr);
    return httpd_resp_send_404(req);
}

// static esp_err_t 
// ws_React_StaticFileHandler(httpd_req_t* ptHttpReq)
// {
//     char acFilePath[WS_REACT_FILE_PATH_SIZE];

//     assert(NULL != ptHttpReq);
 
//     // Root path → serve index.html.gz
//     if (strcmp(ptHttpReq->uri, "/") == 0) {
//         snprintf(acFilePath, sizeof(acFilePath), "/react/index.html.gz");
//         return ws_React_ServeGzipFile(ptHttpReq, acFilePath, "text/html");
//     }
//     else // Build path: /assets/main.js → /react/assets/main.js.gz
//     {
//         snprintf(acFilePath, sizeof(acFilePath), "/react%s.gz", ptHttpReq->uri);
 
//         const char *mime = ws_React_GetMimeType(ptHttpReq->uri);
//         return ws_React_ServeGzipFile(ptHttpReq, acFilePath, mime);
//     }
// }

esp_err_t
Ws_React_RegisterApiHandlers(httpd_handle_t hHttpServer)
{   
    esp_err_t espRslt = Ws_React_RegisterExampleAPI(hHttpServer);

    return espRslt;
}
