#include "Ws_React.h"
#include "WS_React_Routes.h"
#include "esp_http_server.h"
#include "Auth/WS_Auth.h"

esp_err_t
Ws_React_RegisterStaticFiles(httpd_handle_t hHttpServer)
{
    return Ws_React_RegisterRoutes(hHttpServer);
}

esp_err_t
Ws_React_RegisterApiHandlers(httpd_handle_t hHttpServer)
{
    return auth_register_endpoints(hHttpServer);
}
