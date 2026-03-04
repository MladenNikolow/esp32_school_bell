#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief Register the React SPA static-file route handlers with the HTTP server.
 *
 * Registers two dedicated URI handlers:
 *   GET "/"            -> serves /react/index.html[.gz]
 *   GET "/assets/{*}"  -> serves /react/assets/<file>[.gz]
 *
 * The HTTP server must have been started with uri_match_fn = httpd_uri_match_wildcard
 * so that the assets pattern is evaluated correctly.
 */
esp_err_t Ws_React_RegisterRoutes(httpd_handle_t hHttpServer);
