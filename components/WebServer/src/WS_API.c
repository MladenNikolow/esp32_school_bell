#include <stdlib.h>
#include <string.h>
#include "Ws_API.h"
#include "AppErrors.h"
#include "WiFi_Manager_API.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "AP/WS_AccessPoint.h"
#include "STA/WS_Station.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "lwip/inet.h"   // for IPSTR / IP2STR
#include "WS_EventHandlers.h"
#include "mdns.h"

static const char* TAG = "WEBSERVER_API";

#define WS_MDNS_HOSTNAME    "ringy"
#define WS_MDNS_INSTANCE    "Ringy School Bell"

static esp_err_t
ws_InitMdns(void)
{
    esp_err_t espErr = mdns_init();
    if (ESP_OK != espErr)
    {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(espErr));
        return espErr;
    }

    espErr = mdns_hostname_set(WS_MDNS_HOSTNAME);
    if (ESP_OK != espErr)
    {
        ESP_LOGE(TAG, "mDNS hostname set failed: %s", esp_err_to_name(espErr));
        return espErr;
    }

    espErr = mdns_instance_name_set(WS_MDNS_INSTANCE);
    if (ESP_OK != espErr)
    {
        ESP_LOGE(TAG, "mDNS instance name set failed: %s", esp_err_to_name(espErr));
        return espErr;
    }

    /* Advertise HTTP service so the device is discoverable by
       service browsers (e.g. "dns-sd -B _http._tcp") */
    (void)mdns_service_add(WS_MDNS_INSTANCE, "_http", "_tcp", 80, NULL, 0);

    ESP_LOGI(TAG, "mDNS started — device reachable at http://" WS_MDNS_HOSTNAME ".local");
    return ESP_OK;
}

typedef struct _WEB_SERVER_RSC_T
{
    WEB_SERVER_PARAMS_T             tParams;                /* Init params */
    esp_netif_t*                    hNetif;                 /* netif handle (AP or STA) */
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
                        espErr = WS_AccessPoint_Start(ptRsc->tParams.hWiFiManager);
                    }

                    if(ESP_OK == espErr) 
                    {
                        esp_netif_ip_info_t ip;
                        esp_netif_get_ip_info(ptRsc->hNetif, &ip);
                        ESP_LOGI(TAG, "AP IP: " IPSTR, IP2STR(&ip.ip));
                    }

                    /* mDNS: device reachable at ringy.local in AP mode */
                    if (ESP_OK == espErr)
                    {
                        (void)ws_InitMdns();
                    }

                    break;
                }

                case WIFI_MANAGER_CONFIGURATION_STATE_CONFIGURED:
                {
                    espErr = ws_ConfigureSta(ptRsc);

                    if(ESP_OK == espErr)
                    {
                        espErr = WS_Station_Start(ptRsc->tParams.hScheduler,
                                               ptRsc->tParams.hWiFiManager);
                    }

                    if(ESP_OK == espErr)
                    {
                        /* mDNS: device reachable at ringy.local once STA gets an IP */
                        (void)ws_InitMdns();

                        ESP_LOGI(TAG, "STA+AP fallback mode active — "
                                 "connect to AP \"ESP32_Setup\" (pass: 12345678) "
                                 "at 192.168.4.1 or http://ringy.local to reconfigure WiFi");
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

    ptRsc->hNetif = esp_netif_create_default_wifi_ap();
    if(NULL == ptRsc->hNetif)
    {
        espErr = ESP_ERR_NO_MEM;
    }
    else
    {
        /* Create STA netif so the STA interface is available for WiFi scanning
           while the AP is running (requires WIFI_MODE_APSTA below). */
        (void)esp_netif_create_default_wifi_sta();

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

        /* APSTA mode: AP stays visible while STA interface can be used for scanning */
        espErr = esp_wifi_set_mode(WIFI_MODE_APSTA);
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
        ptRsc->hNetif = esp_netif_create_default_wifi_sta();
        if(NULL == ptRsc->hNetif)
        {
            espErr = ESP_ERR_NO_MEM;
        }
        else
        {
            /* Create AP netif so a fallback soft-AP is available while
               the STA interface retries connecting (APSTA mode below).
               Users can connect to the AP and reconfigure credentials
               without waiting for all retries to exhaust.               */
            (void)esp_netif_create_default_wifi_ap();

            wifi_init_config_t tWifiInitCfg = WIFI_INIT_CONFIG_DEFAULT();
            espErr = esp_wifi_init(&tWifiInitCfg);

            /* Credentials are managed by WiFi_Manager (own NVS namespace).
               Prevent the WiFi driver from persisting a duplicate copy —
               stale driver-level NVS data from a previous AP-mode session
               can leave the WPA supplicant partially initialised and crash
               inside pmksa_cache_flush when esp_wifi_set_config is called. */
            if (ESP_OK == espErr)
            {
                espErr = esp_wifi_set_storage(WIFI_STORAGE_RAM);
            }
        }
    }
    
    if(ESP_OK == espErr) 
    {
        espErr = esp_event_handler_instance_register(IP_EVENT, 
                                            IP_EVENT_STA_GOT_IP,
                                            &Ws_EventHandler_StaIP,
                                            ptRsc->hNetif, 
                                            &ptRsc->tEventHandlerStaGotIp);
    }

    if(ESP_OK == espErr) 
    {
        espErr = esp_event_handler_instance_register(WIFI_EVENT, 
                                            WIFI_EVENT_STA_DISCONNECTED,
                                            &Ws_EventHandler_StaWiFi,
                                            ptRsc->hNetif, 
                                            NULL);
    }

    /* Register AP client connect/disconnect events so the reconnect
       timer can be paused while a user is on the setup soft-AP.     */
    if(ESP_OK == espErr) 
    {
        espErr = esp_event_handler_instance_register(WIFI_EVENT, 
                                            WIFI_EVENT_AP_STACONNECTED,
                                            &Ws_EventHandler_ApWiFi,
                                            ptRsc->hNetif, 
                                            NULL);
    }

    if(ESP_OK == espErr) 
    {
        espErr = esp_event_handler_instance_register(WIFI_EVENT, 
                                            WIFI_EVENT_AP_STADISCONNECTED,
                                            &Ws_EventHandler_ApWiFi,
                                            ptRsc->hNetif, 
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

        /* APSTA mode: keep the soft-AP visible with default credentials
           so the user can reconfigure WiFi during STA connection retries. */
        espErr = esp_wifi_set_mode(WIFI_MODE_APSTA);
    }

    /* Configure fallback soft-AP with fixed credentials
       (mirrors WiFi_Manager defaults: "ESP32_Setup" / "12345678"). */
    if (ESP_OK == espErr)
    {
        wifi_config_t tConfigAp = {
            .ap = {
                .ssid           = "ESP32_Setup",
                .ssid_len       = sizeof("ESP32_Setup") - 1,
                .password       = "12345678",
                .max_connection = 4,
                .authmode       = WIFI_AUTH_WPA_WPA2_PSK,
            }
        };
        espErr = esp_wifi_set_config(WIFI_IF_AP, &tConfigAp);
    }

    /* Start WiFi BEFORE setting STA config so the WPA supplicant
       (including its PMKSA cache) is fully initialised.           */
    if(ESP_OK == espErr) 
    {
        espErr = esp_wifi_start();
    }

    if(ESP_OK == espErr) 
    {
        espErr = esp_wifi_set_config(WIFI_IF_STA, &tWifiCfg);
    }
    
    if(ESP_OK == espErr)
    {
        espErr = esp_wifi_connect();
    }

    return espErr;
}