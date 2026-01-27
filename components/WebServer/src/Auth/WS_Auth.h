#include "esp_http_server.h"

esp_err_t auth_register_endpoints(httpd_handle_t server);

/**
 * Checks Authorization: Bearer <token>
 * - returns ESP_OK if valid
 * - otherwise sends 401 JSON and returns error
 * out_user/out_role pointers remain valid until request ends
 */
esp_err_t auth_require_bearer(httpd_req_t* req, const char** out_user, const char** out_role);
