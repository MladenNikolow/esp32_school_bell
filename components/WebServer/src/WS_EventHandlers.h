#include <stdint.h>
#include "esp_event.h"

void 
Ws_EventHandler_StaIP(void* pvArg, 
                      esp_event_base_t tEventBase,
                      int32_t ulEventId, 
                      void* pvEventData);

void 
Ws_EventHandler_StaWiFi(void* pvArg, 
                        esp_event_base_t tEventBase,
                        int32_t ulEventId, 
                        void* pvEventData);

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