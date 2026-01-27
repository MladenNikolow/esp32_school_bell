#pragma once
#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief Static file handler for React build output stored under /react on the device FS.
 *
 * Logic (kept from original):
 *  - "/" -> serves /react/index.html.gz if present, else /react/index.html
 *  - for any other URI:
 *      * if URI ends with .gz -> try /react<uri> (already gz), serve with Content-Encoding:gzip
 *      * else try /react<uri>.gz first, then /react<uri> as fallback
 *  - query strings are stripped ("/assets/app.js?x=1" -> "/assets/app.js")
 */
esp_err_t Ws_React_StaticFileHandler(httpd_req_t* req);
