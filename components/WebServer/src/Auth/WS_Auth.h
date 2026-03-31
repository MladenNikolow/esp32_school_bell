#include "esp_http_server.h"
#include <stdbool.h>

esp_err_t auth_register_endpoints(httpd_handle_t server);

void auth_set_security_headers(httpd_req_t* req);
bool auth_csrf_check(httpd_req_t* req);
esp_err_t auth_require_session(httpd_req_t* req, const char** out_user, const char** out_role);
int auth_active_session_count(void);
