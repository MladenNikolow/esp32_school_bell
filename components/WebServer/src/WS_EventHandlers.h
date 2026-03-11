#include <stdint.h>
#include "esp_event.h"
#include "WiFi_Manager_API.h"

void
Ws_EventHandlers_SetWiFiManager(WIFI_MANAGER_H hWiFiManager);

/**
 * @brief Suspend STA reconnection attempts.
 *
 * Stops any pending reconnect timer and prevents new ones from being
 * scheduled.  Call before operations that need exclusive use of the
 * radio (e.g. WiFi scanning in APSTA fallback mode).
 */
void
Ws_EventHandlers_SuspendReconnect(void);

/**
 * @brief Resume STA reconnection attempts after a previous suspend.
 */
void
Ws_EventHandlers_ResumeReconnect(void);

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

