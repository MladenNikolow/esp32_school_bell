#include <stdint.h>
#include "esp_err.h"
#include "WS_Public.h"

/* Web server handler */
typedef struct _WEB_SERVER_RSC_T*     WEB_SERVER_H;

/* Initialize the web server */
esp_err_t 
Ws_Init(WEB_SERVER_PARAMS_T* ptParams, WEB_SERVER_H* phWebServer);

// esp_err_t
// Ws_Start();