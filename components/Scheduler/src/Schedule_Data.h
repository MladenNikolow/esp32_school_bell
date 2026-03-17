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
#define SCHEDULE_MAX_EXCEPTIONS         40
#define SCHEDULE_MAX_CUSTOM_BELLS       30   /* bells per template or custom set */
#define SCHEDULE_MAX_CUSTOM_BELL_SETS    5
#define SCHEDULE_MAX_TEMPLATES           5
#define SCHEDULE_LABEL_MAX_LEN          48
#define SCHEDULE_TEMPLATE_NAME_LEN      32
#define SCHEDULE_DATE_STR_LEN           11  /* "YYYY-MM-DD\0" */

/* File paths */
#define SCHEDULE_FILE_SETTINGS          "/storage/settings.json"
#define SCHEDULE_FILE_BELLS             "/storage/schedule.json"
#define SCHEDULE_FILE_CALENDAR          "/storage/calendar.json"
#define SCHEDULE_FILE_TEMPLATES         "/storage/templates.json"
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
    EXCEPTION_ACTION_DAY_OFF       = 0,  /* No bells ring */
    EXCEPTION_ACTION_NORMAL        = 1,  /* Normal schedule (both shifts) */
    EXCEPTION_ACTION_FIRST_SHIFT   = 2,  /* Only first shift bells */
    EXCEPTION_ACTION_SECOND_SHIFT  = 3,  /* Only second shift bells */
    EXCEPTION_ACTION_TEMPLATE      = 4,  /* Use a named bell template */
    EXCEPTION_ACTION_CUSTOM        = 5,  /* Use an ad-hoc custom bell set */
} EXCEPTION_ACTION_E;

typedef struct
{
    char               acStartDate[SCHEDULE_DATE_STR_LEN]; /* "YYYY-MM-DD" */
    char               acEndDate[SCHEDULE_DATE_STR_LEN];   /* empty = single day */
    char               acLabel[SCHEDULE_LABEL_MAX_LEN];
    EXCEPTION_ACTION_E eAction;
    int8_t             iTimeOffsetMin;    /* -120..+120: shift all bell times */
    uint8_t            ucTemplateIdx;     /* valid when eAction == TEMPLATE */
    uint8_t            ucCustomBellsIdx;  /* index into custom bell sets, 0xFF = none */
} EXCEPTION_ENTRY_T;

typedef struct
{
    uint8_t      ucBellCount;
    BELL_ENTRY_T atBells[SCHEDULE_MAX_CUSTOM_BELLS];
} EXCEPTION_CUSTOM_BELLS_T;

typedef struct
{
    char         acName[SCHEDULE_TEMPLATE_NAME_LEN];
    uint8_t      ucBellCount;
    BELL_ENTRY_T atBells[SCHEDULE_MAX_CUSTOM_BELLS];
} BELL_TEMPLATE_T;

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

    /* Unified exceptions */
    uint32_t             ulExceptionCount;
    EXCEPTION_ENTRY_T    atExceptions[SCHEDULE_MAX_EXCEPTIONS];

    uint32_t                 ulCustomBellSetCount;
    EXCEPTION_CUSTOM_BELLS_T atCustomBellSets[SCHEDULE_MAX_CUSTOM_BELL_SETS];

    /* Bell templates */
    uint32_t             ulTemplateCount;
    BELL_TEMPLATE_T      atTemplates[SCHEDULE_MAX_TEMPLATES];
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
cJSON* Schedule_Data_ExceptionsToJson(const EXCEPTION_ENTRY_T* ptExceptions, uint32_t ulCount,
                                       const EXCEPTION_CUSTOM_BELLS_T* ptCustomSets, uint32_t ulCustomCount);

/**
 * @brief Load bell templates from SPIFFS.
 */
esp_err_t Schedule_Data_LoadTemplates(SCHEDULE_DATA_T* ptData);

/**
 * @brief Save bell templates to SPIFFS.
 */
esp_err_t Schedule_Data_SaveTemplates(const SCHEDULE_DATA_T* ptData);

/**
 * @brief Serialize bell templates to cJSON (caller must cJSON_Delete).
 */
cJSON* Schedule_Data_TemplatesToJson(const BELL_TEMPLATE_T* ptTemplates, uint32_t ulCount);

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
