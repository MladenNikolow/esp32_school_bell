#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t
Ws_React_RegisterStaticFiles(httpd_handle_t hHttpServer);

esp_err_t
Ws_React_RegisterApiHandlers(httpd_handle_t hHttpServer);