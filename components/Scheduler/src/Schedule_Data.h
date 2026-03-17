#pragma once

#include "esp_err.h"
#include "cJSON.h"
#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Limits                                                              */
/* ------------------------------------------------------------------ */
#define SCHEDULE_MAX_BELLS              100
#define SCHEDULE_MAX_BELLS_PER_SHIFT    50
#define SCHEDULE_MAX_HOLIDAYS           50
#define SCHEDULE_MAX_EXCEPTION_WORKING  30
#define SCHEDULE_MAX_EXCEPTION_HOLIDAY  30
#define SCHEDULE_LABEL_MAX_LEN          48
#define SCHEDULE_DATE_STR_LEN           11  /* "YYYY-MM-DD\0" */

/* File paths */
#define SCHEDULE_FILE_SETTINGS          "/storage/settings.json"
#define SCHEDULE_FILE_BELLS             "/storage/schedule.json"
#define SCHEDULE_FILE_CALENDAR          "/storage/calendar.json"
#define SCHEDULE_FILE_DEFAULTS          "/react/default_schedule.json"

/* ------------------------------------------------------------------ */
/* Data structures                                                     */
/* ------------------------------------------------------------------ */

typedef struct
{
    uint8_t  ucHour;        /* 0-23 */
    uint8_t  ucMinute;      /* 0-59 */
    uint16_t usDurationSec; /* bell ring duration in seconds */
    char     acLabel[SCHEDULE_LABEL_MAX_LEN];
} BELL_ENTRY_T;

typedef struct
{
    char acStartDate[SCHEDULE_DATE_STR_LEN]; /* "YYYY-MM-DD" */
    char acEndDate[SCHEDULE_DATE_STR_LEN];   /* "YYYY-MM-DD" */
    char acLabel[SCHEDULE_LABEL_MAX_LEN];
} HOLIDAY_T;

typedef enum
{
    EXCEPTION_SCHEDULE_DEFAULT = 0,  /* Use normal shift bells */
    EXCEPTION_SCHEDULE_CUSTOM  = 1,  /* Fully custom bell list */
    EXCEPTION_SCHEDULE_REDUCED = 2,  /* Reduced hours with custom breaks */
} EXCEPTION_SCHEDULE_TYPE_E;

typedef struct
{
    char                     acDate[SCHEDULE_DATE_STR_LEN];
    char                     acLabel[SCHEDULE_LABEL_MAX_LEN];
    bool                     bHasCustomBells;
    EXCEPTION_SCHEDULE_TYPE_E eScheduleType;
    uint32_t                 ulCustomBellCount;
    BELL_ENTRY_T             atCustomBells[SCHEDULE_MAX_BELLS];
} EXCEPTION_WORKING_T;

typedef struct
{
    char acDate[SCHEDULE_DATE_STR_LEN];
    char acLabel[SCHEDULE_LABEL_MAX_LEN];
} EXCEPTION_HOLIDAY_T;

/* Settings */
typedef struct
{
    char    acTimezone[64];
    bool    abWorkingDays[7]; /* index 0=Sun, 1=Mon, ..., 6=Sat */
} SCHEDULE_SETTINGS_T;

/* A shift (morning / afternoon) */
typedef struct
{
    bool         bEnabled;
    uint32_t     ulBellCount;
    BELL_ENTRY_T atBells[SCHEDULE_MAX_BELLS_PER_SHIFT];
} SCHEDULE_SHIFT_T;

/* Complete schedule data */
typedef struct
{
    /* Settings */
    SCHEDULE_SETTINGS_T tSettings;

    /* Bell shifts */
    SCHEDULE_SHIFT_T tFirstShift;
    SCHEDULE_SHIFT_T tSecondShift;

    /* Calendar */
    uint32_t             ulHolidayCount;
    HOLIDAY_T            atHolidays[SCHEDULE_MAX_HOLIDAYS];

    uint32_t             ulExceptionWorkingCount;
    EXCEPTION_WORKING_T  atExceptionWorking[SCHEDULE_MAX_EXCEPTION_WORKING];

    uint32_t             ulExceptionHolidayCount;
    EXCEPTION_HOLIDAY_T  atExceptionHoliday[SCHEDULE_MAX_EXCEPTION_HOLIDAY];
} SCHEDULE_DATA_T;

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Load settings from SPIFFS JSON file.
 */
esp_err_t Schedule_Data_LoadSettings(SCHEDULE_SETTINGS_T* ptSettings);

/**
 * @brief Save settings to SPIFFS JSON file.
 */
esp_err_t Schedule_Data_SaveSettings(const SCHEDULE_SETTINGS_T* ptSettings);

/**
 * @brief Load bell shifts from SPIFFS.
 */
esp_err_t Schedule_Data_LoadBells(SCHEDULE_SHIFT_T* ptFirst, SCHEDULE_SHIFT_T* ptSecond);

/**
 * @brief Save bell shifts to SPIFFS.
 */
esp_err_t Schedule_Data_SaveBells(const SCHEDULE_SHIFT_T* ptFirst, const SCHEDULE_SHIFT_T* ptSecond);

/**
 * @brief Load calendar (holidays + exceptions) from SPIFFS.
 */
esp_err_t Schedule_Data_LoadCalendar(SCHEDULE_DATA_T* ptData);

/**
 * @brief Save calendar to SPIFFS.
 */
esp_err_t Schedule_Data_SaveCalendar(const SCHEDULE_DATA_T* ptData);

/**
 * @brief Serialize settings to cJSON (caller must cJSON_Delete).
 */
cJSON* Schedule_Data_SettingsToJson(const SCHEDULE_SETTINGS_T* ptSettings);

/**
 * @brief Serialize bell shifts to cJSON (caller must cJSON_Delete).
 */
cJSON* Schedule_Data_BellsToJson(const SCHEDULE_SHIFT_T* ptFirst, const SCHEDULE_SHIFT_T* ptSecond);

/**
 * @brief Serialize holidays to cJSON (caller must cJSON_Delete).
 */
cJSON* Schedule_Data_HolidaysToJson(const HOLIDAY_T* ptHolidays, uint32_t ulCount);

/**
 * @brief Serialize exceptions to cJSON (caller must cJSON_Delete).
 */
cJSON* Schedule_Data_ExceptionsToJson(const EXCEPTION_WORKING_T* ptWork, uint32_t ulWorkCount,
                                       const EXCEPTION_HOLIDAY_T* ptHol, uint32_t ulHolCount);

/**
 * @brief Read the flashed default_schedule.json and return as cJSON.
 *        Caller must cJSON_Delete the returned object.
 * @return cJSON* root object, or NULL if not available.
 */
cJSON* Schedule_Data_ReadDefaultsJson(void);

/**
 * @brief Create default schedule files if they don't exist.
 */
esp_err_t Schedule_Data_CreateDefaults(void);

/**
 * @brief Remove expired exceptions (date is in the past).
 *        Call from scheduler task after midnight.
 * @return ESP_OK if cleanup succeeded (even if nothing to clean).
 */
esp_err_t Schedule_Data_CleanupExpiredExceptions(void);
