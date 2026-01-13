#include <stdio.h>
#include <string.h>
#include "Ws_React.h"
#include "esp_log.h"
#include "RestAPI/Example/ExampleAPI.h"
 

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

    // Serve JS files
    if(espRslt == ESP_OK)
    {
        httpd_uri_t js_uri = 
        {
            .uri = "/assets/*.js",
            .method = HTTP_GET,
            .handler = ws_React_StaticFileHandler
        };

        espRslt = httpd_register_uri_handler(hHttpServer, &js_uri);
    }

    // Serve CSS files
    if(espRslt == ESP_OK)
    {
        httpd_uri_t css_uri = 
        {
            .uri = "/assets/*.css",
            .method = HTTP_GET,
            .handler = ws_React_StaticFileHandler
        };

        espRslt = httpd_register_uri_handler(hHttpServer, &css_uri);
    }
 
    // Serve images
    if(espRslt == ESP_OK)
    {
        httpd_uri_t png_uri = 
        {
            .uri = "/assets/*.png",
            .method = HTTP_GET,
            .handler = ws_React_StaticFileHandler
        };

        espRslt = httpd_register_uri_handler(hHttpServer, &png_uri);
    }
    
    if(espRslt == ESP_OK)
    {
        httpd_uri_t jpg_uri = 
        {
            .uri = "/assets/*.jpg",
            .method = HTTP_GET,
            .handler = ws_React_StaticFileHandler
        };

        espRslt = httpd_register_uri_handler(hHttpServer, &jpg_uri);
    }
 
    if(espRslt == ESP_OK)
    {
        httpd_uri_t svg_uri = 
        {
            .uri = "/assets/*.svg",
            .method = HTTP_GET,
            .handler = ws_React_StaticFileHandler
        };

        espRslt = httpd_register_uri_handler(hHttpServer, &svg_uri);
    }
 
    // Serve JSON files
    if(espRslt == ESP_OK)
    {
        httpd_uri_t json_uri = 
        {
            .uri = "/assets/*.json",
            .method = HTTP_GET,
            .handler = ws_React_StaticFileHandler
        };

        espRslt = httpd_register_uri_handler(hHttpServer, &json_uri);
    }
 
    // Serve WASM
    if(espRslt == ESP_OK)
    {
        httpd_uri_t wasm_uri = 
        {
            .uri = "/assets/*.wasm",
            .method = HTTP_GET,
            .handler = ws_React_StaticFileHandler
        };

        espRslt = httpd_register_uri_handler(hHttpServer, &wasm_uri);
    }
 
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

    FILE* ptFile = fopen(pcFilePath, "r");

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
 
// ---------------------------------------------------------
// Universal static file handler (React + Vite compatible)
// ---------------------------------------------------------
static esp_err_t 
ws_React_StaticFileHandler(httpd_req_t* ptHttpReq)
{
    char acFilePath[WS_REACT_FILE_PATH_SIZE];

    assert(NULL != ptHttpReq);
 
    // Root path → serve index.html.gz
    if (strcmp(ptHttpReq->uri, "/") == 0) {
        snprintf(acFilePath, sizeof(acFilePath), "/react/index.html.gz");
        return ws_React_ServeGzipFile(ptHttpReq, acFilePath, "text/html");
    }
    else // Build path: /assets/main.js → /react/assets/main.js.gz
    {
        snprintf(acFilePath, sizeof(acFilePath), "/react%s.gz", ptHttpReq->uri);
 
        const char *mime = ws_React_GetMimeType(ptHttpReq->uri);
        return ws_React_ServeGzipFile(ptHttpReq, acFilePath, mime);
    }
}

esp_err_t
Ws_React_RegisterApiHandlers(httpd_handle_t hHttpServer)
{   
    esp_err_t espRslt = Ws_React_RegisterExampleAPI(hHttpServer);

    return espRslt;
}
