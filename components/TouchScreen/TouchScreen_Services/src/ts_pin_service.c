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

/* NVS namespace and keys */
#define PIN_NVS_NAMESPACE   "touchpin"
#define PIN_NVS_KEY         "pin"
#define PIN_NVS_KEY_SET     "pin_set"   /* uint8_t: 1 if explicitly set by user */
#define PIN_DEFAULT         "1234"
#define PIN_MIN_LENGTH      4
#define PIN_MAX_LENGTH      6

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
    size_t ulLen = strlen(pcPin);
    if (ulLen < PIN_MIN_LENGTH || ulLen > PIN_MAX_LENGTH) return false;
    for (size_t i = 0; i < ulLen; i++)
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
    char acBuf[PIN_MAX_LENGTH + 1];
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

    char acStored[PIN_MAX_LENGTH + 1] = {0};
    esp_err_t err = pin_nvs_read(acStored, sizeof(acStored));
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Failed to read PIN from NVS");
        return false;
    }

    if (strcmp(pcPin, acStored) == 0)
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
        ESP_LOGE(TAG, "Invalid PIN format — must be %d-%d digits", PIN_MIN_LENGTH, PIN_MAX_LENGTH);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = pin_nvs_write(pcNewPin);
    if (ESP_OK == err)
    {
        /* Mark PIN as explicitly configured by user */
        nvs_handle_t hNvs;
        if (NVS_Open(PIN_NVS_NAMESPACE, NVS_READWRITE, &hNvs) == ESP_OK)
        {
            uint8_t ucSet = 1;
            NVS_Write(hNvs, PIN_NVS_KEY_SET, &ucSet, sizeof(ucSet));
            NVS_Commit(hNvs);
            NVS_Close(hNvs);
        }
        ESP_LOGI(TAG, "PIN updated (%d digits)", (int)strlen(pcNewPin));
    }
    return err;
}

esp_err_t
TS_Pin_Get(char *pcOutBuf, size_t ulBufLen)
{
    if (pcOutBuf == NULL || ulBufLen < (PIN_MIN_LENGTH + 1))
    {
        return ESP_ERR_INVALID_ARG;
    }
    return pin_nvs_read(pcOutBuf, ulBufLen);
}

uint8_t
TS_Pin_GetLength(void)
{
    char acBuf[PIN_MAX_LENGTH + 1] = {0};
    esp_err_t err = pin_nvs_read(acBuf, sizeof(acBuf));
    if (ESP_OK != err)
    {
        return PIN_MIN_LENGTH;  /* Fallback to minimum */
    }
    return (uint8_t)strlen(acBuf);
}

bool
TS_Pin_IsConfigured(void)
{
    nvs_handle_t hNvs;
    esp_err_t err = NVS_Open(PIN_NVS_NAMESPACE, NVS_READONLY, &hNvs);
    if (ESP_OK != err)
    {
        return false;
    }

    uint8_t ucSet = 0;
    size_t ulLen = sizeof(ucSet);
    err = NVS_Read(hNvs, PIN_NVS_KEY_SET, &ucSet, &ulLen);
    NVS_Close(hNvs);

    return (ESP_OK == err && ucSet != 0);
}

esp_err_t
TS_Pin_Reset(void)
{
    nvs_handle_t hNvs;
    esp_err_t err = NVS_Open(PIN_NVS_NAMESPACE, NVS_READWRITE, &hNvs);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Erase both keys — PIN and the "set" flag */
    nvs_erase_key(hNvs, PIN_NVS_KEY);
    nvs_erase_key(hNvs, PIN_NVS_KEY_SET);
    err = NVS_Commit(hNvs);
    NVS_Close(hNvs);

    if (ESP_OK == err)
    {
        s_bInitialized = false;  /* Will re-init with default on next TS_Pin_Init() */
        ESP_LOGI(TAG, "PIN reset — wizard will re-prompt");
    }
    return err;
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
