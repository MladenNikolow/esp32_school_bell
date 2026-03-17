#include <stdbool.h>
#include "RingBell_API.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char* TAG = "ringbell";

#define RING_BELL_START_STOP_PIN           GPIO_NUM_4  /* D4 */
#define RING_BELL_STATE_ON                 ((int)1)
#define RING_BELL_STATE_OFF                ((int)0)

#define RING_BELL_NVS_NAMESPACE            "bell"
#define RING_BELL_NVS_KEY_PANIC            "panic"

static BELL_STATE_E  s_eBellState = BELL_STATE_IDLE;
static bool          s_bPanic     = false;
static esp_timer_handle_t s_hDurationTimer = NULL;
static bool          s_bGpioConfigured = false;

/* ------------------------------------------------------------------ */
/* GPIO                                                                */
/* ------------------------------------------------------------------ */

static esp_err_t
ringBell_ConfigureGpio(void)
{
    if (s_bGpioConfigured) return ESP_OK;

    gpio_config_t tGpio = {
        .pin_bit_mask  = (1ULL << RING_BELL_START_STOP_PIN),
        .mode          = GPIO_MODE_OUTPUT,
        .pull_up_en    = GPIO_PULLUP_DISABLE,
        .pull_down_en  = GPIO_PULLDOWN_DISABLE,
        .intr_type     = GPIO_INTR_DISABLE
    };

    esp_err_t err = gpio_config(&tGpio);
    if (ESP_OK == err)
    {
        s_bGpioConfigured = true;
    }
    return err;
}

static esp_err_t
ringBell_SetLevel(int iLevel)
{
    esp_err_t err = ringBell_ConfigureGpio();
    if (ESP_OK == err)
    {
        err = gpio_set_level(RING_BELL_START_STOP_PIN, iLevel);
    }
    return err;
}

/* ------------------------------------------------------------------ */
/* Duration timer callback                                             */
/* ------------------------------------------------------------------ */
static void
ringBell_DurationExpired(void* pvArg)
{
    (void)pvArg;
    ringBell_SetLevel(RING_BELL_STATE_OFF);
    if (s_eBellState == BELL_STATE_RINGING)
    {
        s_eBellState = BELL_STATE_IDLE;
    }
    ESP_LOGI(TAG, "Duration timer expired, bell stopped");
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t
RingBell_Init(void)
{
    /* Create one-shot timer for duration-based ringing */
    const esp_timer_create_args_t tTimerArgs = {
        .callback = ringBell_DurationExpired,
        .name     = "bell_dur"
    };

    esp_err_t err = esp_timer_create(&tTimerArgs, &s_hDurationTimer);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Failed to create duration timer");
        return err;
    }

    /* Restore panic state from NVS */
    nvs_handle_t hNvs;
    if (nvs_open(RING_BELL_NVS_NAMESPACE, NVS_READONLY, &hNvs) == ESP_OK)
    {
        uint8_t ucVal = 0;
        size_t ulLen = sizeof(ucVal);
        if (nvs_get_blob(hNvs, RING_BELL_NVS_KEY_PANIC, &ucVal, &ulLen) == ESP_OK)
        {
            s_bPanic = (ucVal != 0);
        }
        nvs_close(hNvs);
    }

    /* If panic was persisted, activate bell */
    if (s_bPanic)
    {
        ESP_LOGW(TAG, "Panic mode restored from NVS — activating bell");
        ringBell_SetLevel(RING_BELL_STATE_ON);
        s_eBellState = BELL_STATE_PANIC;
    }

    return ESP_OK;
}

esp_err_t
RingBell_Run(void)
{
    if (s_eBellState == BELL_STATE_PANIC) return ESP_OK;

    esp_err_t err = ringBell_SetLevel(RING_BELL_STATE_ON);
    if (ESP_OK == err)
    {
        s_eBellState = BELL_STATE_RINGING;
    }
    return err;
}

esp_err_t
RingBell_Stop(void)
{
    if (s_eBellState == BELL_STATE_PANIC) return ESP_OK;

    /* Cancel any pending duration timer */
    if (s_hDurationTimer != NULL)
    {
        esp_timer_stop(s_hDurationTimer);
    }

    esp_err_t err = ringBell_SetLevel(RING_BELL_STATE_OFF);
    if (ESP_OK == err)
    {
        s_eBellState = BELL_STATE_IDLE;
    }
    return err;
}

esp_err_t
RingBell_RunForDuration(uint32_t ulDurationSec)
{
    if (s_eBellState == BELL_STATE_PANIC) return ESP_OK;
    if (0 == ulDurationSec) return ESP_ERR_INVALID_ARG;

    /* Stop any existing timer */
    if (s_hDurationTimer != NULL)
    {
        esp_timer_stop(s_hDurationTimer);
    }

    esp_err_t err = ringBell_SetLevel(RING_BELL_STATE_ON);
    if (ESP_OK == err)
    {
        s_eBellState = BELL_STATE_RINGING;
        err = esp_timer_start_once(s_hDurationTimer, (uint64_t)ulDurationSec * 1000000ULL);
        if (ESP_OK != err)
        {
            ESP_LOGE(TAG, "Failed to start duration timer, stopping bell");
            ringBell_SetLevel(RING_BELL_STATE_OFF);
            s_eBellState = BELL_STATE_IDLE;
        }
        else
        {
            ESP_LOGI(TAG, "Bell ringing for %"PRIu32" seconds", ulDurationSec);
        }
    }

    return err;
}

esp_err_t
RingBell_SetPanic(bool bEnable)
{
    s_bPanic = bEnable;

    /* Persist to NVS */
    nvs_handle_t hNvs;
    if (nvs_open(RING_BELL_NVS_NAMESPACE, NVS_READWRITE, &hNvs) == ESP_OK)
    {
        uint8_t ucVal = bEnable ? 1 : 0;
        nvs_set_blob(hNvs, RING_BELL_NVS_KEY_PANIC, &ucVal, sizeof(ucVal));
        nvs_commit(hNvs);
        nvs_close(hNvs);
    }

    if (bEnable)
    {
        /* Cancel duration timer */
        if (s_hDurationTimer != NULL)
        {
            esp_timer_stop(s_hDurationTimer);
        }
        ringBell_SetLevel(RING_BELL_STATE_ON);
        s_eBellState = BELL_STATE_PANIC;
        ESP_LOGW(TAG, "PANIC mode ENABLED — bell ON continuously");
    }
    else
    {
        ringBell_SetLevel(RING_BELL_STATE_OFF);
        s_eBellState = BELL_STATE_IDLE;
        ESP_LOGI(TAG, "PANIC mode DISABLED — bell OFF");
    }

    return ESP_OK;
}

BELL_STATE_E
RingBell_GetState(void)
{
    return s_eBellState;
}

bool
RingBell_IsPanic(void)
{
    return s_bPanic;
}