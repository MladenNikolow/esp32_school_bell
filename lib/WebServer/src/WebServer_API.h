#include <stdint.h>
#include "WebServer_Public.h"

/* Web server handler */
typedef struct _WEB_SERVER_RSC_T*     WEB_SERVER_H;

/* Initialize the web server */
int32_t 
WebServer_Init(WEB_SERVER_PARAMS_T* ptParams, WEB_SERVER_H* phWebServer);