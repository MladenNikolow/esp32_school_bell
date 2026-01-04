#include <stdint.h>
#include "esp_http_server.h"

esp_err_t
WebServer_Request_WiFiConfig_Get(httpd_req_t *req);

esp_err_t
WebServer_Request_WiFiConfig_Post(httpd_req_t *req);