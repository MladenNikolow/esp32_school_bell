#include "esp_http_server.h"
#include <stdbool.h>

/**
 * @brief  Initialise the authentication subsystem.
 *         Provisions the service account hash on first boot.
 *         Must be called once before any login attempt.
 */
esp_err_t auth_init(void);

esp_err_t auth_register_endpoints(httpd_handle_t server);

void auth_set_security_headers(httpd_req_t* req);
bool auth_csrf_check(httpd_req_t* req);
esp_err_t auth_require_session(httpd_req_t* req, const char** out_user, const char** out_role);

/**
 * @brief  Require a specific role for the current session.
 *         Returns 403 if the session role doesn't match required_role.
 *
 * @param[in]  req           HTTP request
 * @param[in]  required_role Role string to check (e.g. "service")
 * @param[out] out_user      Receives username (may be NULL)
 * @param[out] out_role      Receives role (may be NULL)
 * @return ESP_OK if session is valid AND role matches
 */
esp_err_t auth_require_role(httpd_req_t* req, const char* required_role,
                            const char** out_user, const char** out_role);

int auth_active_session_count(void);

/**
 * @brief  Invalidate all active sessions (forced logout).
 *         Called when credentials change to ensure stale sessions are removed.
 */
void auth_invalidate_all_sessions(void);
