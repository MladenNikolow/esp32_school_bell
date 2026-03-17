#pragma once

#include "esp_err.h"
#include "Schedule_Data.h"
#include <stdbool.h>
#include <time.h>

typedef struct _SCHEDULER_RSC_T* SCHEDULER_H;

typedef enum
{
    DAY_TYPE_OFF               = 0,
    DAY_TYPE_WORKING           = 1,
    DAY_TYPE_HOLIDAY           = 2,
    DAY_TYPE_EXCEPTION_WORKING = 3,
    DAY_TYPE_EXCEPTION_HOLIDAY = 4
} DAY_TYPE_E;

typedef struct
{
    bool        bValid;
    uint8_t     ucHour;
    uint8_t     ucMinute;
    uint16_t    usDurationSec;
    char        acLabel[SCHEDULE_LABEL_MAX_LEN];
} NEXT_BELL_INFO_T;

typedef struct
{
    bool            bRunning;
    bool            bTimeSynced;
    DAY_TYPE_E      eDayType;
    NEXT_BELL_INFO_T tNextBell;
    struct tm       tCurrentTime;
} SCHEDULER_STATUS_T;

/**
 * @brief Initialize scheduler: load data, create background task.
 */
esp_err_t Scheduler_Init(SCHEDULER_H* phScheduler);

/**
 * @brief Reload schedule data from SPIFFS (call after API writes).
 */
esp_err_t Scheduler_ReloadSchedule(SCHEDULER_H hScheduler);

/**
 * @brief Get info about next scheduled bell.
 */
esp_err_t Scheduler_GetNextBell(SCHEDULER_H hScheduler, NEXT_BELL_INFO_T* ptInfo);

/**
 * @brief Get full scheduler status.
 */
esp_err_t Scheduler_GetStatus(SCHEDULER_H hScheduler, SCHEDULER_STATUS_T* ptStatus);
