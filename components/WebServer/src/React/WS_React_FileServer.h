#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include <stdbool.h>

/**
 * @brief Return the MIME type string for the given file path based on its extension.
 *        Falls back to "text/plain" for unknown extensions.
 */
const char* WS_React_FileServer_GetMime(const char* path);

/**
 * @brief Resolve a request URI to an absolute filesystem path under /react/.
 *
 * Resolution rules (matching the original Ws_React_Static.c logic):
 *   "/"          -> tries /react/index.html.gz then /react/index.html
 *   "<uri>.gz"   -> tries /react<uri> directly (already compressed)
 *   "<uri>"      -> tries /react<uri>.gz first, then /react<uri> as fallback
 *
 * @param uri      Stripped URI string (no query string, must start with '/').
 * @param out_path Output buffer that receives the absolute FS path.
 * @param out_len  Size of out_path buffer.
 * @param out_gz   Set to true when the resolved file is gzip-compressed.
 * @return         ESP_OK when a matching file exists, ESP_ERR_NOT_FOUND otherwise.
 */
esp_err_t WS_React_FileServer_ResolvePath(const char* uri,
                                          char*        out_path,
                                          size_t       out_len,
                                          bool*        out_gz);

/**
 * @brief Stream a file from the filesystem to the HTTP client as a chunked response.
 *
 * Sets Content-Type and, when is_gz is true, Content-Encoding: gzip.
 * Sends the terminating zero-length chunk on success.
 */
esp_err_t WS_React_FileServer_ServeFile(httpd_req_t* req,
                                        const char*  path,
                                        const char*  mime,
                                        bool         is_gz);
