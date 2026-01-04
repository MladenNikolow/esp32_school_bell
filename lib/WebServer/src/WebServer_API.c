#include <stdlib.h>
#include <string.h>
#include "WebServer_API.h"
#include "Definitions/AppErrors.h"
#include "WiFi_Manager_API.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "WebServer_AP.h"

typedef struct _WEB_SERVER_RSC_T
{
    WEB_SERVER_PARAMS_T    tParams;               /* Init params */
} WEB_SERVER_RSC_T;

int32_t 
WebServer_Init(WEB_SERVER_PARAMS_T* ptParams, WEB_SERVER_H* phWebServer)
{
    int32_t lResult = APP_SUCCESS;
    WEB_SERVER_RSC_T* ptRsc = NULL;
    uint32_t ulWiFiConfigState = 0;

    if ((NULL == phWebServer) ||
        (NULL == ptParams))
    {
        return APP_ERROR_INVALID_PARAM;
    }

    ptRsc = (WEB_SERVER_RSC_T*)calloc(1, sizeof(WEB_SERVER_RSC_T));
    if (NULL == ptRsc)
    {
        lResult = APP_ERROR_OUT_OF_MEMORY;
    }
    else
    {             
        lResult = WiFi_Manager_GetConfigurationState(ptParams->hWiFiManager,
                                                     &ulWiFiConfigState);
                                                     
        switch(ulWiFiConfigState)
        {
            case WIFI_MANAGER_CONFIGURATION_STATE_WAITING_CONFIGURATION:
            {
                esp_netif_init();
                esp_event_loop_create_default();
                esp_netif_create_default_wifi_ap();
                wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
                esp_wifi_init(&cfg);

                uint8_t abSsid[WIFI_MANAGER_MAX_SSID_LENGTH + 1] = {0};
                uint8_t abPass[WIFI_MANAGER_MAX_PASS_LENGTH + 1] = {0};

                size_t ssidLen = sizeof(abSsid);
                size_t passLen = sizeof(abPass);

                WiFi_Manager_GetSsid(ptParams->hWiFiManager,
                                    abSsid,
                                    &ssidLen);
                
                WiFi_Manager_GetPassword(ptParams->hWiFiManager,
                                         abPass,
                                         &passLen);

                wifi_config_t ap_config = {
                    .ap = {
                        .ssid_len       = 0,
                        .max_connection = 4,
                        .authmode       = WIFI_AUTH_WPA_WPA2_PSK,
                    }
                };

                /* SSID */
                memcpy(ap_config.ap.ssid, abSsid, WIFI_MANAGER_MAX_SSID_LENGTH);
                ap_config.ap.ssid_len = strnlen((char*)abSsid, WIFI_MANAGER_MAX_SSID_LENGTH);

                /* Copy password */
                memcpy(ap_config.ap.password, abPass, WIFI_MANAGER_MAX_PASS_LENGTH);
 

                if (strlen((char*)abPass) == 0) {
                    ap_config.ap.authmode = WIFI_AUTH_OPEN;
                }

                esp_wifi_set_mode(WIFI_MODE_AP);
                esp_wifi_set_config(WIFI_IF_AP, &ap_config);
                esp_wifi_start();

                WebServer_AP_Start();

                break;
            }

            case WIFI_MANAGER_CONFIGURATION_STATE_CONFIGURED:
            {
                break;
            }

            case WIFI_MANAGER_CONFIGURATION_STATE_NOT_CONFIGURED:
            default:
            {
                lResult = APP_ERROR_UNEXPECTED;
                break;
            }
        }
        ptRsc->tParams = *ptParams;
        *phWebServer = (WEB_SERVER_H)ptRsc;
    }

    return lResult;
}