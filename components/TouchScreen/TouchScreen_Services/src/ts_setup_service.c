/* ================================================================== */
/* ts_setup_service.c — First-boot setup wizard flag (NVS)             */
/* ================================================================== */
#include "TouchScreen_Services.h"
#include "NVS_API.h"
#include "WiFi_Manager_API.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "TS_SETUP";

/* NVS namespace and key */
#define SETUP_NVS_NAMESPACE   "setup"
#define SETUP_NVS_KEY         "done"

/* Module state */
static bool s_bSetupComplete  = false;
static bool s_bInitialized    = false;

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t
TS_Setup_Init(void)
{
    if (s_bInitialized)
    {
        return ESP_OK;
    }

    nvs_handle_t hNvs;
    esp_err_t err = NVS_Open(SETUP_NVS_NAMESPACE, NVS_READONLY, &hNvs);

    if (ESP_OK == err)
    {
        uint8_t ucVal = 0;
        size_t ulLen = sizeof(ucVal);
        err = NVS_Read(hNvs, SETUP_NVS_KEY, &ucVal, &ulLen);

        if (ESP_OK == err && ucVal != 0)
        {
            s_bSetupComplete = true;
            ESP_LOGI(TAG, "Setup already completed");
        }
        else if (ESP_ERR_NVS_NOT_FOUND == err)
        {
            s_bSetupComplete = false;
            ESP_LOGI(TAG, "Setup not yet completed");
            err = ESP_OK; /* Not an error — just first boot */
        }

        NVS_Close(hNvs);
    }
    else if (ESP_ERR_NVS_NOT_FOUND == err)
    {
        /* Namespace doesn't exist yet — first boot */
        s_bSetupComplete = false;
        err = ESP_OK;
        ESP_LOGI(TAG, "Setup namespace not found — first boot");
    }

    s_bInitialized = true;
    return err;
}

bool
TS_Setup_IsComplete(void)
{
    return s_bSetupComplete;
}

esp_err_t
TS_Setup_MarkComplete(void)
{
    nvs_handle_t hNvs;
    esp_err_t err = NVS_Open(SETUP_NVS_NAMESPACE, NVS_READWRITE, &hNvs);

    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t ucVal = 1;
    err = NVS_Write(hNvs, SETUP_NVS_KEY, &ucVal, sizeof(ucVal));

    if (ESP_OK == err)
    {
        err = NVS_Commit(hNvs);
    }

    NVS_Close(hNvs);

    if (ESP_OK == err)
    {
        s_bSetupComplete = true;
        ESP_LOGI(TAG, "Setup marked as complete");
    }

    return err;
}

esp_err_t
TS_Setup_Reset(void)
{
    nvs_handle_t hNvs;
    esp_err_t err = NVS_Open(SETUP_NVS_NAMESPACE, NVS_READWRITE, &hNvs);

    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t ucVal = 0;
    err = NVS_Write(hNvs, SETUP_NVS_KEY, &ucVal, sizeof(ucVal));

    if (ESP_OK == err)
    {
        err = NVS_Commit(hNvs);
    }

    NVS_Close(hNvs);

    if (ESP_OK == err)
    {
        s_bSetupComplete = false;
        ESP_LOGI(TAG, "Setup flag reset — wizard will show on next boot");
    }

    return err;
}

bool
TS_Setup_CheckMigration(void)
{
    if (s_bSetupComplete)
    {
        return false;   /* Already complete, no migration needed */
    }

    /* Check if PIN is already configured (not default) */
    bool bPinConfigured = TS_Pin_IsConfigured();

    if (bPinConfigured)
    {
        /* Existing device with explicit PIN — auto-mark as complete */
        ESP_LOGI(TAG, "Migration: PIN is configured — auto-completing setup");
        TS_Setup_MarkComplete();
        return true;
    }

    return false;
}
