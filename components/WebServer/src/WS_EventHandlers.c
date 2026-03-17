#include "WS_EventHandlers.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_timer.h"

#include <string.h>

static const char* TAG = "WS_EVENT_HANDLERS";

/* ----------------------------------------------------------------
   Reconnect timer — exponential back-off retries for STA mode.

   Delay schedule (doubles each attempt, capped at MAX):
     attempt 0 :  5 s
     attempt 1 : 10 s
     attempt 2 : 20 s
     attempt 3 : 40 s
     attempt 4+: 60 s  (capped)

   Retries are **indefinite** — the device never clears stored
   credentials on its own.  The soft-AP ("ESP32_Setup") launched
   by ws_ConfigureSta() in APSTA mode stays reachable between
   reconnect attempts, so a user can always connect to it and
   submit new credentials via the web UI at 192.168.4.1.
   ---------------------------------------------------------------- */
#define WS_RECONNECT_DELAY_MIN_MS    5000ULL
#define WS_RECONNECT_DELAY_MAX_MS   60000ULL

static esp_timer_handle_t s_hReconnectTimer  = NULL;
static uint32_t           s_ulRetryCount     = 0;
static uint32_t           s_ulApClientCount  = 0;

static void
ws_ReconnectTimerCb(void* pvArg)
{
    (void)pvArg;

    /* A client is connected to the soft-AP — do NOT call esp_wifi_connect()
       because the STA connection / channel-scan disrupts the AP interface
       (WPA handshake failures, DHCP timeouts).  The retry will be
       re-scheduled once the last AP client disconnects.                    */
    if (s_ulApClientCount > 0U)
    {
        ESP_LOGI(TAG, "STA reconnect deferred — %" PRIu32 " AP client(s) connected.",
                 s_ulApClientCount);
        return;
    }

    ESP_LOGI(TAG, "Retrying WiFi connection (attempt %" PRIu32 ")...", s_ulRetryCount);
    (void)esp_wifi_connect();
}

static void
ws_ScheduleReconnect(void)
{
    /* Exponential back-off: delay = MIN * 2^retries, capped at MAX.
       Retries are indefinite — the soft-AP ("ESP32_Setup") started
       in APSTA mode by ws_ConfigureSta() remains reachable between
       attempts so a user can always reconfigure credentials.        */
    uint64_t ullDelayMs = WS_RECONNECT_DELAY_MIN_MS;
    for (uint32_t i = 0; i < s_ulRetryCount; i++)
    {
        ullDelayMs <<= 1U;
        if (ullDelayMs >= WS_RECONNECT_DELAY_MAX_MS)
        {
            ullDelayMs = WS_RECONNECT_DELAY_MAX_MS;
            break;
        }
    }

    s_ulRetryCount++;

    if (NULL == s_hReconnectTimer)
    {
        const esp_timer_create_args_t tTimerArgs = {
            .callback = ws_ReconnectTimerCb,
            .name     = "wifi_reconnect",
        };
        (void)esp_timer_create(&tTimerArgs, &s_hReconnectTimer);
    }
    else
    {
        /* Cancel any already-pending retry before arming a new one */
        (void)esp_timer_stop(s_hReconnectTimer);
    }

    ESP_LOGI(TAG, "WiFi reconnect scheduled in %" PRIu64 " ms (retry #%" PRIu32 ")",
             ullDelayMs, s_ulRetryCount);

    (void)esp_timer_start_once(s_hReconnectTimer, ullDelayMs * 1000ULL /* µs */);
}

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
            /* Network is back — reset retry state */
            s_ulRetryCount = 0;
            if (NULL != s_hReconnectTimer)
            {
                (void)esp_timer_stop(s_hReconnectTimer);
            }

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
            wifi_event_sta_disconnected_t* ptDiscData = (wifi_event_sta_disconnected_t*)pvEventData;
            uint8_t ucReason = ptDiscData->reason;

            /* Classify the disconnect reason for logging purposes only.
               ALL reasons are retried indefinitely with exponential
               back-off — the soft-AP stays reachable between attempts
               so the user can reconfigure credentials at any time.     */
            bool fIsCredentialError = (ucReason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                                       ucReason == WIFI_REASON_AUTH_FAIL               ||
                                       ucReason == WIFI_REASON_HANDSHAKE_TIMEOUT       ||
                                       ucReason == WIFI_REASON_NO_AP_FOUND);

            if (fIsCredentialError)
            {
                ESP_LOGW(TAG, "WiFi credential/network error (reason=%d) — "
                              "retrying with back-off. Connect to 'ESP32_Setup' "
                              "AP at 192.168.4.1 to reconfigure.", (int)ucReason);
            }
            else
            {
                ESP_LOGI(TAG, "WiFi disconnected (reason=%d) — will retry with back-off.", (int)ucReason);
            }

            ws_ScheduleReconnect();
            break;
        }

        default:
        {
            break;
        }
    }
}

void 
Ws_EventHandler_ApWiFi(void* pvArg,
                       esp_event_base_t tEventBase,
                       int32_t ulEventId, 
                       void* pvEventData)
{
    (void)pvArg;
    (void)tEventBase;

    switch (ulEventId)
    {
        case WIFI_EVENT_AP_STACONNECTED:
        {
            wifi_event_ap_staconnected_t* ptData =
                (wifi_event_ap_staconnected_t*)pvEventData;

            s_ulApClientCount++;
            ESP_LOGI(TAG, "AP client connected (AID=%d, total=%" PRIu32
                          ") — STA reconnect paused.",
                     (int)ptData->aid, s_ulApClientCount);

            /* Stop the pending reconnect timer so that esp_wifi_connect()
               is not called while an AP client needs a stable link.       */
            if (NULL != s_hReconnectTimer)
            {
                (void)esp_timer_stop(s_hReconnectTimer);
            }

            /* Disconnect the STA interface to stop any in-progress
               connection attempt that would cause channel hopping.  */
            (void)esp_wifi_disconnect();
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            wifi_event_ap_stadisconnected_t* ptData =
                (wifi_event_ap_stadisconnected_t*)pvEventData;

            if (s_ulApClientCount > 0U)
            {
                s_ulApClientCount--;
            }
            ESP_LOGI(TAG, "AP client disconnected (AID=%d, remaining=%" PRIu32 ").",
                     (int)ptData->aid, s_ulApClientCount);

            /* Last AP client left — resume STA reconnect attempts.  */
            if (0U == s_ulApClientCount)
            {
                ESP_LOGI(TAG, "No AP clients — resuming STA reconnect.");
                ws_ScheduleReconnect();
            }
            break;
        }

        default:
        {
            break;
        }
    }
}