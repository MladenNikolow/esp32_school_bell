#include <stdbool.h>
#include "RingBell_API.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp32_s3_touch_lcd_4.h"

static const char* TAG = "ringbell";

/* PCA9554PW I2C IO Expander ("Sirius" board)
   Address pins: A2=1, A1=1, A0=0 → 0x26 */
#define PCA9554_I2C_ADDR                   (0x26)
#define PCA9554_I2C_SPEED_HZ               (100000)
#define PCA9554_I2C_TIMEOUT_MS             (100)

/* Retry parameters for I2C operations on the shared bus */
#define PCA9554_MAX_RETRIES                (5)
#define PCA9554_RETRY_DELAY_MS             (100)

/* PCA9554PW registers */
#define PCA9554_REG_INPUT                  (0x00)
#define PCA9554_REG_OUTPUT                 (0x01)
#define PCA9554_REG_POLARITY               (0x02)
#define PCA9554_REG_CONFIG                 (0x03)

/* Relay on P0 */
#define PCA9554_RELAY_PIN_MASK             (0x01)  /* bit 0 = P0 */

#define RING_BELL_STATE_ON                 ((int)1)
#define RING_BELL_STATE_OFF                ((int)0)

#define RING_BELL_NVS_NAMESPACE            "bell"
#define RING_BELL_NVS_KEY_PANIC            "panic"

static BELL_STATE_E  s_eBellState = BELL_STATE_IDLE;
static bool          s_bPanic     = false;
static esp_timer_handle_t s_hDurationTimer = NULL;
static i2c_master_dev_handle_t s_hI2cDev   = NULL;

/* ------------------------------------------------------------------ */
/* PCA9554PW I2C helpers                                               */
/* ------------------------------------------------------------------ */

static esp_err_t
pca9554_WriteReg(uint8_t ucReg, uint8_t ucVal)
{
    uint8_t aucBuf[2] = { ucReg, ucVal };
    return i2c_master_transmit(s_hI2cDev, aucBuf, sizeof(aucBuf),
                              PCA9554_I2C_TIMEOUT_MS);
}

static esp_err_t __attribute__((unused))
pca9554_ReadReg(uint8_t ucReg, uint8_t* pucVal)
{
    return i2c_master_transmit_receive(s_hI2cDev,
                                      &ucReg, 1,
                                      pucVal, 1,
                                      PCA9554_I2C_TIMEOUT_MS);
}

/**
 * @brief Write a PCA9554 register with retries.
 *
 * The shared I2C bus (GT911 touch + CH32V003 expander) can cause
 * NACKs when another device is mid-transaction.  Retry with a
 * short delay to ride out the contention window.
 */
static esp_err_t
pca9554_WriteRegRetry(uint8_t ucReg, uint8_t ucVal)
{
    esp_err_t err = ESP_FAIL;

    for (int i = 0; i < PCA9554_MAX_RETRIES; i++)
    {
        err = pca9554_WriteReg(ucReg, ucVal);
        if (ESP_OK == err) return ESP_OK;

        ESP_LOGW(TAG, "PCA9554 write reg 0x%02X attempt %d/%d failed: %s",
                 ucReg, i + 1, PCA9554_MAX_RETRIES, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(PCA9554_RETRY_DELAY_MS));
    }

    return err;
}

static void
ringBell_I2cBusScan(i2c_master_bus_handle_t hBus)
{
    ESP_LOGW(TAG, "I2C bus scan — probing 0x08..0x77:");
    char acLine[128];
    int iPos = 0;
    int iFound = 0;
    bool bPca9554Found = false;

    for (uint8_t ucAddr = 0x08; ucAddr <= 0x77; ucAddr++)
    {
        esp_err_t ret = i2c_master_probe(hBus, ucAddr, 50);
        if (ESP_OK == ret)
        {
            iPos += snprintf(acLine + iPos, sizeof(acLine) - iPos,
                             " 0x%02X", ucAddr);
            iFound++;
            if (ucAddr == PCA9554_I2C_ADDR) bPca9554Found = true;
        }
    }

    if (iFound > 0)
    {
        ESP_LOGW(TAG, "Found %d device(s):%s", iFound, acLine);
    }
    else
    {
        ESP_LOGE(TAG, "No I2C devices found on bus!");
    }

    if (bPca9554Found)
    {
        ESP_LOGW(TAG, "PCA9554PW at 0x%02X — present on bus", PCA9554_I2C_ADDR);
    }
    else
    {
        ESP_LOGE(TAG, "PCA9554PW at 0x%02X — NOT found! Check wiring/pull-ups",
                 PCA9554_I2C_ADDR);
    }
}

static esp_err_t
ringBell_I2cInit(void)
{
    if (s_hI2cDev != NULL)
        return ESP_OK;

    i2c_master_bus_handle_t hBus = bsp_i2c_get_handle();
    if (NULL == hBus)
    {
        ESP_LOGE(TAG, "I2C bus not available");
        return ESP_ERR_INVALID_STATE;
    }

    /* Try to recover the bus in case SDA is stuck low from a
       previous incomplete transaction (power glitch, etc.). */
    esp_err_t err = i2c_master_bus_reset(hBus);
    if (ESP_OK != err)
    {
        ESP_LOGW(TAG, "I2C bus reset returned: %s (continuing)",
                 esp_err_to_name(err));
    }

    /* Debug: scan the bus to see what's actually responding */
    ringBell_I2cBusScan(hBus);

    const i2c_device_config_t tDevCfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PCA9554_I2C_ADDR,
        .scl_speed_hz    = PCA9554_I2C_SPEED_HZ,
    };

    err = i2c_master_bus_add_device(hBus, &tDevCfg, &s_hI2cDev);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Failed to add PCA9554PW device: %s",
                 esp_err_to_name(err));
        return err;
    }

    /* Register 3 — Configuration: 0 = output, 1 = input (default).
       Set P0 as output, P1-P7 remain inputs.  Value: 0xFE. */
    err = pca9554_WriteRegRetry(PCA9554_REG_CONFIG,
                                (uint8_t)~PCA9554_RELAY_PIN_MASK);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Failed to configure PCA9554PW direction after retries");
        i2c_master_bus_rm_device(s_hI2cDev);
        s_hI2cDev = NULL;
        return err;
    }

    /* Register 1 — Output: all LOW (relay OFF) */
    err = pca9554_WriteRegRetry(PCA9554_REG_OUTPUT, 0x00);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Failed to set PCA9554PW initial output");
        i2c_master_bus_rm_device(s_hI2cDev);
        s_hI2cDev = NULL;
        return err;
    }

    ESP_LOGI(TAG, "PCA9554PW initialized at 0x%02X — P0 relay output",
             PCA9554_I2C_ADDR);
    return ESP_OK;
}

static esp_err_t
ringBell_SetLevel(int iLevel)
{
    esp_err_t err = ringBell_I2cInit();
    if (ESP_OK != err) return err;

    /* Defensive: re-assert P0 as output in case the PCA9554PW
       reverted to defaults after a power glitch (all pins input). */
    err = pca9554_WriteRegRetry(PCA9554_REG_CONFIG,
                                (uint8_t)~PCA9554_RELAY_PIN_MASK);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Failed to re-assert config register");
        return err;
    }

    /* P1-P7 are configured as inputs — per PCA9554 datasheet §6.1.3
       "Bit values in this register have no effect on pins defined as
       inputs", so we can safely write the full register directly
       without a read-modify-write cycle. */
    uint8_t ucOutVal = iLevel ? PCA9554_RELAY_PIN_MASK : 0x00;

    return pca9554_WriteRegRetry(PCA9554_REG_OUTPUT, ucOutVal);
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

    /* Eagerly initialise the PCA9554PW now rather than on first ring.
       This surfaces I2C / wiring problems at boot instead of silently
       failing when the bell should actually ring. */
    err = ringBell_I2cInit();
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "PCA9554PW init failed — bell will not work: %s",
                 esp_err_to_name(err));
        /* Continue anyway so the rest of the system still boots.
           Every subsequent ringBell_SetLevel() call will retry init. */
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