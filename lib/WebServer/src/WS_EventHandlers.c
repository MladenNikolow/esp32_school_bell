#include "WS_EventHandlers.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi_types.h"
#include "WiFi_Manager_API.h"

static const char* TAG = "WS_EVENT_HANDLERS";

void 
Ws_EventHandler_StaIP(void* pvArg, 
                      esp_event_base_t tEventBase,
                      int32_t ulEventId, 
                      void* pvEventData)
{
    assert((NULL != pvArg)        &&
           (NULL != pvEventData)  &&
           (IP_EVENT == tEventBase));

    switch(ulEventId)
    {
        case IP_EVENT_STA_GOT_IP:
        {
            ip_event_got_ip_t* tData = (ip_event_got_ip_t*) pvEventData;
            if(NULL != tData)
            {
                esp_netif_ip_info_t* ip_info = &tData->ip_info;
                ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&ip_info->ip));
            }
            break;
        }

        default:
        {
            ESP_LOGW(TAG, "Unhandled STA IP Event ID: %" PRId32, ulEventId);
            break;
        }
    }
}

void 
Ws_EventHandler_StaWiFi(void* pvArg, 
                        esp_event_base_t tEventBase,
                        int32_t ulEventId, 
                        void* pvEventData)
{
    assert((NULL != pvArg)          &&
           (NULL != pvEventData)    &&
           (WIFI_EVENT == tEventBase));

    switch(ulEventId)
    {
        case WIFI_EVENT_STA_DISCONNECTED:
        {
            ESP_LOGI(TAG, "Station disconnected, clearing saved WiFi credentials.");
            (void)WiFi_Manager_ClearCredentials();   

            ESP_LOGI(TAG, "The device is restareted in AP mode again.");
            esp_restart();
            break;
        }
    }
}

void 
Ws_EventHandler_ApIP(void* pvArg, 
                     esp_event_base_t tEventBase,
                     int32_t ulEventId, 
                     void* pvEventData);

void 
Ws_EventHandler_ApWiFi(void* pvArg,
                       esp_event_base_t tEventBase,
                       int32_t ulEventId, 
                       void* pvEventData);