#include "WS_SchedulePage.h"
#include "esp_log.h"
#include "Definitions/AppErrors.h"
#include "WiFi_Manager_API.h"
#include "RingBell_API.h"

#define WEBI_SERVER_MAX_FORM_BUFFER_SIZE    256

static const char* SCHEDULE_PAGE =
    "<!DOCTYPE html>"
    "<html>"
    "<body>"
    "<h2>Ring Bell</h2>"
    "<form action=\"/schedule\" method=\"post\">"
    "<button type=\"submit\" name=\"state\" value=\"on\">Turn ON</button>"
    "<button type=\"submit\" name=\"state\" value=\"off\">Turn OFF</button>"
    "</form>"
    "</body>"
    "</html>";


esp_err_t
Ws_SchedulePage_Get(httpd_req_t *req)
{
    esp_err_t espErr = httpd_resp_set_type(req, "text/html");

    if(ESP_OK == espErr)
    {
        espErr = httpd_resp_send(req, SCHEDULE_PAGE, HTTPD_RESP_USE_STRLEN);
    }
    
    return espErr;
}

esp_err_t send_schedule_page_with_message(httpd_req_t *req, const char *message)
{
    httpd_resp_set_type(req, "text/html");

    char html[512];
    snprintf(html, sizeof(html),
        "<!DOCTYPE html>"
        "<html>"
        "<body>"
        "<h3>%s</h3>"           // <-- message injected here
        "<h2>Ring Bell</h2>"
        "<form action=\"/schedule\" method=\"post\">"
        "<button type=\"submit\" name=\"state\" value=\"on\">Turn ON</button>"
        "<button type=\"submit\" name=\"state\" value=\"off\">Turn OFF</button>"
        "</form>"
        "</body>"
        "</html>",
        message
    );

    return httpd_resp_sendstr(req, html);
}

esp_err_t Ws_SchedulePage_Post(httpd_req_t *req)
{
    char buf[WEBI_SERVER_MAX_FORM_BUFFER_SIZE];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (len <= 0)
        return httpd_resp_sendstr(req, "Invalid request");

    buf[len] = '\0';

    char state[4] = {0};
    if (httpd_query_key_value(buf, "state", state, sizeof(state)) != ESP_OK)
        return httpd_resp_sendstr(req, "Missing state");

    if (strcmp(state, "on") == 0)
    {
        RingBell_Run();
        return send_schedule_page_with_message(req, "Bell turned ON");
    }

    if (strcmp(state, "off") == 0)
    {
        RingBell_Stop();
        return send_schedule_page_with_message(req, "Bell turned OFF");
    }

    return httpd_resp_sendstr(req, "Invalid state");
}
