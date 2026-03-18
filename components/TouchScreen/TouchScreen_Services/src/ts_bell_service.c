/* ================================================================== */
/* ts_bell_service.c — Bell state, panic, test ring                    */
/* ================================================================== */
#include "TouchScreen_Services.h"
#include "esp_log.h"

static const char *TAG = "TS_BELL";

#define TEST_BELL_DEFAULT_DURATION_SEC  3
#define TEST_BELL_MAX_DURATION_SEC      30

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

BELL_STATE_E
TS_Bell_GetState(void)
{
    return RingBell_GetState();
}

bool
TS_Bell_IsPanic(void)
{
    return RingBell_IsPanic();
}

esp_err_t
TS_Bell_SetPanic(bool bEnable)
{
    ESP_LOGI(TAG, "Setting panic mode: %s", bEnable ? "ON" : "OFF");
    return RingBell_SetPanic(bEnable);
}

esp_err_t
TS_Bell_TestRing(uint32_t ulDurationSec)
{
    if (ulDurationSec == 0)
    {
        ulDurationSec = TEST_BELL_DEFAULT_DURATION_SEC;
    }
    if (ulDurationSec > TEST_BELL_MAX_DURATION_SEC)
    {
        ulDurationSec = TEST_BELL_MAX_DURATION_SEC;
    }

    /* Don't test-ring if already in panic mode */
    if (RingBell_IsPanic())
    {
        ESP_LOGW(TAG, "Cannot test ring while in panic mode");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Test ring for %lu seconds", (unsigned long)ulDurationSec);
    return RingBell_RunForDuration(ulDurationSec);
}
