#include "WiFi_Manager_API.h"
#include "Scheduler_API.h"

typedef struct  _WEB_SERVER_PARAMS_T
{
    WIFI_MANAGER_H       hWiFiManager;           /* WiFi manager handle */
    SCHEDULER_H          hScheduler;             /* Scheduler handle */
} WEB_SERVER_PARAMS_T;