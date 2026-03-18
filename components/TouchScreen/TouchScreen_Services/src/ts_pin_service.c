/* ================================================================== */
/* ts_pin_service.c — PIN validation, storage (NVS), lockout logic     */
/* ================================================================== */
#include "TouchScreen_Services.h"
#include "NVS_API.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <string.h>
#include <ctype.h>

static const char *TAG = "TS_PIN";

/* NVS namespace and key */
#define PIN_NVS_NAMESPACE   "touchpin"
#define PIN_NVS_KEY         "pin"
#define PIN_DEFAULT         "1234"
#define PIN_LENGTH          4

/* Lockout policy */
#define PIN_MAX_ATTEMPTS    3
#define PIN_LOCKOUT_US      (30ULL * 1000000ULL)  /* 30 seconds in microseconds */

/* Module state */
static uint8_t  s_ucFailCount     = 0;
static int64_t  s_llLockoutStart  = 0;   /* esp_timer_get_time() value when locked */
static bool     s_bInitialized    = false;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static bool
pin_is_valid_format(const char *pcPin)
{
    if (pcPin == NULL) return false;
    if (strlen(pcPin) != PIN_LENGTH) return false;
    for (int i = 0; i < PIN_LENGTH; i++)
    {
        if (!isdigit((unsigned char)pcPin[i])) return false;
    }
    return true;
}

static esp_err_t
pin_nvs_read(char *pcBuf, size_t ulBufLen)
{
    nvs_handle_t hNvs;
    esp_err_t err = NVS_Open(PIN_NVS_NAMESPACE, NVS_READWRITE, &hNvs);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    size_t usLen = ulBufLen;
    err = NVS_ReadString(hNvs, PIN_NVS_KEY, pcBuf, &usLen);
    NVS_Close(hNvs);
    return err;
}

static esp_err_t
pin_nvs_write(const char *pcPin)
{
    nvs_handle_t hNvs;
    esp_err_t err = NVS_Open(PIN_NVS_NAMESPACE, NVS_READWRITE, &hNvs);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    err = NVS_WriteString(hNvs, PIN_NVS_KEY, pcPin);
    if (ESP_OK == err)
    {
        err = NVS_Commit(hNvs);
    }
    NVS_Close(hNvs);
    return err;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t
TS_Pin_Init(void)
{
    if (s_bInitialized)
    {
        return ESP_OK;
    }

    /* Try to read existing PIN; if not found, write default */
    char acBuf[PIN_LENGTH + 1];
    esp_err_t err = pin_nvs_read(acBuf, sizeof(acBuf));
    if (ESP_ERR_NVS_NOT_FOUND == err)
    {
        ESP_LOGI(TAG, "No PIN stored — writing default");
        err = pin_nvs_write(PIN_DEFAULT);
    }

    if (ESP_OK == err)
    {
        s_bInitialized = true;
        ESP_LOGI(TAG, "PIN service initialized");
    }

    return err;
}

bool
TS_Pin_Validate(const char *pcPin)
{
    if (TS_Pin_IsLockedOut())
    {
        ESP_LOGW(TAG, "PIN validation rejected — locked out");
        return false;
    }

    if (!pin_is_valid_format(pcPin))
    {
        return false;
    }

    char acStored[PIN_LENGTH + 1] = {0};
    esp_err_t err = pin_nvs_read(acStored, sizeof(acStored));
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Failed to read PIN from NVS");
        return false;
    }

    if (strncmp(pcPin, acStored, PIN_LENGTH) == 0)
    {
        TS_Pin_ResetAttempts();
        return true;
    }

    /* Wrong PIN */
    s_ucFailCount++;
    ESP_LOGW(TAG, "Wrong PIN attempt %u/%u", s_ucFailCount, PIN_MAX_ATTEMPTS);

    if (s_ucFailCount >= PIN_MAX_ATTEMPTS)
    {
        s_llLockoutStart = esp_timer_get_time();
        ESP_LOGW(TAG, "Lockout activated for %u seconds", (unsigned)(PIN_LOCKOUT_US / 1000000ULL));
    }

    return false;
}

esp_err_t
TS_Pin_Set(const char *pcNewPin)
{
    if (!pin_is_valid_format(pcNewPin))
    {
        ESP_LOGE(TAG, "Invalid PIN format — must be %d digits", PIN_LENGTH);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = pin_nvs_write(pcNewPin);
    if (ESP_OK == err)
    {
        ESP_LOGI(TAG, "PIN updated");
    }
    return err;
}

esp_err_t
TS_Pin_Get(char *pcOutBuf, size_t ulBufLen)
{
    if (pcOutBuf == NULL || ulBufLen < (PIN_LENGTH + 1))
    {
        return ESP_ERR_INVALID_ARG;
    }
    return pin_nvs_read(pcOutBuf, ulBufLen);
}

bool
TS_Pin_IsLockedOut(void)
{
    if (s_ucFailCount < PIN_MAX_ATTEMPTS)
    {
        return false;
    }

    int64_t llNow = esp_timer_get_time();
    int64_t llElapsed = llNow - s_llLockoutStart;

    if (llElapsed >= (int64_t)PIN_LOCKOUT_US)
    {
        /* Lockout expired */
        TS_Pin_ResetAttempts();
        return false;
    }

    return true;
}

uint32_t
TS_Pin_GetLockoutRemaining(void)
{
    if (!TS_Pin_IsLockedOut())
    {
        return 0;
    }

    int64_t llNow = esp_timer_get_time();
    int64_t llRemaining = (int64_t)PIN_LOCKOUT_US - (llNow - s_llLockoutStart);

    if (llRemaining <= 0)
    {
        return 0;
    }

    return (uint32_t)(llRemaining / 1000000LL);  /* Convert us → seconds */
}

void
TS_Pin_ResetAttempts(void)
{
    s_ucFailCount = 0;
    s_llLockoutStart = 0;
}
