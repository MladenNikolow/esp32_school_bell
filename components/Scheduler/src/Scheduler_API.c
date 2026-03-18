#include "Scheduler_API.h"
#include "Schedule_Data.h"
#include "TimeSync_API.h"
#include "RingBell_API.h"
#include "SPIFFS_API.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char* TAG = "scheduler";

#define SCHEDULER_TASK_STACK_SIZE   8192
#define SCHEDULER_TASK_PRIORITY     2
#define SCHEDULER_CHECK_INTERVAL_MS 1000

/* ------------------------------------------------------------------ */
/* Resource struct                                                     */
/* ------------------------------------------------------------------ */

typedef struct _SCHEDULER_RSC_T
{
    TaskHandle_t        hTask;
    SemaphoreHandle_t   hMutex;
    SCHEDULE_DATA_T*    ptData;
    
    /* Runtime state */
    bool                bRunning;
    int                 iLastFiredDay;       /* tm_yday of last reset */
    uint8_t             abFiredBitmap[SCHEDULE_MAX_BELLS / 8 + 1];
    DAY_TYPE_E          eCachedDayType;
    int                 iCachedDayYday;     /* tm_yday when day type was cached */
} SCHEDULER_RSC_T;

/* ------------------------------------------------------------------ */
/* Date helpers                                                        */
/* ------------------------------------------------------------------ */

/** Convert "YYYY-MM-DD" to days-since-epoch for comparison */
static int
dateStrToOrdinal(const char* pcDate)
{
    int iYear = 0, iMonth = 0, iDay = 0;
    if (sscanf(pcDate, "%d-%d-%d", &iYear, &iMonth, &iDay) != 3)
    {
        return -1;
    }

    struct tm tTm = { 0 };
    tTm.tm_year = iYear - 1900;
    tTm.tm_mon  = iMonth - 1;
    tTm.tm_mday = iDay;
    time_t t = mktime(&tTm);
    return (int)(t / 86400);
}

/** Format struct tm as "YYYY-MM-DD" */
static void
tmToDateStr(const struct tm* ptTm, char* pcOut, size_t ulLen)
{
    snprintf(pcOut, ulLen, "%04d-%02d-%02d",
             ptTm->tm_year + 1900, ptTm->tm_mon + 1, ptTm->tm_mday);
}

/* ------------------------------------------------------------------ */
/* Day type determination                                              */
/* ------------------------------------------------------------------ */

/** Check if today falls within an exception's date range (or exact date) */
static bool
exceptionMatchesDate(const EXCEPTION_ENTRY_T* ptEx, const char* pcToday, int iTodayOrd)
{
    if (ptEx->acEndDate[0] != '\0')
    {
        /* Date range: startDate <= today <= endDate */
        int iStart = dateStrToOrdinal(ptEx->acStartDate);
        int iEnd   = dateStrToOrdinal(ptEx->acEndDate);
        return (iStart >= 0 && iEnd >= 0 && iTodayOrd >= iStart && iTodayOrd <= iEnd);
    }
    /* Single day: exact match */
    return strcmp(pcToday, ptEx->acStartDate) == 0;
}

static DAY_TYPE_E
scheduler_DetermineDayType(const SCHEDULER_RSC_T* ptRsc, const struct tm* ptNow)
{
    char acToday[SCHEDULE_DATE_STR_LEN];
    tmToDateStr(ptNow, acToday, sizeof(acToday));
    int iTodayOrd = dateStrToOrdinal(acToday);

    const SCHEDULE_DATA_T* ptData = ptRsc->ptData;

    /* Priority 1 & 2: Unified exceptions — first match wins.
     * DAY_OFF action → EXCEPTION_HOLIDAY, all others → EXCEPTION_WORKING */
    for (uint32_t i = 0; i < ptData->ulExceptionCount; i++)
    {
        if (exceptionMatchesDate(&ptData->atExceptions[i], acToday, iTodayOrd))
        {
            if (ptData->atExceptions[i].eAction == EXCEPTION_ACTION_DAY_OFF)
            {
                ESP_LOGI(TAG, "Today is exception day-off: %s", ptData->atExceptions[i].acLabel);
                return DAY_TYPE_EXCEPTION_HOLIDAY;
            }
            else
            {
                ESP_LOGI(TAG, "Today is exception working: %s", ptData->atExceptions[i].acLabel);
                return DAY_TYPE_EXCEPTION_WORKING;
            }
        }
    }

    /* Priority 3: Holiday range → OFF */
    for (uint32_t i = 0; i < ptData->ulHolidayCount; i++)
    {
        int iStart = dateStrToOrdinal(ptData->atHolidays[i].acStartDate);
        int iEnd   = dateStrToOrdinal(ptData->atHolidays[i].acEndDate);
        if (iStart >= 0 && iEnd >= 0 && iTodayOrd >= iStart && iTodayOrd <= iEnd)
        {
            ESP_LOGI(TAG, "Today is in holiday range: %s", ptData->atHolidays[i].acLabel);
            return DAY_TYPE_HOLIDAY;
        }
    }

    /* Priority 4: Working day check */
    int iWday = ptNow->tm_wday; /* 0=Sun..6=Sat */
    if (ptData->tSettings.abWorkingDays[iWday])
    {
        return DAY_TYPE_WORKING;
    }

    return DAY_TYPE_OFF;
}

/* ------------------------------------------------------------------ */
/* Get active bells for today                                          */
/* ------------------------------------------------------------------ */

/**
 * Build a merged flat array of enabled bells from both shifts.
 * Returns the count written to ptOut (up to ulMaxOut).
 */
static uint32_t
scheduler_MergeBells(const SCHEDULE_DATA_T* ptData, BELL_ENTRY_T* ptOut, uint32_t ulMaxOut)
{
    uint32_t ulTotal = 0;

    if (ptData->tFirstShift.bEnabled)
    {
        for (uint32_t i = 0; i < ptData->tFirstShift.ulBellCount && ulTotal < ulMaxOut; i++)
        {
            ptOut[ulTotal++] = ptData->tFirstShift.atBells[i];
        }
    }

    if (ptData->tSecondShift.bEnabled)
    {
        for (uint32_t i = 0; i < ptData->tSecondShift.ulBellCount && ulTotal < ulMaxOut; i++)
        {
            ptOut[ulTotal++] = ptData->tSecondShift.atBells[i];
        }
    }

    return ulTotal;
}

/** Build bells for first shift only */
static uint32_t
scheduler_FirstShiftBells(const SCHEDULE_DATA_T* ptData, BELL_ENTRY_T* ptOut, uint32_t ulMaxOut)
{
    uint32_t ulTotal = 0;
    for (uint32_t i = 0; i < ptData->tFirstShift.ulBellCount && ulTotal < ulMaxOut; i++)
    {
        ptOut[ulTotal++] = ptData->tFirstShift.atBells[i];
    }
    return ulTotal;
}

/** Build bells for second shift only */
static uint32_t
scheduler_SecondShiftBells(const SCHEDULE_DATA_T* ptData, BELL_ENTRY_T* ptOut, uint32_t ulMaxOut)
{
    uint32_t ulTotal = 0;
    for (uint32_t i = 0; i < ptData->tSecondShift.ulBellCount && ulTotal < ulMaxOut; i++)
    {
        ptOut[ulTotal++] = ptData->tSecondShift.atBells[i];
    }
    return ulTotal;
}

/** Apply time offset (minutes) to all bells in array */
static void
scheduler_ApplyTimeOffset(BELL_ENTRY_T* ptBells, uint32_t ulCount, int8_t iOffsetMin)
{
    if (iOffsetMin == 0) return;

    for (uint32_t i = 0; i < ulCount; i++)
    {
        int iMinutes = ptBells[i].ucHour * 60 + ptBells[i].ucMinute + iOffsetMin;

        /* Clamp to 00:00 – 23:59 */
        if (iMinutes < 0) iMinutes = 0;
        if (iMinutes >= 24 * 60) iMinutes = 24 * 60 - 1;

        ptBells[i].ucHour   = (uint8_t)(iMinutes / 60);
        ptBells[i].ucMinute = (uint8_t)(iMinutes % 60);
    }
}

/**
 * Resolve bells for an exception working day.
 * Returns the bell count written to ptOut.
 */
static uint32_t
scheduler_ResolveExceptionBells(const SCHEDULE_DATA_T* ptData,
                                 const EXCEPTION_ENTRY_T* ptEx,
                                 BELL_ENTRY_T* ptOut, uint32_t ulMaxOut)
{
    uint32_t ulCount = 0;

    switch (ptEx->eAction)
    {
        case EXCEPTION_ACTION_NORMAL:
            ulCount = scheduler_MergeBells(ptData, ptOut, ulMaxOut);
            break;

        case EXCEPTION_ACTION_FIRST_SHIFT:
            ulCount = scheduler_FirstShiftBells(ptData, ptOut, ulMaxOut);
            break;

        case EXCEPTION_ACTION_SECOND_SHIFT:
            ulCount = scheduler_SecondShiftBells(ptData, ptOut, ulMaxOut);
            break;

        case EXCEPTION_ACTION_TEMPLATE:
            if (ptEx->ucTemplateIdx < ptData->ulTemplateCount)
            {
                const BELL_TEMPLATE_T* ptTpl = &ptData->atTemplates[ptEx->ucTemplateIdx];
                ulCount = ptTpl->ucBellCount;
                if (ulCount > ulMaxOut) ulCount = ulMaxOut;
                memcpy(ptOut, ptTpl->atBells, ulCount * sizeof(BELL_ENTRY_T));
            }
            else
            {
                /* Fallback: use normal bells if template index invalid */
                ulCount = scheduler_MergeBells(ptData, ptOut, ulMaxOut);
            }
            break;

        case EXCEPTION_ACTION_CUSTOM:
            if (ptEx->ucCustomBellsIdx < ptData->ulCustomBellSetCount)
            {
                const EXCEPTION_CUSTOM_BELLS_T* ptSet = &ptData->atCustomBellSets[ptEx->ucCustomBellsIdx];
                ulCount = ptSet->ucBellCount;
                if (ulCount > ulMaxOut) ulCount = ulMaxOut;
                memcpy(ptOut, ptSet->atBells, ulCount * sizeof(BELL_ENTRY_T));
            }
            break;

        default: /* DAY_OFF — should not reach here */
            break;
    }

    /* Apply time offset */
    scheduler_ApplyTimeOffset(ptOut, ulCount, ptEx->iTimeOffsetMin);

    return ulCount;
}

/* ------------------------------------------------------------------ */
/* Find next bell                                                      */
/* ------------------------------------------------------------------ */

static void
scheduler_FindNextBell(const SCHEDULER_RSC_T* ptRsc, const struct tm* ptNow,
                       NEXT_BELL_INFO_T* ptOut)
{
    ptOut->bValid = false;

    if (ptRsc->eCachedDayType == DAY_TYPE_OFF ||
        ptRsc->eCachedDayType == DAY_TYPE_HOLIDAY ||
        ptRsc->eCachedDayType == DAY_TYPE_EXCEPTION_HOLIDAY)
    {
        return;
    }

    const SCHEDULE_DATA_T* ptData = ptRsc->ptData;
    BELL_ENTRY_T atMerged[SCHEDULE_MAX_BELLS];
    uint32_t ulCount = 0;

    if (ptRsc->eCachedDayType == DAY_TYPE_EXCEPTION_WORKING)
    {
        char acToday[SCHEDULE_DATE_STR_LEN];
        tmToDateStr(ptNow, acToday, sizeof(acToday));
        int iTodayOrd = dateStrToOrdinal(acToday);

        for (uint32_t i = 0; i < ptData->ulExceptionCount; i++)
        {
            if (exceptionMatchesDate(&ptData->atExceptions[i], acToday, iTodayOrd) &&
                ptData->atExceptions[i].eAction != EXCEPTION_ACTION_DAY_OFF)
            {
                ulCount = scheduler_ResolveExceptionBells(ptData, &ptData->atExceptions[i],
                                                           atMerged, SCHEDULE_MAX_BELLS);
                goto search;
            }
        }
    }

    ulCount = scheduler_MergeBells(ptData, atMerged, SCHEDULE_MAX_BELLS);

search:
    {
        int iNowMinutes = ptNow->tm_hour * 60 + ptNow->tm_min;
        int iBestMinutes = 24 * 60 + 1;

        for (uint32_t i = 0; i < ulCount; i++)
        {
            int iBellMinutes = atMerged[i].ucHour * 60 + atMerged[i].ucMinute;
            if (iBellMinutes > iNowMinutes && iBellMinutes < iBestMinutes)
            {
                iBestMinutes = iBellMinutes;
                ptOut->bValid       = true;
                ptOut->ucHour       = atMerged[i].ucHour;
                ptOut->ucMinute     = atMerged[i].ucMinute;
                ptOut->usDurationSec = atMerged[i].usDurationSec;
                strncpy(ptOut->acLabel, atMerged[i].acLabel, SCHEDULE_LABEL_MAX_LEN - 1);
                ptOut->acLabel[SCHEDULE_LABEL_MAX_LEN - 1] = '\0';
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Mark / check fired bells                                            */
/* ------------------------------------------------------------------ */

static bool
scheduler_IsBellFired(const SCHEDULER_RSC_T* ptRsc, uint32_t ulIndex)
{
    return (ptRsc->abFiredBitmap[ulIndex / 8] & (1 << (ulIndex % 8))) != 0;
}

static void
scheduler_MarkBellFired(SCHEDULER_RSC_T* ptRsc, uint32_t ulIndex)
{
    ptRsc->abFiredBitmap[ulIndex / 8] |= (1 << (ulIndex % 8));
}

/* ------------------------------------------------------------------ */
/* Background task                                                     */
/* ------------------------------------------------------------------ */

static void
scheduler_Task(void* pvArg)
{
    SCHEDULER_RSC_T* ptRsc = (SCHEDULER_RSC_T*)pvArg;
    ptRsc->bRunning = true;

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(SCHEDULER_CHECK_INTERVAL_MS));

        /* Skip if time not synced */
        if (!TimeSync_IsSynced()) continue;

        /* Skip if panic mode — bell is already on continuously */
        if (RingBell_IsPanic()) continue;

        struct tm tNow;
        TimeSync_GetLocalTime(&tNow);

        /* Defense-in-depth: reject obviously invalid system time */
        if (tNow.tm_year < 124) continue;  /* year < 2024 */

        xSemaphoreTake(ptRsc->hMutex, portMAX_DELAY);

        /* Reset fired bitmap at midnight */
        if (tNow.tm_yday != ptRsc->iLastFiredDay)
        {
            memset(ptRsc->abFiredBitmap, 0, sizeof(ptRsc->abFiredBitmap));
            ptRsc->iLastFiredDay = tNow.tm_yday;
            ptRsc->iCachedDayYday = -1; /* force recalculation */

            /* Clean up expired exceptions (date before today) */
            xSemaphoreGive(ptRsc->hMutex);
            Schedule_Data_CleanupExpiredExceptions();
            /* Reload to pick up cleaned data */
            Scheduler_ReloadSchedule(ptRsc);
            continue; /* re-enter loop with fresh data */
        }

        /* Cache day type (once per day) */
        if (tNow.tm_yday != ptRsc->iCachedDayYday)
        {
            ptRsc->eCachedDayType = scheduler_DetermineDayType(ptRsc, &tNow);
            ptRsc->iCachedDayYday = tNow.tm_yday;
            ESP_LOGI(TAG, "Day type for today: %d", ptRsc->eCachedDayType);
        }

        /* Only fire on working days */
        if (ptRsc->eCachedDayType == DAY_TYPE_WORKING ||
            ptRsc->eCachedDayType == DAY_TYPE_EXCEPTION_WORKING)
        {
            BELL_ENTRY_T atMerged[SCHEDULE_MAX_BELLS];
            uint32_t ulCount = 0;

            /* Exception working: resolve bells based on action */
            bool bResolved = false;
            if (ptRsc->eCachedDayType == DAY_TYPE_EXCEPTION_WORKING)
            {
                char acToday[SCHEDULE_DATE_STR_LEN];
                tmToDateStr(&tNow, acToday, sizeof(acToday));
                int iTodayOrd = dateStrToOrdinal(acToday);

                for (uint32_t i = 0; i < ptRsc->ptData->ulExceptionCount; i++)
                {
                    if (exceptionMatchesDate(&ptRsc->ptData->atExceptions[i], acToday, iTodayOrd) &&
                        ptRsc->ptData->atExceptions[i].eAction != EXCEPTION_ACTION_DAY_OFF)
                    {
                        ulCount = scheduler_ResolveExceptionBells(ptRsc->ptData,
                                    &ptRsc->ptData->atExceptions[i], atMerged, SCHEDULE_MAX_BELLS);
                        bResolved = true;
                        break;
                    }
                }
            }

            if (!bResolved)
            {
                ulCount = scheduler_MergeBells(ptRsc->ptData, atMerged, SCHEDULE_MAX_BELLS);
            }

            for (uint32_t i = 0; i < ulCount; i++)
            {
                if (tNow.tm_hour == atMerged[i].ucHour &&
                    tNow.tm_min  == atMerged[i].ucMinute &&
                    !scheduler_IsBellFired(ptRsc, i))
                {
                    ESP_LOGI(TAG, "Firing bell: %02d:%02d [%s] for %d sec",
                             atMerged[i].ucHour, atMerged[i].ucMinute,
                             atMerged[i].acLabel, atMerged[i].usDurationSec);

                    RingBell_RunForDuration(atMerged[i].usDurationSec);
                    scheduler_MarkBellFired(ptRsc, i);
                }
            }
        }

        xSemaphoreGive(ptRsc->hMutex);
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t
Scheduler_Init(SCHEDULER_H* phScheduler)
{
    if (NULL == phScheduler) return ESP_ERR_INVALID_ARG;

    SCHEDULER_RSC_T* ptRsc = (SCHEDULER_RSC_T*)calloc(1, sizeof(SCHEDULER_RSC_T));
    if (NULL == ptRsc) return ESP_ERR_NO_MEM;

    ptRsc->ptData = (SCHEDULE_DATA_T*)calloc(1, sizeof(SCHEDULE_DATA_T));
    if (NULL == ptRsc->ptData)
    {
        free(ptRsc);
        return ESP_ERR_NO_MEM;
    }

    ptRsc->hMutex = xSemaphoreCreateMutex();
    if (NULL == ptRsc->hMutex)
    {
        free(ptRsc->ptData);
        free(ptRsc);
        return ESP_FAIL;
    }

    ptRsc->iLastFiredDay  = -1;
    ptRsc->iCachedDayYday = -1;

    /* Create defaults if needed */
    Schedule_Data_CreateDefaults();

    /* Load schedule data */
    Schedule_Data_LoadSettings(&ptRsc->ptData->tSettings);
    Schedule_Data_LoadBells(&ptRsc->ptData->tFirstShift, &ptRsc->ptData->tSecondShift);
    Schedule_Data_LoadCalendar(ptRsc->ptData);
    Schedule_Data_LoadTemplates(ptRsc->ptData);

    ESP_LOGI(TAG, "Loaded schedule: 1st(%s,%"PRIu32") 2nd(%s,%"PRIu32") %"PRIu32" holidays, %"PRIu32" exceptions, %"PRIu32" templates",
             ptRsc->ptData->tFirstShift.bEnabled ? "on" : "off",
             ptRsc->ptData->tFirstShift.ulBellCount,
             ptRsc->ptData->tSecondShift.bEnabled ? "on" : "off",
             ptRsc->ptData->tSecondShift.ulBellCount,
             ptRsc->ptData->ulHolidayCount,
             ptRsc->ptData->ulExceptionCount,
             ptRsc->ptData->ulTemplateCount);

    /* Create background task */
    BaseType_t xResult = xTaskCreate(scheduler_Task, "SCHEDULER",
                                     SCHEDULER_TASK_STACK_SIZE,
                                     ptRsc,
                                     SCHEDULER_TASK_PRIORITY,
                                     &ptRsc->hTask);

    if (pdPASS != xResult)
    {
        free(ptRsc->ptData);
        vSemaphoreDelete(ptRsc->hMutex);
        free(ptRsc);
        return ESP_FAIL;
    }

    *phScheduler = ptRsc;
    ESP_LOGI(TAG, "Scheduler initialised");
    return ESP_OK;
}

esp_err_t
Scheduler_ReloadSchedule(SCHEDULER_H hScheduler)
{
    if (NULL == hScheduler) return ESP_ERR_INVALID_ARG;
    SCHEDULER_RSC_T* ptRsc = (SCHEDULER_RSC_T*)hScheduler;

    xSemaphoreTake(ptRsc->hMutex, portMAX_DELAY);

    Schedule_Data_LoadSettings(&ptRsc->ptData->tSettings);
    Schedule_Data_LoadBells(&ptRsc->ptData->tFirstShift, &ptRsc->ptData->tSecondShift);
    Schedule_Data_LoadCalendar(ptRsc->ptData);
    Schedule_Data_LoadTemplates(ptRsc->ptData);

    /* Reset cached day type so it recalculates */
    ptRsc->iCachedDayYday = -1;
    /* Reset fired bitmap to re-evaluate today's bells */
    memset(ptRsc->abFiredBitmap, 0, sizeof(ptRsc->abFiredBitmap));

    xSemaphoreGive(ptRsc->hMutex);

    ESP_LOGI(TAG, "Schedule reloaded");
    return ESP_OK;
}

esp_err_t
Scheduler_GetNextBell(SCHEDULER_H hScheduler, NEXT_BELL_INFO_T* ptInfo)
{
    if ((NULL == hScheduler) || (NULL == ptInfo)) return ESP_ERR_INVALID_ARG;
    SCHEDULER_RSC_T* ptRsc = (SCHEDULER_RSC_T*)hScheduler;

    struct tm tNow;
    TimeSync_GetLocalTime(&tNow);

    xSemaphoreTake(ptRsc->hMutex, portMAX_DELAY);
    scheduler_FindNextBell(ptRsc, &tNow, ptInfo);
    xSemaphoreGive(ptRsc->hMutex);

    return ESP_OK;
}

esp_err_t
Scheduler_GetStatus(SCHEDULER_H hScheduler, SCHEDULER_STATUS_T* ptStatus)
{
    if ((NULL == hScheduler) || (NULL == ptStatus)) return ESP_ERR_INVALID_ARG;
    SCHEDULER_RSC_T* ptRsc = (SCHEDULER_RSC_T*)hScheduler;

    memset(ptStatus, 0, sizeof(SCHEDULER_STATUS_T));
    ptStatus->bRunning         = ptRsc->bRunning;
    ptStatus->bTimeSynced      = TimeSync_IsSynced();
    ptStatus->ulLastSyncAgeSec = TimeSync_GetLastSyncAgeSec();

    TimeSync_GetLocalTime(&ptStatus->tCurrentTime);

    xSemaphoreTake(ptRsc->hMutex, portMAX_DELAY);
    ptStatus->eDayType = ptRsc->eCachedDayType;
    scheduler_FindNextBell(ptRsc, &ptStatus->tCurrentTime, &ptStatus->tNextBell);
    xSemaphoreGive(ptRsc->hMutex);

    return ESP_OK;
}
