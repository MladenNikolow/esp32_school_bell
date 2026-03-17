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

/** Map action enum to JSON string */
static const char*
actionToStr(EXCEPTION_ACTION_E eAction)
{
    switch (eAction)
    {
        case EXCEPTION_ACTION_DAY_OFF:       return "day-off";
        case EXCEPTION_ACTION_NORMAL:        return "normal";
        case EXCEPTION_ACTION_FIRST_SHIFT:   return "first-shift";
        case EXCEPTION_ACTION_SECOND_SHIFT:  return "second-shift";
        case EXCEPTION_ACTION_TEMPLATE:      return "template";
        case EXCEPTION_ACTION_CUSTOM:        return "custom";
        default:                             return "day-off";
    }
}

/** Map JSON string to action enum */
static EXCEPTION_ACTION_E
strToAction(const char* pcStr)
{
    if (!pcStr) return EXCEPTION_ACTION_DAY_OFF;
    if (strcmp(pcStr, "normal")       == 0) return EXCEPTION_ACTION_NORMAL;
    if (strcmp(pcStr, "first-shift")  == 0) return EXCEPTION_ACTION_FIRST_SHIFT;
    if (strcmp(pcStr, "second-shift") == 0) return EXCEPTION_ACTION_SECOND_SHIFT;
    if (strcmp(pcStr, "template")     == 0) return EXCEPTION_ACTION_TEMPLATE;
    if (strcmp(pcStr, "custom")       == 0) return EXCEPTION_ACTION_CUSTOM;
    return EXCEPTION_ACTION_DAY_OFF;
}

/** Migrate old split format to unified exceptions */
static void
migrateOldExceptions(cJSON* ptRoot, SCHEDULE_DATA_T* ptData)
{
    /* Old exceptionWorking → unified entries */
    cJSON* ptExWork = cJSON_GetObjectItem(ptRoot, "exceptionWorking");
    if (ptExWork && cJSON_IsArray(ptExWork))
    {
        int iSize = cJSON_GetArraySize(ptExWork);
        for (int i = 0; i < iSize && ptData->ulExceptionCount < SCHEDULE_MAX_EXCEPTIONS; i++)
        {
            cJSON* ptItem = cJSON_GetArrayItem(ptExWork, i);
            cJSON* ptDate = cJSON_GetObjectItem(ptItem, "date");
            if (!ptDate || !cJSON_IsString(ptDate)) continue;

            EXCEPTION_ENTRY_T* ptEx = &ptData->atExceptions[ptData->ulExceptionCount];
            memset(ptEx, 0, sizeof(EXCEPTION_ENTRY_T));
            strncpy(ptEx->acStartDate, ptDate->valuestring, SCHEDULE_DATE_STR_LEN - 1);
            ptEx->ucCustomBellsIdx = 0xFF;

            cJSON* ptLbl = cJSON_GetObjectItem(ptItem, "label");
            if (ptLbl && cJSON_IsString(ptLbl))
                strncpy(ptEx->acLabel, ptLbl->valuestring, SCHEDULE_LABEL_MAX_LEN - 1);

            /* Map old scheduleType to new action */
            cJSON* ptSchedType = cJSON_GetObjectItem(ptItem, "scheduleType");
            const char* pcType = ptSchedType && cJSON_IsString(ptSchedType) ? ptSchedType->valuestring : "default";

            cJSON* ptCustom = cJSON_GetObjectItem(ptItem, "customBells");
            bool bHasCustom = ptCustom && cJSON_IsArray(ptCustom) && cJSON_GetArraySize(ptCustom) > 0;

            if (strcmp(pcType, "custom") == 0 || strcmp(pcType, "reduced") == 0)
            {
                if (bHasCustom && ptData->ulCustomBellSetCount < SCHEDULE_MAX_CUSTOM_BELL_SETS)
                {
                    ptEx->eAction = EXCEPTION_ACTION_CUSTOM;
                    ptEx->ucCustomBellsIdx = (uint8_t)ptData->ulCustomBellSetCount;
                    EXCEPTION_CUSTOM_BELLS_T* ptSet = &ptData->atCustomBellSets[ptData->ulCustomBellSetCount];
                    uint32_t ulTmpCount = 0;
                    parseBellArray(ptCustom, ptSet->atBells, &ulTmpCount, SCHEDULE_MAX_CUSTOM_BELLS);
                    ptSet->ucBellCount = (uint8_t)ulTmpCount;
                    ptData->ulCustomBellSetCount++;
                }
                else
                {
                    ptEx->eAction = EXCEPTION_ACTION_NORMAL;
                }
            }
            else
            {
                ptEx->eAction = EXCEPTION_ACTION_NORMAL;
            }

            ptData->ulExceptionCount++;
        }
    }

    /* Old exceptionHoliday → unified entries with DAY_OFF action */
    cJSON* ptExHol = cJSON_GetObjectItem(ptRoot, "exceptionHoliday");
    if (ptExHol && cJSON_IsArray(ptExHol))
    {
        int iSize = cJSON_GetArraySize(ptExHol);
        for (int i = 0; i < iSize && ptData->ulExceptionCount < SCHEDULE_MAX_EXCEPTIONS; i++)
        {
            cJSON* ptItem = cJSON_GetArrayItem(ptExHol, i);
            cJSON* ptDate = cJSON_GetObjectItem(ptItem, "date");
            if (!ptDate || !cJSON_IsString(ptDate)) continue;

            EXCEPTION_ENTRY_T* ptEx = &ptData->atExceptions[ptData->ulExceptionCount];
            memset(ptEx, 0, sizeof(EXCEPTION_ENTRY_T));
            strncpy(ptEx->acStartDate, ptDate->valuestring, SCHEDULE_DATE_STR_LEN - 1);
            ptEx->eAction = EXCEPTION_ACTION_DAY_OFF;
            ptEx->ucCustomBellsIdx = 0xFF;

            cJSON* ptLbl = cJSON_GetObjectItem(ptItem, "label");
            if (ptLbl && cJSON_IsString(ptLbl))
                strncpy(ptEx->acLabel, ptLbl->valuestring, SCHEDULE_LABEL_MAX_LEN - 1);

            ptData->ulExceptionCount++;
        }
    }

    ESP_LOGI(TAG, "Migrated old format: %"PRIu32" exceptions, %"PRIu32" custom bell sets",
             ptData->ulExceptionCount, ptData->ulCustomBellSetCount);
}

esp_err_t
Schedule_Data_LoadCalendar(SCHEDULE_DATA_T* ptData)
{
    if (NULL == ptData) return ESP_ERR_INVALID_ARG;

    ptData->ulHolidayCount = 0;
    ptData->ulExceptionCount = 0;
    ptData->ulCustomBellSetCount = 0;

    cJSON* ptRoot = readJsonFile(SCHEDULE_FILE_CALENDAR);
    if (NULL == ptRoot) return ESP_ERR_NOT_FOUND;

    /* Holidays (unchanged) */
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

    /* Check for new unified format first */
    cJSON* ptExceptions = cJSON_GetObjectItem(ptRoot, "exceptions");
    if (ptExceptions && cJSON_IsArray(ptExceptions))
    {
        /* New unified format */
        int iSize = cJSON_GetArraySize(ptExceptions);
        if ((uint32_t)iSize > SCHEDULE_MAX_EXCEPTIONS) iSize = SCHEDULE_MAX_EXCEPTIONS;

        for (int i = 0; i < iSize; i++)
        {
            cJSON* ptItem = cJSON_GetArrayItem(ptExceptions, i);
            cJSON* ptStart = cJSON_GetObjectItem(ptItem, "startDate");
            if (!ptStart || !cJSON_IsString(ptStart)) continue;

            EXCEPTION_ENTRY_T* ptEx = &ptData->atExceptions[ptData->ulExceptionCount];
            memset(ptEx, 0, sizeof(EXCEPTION_ENTRY_T));
            strncpy(ptEx->acStartDate, ptStart->valuestring, SCHEDULE_DATE_STR_LEN - 1);
            ptEx->ucCustomBellsIdx = 0xFF;

            cJSON* ptEnd = cJSON_GetObjectItem(ptItem, "endDate");
            if (ptEnd && cJSON_IsString(ptEnd))
                strncpy(ptEx->acEndDate, ptEnd->valuestring, SCHEDULE_DATE_STR_LEN - 1);

            cJSON* ptLbl = cJSON_GetObjectItem(ptItem, "label");
            if (ptLbl && cJSON_IsString(ptLbl))
                strncpy(ptEx->acLabel, ptLbl->valuestring, SCHEDULE_LABEL_MAX_LEN - 1);

            cJSON* ptAction = cJSON_GetObjectItem(ptItem, "action");
            ptEx->eAction = strToAction(ptAction && cJSON_IsString(ptAction) ? ptAction->valuestring : NULL);

            cJSON* ptOffset = cJSON_GetObjectItem(ptItem, "timeOffsetMin");
            if (ptOffset && cJSON_IsNumber(ptOffset))
            {
                int iOff = ptOffset->valueint;
                if (iOff < -120) iOff = -120;
                if (iOff > 120) iOff = 120;
                ptEx->iTimeOffsetMin = (int8_t)iOff;
            }

            cJSON* ptTplIdx = cJSON_GetObjectItem(ptItem, "templateIdx");
            if (ptTplIdx && cJSON_IsNumber(ptTplIdx))
                ptEx->ucTemplateIdx = (uint8_t)ptTplIdx->valueint;

            cJSON* ptCustIdx = cJSON_GetObjectItem(ptItem, "customBellsIdx");
            if (ptCustIdx && cJSON_IsNumber(ptCustIdx) && ptCustIdx->valueint >= 0)
                ptEx->ucCustomBellsIdx = (uint8_t)ptCustIdx->valueint;

            ptData->ulExceptionCount++;
        }

        /* Custom bell sets */
        cJSON* ptCustSets = cJSON_GetObjectItem(ptRoot, "customBellSets");
        if (ptCustSets && cJSON_IsArray(ptCustSets))
        {
            int iSetSize = cJSON_GetArraySize(ptCustSets);
            if ((uint32_t)iSetSize > SCHEDULE_MAX_CUSTOM_BELL_SETS) iSetSize = SCHEDULE_MAX_CUSTOM_BELL_SETS;

            for (int i = 0; i < iSetSize; i++)
            {
                cJSON* ptSetItem = cJSON_GetArrayItem(ptCustSets, i);
                cJSON* ptBells = cJSON_GetObjectItem(ptSetItem, "bells");
                if (ptBells && cJSON_IsArray(ptBells))
                {
                    EXCEPTION_CUSTOM_BELLS_T* ptSet = &ptData->atCustomBellSets[ptData->ulCustomBellSetCount];
                    uint32_t ulTmpCount = 0;
                    parseBellArray(ptBells, ptSet->atBells, &ulTmpCount, SCHEDULE_MAX_CUSTOM_BELLS);
                    ptSet->ucBellCount = (uint8_t)ulTmpCount;
                    ptData->ulCustomBellSetCount++;
                }
            }
        }
    }
    else
    {
        /* Old split format — migrate */
        migrateOldExceptions(ptRoot, ptData);
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

    /* Unified exceptions */
    cJSON* ptExArr = cJSON_AddArrayToObject(ptRoot, "exceptions");
    for (uint32_t i = 0; i < ptData->ulExceptionCount; i++)
    {
        const EXCEPTION_ENTRY_T* ptEx = &ptData->atExceptions[i];
        cJSON* ptItem = cJSON_CreateObject();
        cJSON_AddStringToObject(ptItem, "startDate", ptEx->acStartDate);
        cJSON_AddStringToObject(ptItem, "endDate", ptEx->acEndDate);
        cJSON_AddStringToObject(ptItem, "label", ptEx->acLabel);
        cJSON_AddStringToObject(ptItem, "action", actionToStr(ptEx->eAction));
        cJSON_AddNumberToObject(ptItem, "timeOffsetMin", ptEx->iTimeOffsetMin);
        cJSON_AddNumberToObject(ptItem, "templateIdx", ptEx->ucTemplateIdx);
        cJSON_AddNumberToObject(ptItem, "customBellsIdx",
                                ptEx->ucCustomBellsIdx == 0xFF ? -1 : ptEx->ucCustomBellsIdx);
        cJSON_AddItemToArray(ptExArr, ptItem);
    }

    /* Custom bell sets */
    cJSON* ptCustArr = cJSON_AddArrayToObject(ptRoot, "customBellSets");
    for (uint32_t i = 0; i < ptData->ulCustomBellSetCount; i++)
    {
        cJSON* ptSetItem = cJSON_CreateObject();
        cJSON* ptBells = bellsToJsonArray(ptData->atCustomBellSets[i].atBells,
                                          ptData->atCustomBellSets[i].ucBellCount);
        cJSON_AddItemToObject(ptSetItem, "bells", ptBells);
        cJSON_AddItemToArray(ptCustArr, ptSetItem);
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
Schedule_Data_ExceptionsToJson(const EXCEPTION_ENTRY_T* ptExceptions, uint32_t ulCount,
                                const EXCEPTION_CUSTOM_BELLS_T* ptCustomSets, uint32_t ulCustomCount)
{
    cJSON* ptRoot = cJSON_CreateObject();

    cJSON* ptExArr = cJSON_AddArrayToObject(ptRoot, "exceptions");
    for (uint32_t i = 0; i < ulCount; i++)
    {
        const EXCEPTION_ENTRY_T* ptEx = &ptExceptions[i];
        cJSON* ptItem = cJSON_CreateObject();
        cJSON_AddStringToObject(ptItem, "startDate", ptEx->acStartDate);
        cJSON_AddStringToObject(ptItem, "endDate", ptEx->acEndDate);
        cJSON_AddStringToObject(ptItem, "label", ptEx->acLabel);
        cJSON_AddStringToObject(ptItem, "action", actionToStr(ptEx->eAction));
        cJSON_AddNumberToObject(ptItem, "timeOffsetMin", ptEx->iTimeOffsetMin);
        cJSON_AddNumberToObject(ptItem, "templateIdx", ptEx->ucTemplateIdx);
        cJSON_AddNumberToObject(ptItem, "customBellsIdx",
                                ptEx->ucCustomBellsIdx == 0xFF ? -1 : ptEx->ucCustomBellsIdx);
        cJSON_AddItemToArray(ptExArr, ptItem);
    }

    cJSON* ptCustArr = cJSON_AddArrayToObject(ptRoot, "customBellSets");
    for (uint32_t i = 0; i < ulCustomCount; i++)
    {
        cJSON* ptSetItem = cJSON_CreateObject();
        cJSON* ptBells = bellsToJsonArray(ptCustomSets[i].atBells, ptCustomSets[i].ucBellCount);
        cJSON_AddItemToObject(ptSetItem, "bells", ptBells);
        cJSON_AddItemToArray(ptCustArr, ptSetItem);
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

        /* Copy holidays from flashed defaults if present */
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
        }
        else
        {
            cJSON_AddItemToObject(ptRoot, "holidays", cJSON_CreateArray());
        }

        /* Empty unified exceptions */
        cJSON_AddItemToObject(ptRoot, "exceptions", cJSON_CreateArray());
        cJSON_AddItemToObject(ptRoot, "customBellSets", cJSON_CreateArray());

        writeJsonFile(SCHEDULE_FILE_CALENDAR, ptRoot);
        cJSON_Delete(ptRoot);
        ESP_LOGI(TAG, "Created default calendar.json");
    }

    /* Templates */
    if (!SPIFFS_FileExists(SCHEDULE_FILE_TEMPLATES))
    {
        cJSON* ptRoot = cJSON_CreateObject();
        cJSON_AddItemToObject(ptRoot, "templates", cJSON_CreateArray());
        writeJsonFile(SCHEDULE_FILE_TEMPLATES, ptRoot);
        cJSON_Delete(ptRoot);
        ESP_LOGI(TAG, "Created default templates.json");
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

    /* Remove expired unified exceptions */
    uint32_t ulNewCount = 0;
    for (uint32_t i = 0; i < ptData->ulExceptionCount; i++)
    {
        /* For date-range exceptions, use endDate; for single-day, use startDate */
        const char* pcExpDate = ptData->atExceptions[i].acEndDate[0]
                                ? ptData->atExceptions[i].acEndDate
                                : ptData->atExceptions[i].acStartDate;
        int iExOrd = dateStrToOrdinal(pcExpDate);
        if (iExOrd >= 0 && iExOrd < iTodayOrd)
        {
            ESP_LOGI(TAG, "Cleaning up expired exception: %s (%s)",
                     ptData->atExceptions[i].acStartDate,
                     ptData->atExceptions[i].acLabel);
            bChanged = true;
            continue;
        }
        if (ulNewCount != i)
        {
            ptData->atExceptions[ulNewCount] = ptData->atExceptions[i];
        }
        ulNewCount++;
    }
    ptData->ulExceptionCount = ulNewCount;

    /* Garbage-collect unreferenced custom bell sets */
    if (bChanged && ptData->ulCustomBellSetCount > 0)
    {
        bool abUsed[SCHEDULE_MAX_CUSTOM_BELL_SETS] = { false };
        for (uint32_t i = 0; i < ptData->ulExceptionCount; i++)
        {
            if (ptData->atExceptions[i].eAction == EXCEPTION_ACTION_CUSTOM &&
                ptData->atExceptions[i].ucCustomBellsIdx < ptData->ulCustomBellSetCount)
            {
                abUsed[ptData->atExceptions[i].ucCustomBellsIdx] = true;
            }
        }

        /* Compact: remove unused and remap indices */
        uint8_t aucRemap[SCHEDULE_MAX_CUSTOM_BELL_SETS];
        uint32_t ulNewSetCount = 0;
        for (uint32_t i = 0; i < ptData->ulCustomBellSetCount; i++)
        {
            if (abUsed[i])
            {
                aucRemap[i] = (uint8_t)ulNewSetCount;
                if (ulNewSetCount != i)
                    ptData->atCustomBellSets[ulNewSetCount] = ptData->atCustomBellSets[i];
                ulNewSetCount++;
            }
            else
            {
                aucRemap[i] = 0xFF;
            }
        }
        ptData->ulCustomBellSetCount = ulNewSetCount;

        /* Update exception references */
        for (uint32_t i = 0; i < ptData->ulExceptionCount; i++)
        {
            if (ptData->atExceptions[i].ucCustomBellsIdx != 0xFF &&
                ptData->atExceptions[i].ucCustomBellsIdx < SCHEDULE_MAX_CUSTOM_BELL_SETS)
            {
                ptData->atExceptions[i].ucCustomBellsIdx = aucRemap[ptData->atExceptions[i].ucCustomBellsIdx];
            }
        }
    }

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
        ESP_LOGI(TAG, "Calendar cleaned: %"PRIu32" holidays, %"PRIu32" exceptions remaining",
                 ptData->ulHolidayCount, ptData->ulExceptionCount);
    }

    free(ptData);
    return err;
}

/* ================================================================== */
/* Bell templates                                                      */
/* ================================================================== */

esp_err_t
Schedule_Data_LoadTemplates(SCHEDULE_DATA_T* ptData)
{
    if (NULL == ptData) return ESP_ERR_INVALID_ARG;

    ptData->ulTemplateCount = 0;

    cJSON* ptRoot = readJsonFile(SCHEDULE_FILE_TEMPLATES);
    if (NULL == ptRoot) return ESP_ERR_NOT_FOUND;

    cJSON* ptArr = cJSON_GetObjectItem(ptRoot, "templates");
    if (ptArr && cJSON_IsArray(ptArr))
    {
        int iSize = cJSON_GetArraySize(ptArr);
        if ((uint32_t)iSize > SCHEDULE_MAX_TEMPLATES) iSize = SCHEDULE_MAX_TEMPLATES;

        for (int i = 0; i < iSize; i++)
        {
            cJSON* ptItem = cJSON_GetArrayItem(ptArr, i);
            BELL_TEMPLATE_T* ptTpl = &ptData->atTemplates[ptData->ulTemplateCount];
            memset(ptTpl, 0, sizeof(BELL_TEMPLATE_T));

            cJSON* ptName = cJSON_GetObjectItem(ptItem, "name");
            if (ptName && cJSON_IsString(ptName))
                strncpy(ptTpl->acName, ptName->valuestring, SCHEDULE_TEMPLATE_NAME_LEN - 1);

            cJSON* ptBells = cJSON_GetObjectItem(ptItem, "bells");
            if (ptBells && cJSON_IsArray(ptBells))
            {
                uint32_t ulTmpCount = 0;
                parseBellArray(ptBells, ptTpl->atBells, &ulTmpCount, SCHEDULE_MAX_CUSTOM_BELLS);
                ptTpl->ucBellCount = (uint8_t)ulTmpCount;
            }

            ptData->ulTemplateCount++;
        }
    }

    cJSON_Delete(ptRoot);
    return ESP_OK;
}

esp_err_t
Schedule_Data_SaveTemplates(const SCHEDULE_DATA_T* ptData)
{
    if (NULL == ptData) return ESP_ERR_INVALID_ARG;

    cJSON* ptRoot = cJSON_CreateObject();
    cJSON* ptArr = cJSON_AddArrayToObject(ptRoot, "templates");

    for (uint32_t i = 0; i < ptData->ulTemplateCount; i++)
    {
        cJSON* ptItem = cJSON_CreateObject();
        cJSON_AddStringToObject(ptItem, "name", ptData->atTemplates[i].acName);
        cJSON* ptBells = bellsToJsonArray(ptData->atTemplates[i].atBells,
                                          ptData->atTemplates[i].ucBellCount);
        cJSON_AddItemToObject(ptItem, "bells", ptBells);
        cJSON_AddItemToArray(ptArr, ptItem);
    }

    esp_err_t err = writeJsonFile(SCHEDULE_FILE_TEMPLATES, ptRoot);
    cJSON_Delete(ptRoot);
    return err;
}

cJSON*
Schedule_Data_TemplatesToJson(const BELL_TEMPLATE_T* ptTemplates, uint32_t ulCount)
{
    cJSON* ptRoot = cJSON_CreateObject();
    cJSON* ptArr = cJSON_AddArrayToObject(ptRoot, "templates");

    for (uint32_t i = 0; i < ulCount; i++)
    {
        cJSON* ptItem = cJSON_CreateObject();
        cJSON_AddStringToObject(ptItem, "name", ptTemplates[i].acName);
        cJSON* ptBells = bellsToJsonArray(ptTemplates[i].atBells, ptTemplates[i].ucBellCount);
        cJSON_AddItemToObject(ptItem, "bells", ptBells);
        cJSON_AddItemToArray(ptArr, ptItem);
    }

    return ptRoot;
}
