#include "WS_EventHandlers.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_timer.h"
#include "WiFi_Manager_API.h"

static const char* TAG = "WS_EVENT_HANDLERS";

/* ----------------------------------------------------------------
   Reconnect timer — exponential back-off retries for STA mode.

   Delay schedule (doubles each attempt, capped at MAX):
     attempt 0 :  5 s
     attempt 1 : 10 s
     attempt 2 : 20 s
     attempt 3 : 40 s
     attempt 4 : 60 s  (capped)

   After WS_RECONNECT_MAX_RETRIES the device clears stored
   credentials and restarts into AP-setup mode so the user can
   reconfigure WiFi (e.g. pick a 2.4 GHz network — ESP32 does
   not support 5 GHz).
   ---------------------------------------------------------------- */
#define WS_RECONNECT_DELAY_MIN_MS    5000ULL
#define WS_RECONNECT_DELAY_MAX_MS   60000ULL
#define WS_RECONNECT_MAX_RETRIES     5U

static esp_timer_handle_t s_hReconnectTimer = NULL;
static uint32_t           s_ulRetryCount    = 0;

static void
ws_ReconnectTimerCb(void* pvArg)
{
    (void)pvArg;
    ESP_LOGI(TAG, "Retrying WiFi connection (attempt %" PRIu32 ")...", s_ulRetryCount);
    (void)esp_wifi_connect();
}

static void
ws_ScheduleReconnect(void)
{
    /* All retries exhausted — fall back to AP setup mode */
    if (s_ulRetryCount >= WS_RECONNECT_MAX_RETRIES)
    {
        ESP_LOGW(TAG,
                 "WiFi connection failed after %" PRIu32 " retries — "
                 "clearing credentials and restarting into AP setup mode.",
                 s_ulRetryCount);
        (void)WiFi_Manager_ClearCredentials();
        esp_restart();
        return;   /* unreachable, but keeps intent clear */
    }

    /* Exponential back-off: delay = MIN * 2^retries, capped at MAX */
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

    ESP_LOGI(TAG, "WiFi reconnect scheduled in %" PRIu64 " ms (retry #%" PRIu32 " of %u)",
             ullDelayMs, s_ulRetryCount, WS_RECONNECT_MAX_RETRIES);

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

            /* Only genuine credential failures warrant clearing NVS and dropping
               back to AP setup mode.  Everything else (router reboot, signal loss,
               AP not found yet, etc.) is treated as a temporary outage and retried
               with exponential back-off.                                           */
            bool fIsCredentialError = (ucReason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                                       ucReason == WIFI_REASON_AUTH_FAIL               ||
                                       ucReason == WIFI_REASON_HANDSHAKE_TIMEOUT);

            if (fIsCredentialError)
            {
                ESP_LOGI(TAG, "WiFi credential error (reason=%d) — clearing credentials and restarting.", (int)ucReason);
                (void)WiFi_Manager_ClearCredentials();
                esp_restart();
            }
            else
            {
                ESP_LOGI(TAG, "WiFi disconnected (reason=%d) — will retry with back-off.", (int)ucReason);
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