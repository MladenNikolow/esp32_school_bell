/* ================================================================== */
/* ts_schedule_service.c — Read today's schedule from Scheduler        */
/* ================================================================== */
#include "TouchScreen_Services.h"
#include "Schedule_Data.h"
#include "esp_log.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

static const char *TAG = "TS_SCHEDULE";

/* Module state — holds the Scheduler handle */
static SCHEDULER_H s_hScheduler = NULL;

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t
TS_Schedule_Init(SCHEDULER_H hScheduler)
{
    if (hScheduler == NULL)
    {
        ESP_LOGE(TAG, "Invalid scheduler handle");
        return ESP_ERR_INVALID_ARG;
    }
    s_hScheduler = hScheduler;
    ESP_LOGI(TAG, "Schedule service initialized");
    return ESP_OK;
}

esp_err_t
TS_Schedule_GetStatus(SCHEDULER_STATUS_T *ptStatus)
{
    if (ptStatus == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_hScheduler == NULL)
    {
        ESP_LOGE(TAG, "Schedule service not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    return Scheduler_GetStatus(s_hScheduler, ptStatus);
}

esp_err_t
TS_Schedule_GetNextBell(NEXT_BELL_INFO_T *ptInfo)
{
    if (ptInfo == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_hScheduler == NULL)
    {
        ESP_LOGE(TAG, "Schedule service not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    return Scheduler_GetNextBell(s_hScheduler, ptInfo);
}

esp_err_t
TS_Schedule_GetShiftBells(uint8_t ucShift, BELL_ENTRY_T *ptBells,
                           uint32_t *pulCount, bool *pbEnabled)
{
    if (ptBells == NULL || pulCount == NULL || pbEnabled == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (ucShift > 1)
    {
        ESP_LOGE(TAG, "Invalid shift index: %u (must be 0 or 1)", ucShift);
        return ESP_ERR_INVALID_ARG;
    }

    /* Heap-allocate shifts to avoid ~5KB stack usage that causes overflow
       when called from LVGL event callbacks with limited stack depth */
    SCHEDULE_SHIFT_T *ptFirst  = calloc(1, sizeof(SCHEDULE_SHIFT_T));
    SCHEDULE_SHIFT_T *ptSecond = calloc(1, sizeof(SCHEDULE_SHIFT_T));
    if (ptFirst == NULL || ptSecond == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate shift buffers");
        free(ptFirst);
        free(ptSecond);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = Schedule_Data_LoadBells(ptFirst, ptSecond);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "Failed to load bell data: %s", esp_err_to_name(err));
        free(ptFirst);
        free(ptSecond);
        return err;
    }

    const SCHEDULE_SHIFT_T *ptShift = (ucShift == 0) ? ptFirst : ptSecond;

    *pbEnabled = ptShift->bEnabled;
    *pulCount  = ptShift->ulBellCount;

    if (ptShift->ulBellCount > 0)
    {
        memcpy(ptBells, ptShift->atBells,
               ptShift->ulBellCount * sizeof(BELL_ENTRY_T));
    }

    free(ptFirst);
    free(ptSecond);
    return ESP_OK;
}

esp_err_t
TS_Schedule_GetSettings(SCHEDULE_SETTINGS_T *ptSettings)
{
    if (ptSettings == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    return Schedule_Data_LoadSettings(ptSettings);
}

/* ------------------------------------------------------------------ */
/* Day Override: add/replace a temporary single-day exception           */
/* ------------------------------------------------------------------ */
esp_err_t
TS_Schedule_SetTodayOverride(EXCEPTION_ACTION_E eAction)
{
    if (s_hScheduler == NULL)
    {
        ESP_LOGE(TAG, "Schedule service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Only allow day-off or normal (day-on with default schedule) */
    if (eAction != EXCEPTION_ACTION_DAY_OFF && eAction != EXCEPTION_ACTION_NORMAL)
    {
        ESP_LOGE(TAG, "Invalid override action: %d", eAction);
        return ESP_ERR_INVALID_ARG;
    }

    /* Get today's date string */
    time_t now;
    time(&now);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    char today_str[SCHEDULE_DATE_STR_LEN];
    snprintf(today_str, sizeof(today_str), "%04d-%02d-%02d",
             tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);

    ESP_LOGI(TAG, "Setting today override: date=%s action=%d", today_str, eAction);

    /* Load current calendar data (heap-allocated due to large size) */
    SCHEDULE_DATA_T *ptData = calloc(1, sizeof(SCHEDULE_DATA_T));
    if (ptData == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate calendar buffer");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = Schedule_Data_LoadCalendar(ptData);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to load calendar: %s", esp_err_to_name(err));
        free(ptData);
        return err;
    }

    /* Scan existing exceptions: remove any matching today's date */
    for (uint32_t i = 0; i < ptData->ulExceptionCount; /* no increment */)
    {
        EXCEPTION_ENTRY_T *pEx = &ptData->atExceptions[i];

        /* Match single-day exceptions for today */
        bool is_today = (strcmp(pEx->acStartDate, today_str) == 0)
                        && (pEx->acEndDate[0] == '\0'
                            || strcmp(pEx->acEndDate, today_str) == 0);

        if (is_today)
        {
            ESP_LOGI(TAG, "Removing existing today exception at index %" PRIu32, i);
            /* Shift remaining entries down */
            uint32_t remaining = ptData->ulExceptionCount - i - 1;
            if (remaining > 0)
            {
                memmove(&ptData->atExceptions[i],
                        &ptData->atExceptions[i + 1],
                        remaining * sizeof(EXCEPTION_ENTRY_T));
            }
            ptData->ulExceptionCount--;
            /* Don't increment i — check the entry that slid into this slot */
        }
        else
        {
            i++;
        }
    }

    /* Append new single-day exception for today */
    if (ptData->ulExceptionCount >= SCHEDULE_MAX_EXCEPTIONS)
    {
        ESP_LOGE(TAG, "Exception list full (%d), cannot add override",
                 SCHEDULE_MAX_EXCEPTIONS);
        free(ptData);
        return ESP_ERR_NO_MEM;
    }

    EXCEPTION_ENTRY_T *pNew = &ptData->atExceptions[ptData->ulExceptionCount];
    memset(pNew, 0, sizeof(EXCEPTION_ENTRY_T));
    strncpy(pNew->acStartDate, today_str, SCHEDULE_DATE_STR_LEN - 1);
    pNew->acEndDate[0] = '\0';   /* Single day */
    strncpy(pNew->acLabel, "Manual Override", SCHEDULE_LABEL_MAX_LEN - 1);
    pNew->eAction          = eAction;
    pNew->iTimeOffsetMin   = 0;
    pNew->ucTemplateIdx    = 0;
    pNew->ucCustomBellsIdx = 0xFF;
    ptData->ulExceptionCount++;

    /* Save back to SPIFFS */
    err = Schedule_Data_SaveCalendar(ptData);
    free(ptData);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save calendar: %s", esp_err_to_name(err));
        return err;
    }

    /* Reload schedule so the change takes effect immediately */
    err = Scheduler_ReloadSchedule(s_hScheduler);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Schedule reload failed: %s (override saved but not active yet)",
                 esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Today override set successfully: %s = %s",
             today_str, (eAction == EXCEPTION_ACTION_DAY_OFF) ? "Day Off" : "Day On");
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Cancel today's manual override (remove the exception entirely)      */
/* ------------------------------------------------------------------ */
esp_err_t
TS_Schedule_CancelTodayOverride(void)
{
    if (s_hScheduler == NULL)
    {
        ESP_LOGE(TAG, "Schedule service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    time_t now;
    time(&now);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    char today_str[SCHEDULE_DATE_STR_LEN];
    snprintf(today_str, sizeof(today_str), "%04d-%02d-%02d",
             tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);

    ESP_LOGI(TAG, "Cancelling today override for %s", today_str);

    SCHEDULE_DATA_T *ptData = calloc(1, sizeof(SCHEDULE_DATA_T));
    if (ptData == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate calendar buffer");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = Schedule_Data_LoadCalendar(ptData);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to load calendar: %s", esp_err_to_name(err));
        free(ptData);
        return err;
    }

    bool bFound = false;
    for (uint32_t i = 0; i < ptData->ulExceptionCount; /* no increment */)
    {
        EXCEPTION_ENTRY_T *pEx = &ptData->atExceptions[i];
        bool is_today = (strcmp(pEx->acStartDate, today_str) == 0)
                        && (pEx->acEndDate[0] == '\0'
                            || strcmp(pEx->acEndDate, today_str) == 0)
                        && (strcmp(pEx->acLabel, "Manual Override") == 0);

        if (is_today)
        {
            ESP_LOGI(TAG, "Removing manual override at index %" PRIu32, i);
            uint32_t remaining = ptData->ulExceptionCount - i - 1;
            if (remaining > 0)
            {
                memmove(&ptData->atExceptions[i],
                        &ptData->atExceptions[i + 1],
                        remaining * sizeof(EXCEPTION_ENTRY_T));
            }
            ptData->ulExceptionCount--;
            bFound = true;
        }
        else
        {
            i++;
        }
    }

    if (!bFound)
    {
        ESP_LOGI(TAG, "No manual override found for today");
        free(ptData);
        return ESP_OK;
    }

    err = Schedule_Data_SaveCalendar(ptData);
    free(ptData);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save calendar: %s", esp_err_to_name(err));
        return err;
    }

    err = Scheduler_ReloadSchedule(s_hScheduler);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Schedule reload failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Today manual override cancelled");
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Query today's manual override action (-1 if none)                   */
/* ------------------------------------------------------------------ */
int
TS_Schedule_GetTodayOverrideAction(void)
{
    SCHEDULE_DATA_T *ptData = calloc(1, sizeof(SCHEDULE_DATA_T));
    if (ptData == NULL)
    {
        return -1;
    }

    if (Schedule_Data_LoadCalendar(ptData) != ESP_OK)
    {
        free(ptData);
        return -1;
    }

    time_t now;
    time(&now);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    char today_str[SCHEDULE_DATE_STR_LEN];
    snprintf(today_str, sizeof(today_str), "%04d-%02d-%02d",
             tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);

    int result = -1;
    for (uint32_t i = 0; i < ptData->ulExceptionCount; i++)
    {
        EXCEPTION_ENTRY_T *pEx = &ptData->atExceptions[i];
        bool is_today = (strcmp(pEx->acStartDate, today_str) == 0)
                        && (pEx->acEndDate[0] == '\0'
                            || strcmp(pEx->acEndDate, today_str) == 0)
                        && (strcmp(pEx->acLabel, "Manual Override") == 0);
        if (is_today)
        {
            result = (int)pEx->eAction;
            break;
        }
    }

    free(ptData);
    return result;
}
