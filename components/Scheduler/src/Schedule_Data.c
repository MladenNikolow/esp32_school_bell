#include "Schedule_Data.h"
#include "SPIFFS_API.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

static const char* TAG = "schedule_data";

#define JSON_READ_BUF_SIZE  8192

/* ================================================================== */
/* Internal helpers                                                    */
/* ================================================================== */

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

static cJSON*
readJsonFile(const char* pcPath)
{
    char* pcBuf = (char*)malloc(JSON_READ_BUF_SIZE);
    if (NULL == pcBuf) return NULL;

    size_t ulRead = 0;
    esp_err_t err = SPIFFS_ReadFile(pcPath, pcBuf, JSON_READ_BUF_SIZE, &ulRead);
    if (err != ESP_OK)
    {
        free(pcBuf);
        return NULL;
    }

    cJSON* ptRoot = cJSON_Parse(pcBuf);
    free(pcBuf);
    return ptRoot;
}

static esp_err_t
writeJsonFile(const char* pcPath, cJSON* ptRoot)
{
    const char* pcJson = cJSON_PrintUnformatted(ptRoot);
    if (NULL == pcJson) return ESP_ERR_NO_MEM;

    esp_err_t err = SPIFFS_WriteFile(pcPath, pcJson, strlen(pcJson));
    free((void*)pcJson);
    return err;
}

static void
parseBellArray(cJSON* ptArray, BELL_ENTRY_T* ptBells, uint32_t* pulCount, uint32_t ulMax)
{
    *pulCount = 0;
    if (!cJSON_IsArray(ptArray)) return;

    int iSize = cJSON_GetArraySize(ptArray);
    if ((uint32_t)iSize > ulMax) iSize = (int)ulMax;

    for (int i = 0; i < iSize; i++)
    {
        cJSON* ptItem = cJSON_GetArrayItem(ptArray, i);
        cJSON* ptHour = cJSON_GetObjectItem(ptItem, "hour");
        cJSON* ptMin  = cJSON_GetObjectItem(ptItem, "minute");
        cJSON* ptDur  = cJSON_GetObjectItem(ptItem, "durationSec");
        cJSON* ptLbl  = cJSON_GetObjectItem(ptItem, "label");

        if (cJSON_IsNumber(ptHour) && cJSON_IsNumber(ptMin))
        {
            ptBells[*pulCount].ucHour        = (uint8_t)ptHour->valueint;
            ptBells[*pulCount].ucMinute       = (uint8_t)ptMin->valueint;
            ptBells[*pulCount].usDurationSec  = (ptDur && cJSON_IsNumber(ptDur)) ? (uint16_t)ptDur->valueint : 3;
            memset(ptBells[*pulCount].acLabel, 0, SCHEDULE_LABEL_MAX_LEN);
            if (ptLbl && cJSON_IsString(ptLbl))
            {
                strncpy(ptBells[*pulCount].acLabel, ptLbl->valuestring, SCHEDULE_LABEL_MAX_LEN - 1);
            }
            (*pulCount)++;
        }
    }
}

static cJSON*
bellsToJsonArray(const BELL_ENTRY_T* ptBells, uint32_t ulCount)
{
    cJSON* ptArr = cJSON_CreateArray();
    for (uint32_t i = 0; i < ulCount; i++)
    {
        cJSON* ptItem = cJSON_CreateObject();
        cJSON_AddNumberToObject(ptItem, "hour", ptBells[i].ucHour);
        cJSON_AddNumberToObject(ptItem, "minute", ptBells[i].ucMinute);
        cJSON_AddNumberToObject(ptItem, "durationSec", ptBells[i].usDurationSec);
        cJSON_AddStringToObject(ptItem, "label", ptBells[i].acLabel);
        cJSON_AddItemToArray(ptArr, ptItem);
    }
    return ptArr;
}

/* ================================================================== */
/* Settings                                                            */
/* ================================================================== */

esp_err_t
Schedule_Data_LoadSettings(SCHEDULE_SETTINGS_T* ptSettings)
{
    if (NULL == ptSettings) return ESP_ERR_INVALID_ARG;

    memset(ptSettings, 0, sizeof(SCHEDULE_SETTINGS_T));
    strncpy(ptSettings->acTimezone, "UTC0", sizeof(ptSettings->acTimezone) - 1);

    cJSON* ptRoot = readJsonFile(SCHEDULE_FILE_SETTINGS);
    if (NULL == ptRoot) return ESP_ERR_NOT_FOUND;

    cJSON* ptTz = cJSON_GetObjectItem(ptRoot, "timezone");
    if (ptTz && cJSON_IsString(ptTz))
    {
        strncpy(ptSettings->acTimezone, ptTz->valuestring, sizeof(ptSettings->acTimezone) - 1);
    }

    cJSON* ptDays = cJSON_GetObjectItem(ptRoot, "workingDays");
    if (ptDays && cJSON_IsArray(ptDays))
    {
        int iSize = cJSON_GetArraySize(ptDays);
        for (int i = 0; i < iSize; i++)
        {
            cJSON* ptDay = cJSON_GetArrayItem(ptDays, i);
            if (cJSON_IsNumber(ptDay))
            {
                int iDay = ptDay->valueint;
                if (iDay >= 0 && iDay <= 6)
                {
                    ptSettings->abWorkingDays[iDay] = true;
                }
            }
        }
    }

    cJSON_Delete(ptRoot);
    return ESP_OK;
}

esp_err_t
Schedule_Data_SaveSettings(const SCHEDULE_SETTINGS_T* ptSettings)
{
    if (NULL == ptSettings) return ESP_ERR_INVALID_ARG;

    cJSON* ptRoot = Schedule_Data_SettingsToJson(ptSettings);
    if (NULL == ptRoot) return ESP_ERR_NO_MEM;

    esp_err_t err = writeJsonFile(SCHEDULE_FILE_SETTINGS, ptRoot);
    cJSON_Delete(ptRoot);
    return err;
}

/* ================================================================== */
/* Bells (two shifts)                                                  */
/* ================================================================== */

static void
parseShift(cJSON* ptShiftObj, SCHEDULE_SHIFT_T* ptShift, uint32_t ulMaxBells)
{
    ptShift->bEnabled = true;  /* default enabled */
    ptShift->ulBellCount = 0;

    if (!ptShiftObj || !cJSON_IsObject(ptShiftObj)) return;

    cJSON* ptEnabled = cJSON_GetObjectItem(ptShiftObj, "enabled");
    if (ptEnabled && cJSON_IsBool(ptEnabled))
    {
        ptShift->bEnabled = cJSON_IsTrue(ptEnabled);
    }

    cJSON* ptArr = cJSON_GetObjectItem(ptShiftObj, "bells");
    parseBellArray(ptArr, ptShift->atBells, &ptShift->ulBellCount, ulMaxBells);
}

static cJSON*
shiftToJson(const SCHEDULE_SHIFT_T* ptShift)
{
    cJSON* ptObj = cJSON_CreateObject();
    cJSON_AddBoolToObject(ptObj, "enabled", ptShift->bEnabled);
    cJSON* ptArr = bellsToJsonArray(ptShift->atBells, ptShift->ulBellCount);
    cJSON_AddItemToObject(ptObj, "bells", ptArr);
    return ptObj;
}

esp_err_t
Schedule_Data_LoadBells(SCHEDULE_SHIFT_T* ptFirst, SCHEDULE_SHIFT_T* ptSecond)
{
    if ((NULL == ptFirst) || (NULL == ptSecond)) return ESP_ERR_INVALID_ARG;

    memset(ptFirst, 0, sizeof(SCHEDULE_SHIFT_T));
    memset(ptSecond, 0, sizeof(SCHEDULE_SHIFT_T));
    ptFirst->bEnabled = true;

    cJSON* ptRoot = readJsonFile(SCHEDULE_FILE_BELLS);
    if (NULL == ptRoot) return ESP_ERR_NOT_FOUND;

    /* Support new two-shift format */
    cJSON* ptFirstShift  = cJSON_GetObjectItem(ptRoot, "firstShift");
    cJSON* ptSecondShift = cJSON_GetObjectItem(ptRoot, "secondShift");

    if (ptFirstShift || ptSecondShift)
    {
        parseShift(ptFirstShift, ptFirst, SCHEDULE_MAX_BELLS_PER_SHIFT);
        parseShift(ptSecondShift, ptSecond, SCHEDULE_MAX_BELLS_PER_SHIFT);
    }
    else
    {
        /* Legacy: single "bells" array → import as first shift */
        cJSON* ptArr = cJSON_GetObjectItem(ptRoot, "bells");
        ptFirst->bEnabled = true;
        parseBellArray(ptArr, ptFirst->atBells, &ptFirst->ulBellCount, SCHEDULE_MAX_BELLS_PER_SHIFT);
        ptSecond->bEnabled = false;
        ptSecond->ulBellCount = 0;
    }

    cJSON_Delete(ptRoot);
    return ESP_OK;
}

esp_err_t
Schedule_Data_SaveBells(const SCHEDULE_SHIFT_T* ptFirst, const SCHEDULE_SHIFT_T* ptSecond)
{
    if ((NULL == ptFirst) || (NULL == ptSecond)) return ESP_ERR_INVALID_ARG;

    cJSON* ptRoot = Schedule_Data_BellsToJson(ptFirst, ptSecond);
    if (NULL == ptRoot) return ESP_ERR_NO_MEM;

    esp_err_t err = writeJsonFile(SCHEDULE_FILE_BELLS, ptRoot);
    cJSON_Delete(ptRoot);
    return err;
}

/* ================================================================== */
/* Calendar                                                            */
/* ================================================================== */

esp_err_t
Schedule_Data_LoadCalendar(SCHEDULE_DATA_T* ptData)
{
    if (NULL == ptData) return ESP_ERR_INVALID_ARG;

    ptData->ulHolidayCount = 0;
    ptData->ulExceptionWorkingCount = 0;
    ptData->ulExceptionHolidayCount = 0;

    cJSON* ptRoot = readJsonFile(SCHEDULE_FILE_CALENDAR);
    if (NULL == ptRoot) return ESP_ERR_NOT_FOUND;

    /* Holidays */
    cJSON* ptHolidays = cJSON_GetObjectItem(ptRoot, "holidays");
    if (ptHolidays && cJSON_IsArray(ptHolidays))
    {
        int iSize = cJSON_GetArraySize(ptHolidays);
        if ((uint32_t)iSize > SCHEDULE_MAX_HOLIDAYS) iSize = SCHEDULE_MAX_HOLIDAYS;

        for (int i = 0; i < iSize; i++)
        {
            cJSON* ptItem = cJSON_GetArrayItem(ptHolidays, i);
            cJSON* ptStart = cJSON_GetObjectItem(ptItem, "startDate");
            cJSON* ptEnd   = cJSON_GetObjectItem(ptItem, "endDate");
            cJSON* ptLbl   = cJSON_GetObjectItem(ptItem, "label");

            if (ptStart && cJSON_IsString(ptStart) && ptEnd && cJSON_IsString(ptEnd))
            {
                HOLIDAY_T* ptH = &ptData->atHolidays[ptData->ulHolidayCount];
                strncpy(ptH->acStartDate, ptStart->valuestring, SCHEDULE_DATE_STR_LEN - 1);
                strncpy(ptH->acEndDate, ptEnd->valuestring, SCHEDULE_DATE_STR_LEN - 1);
                memset(ptH->acLabel, 0, SCHEDULE_LABEL_MAX_LEN);
                if (ptLbl && cJSON_IsString(ptLbl))
                {
                    strncpy(ptH->acLabel, ptLbl->valuestring, SCHEDULE_LABEL_MAX_LEN - 1);
                }
                ptData->ulHolidayCount++;
            }
        }
    }

    /* Exception working */
    cJSON* ptExWork = cJSON_GetObjectItem(ptRoot, "exceptionWorking");
    if (ptExWork && cJSON_IsArray(ptExWork))
    {
        int iSize = cJSON_GetArraySize(ptExWork);
        if ((uint32_t)iSize > SCHEDULE_MAX_EXCEPTION_WORKING) iSize = SCHEDULE_MAX_EXCEPTION_WORKING;

        for (int i = 0; i < iSize; i++)
        {
            cJSON* ptItem = cJSON_GetArrayItem(ptExWork, i);
            cJSON* ptDate = cJSON_GetObjectItem(ptItem, "date");
            cJSON* ptLbl  = cJSON_GetObjectItem(ptItem, "label");

            if (ptDate && cJSON_IsString(ptDate))
            {
                EXCEPTION_WORKING_T* ptEx = &ptData->atExceptionWorking[ptData->ulExceptionWorkingCount];
                strncpy(ptEx->acDate, ptDate->valuestring, SCHEDULE_DATE_STR_LEN - 1);
                memset(ptEx->acLabel, 0, SCHEDULE_LABEL_MAX_LEN);
                if (ptLbl && cJSON_IsString(ptLbl))
                {
                    strncpy(ptEx->acLabel, ptLbl->valuestring, SCHEDULE_LABEL_MAX_LEN - 1);
                }

                /* Schedule type */
                ptEx->eScheduleType = EXCEPTION_SCHEDULE_DEFAULT;
                cJSON* ptSchedType = cJSON_GetObjectItem(ptItem, "scheduleType");
                if (ptSchedType && cJSON_IsString(ptSchedType))
                {
                    if (strcmp(ptSchedType->valuestring, "custom") == 0)
                        ptEx->eScheduleType = EXCEPTION_SCHEDULE_CUSTOM;
                    else if (strcmp(ptSchedType->valuestring, "reduced") == 0)
                        ptEx->eScheduleType = EXCEPTION_SCHEDULE_REDUCED;
                }

                /* Custom bells */
                cJSON* ptCustom = cJSON_GetObjectItem(ptItem, "customBells");
                if (ptCustom && cJSON_IsArray(ptCustom) && cJSON_GetArraySize(ptCustom) > 0)
                {
                    ptEx->bHasCustomBells = true;
                    parseBellArray(ptCustom, ptEx->atCustomBells, &ptEx->ulCustomBellCount, SCHEDULE_MAX_BELLS);
                }
                else
                {
                    ptEx->bHasCustomBells = false;
                    ptEx->ulCustomBellCount = 0;
                }

                ptData->ulExceptionWorkingCount++;
            }
        }
    }

    /* Exception holidays */
    cJSON* ptExHol = cJSON_GetObjectItem(ptRoot, "exceptionHoliday");
    if (ptExHol && cJSON_IsArray(ptExHol))
    {
        int iSize = cJSON_GetArraySize(ptExHol);
        if ((uint32_t)iSize > SCHEDULE_MAX_EXCEPTION_HOLIDAY) iSize = SCHEDULE_MAX_EXCEPTION_HOLIDAY;

        for (int i = 0; i < iSize; i++)
        {
            cJSON* ptItem = cJSON_GetArrayItem(ptExHol, i);
            cJSON* ptDate = cJSON_GetObjectItem(ptItem, "date");
            cJSON* ptLbl  = cJSON_GetObjectItem(ptItem, "label");

            if (ptDate && cJSON_IsString(ptDate))
            {
                EXCEPTION_HOLIDAY_T* ptEx = &ptData->atExceptionHoliday[ptData->ulExceptionHolidayCount];
                strncpy(ptEx->acDate, ptDate->valuestring, SCHEDULE_DATE_STR_LEN - 1);
                memset(ptEx->acLabel, 0, SCHEDULE_LABEL_MAX_LEN);
                if (ptLbl && cJSON_IsString(ptLbl))
                {
                    strncpy(ptEx->acLabel, ptLbl->valuestring, SCHEDULE_LABEL_MAX_LEN - 1);
                }
                ptData->ulExceptionHolidayCount++;
            }
        }
    }

    cJSON_Delete(ptRoot);
    return ESP_OK;
}

esp_err_t
Schedule_Data_SaveCalendar(const SCHEDULE_DATA_T* ptData)
{
    if (NULL == ptData) return ESP_ERR_INVALID_ARG;

    cJSON* ptRoot = cJSON_CreateObject();

    /* Holidays */
    cJSON* ptHolArr = cJSON_AddArrayToObject(ptRoot, "holidays");
    for (uint32_t i = 0; i < ptData->ulHolidayCount; i++)
    {
        cJSON* ptItem = cJSON_CreateObject();
        cJSON_AddStringToObject(ptItem, "startDate", ptData->atHolidays[i].acStartDate);
        cJSON_AddStringToObject(ptItem, "endDate", ptData->atHolidays[i].acEndDate);
        cJSON_AddStringToObject(ptItem, "label", ptData->atHolidays[i].acLabel);
        cJSON_AddItemToArray(ptHolArr, ptItem);
    }

    /* Exception working */
    cJSON* ptExWArr = cJSON_AddArrayToObject(ptRoot, "exceptionWorking");
    for (uint32_t i = 0; i < ptData->ulExceptionWorkingCount; i++)
    {
        const EXCEPTION_WORKING_T* ptEx = &ptData->atExceptionWorking[i];
        cJSON* ptItem = cJSON_CreateObject();
        cJSON_AddStringToObject(ptItem, "date", ptEx->acDate);
        cJSON_AddStringToObject(ptItem, "label", ptEx->acLabel);

        const char* pcScheduleType = "default";
        if (ptEx->eScheduleType == EXCEPTION_SCHEDULE_CUSTOM) pcScheduleType = "custom";
        else if (ptEx->eScheduleType == EXCEPTION_SCHEDULE_REDUCED) pcScheduleType = "reduced";
        cJSON_AddStringToObject(ptItem, "scheduleType", pcScheduleType);

        if (ptEx->bHasCustomBells && ptEx->ulCustomBellCount > 0)
        {
            cJSON* ptCustom = bellsToJsonArray(ptEx->atCustomBells, ptEx->ulCustomBellCount);
            cJSON_AddItemToObject(ptItem, "customBells", ptCustom);
        }
        cJSON_AddItemToArray(ptExWArr, ptItem);
    }

    /* Exception holidays */
    cJSON* ptExHArr = cJSON_AddArrayToObject(ptRoot, "exceptionHoliday");
    for (uint32_t i = 0; i < ptData->ulExceptionHolidayCount; i++)
    {
        cJSON* ptItem = cJSON_CreateObject();
        cJSON_AddStringToObject(ptItem, "date", ptData->atExceptionHoliday[i].acDate);
        cJSON_AddStringToObject(ptItem, "label", ptData->atExceptionHoliday[i].acLabel);
        cJSON_AddItemToArray(ptExHArr, ptItem);
    }

    esp_err_t err = writeJsonFile(SCHEDULE_FILE_CALENDAR, ptRoot);
    cJSON_Delete(ptRoot);
    return err;
}

/* ================================================================== */
/* JSON serialization helpers (for API responses)                      */
/* ================================================================== */

cJSON*
Schedule_Data_SettingsToJson(const SCHEDULE_SETTINGS_T* ptSettings)
{
    cJSON* ptRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(ptRoot, "timezone", ptSettings->acTimezone);

    cJSON* ptDays = cJSON_AddArrayToObject(ptRoot, "workingDays");
    for (int i = 0; i < 7; i++)
    {
        if (ptSettings->abWorkingDays[i])
        {
            cJSON_AddItemToArray(ptDays, cJSON_CreateNumber(i));
        }
    }

    return ptRoot;
}

cJSON*
Schedule_Data_BellsToJson(const SCHEDULE_SHIFT_T* ptFirst, const SCHEDULE_SHIFT_T* ptSecond)
{
    cJSON* ptRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(ptRoot, "firstShift", shiftToJson(ptFirst));
    cJSON_AddItemToObject(ptRoot, "secondShift", shiftToJson(ptSecond));
    return ptRoot;
}

cJSON*
Schedule_Data_HolidaysToJson(const HOLIDAY_T* ptHolidays, uint32_t ulCount)
{
    cJSON* ptRoot = cJSON_CreateObject();
    cJSON* ptArr = cJSON_AddArrayToObject(ptRoot, "holidays");
    for (uint32_t i = 0; i < ulCount; i++)
    {
        cJSON* ptItem = cJSON_CreateObject();
        cJSON_AddStringToObject(ptItem, "startDate", ptHolidays[i].acStartDate);
        cJSON_AddStringToObject(ptItem, "endDate", ptHolidays[i].acEndDate);
        cJSON_AddStringToObject(ptItem, "label", ptHolidays[i].acLabel);
        cJSON_AddItemToArray(ptArr, ptItem);
    }
    return ptRoot;
}

cJSON*
Schedule_Data_ExceptionsToJson(const EXCEPTION_WORKING_T* ptWork, uint32_t ulWorkCount,
                                const EXCEPTION_HOLIDAY_T* ptHol, uint32_t ulHolCount)
{
    cJSON* ptRoot = cJSON_CreateObject();

    cJSON* ptWArr = cJSON_AddArrayToObject(ptRoot, "exceptionWorking");
    for (uint32_t i = 0; i < ulWorkCount; i++)
    {
        cJSON* ptItem = cJSON_CreateObject();
        cJSON_AddStringToObject(ptItem, "date", ptWork[i].acDate);
        cJSON_AddStringToObject(ptItem, "label", ptWork[i].acLabel);

        const char* pcScheduleType = "default";
        if (ptWork[i].eScheduleType == EXCEPTION_SCHEDULE_CUSTOM) pcScheduleType = "custom";
        else if (ptWork[i].eScheduleType == EXCEPTION_SCHEDULE_REDUCED) pcScheduleType = "reduced";
        cJSON_AddStringToObject(ptItem, "scheduleType", pcScheduleType);

        if (ptWork[i].bHasCustomBells && ptWork[i].ulCustomBellCount > 0)
        {
            cJSON* ptCustom = bellsToJsonArray(ptWork[i].atCustomBells, ptWork[i].ulCustomBellCount);
            cJSON_AddItemToObject(ptItem, "customBells", ptCustom);
        }
        cJSON_AddItemToArray(ptWArr, ptItem);
    }

    cJSON* ptHArr = cJSON_AddArrayToObject(ptRoot, "exceptionHoliday");
    for (uint32_t i = 0; i < ulHolCount; i++)
    {
        cJSON* ptItem = cJSON_CreateObject();
        cJSON_AddStringToObject(ptItem, "date", ptHol[i].acDate);
        cJSON_AddStringToObject(ptItem, "label", ptHol[i].acLabel);
        cJSON_AddItemToArray(ptHArr, ptItem);
    }

    return ptRoot;
}

/* ================================================================== */
/* Create defaults (reads from flashed config if available)            */
/* ================================================================== */

static cJSON*
readDefaultsFromFlash(void)
{
    char* pcBuf = (char*)malloc(JSON_READ_BUF_SIZE);
    if (NULL == pcBuf) return NULL;

    FILE* pFile = fopen(SCHEDULE_FILE_DEFAULTS, "r");
    if (NULL == pFile)
    {
        free(pcBuf);
        ESP_LOGI(TAG, "No flashed default config found at %s", SCHEDULE_FILE_DEFAULTS);
        return NULL;
    }

    size_t ulRead = fread(pcBuf, 1, JSON_READ_BUF_SIZE - 1, pFile);
    pcBuf[ulRead] = '\0';
    fclose(pFile);

    cJSON* ptRoot = cJSON_Parse(pcBuf);
    free(pcBuf);

    if (ptRoot)
    {
        ESP_LOGI(TAG, "Loaded default config from %s", SCHEDULE_FILE_DEFAULTS);
    }
    return ptRoot;
}

cJSON*
Schedule_Data_ReadDefaultsJson(void)
{
    return readDefaultsFromFlash();
}

esp_err_t
Schedule_Data_CreateDefaults(void)
{
    cJSON* ptDefaults = readDefaultsFromFlash();

    /* Settings */
    if (!SPIFFS_FileExists(SCHEDULE_FILE_SETTINGS))
    {
        SCHEDULE_SETTINGS_T tDefSettings = { 0 };
        strncpy(tDefSettings.acTimezone, "UTC0", sizeof(tDefSettings.acTimezone) - 1);
        /* Mon-Fri working days */
        tDefSettings.abWorkingDays[1] = true;
        tDefSettings.abWorkingDays[2] = true;
        tDefSettings.abWorkingDays[3] = true;
        tDefSettings.abWorkingDays[4] = true;
        tDefSettings.abWorkingDays[5] = true;

        if (ptDefaults)
        {
            cJSON* ptTz = cJSON_GetObjectItem(ptDefaults, "timezone");
            if (ptTz && cJSON_IsString(ptTz))
            {
                strncpy(tDefSettings.acTimezone, ptTz->valuestring, sizeof(tDefSettings.acTimezone) - 1);
            }

            cJSON* ptDays = cJSON_GetObjectItem(ptDefaults, "workingDays");
            if (ptDays && cJSON_IsArray(ptDays))
            {
                memset(tDefSettings.abWorkingDays, 0, sizeof(tDefSettings.abWorkingDays));
                int iSize = cJSON_GetArraySize(ptDays);
                for (int i = 0; i < iSize; i++)
                {
                    cJSON* ptDay = cJSON_GetArrayItem(ptDays, i);
                    if (cJSON_IsNumber(ptDay))
                    {
                        int iDay = ptDay->valueint;
                        if (iDay >= 0 && iDay <= 6)
                        {
                            tDefSettings.abWorkingDays[iDay] = true;
                        }
                    }
                }
            }
        }

        Schedule_Data_SaveSettings(&tDefSettings);
        ESP_LOGI(TAG, "Created default settings.json");
    }

    /* Bells (two shifts) — heap-allocated to avoid stack overflow
       (each SCHEDULE_SHIFT_T is ~2.6 KB, APP_TASK stack is only 4 KB) */
    if (!SPIFFS_FileExists(SCHEDULE_FILE_BELLS))
    {
        SCHEDULE_SHIFT_T* ptFirst  = (SCHEDULE_SHIFT_T*)calloc(1, sizeof(SCHEDULE_SHIFT_T));
        SCHEDULE_SHIFT_T* ptSecond = (SCHEDULE_SHIFT_T*)calloc(1, sizeof(SCHEDULE_SHIFT_T));

        if (ptFirst && ptSecond)
        {
            ptFirst->bEnabled  = true;
            ptSecond->bEnabled = false;

            if (ptDefaults)
            {
                cJSON* ptFirstShift = cJSON_GetObjectItem(ptDefaults, "firstShift");
                if (ptFirstShift)
                {
                    parseShift(ptFirstShift, ptFirst, SCHEDULE_MAX_BELLS_PER_SHIFT);
                }
                cJSON* ptSecondShift = cJSON_GetObjectItem(ptDefaults, "secondShift");
                if (ptSecondShift)
                {
                    parseShift(ptSecondShift, ptSecond, SCHEDULE_MAX_BELLS_PER_SHIFT);
                }
            }

            Schedule_Data_SaveBells(ptFirst, ptSecond);
            ESP_LOGI(TAG, "Created default schedule.json (%"PRIu32" + %"PRIu32" bells)",
                     ptFirst->ulBellCount, ptSecond->ulBellCount);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to allocate memory for default bells");
        }

        free(ptFirst);
        free(ptSecond);
    }

    /* Calendar */
    if (!SPIFFS_FileExists(SCHEDULE_FILE_CALENDAR))
    {
        cJSON* ptRoot = cJSON_CreateObject();

        /* Copy holidays/exceptions from flashed defaults if present */
        if (ptDefaults)
        {
            cJSON* ptDefHol = cJSON_GetObjectItem(ptDefaults, "holidays");
            if (ptDefHol && cJSON_IsArray(ptDefHol))
            {
                cJSON_AddItemToObject(ptRoot, "holidays", cJSON_Duplicate(ptDefHol, true));
            }
            else
            {
                cJSON_AddItemToObject(ptRoot, "holidays", cJSON_CreateArray());
            }

            cJSON* ptDefExW = cJSON_GetObjectItem(ptDefaults, "exceptionWorking");
            if (ptDefExW && cJSON_IsArray(ptDefExW))
            {
                cJSON_AddItemToObject(ptRoot, "exceptionWorking", cJSON_Duplicate(ptDefExW, true));
            }
            else
            {
                cJSON_AddItemToObject(ptRoot, "exceptionWorking", cJSON_CreateArray());
            }

            cJSON* ptDefExH = cJSON_GetObjectItem(ptDefaults, "exceptionHoliday");
            if (ptDefExH && cJSON_IsArray(ptDefExH))
            {
                cJSON_AddItemToObject(ptRoot, "exceptionHoliday", cJSON_Duplicate(ptDefExH, true));
            }
            else
            {
                cJSON_AddItemToObject(ptRoot, "exceptionHoliday", cJSON_CreateArray());
            }
        }
        else
        {
            cJSON_AddItemToObject(ptRoot, "holidays", cJSON_CreateArray());
            cJSON_AddItemToObject(ptRoot, "exceptionWorking", cJSON_CreateArray());
            cJSON_AddItemToObject(ptRoot, "exceptionHoliday", cJSON_CreateArray());
        }

        writeJsonFile(SCHEDULE_FILE_CALENDAR, ptRoot);
        cJSON_Delete(ptRoot);
        ESP_LOGI(TAG, "Created default calendar.json");
    }

    if (ptDefaults)
    {
        cJSON_Delete(ptDefaults);
    }

    return ESP_OK;
}

/* ================================================================== */
/* Cleanup expired exceptions                                          */
/* ================================================================== */

esp_err_t
Schedule_Data_CleanupExpiredExceptions(void)
{
    SCHEDULE_DATA_T* ptData = (SCHEDULE_DATA_T*)calloc(1, sizeof(SCHEDULE_DATA_T));
    if (NULL == ptData) return ESP_ERR_NO_MEM;

    esp_err_t err = Schedule_Data_LoadCalendar(ptData);
    if (err != ESP_OK)
    {
        free(ptData);
        return err;
    }

    /* Get today's ordinal */
    time_t tNow = time(NULL);
    struct tm tTm;
    localtime_r(&tNow, &tTm);
    char acToday[SCHEDULE_DATE_STR_LEN];
    snprintf(acToday, sizeof(acToday), "%04d-%02d-%02d",
             tTm.tm_year + 1900, tTm.tm_mon + 1, tTm.tm_mday);
    int iTodayOrd = dateStrToOrdinal(acToday);

    bool bChanged = false;

    /* Remove expired exception working days */
    uint32_t ulNewWorkCount = 0;
    for (uint32_t i = 0; i < ptData->ulExceptionWorkingCount; i++)
    {
        int iExOrd = dateStrToOrdinal(ptData->atExceptionWorking[i].acDate);
        if (iExOrd >= 0 && iExOrd < iTodayOrd)
        {
            ESP_LOGI(TAG, "Cleaning up expired exception working: %s (%s)",
                     ptData->atExceptionWorking[i].acDate,
                     ptData->atExceptionWorking[i].acLabel);
            bChanged = true;
            continue;
        }
        if (ulNewWorkCount != i)
        {
            ptData->atExceptionWorking[ulNewWorkCount] = ptData->atExceptionWorking[i];
        }
        ulNewWorkCount++;
    }
    ptData->ulExceptionWorkingCount = ulNewWorkCount;

    /* Remove expired exception holidays */
    uint32_t ulNewHolCount = 0;
    for (uint32_t i = 0; i < ptData->ulExceptionHolidayCount; i++)
    {
        int iExOrd = dateStrToOrdinal(ptData->atExceptionHoliday[i].acDate);
        if (iExOrd >= 0 && iExOrd < iTodayOrd)
        {
            ESP_LOGI(TAG, "Cleaning up expired exception holiday: %s (%s)",
                     ptData->atExceptionHoliday[i].acDate,
                     ptData->atExceptionHoliday[i].acLabel);
            bChanged = true;
            continue;
        }
        if (ulNewHolCount != i)
        {
            ptData->atExceptionHoliday[ulNewHolCount] = ptData->atExceptionHoliday[i];
        }
        ulNewHolCount++;
    }
    ptData->ulExceptionHolidayCount = ulNewHolCount;

    /* Also remove expired holiday ranges */
    uint32_t ulNewRangeCount = 0;
    for (uint32_t i = 0; i < ptData->ulHolidayCount; i++)
    {
        int iEndOrd = dateStrToOrdinal(ptData->atHolidays[i].acEndDate);
        if (iEndOrd >= 0 && iEndOrd < iTodayOrd)
        {
            ESP_LOGI(TAG, "Cleaning up expired holiday range: %s to %s (%s)",
                     ptData->atHolidays[i].acStartDate,
                     ptData->atHolidays[i].acEndDate,
                     ptData->atHolidays[i].acLabel);
            bChanged = true;
            continue;
        }
        if (ulNewRangeCount != i)
        {
            ptData->atHolidays[ulNewRangeCount] = ptData->atHolidays[i];
        }
        ulNewRangeCount++;
    }
    ptData->ulHolidayCount = ulNewRangeCount;

    if (bChanged)
    {
        err = Schedule_Data_SaveCalendar(ptData);
        ESP_LOGI(TAG, "Calendar cleaned: %"PRIu32" holidays, %"PRIu32" exc-work, %"PRIu32" exc-hol remaining",
                 ptData->ulHolidayCount, ptData->ulExceptionWorkingCount, ptData->ulExceptionHolidayCount);
    }

    free(ptData);
    return err;
}
