#include "WS_EventHandlers.h"

#include <string.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_timer.h"
#include "WiFi_Manager_API.h"
#include "AP/WS_AccessPoint.h"
#include "STA/WS_Station.h"

static const char* TAG = "WS_EVENT_HANDLERS";

/* ----------------------------------------------------------------
   Reconnect timer — exponential back-off retries for STA mode.

   Delay schedule (doubles each attempt, capped at MAX):
     attempt 0 :  5 s
     attempt 1 : 10 s
     attempt 2 : 20 s
     attempt 3 : 40 s
     attempt 4+: 60 s  (capped)
   ---------------------------------------------------------------- */
#define WS_RECONNECT_DELAY_MIN_MS    5000ULL
#define WS_RECONNECT_DELAY_MAX_MS   60000ULL

/* Default AP credentials for the fallback hotspot (must match WiFi Manager defaults) */
#define WS_FALLBACK_AP_SSID          "ESP32_Setup"
#define WS_FALLBACK_AP_PASS          "12345678"

static esp_timer_handle_t s_hReconnectTimer = NULL;
static uint32_t           s_ulRetryCount    = 0;

/* ----------------------------------------------------------------
   Fallback AP — when STA loses connection we spin up a temporary
   AP so the user can reach the configuration pages and change the
   SSID / password.  The AP is torn down as soon as STA reconnects.
   ---------------------------------------------------------------- */
static WIFI_MANAGER_H  s_hWiFiManager        = NULL;
static bool            s_fFallbackApActive    = false;
static esp_netif_t*    s_hApNetif             = NULL;
static bool            s_fReconnectSuspended  = false;

static void ws_ScheduleReconnect(void);

void
Ws_EventHandlers_SetWiFiManager(WIFI_MANAGER_H hWiFiManager)
{
    s_hWiFiManager = hWiFiManager;
}

void
Ws_EventHandlers_SuspendReconnect(void)
{
    s_fReconnectSuspended = true;

    if (NULL != s_hReconnectTimer)
    {
        (void)esp_timer_stop(s_hReconnectTimer);
    }

    ESP_LOGI(TAG, "STA reconnect suspended.");
}

void
Ws_EventHandlers_ResumeReconnect(void)
{
    s_fReconnectSuspended = false;

    ESP_LOGI(TAG, "STA reconnect resumed.");
    ws_ScheduleReconnect();
}

static void
ws_StartFallbackAp(void)
{
    if (s_fFallbackApActive || (NULL == s_hWiFiManager))
    {
        return;
    }

    ESP_LOGI(TAG, "Starting fallback AP for WiFi reconfiguration...");

    /* Create AP netif once (kept alive for the lifetime of the process) */
    if (NULL == s_hApNetif)
    {
        s_hApNetif = esp_netif_create_default_wifi_ap();
        if (NULL == s_hApNetif)
        {
            ESP_LOGE(TAG, "Failed to create AP netif");
            return;
        }
    }

    /* Build AP configuration with fixed default credentials so the
       fallback hotspot is always discoverable as "ESP32_Setup",
       regardless of which STA credentials are stored in NVS.      */
    wifi_config_t tConfigAp = {
        .ap = {
            .ssid_len       = 0,
            .max_connection = 4,
            .authmode       = WIFI_AUTH_WPA_WPA2_PSK,
        }
    };

    memcpy(tConfigAp.ap.ssid, WS_FALLBACK_AP_SSID, sizeof(WS_FALLBACK_AP_SSID));
    tConfigAp.ap.ssid_len = sizeof(WS_FALLBACK_AP_SSID) - 1U;
    memcpy(tConfigAp.ap.password, WS_FALLBACK_AP_PASS, sizeof(WS_FALLBACK_AP_PASS));

    /* Stop the STA HTTP server — port 80 can only be bound by one server */
    (void)WS_Station_Stop();

    /* Switch to APSTA so the STA keeps retrying while the AP is visible */
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Failed to set APSTA mode: %s", esp_err_to_name(err));
        return;
    }

    err = esp_wifi_set_config(WIFI_IF_AP, &tConfigAp);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Failed to configure fallback AP: %s", esp_err_to_name(err));
        return;
    }

    /* Start the AP HTTP server with config pages */
    err = WS_AccessPoint_Start(s_hWiFiManager);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Failed to start AP HTTP server: %s", esp_err_to_name(err));
        return;
    }

    s_fFallbackApActive = true;
    ESP_LOGI(TAG, "Fallback AP active — connect to configure WiFi.");
}

static void
ws_StopFallbackAp(void)
{
    if (!s_fFallbackApActive)
    {
        return;
    }

    ESP_LOGI(TAG, "STA reconnected — stopping fallback AP.");

    (void)WS_AccessPoint_Stop();

    /* Revert to STA-only mode */
    (void)esp_wifi_set_mode(WIFI_MODE_STA);

    /* Bring the STA HTTP server back */
    (void)WS_Station_Start();

    s_fFallbackApActive = false;
}

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

    ESP_LOGI(TAG, "WiFi reconnect scheduled in %" PRIu64 " ms (retry #%" PRIu32 ")",
             ullDelayMs, s_ulRetryCount);

    (void)esp_timer_start_once(s_hReconnectTimer, ullDelayMs * 1000ULL /* µs */);
}

static void
ws_ScheduleReconnectIfAllowed(void)
{
    if (!s_fReconnectSuspended)
    {
        ws_ScheduleReconnect();
    }
    else
    {
        ESP_LOGI(TAG, "Reconnect skipped (suspended for scan).");
    }
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

            /* Tear down the fallback AP if it was active */
            ws_StopFallbackAp();

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
                /* If the fallback AP is already serving, a handshake timeout
                   is likely just the router still being off — keep retrying
                   instead of wiping credentials and restarting.             */
                if (s_fFallbackApActive)
                {
                    ESP_LOGW(TAG, "WiFi auth error (reason=%d) while fallback AP active — retrying.", (int)ucReason);
                    ws_ScheduleReconnectIfAllowed();
                }
                else
                {
                    ESP_LOGI(TAG, "WiFi credential error (reason=%d) — clearing credentials and restarting.", (int)ucReason);
                    (void)WiFi_Manager_ClearCredentials();
                    esp_restart();
                }
            }
            else
            {
                ESP_LOGI(TAG, "WiFi disconnected (reason=%d) — will retry with back-off.", (int)ucReason);
                ws_ScheduleReconnectIfAllowed();
                ws_StartFallbackAp();
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