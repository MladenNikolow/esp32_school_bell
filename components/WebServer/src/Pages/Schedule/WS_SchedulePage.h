#include <stdint.h>
#include "esp_http_server.h"

esp_err_t
Ws_SchedulePage_Get(httpd_req_t *req);

esp_err_t
Ws_SchedulePage_Post(httpd_req_t *req);