#include <stdlib.h>
#include <string.h>
#include "Ws_API.h"
#include "Definitions/AppErrors.h"
#include "WiFi_Manager_API.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "AP/WS_AccessPoint.h"
#include "STA/WS_Station.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "lwip/inet.h"   // for IPSTR / IP2STR
#include "WS_EventHandlers.h"

static const char* TAG = "WEBSERVER_API";

typedef struct _WEB_SERVER_RSC_T
{
    WEB_SERVER_PARAMS_T             tParams;                /* Init params */
    esp_netif_t*                    hApNetif;               /* AP netif handle */
    esp_event_handler_instance_t    tEventHandlerStaGotIp;  /* STA got IP event handler instance */  
} WEB_SERVER_RSC_T;

static esp_err_t
ws_ConfigureAp(WEB_SERVER_RSC_T* ptRsc);

static esp_err_t
ws_ConfigureSta(WEB_SERVER_RSC_T* ptRsc);

esp_err_t 
Ws_Init(WEB_SERVER_PARAMS_T* ptParams, WEB_SERVER_H* phWebServer)
{
    esp_err_t espErr = ESP_OK;
    WEB_SERVER_RSC_T* ptRsc = NULL;
    uint32_t ulWiFiConfigState = 0;

    if ((NULL == phWebServer) ||
        (NULL == ptParams))
    {
        return ESP_ERR_INVALID_ARG;
    }

    ptRsc = (WEB_SERVER_RSC_T*)calloc(1, sizeof(WEB_SERVER_RSC_T));
    if (NULL == ptRsc)
    {
        espErr = ESP_ERR_NO_MEM;
    }
    else
    {         
        ptRsc->tParams = *ptParams;

        espErr = WiFi_Manager_GetConfigurationState(ptParams->hWiFiManager,
                                                    &ulWiFiConfigState);

        if(ESP_OK == espErr)
        {
            switch(ulWiFiConfigState)
            {
                case WIFI_MANAGER_CONFIGURATION_STATE_WAITING_CONFIGURATION:
                {
                    espErr = ws_ConfigureAp(ptRsc);

                    // TODO : Move WS_AccessPoint_Start out of here (in event handler)
                    if(ESP_OK == espErr)
                    {
                        espErr = WS_AccessPoint_Start();
                    }

                    if(ESP_OK == espErr) 
                    {
                        esp_netif_ip_info_t ip;
                        esp_netif_get_ip_info(ptRsc->hApNetif, &ip);
                        ESP_LOGI(TAG, "AP IP: " IPSTR, IP2STR(&ip.ip));
                    }

                    break;
                }

                case WIFI_MANAGER_CONFIGURATION_STATE_CONFIGURED:
                {
                    espErr = ws_ConfigureSta(ptRsc);

                    if(ESP_OK == espErr)
                    {
                        espErr = WS_Station_Start();
                    }
                    
                    break;
                }

                case WIFI_MANAGER_CONFIGURATION_STATE_NOT_CONFIGURED:
                default:
                {
                    espErr = ESP_ERR_INVALID_STATE;
                    break;
                }
            }

            if(ESP_OK == espErr) 
            {
                *phWebServer = (WEB_SERVER_H)ptRsc;
            }
        }
                                                     
    }

    return espErr;
}

static esp_err_t
ws_ConfigureAp(WEB_SERVER_RSC_T* ptRsc)
{
    uint8_t abSsid[WIFI_MANAGER_MAX_SSID_LENGTH + 1] = {0};
    uint8_t abPass[WIFI_MANAGER_MAX_PASS_LENGTH + 1] = {0};

    size_t ssidLen = sizeof(abSsid);
    size_t passLen = sizeof(abPass);

    wifi_config_t tConfigAp = 
    {
        .ap = {
            .ssid_len       = 0,
            .max_connection = 4,
            .authmode       = WIFI_AUTH_WPA_WPA2_PSK,
        }
    };

    esp_err_t espErr = esp_netif_init();

    assert(NULL != ptRsc);

    if(ESP_OK == espErr) 
    {
        espErr = esp_event_loop_create_default();
    }

    ptRsc->hApNetif = esp_netif_create_default_wifi_ap();
    if(NULL == ptRsc->hApNetif)
    {
        espErr = ESP_ERR_NO_MEM;
    }
    else
    {
        wifi_init_config_t tWifiInitCfg = WIFI_INIT_CONFIG_DEFAULT();
        espErr = esp_wifi_init(&tWifiInitCfg);
    }

    if(ESP_OK == espErr) 
    {
        // TODO: Error handling
        WiFi_Manager_GetSsid(ptRsc->tParams.hWiFiManager,
                             abSsid,
                             &ssidLen);
        
        WiFi_Manager_GetPassword(ptRsc->tParams.hWiFiManager,
                                abPass,
                                &passLen);

        /* SSID */
        memcpy(tConfigAp.ap.ssid, abSsid, WIFI_MANAGER_MAX_SSID_LENGTH);
        tConfigAp.ap.ssid_len = strnlen((char*)abSsid, WIFI_MANAGER_MAX_SSID_LENGTH);

        /* Password */
        memcpy(tConfigAp.ap.password, abPass, WIFI_MANAGER_MAX_PASS_LENGTH);

        if (strlen((char*)abPass) == 0) 
        {
            tConfigAp.ap.authmode = WIFI_AUTH_OPEN;
        }

        espErr = esp_wifi_set_mode(WIFI_MODE_AP);
    }

    if(ESP_OK == espErr) 
    {
        espErr = esp_wifi_set_config(WIFI_IF_AP, &tConfigAp);
    }

    if(ESP_OK == espErr)
    {
        espErr = esp_wifi_start();
    }

    return espErr;
}

static esp_err_t
ws_ConfigureSta(WEB_SERVER_RSC_T* ptRsc)
{
    uint8_t abSsid[WIFI_MANAGER_MAX_SSID_LENGTH + 1] = {0};
    uint8_t abPass[WIFI_MANAGER_MAX_PASS_LENGTH + 1] = {0};
    size_t ssidLen = sizeof(abSsid);
    size_t passLen = sizeof(abPass);

    wifi_config_t tWifiCfg = 
    {
        .sta = {
            .ssid = {0},
            .password = {0},
            .threshold = {
                .authmode = WIFI_AUTH_WPA_WPA2_PSK
            },
            .pmf_cfg = {
                .capable = true,
                .required = false
            }
        }
    };

    esp_err_t espErr = esp_netif_init();

    assert(NULL != ptRsc);

    if(ESP_OK == espErr) 
    {
        espErr = esp_event_loop_create_default();
    }
    
    if(ESP_OK == espErr) 
    {
        ptRsc->hApNetif = esp_netif_create_default_wifi_sta();
        if(NULL == ptRsc->hApNetif)
        {
            espErr = ESP_ERR_NO_MEM;
        }
        else
        {
            wifi_init_config_t tWifiInitCfg = WIFI_INIT_CONFIG_DEFAULT();
            espErr = esp_wifi_init(&tWifiInitCfg);
        }
    }
    
    if(ESP_OK == espErr) 
    {
        espErr = esp_event_handler_instance_register(IP_EVENT, 
                                            IP_EVENT_STA_GOT_IP,
                                            &Ws_EventHandler_StaIP,
                                            ptRsc->hApNetif, 
                                            &ptRsc->tEventHandlerStaGotIp);
    }

    if(ESP_OK == espErr) 
    {
        espErr = esp_event_handler_instance_register(WIFI_EVENT, 
                                            WIFI_EVENT_STA_DISCONNECTED,
                                            &Ws_EventHandler_StaWiFi,
                                            ptRsc->hApNetif, 
                                            NULL);
    }
                             
    if(ESP_OK == espErr) 
    {
        // TODO: Error handling
        WiFi_Manager_GetSsid(ptRsc->tParams.hWiFiManager,
                            abSsid,
                            &ssidLen);
        
        WiFi_Manager_GetPassword(ptRsc->tParams.hWiFiManager,
                                abPass,
                                &passLen);

        ESP_LOGI(TAG, "WIFI SSID: %s", abSsid);
        ESP_LOGI(TAG, "WIFI PASS: %s", abPass);

        /* Copy SSID and ensure null termination */
        memcpy(tWifiCfg.sta.ssid, abSsid, WIFI_MANAGER_MAX_SSID_LENGTH);
        tWifiCfg.sta.ssid[WIFI_MANAGER_MAX_SSID_LENGTH - 1] = '\0';

        /* Copy password and ensure null termination */
        memcpy(tWifiCfg.sta.password, abPass, WIFI_MANAGER_MAX_PASS_LENGTH);
        tWifiCfg.sta.password[WIFI_MANAGER_MAX_PASS_LENGTH - 1] = '\0';

        /* If password is empty, allow open APs (station will still try to connect) */
        if (strlen((char*)abPass) == 0) 
        {
            /* For STA there is no direct authmode field to force open,
                but threshold.authmode can be lowered if needed. Keep default. */
            tWifiCfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
        }

        /* Initialize WiFi (assumes esp_wifi_init called elsewhere with cfg) */
        espErr = esp_wifi_set_mode(WIFI_MODE_STA);
    }

    if(ESP_OK == espErr) 
    {
        espErr = esp_wifi_set_config(WIFI_IF_STA, &tWifiCfg);
    }
    
    if(ESP_OK == espErr) 
    {
        espErr = esp_wifi_start();
    }
    
    if(ESP_OK == espErr)
    {
        espErr = esp_wifi_connect();
    }

    return espErr;
}