#include "ScheduleAPI.h"
#include "Schedule_Data.h"
#include "Scheduler_API.h"
#include "RingBell_API.h"
#include "TimeSync_API.h"
#include "Auth/WS_Auth.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char* TAG = "schedule_api";

#define REQ_BODY_MAX 8192

/* ================================================================== */
/* Resource                                                            */
/* ================================================================== */

typedef struct _SCHEDULE_API_RSC_T
{
    SCHEDULER_H hScheduler;
} SCHEDULE_API_RSC_T;

/* ================================================================== */
/* Helpers                                                             */
/* ================================================================== */

static esp_err_t
sendJson(httpd_req_t* ptReq, cJSON* ptRoot)
{
    const char* pcJson = cJSON_PrintUnformatted(ptRoot);
    httpd_resp_set_type(ptReq, "application/json");
    httpd_resp_sendstr(ptReq, pcJson);
    free((void*)pcJson);
    cJSON_Delete(ptRoot);
    return ESP_OK;
}

static esp_err_t
sendError(httpd_req_t* ptReq, const char* pcStatus, const char* pcMsg)
{
    cJSON* ptRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(ptRoot, "error", pcMsg);
    const char* pcJson = cJSON_PrintUnformatted(ptRoot);
    httpd_resp_set_status(ptReq, pcStatus);
    httpd_resp_set_type(ptReq, "application/json");
    httpd_resp_sendstr(ptReq, pcJson);
    free((void*)pcJson);
    cJSON_Delete(ptRoot);
    return ESP_OK;
}

static int
readBody(httpd_req_t* ptReq, char* pcBuf, size_t ulBufSize)
{
    int iLen = httpd_req_recv(ptReq, pcBuf, ulBufSize - 1);
    if (iLen > 0)
    {
        pcBuf[iLen] = '\0';
    }
    return iLen;
}

/* ================================================================== */
/* GET /api/schedule/settings                                          */
/* ================================================================== */

static esp_err_t
handler_GetSettings(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    SCHEDULE_SETTINGS_T tSettings;
    Schedule_Data_LoadSettings(&tSettings);

    cJSON* ptRoot = Schedule_Data_SettingsToJson(&tSettings);
    return sendJson(ptReq, ptRoot);
}

/* ================================================================== */
/* POST /api/schedule/settings                                         */
/* ================================================================== */

static esp_err_t
handler_PostSettings(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    SCHEDULE_API_RSC_T* ptRsc = (SCHEDULE_API_RSC_T*)ptReq->user_ctx;

    char acBuf[1024];
    int iLen = readBody(ptReq, acBuf, sizeof(acBuf));
    if (iLen <= 0) return sendError(ptReq, "400 Bad Request", "Empty body");

    cJSON* ptRoot = cJSON_Parse(acBuf);
    if (!ptRoot) return sendError(ptReq, "400 Bad Request", "Invalid JSON");

    SCHEDULE_SETTINGS_T tSettings = { 0 };

    cJSON* ptTz = cJSON_GetObjectItem(ptRoot, "timezone");
    if (ptTz && cJSON_IsString(ptTz))
    {
        strncpy(tSettings.acTimezone, ptTz->valuestring, sizeof(tSettings.acTimezone) - 1);
        /* Also update system timezone */
        TimeSync_SetTimezone(ptTz->valuestring);
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
                int d = ptDay->valueint;
                if (d >= 0 && d <= 6) tSettings.abWorkingDays[d] = true;
            }
        }
    }

    cJSON_Delete(ptRoot);

    esp_err_t err = Schedule_Data_SaveSettings(&tSettings);
    if (err != ESP_OK) return sendError(ptReq, "500 Internal Server Error", "Failed to save");

    Scheduler_ReloadSchedule(ptRsc->hScheduler);

    cJSON* ptResp = cJSON_CreateObject();
    cJSON_AddStringToObject(ptResp, "status", "ok");
    return sendJson(ptReq, ptResp);
}

/* ================================================================== */
/* GET /api/schedule/bells                                             */
/* ================================================================== */

static esp_err_t
handler_GetBells(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    SCHEDULE_SHIFT_T tFirst, tSecond;
    Schedule_Data_LoadBells(&tFirst, &tSecond);

    cJSON* ptRoot = Schedule_Data_BellsToJson(&tFirst, &tSecond);
    return sendJson(ptReq, ptRoot);
}

/* ================================================================== */
/* POST /api/schedule/bells                                            */
/* ================================================================== */

static void
parseBellArrayFromJson(cJSON* ptArr, BELL_ENTRY_T* ptBells, uint32_t* pulCount, uint32_t ulMax)
{
    *pulCount = 0;
    if (!ptArr || !cJSON_IsArray(ptArr)) return;

    int iSize = cJSON_GetArraySize(ptArr);
    if ((uint32_t)iSize > ulMax) iSize = (int)ulMax;

    for (int i = 0; i < iSize; i++)
    {
        cJSON* ptItem = cJSON_GetArrayItem(ptArr, i);
        cJSON* ptH = cJSON_GetObjectItem(ptItem, "hour");
        cJSON* ptM = cJSON_GetObjectItem(ptItem, "minute");
        cJSON* ptD = cJSON_GetObjectItem(ptItem, "durationSec");
        cJSON* ptL = cJSON_GetObjectItem(ptItem, "label");

        if (cJSON_IsNumber(ptH) && cJSON_IsNumber(ptM))
        {
            ptBells[*pulCount].ucHour = (uint8_t)ptH->valueint;
            ptBells[*pulCount].ucMinute = (uint8_t)ptM->valueint;
            ptBells[*pulCount].usDurationSec = (ptD && cJSON_IsNumber(ptD)) ? (uint16_t)ptD->valueint : 3;
            memset(ptBells[*pulCount].acLabel, 0, SCHEDULE_LABEL_MAX_LEN);
            if (ptL && cJSON_IsString(ptL))
            {
                strncpy(ptBells[*pulCount].acLabel, ptL->valuestring, SCHEDULE_LABEL_MAX_LEN - 1);
            }
            (*pulCount)++;
        }
    }
}

static void
parseShiftFromJson(cJSON* ptShiftObj, SCHEDULE_SHIFT_T* ptShift)
{
    ptShift->bEnabled = true;
    ptShift->ulBellCount = 0;

    if (!ptShiftObj || !cJSON_IsObject(ptShiftObj)) return;

    cJSON* ptEnabled = cJSON_GetObjectItem(ptShiftObj, "enabled");
    if (ptEnabled && cJSON_IsBool(ptEnabled))
    {
        ptShift->bEnabled = cJSON_IsTrue(ptEnabled);
    }

    cJSON* ptArr = cJSON_GetObjectItem(ptShiftObj, "bells");
    parseBellArrayFromJson(ptArr, ptShift->atBells, &ptShift->ulBellCount, SCHEDULE_MAX_BELLS_PER_SHIFT);
}

static esp_err_t
handler_PostBells(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    SCHEDULE_API_RSC_T* ptRsc = (SCHEDULE_API_RSC_T*)ptReq->user_ctx;

    char* pcBuf = (char*)malloc(REQ_BODY_MAX);
    if (!pcBuf) return sendError(ptReq, "500 Internal Server Error", "Out of memory");

    int iLen = readBody(ptReq, pcBuf, REQ_BODY_MAX);
    if (iLen <= 0) { free(pcBuf); return sendError(ptReq, "400 Bad Request", "Empty body"); }

    cJSON* ptRoot = cJSON_Parse(pcBuf);
    free(pcBuf);
    if (!ptRoot) return sendError(ptReq, "400 Bad Request", "Invalid JSON");

    SCHEDULE_SHIFT_T tFirst  = { .bEnabled = true,  .ulBellCount = 0 };
    SCHEDULE_SHIFT_T tSecond = { .bEnabled = false, .ulBellCount = 0 };

    cJSON* ptFirstShift  = cJSON_GetObjectItem(ptRoot, "firstShift");
    cJSON* ptSecondShift = cJSON_GetObjectItem(ptRoot, "secondShift");

    if (!ptFirstShift && !ptSecondShift)
    {
        cJSON_Delete(ptRoot);
        return sendError(ptReq, "400 Bad Request", "Missing 'firstShift' or 'secondShift'");
    }

    parseShiftFromJson(ptFirstShift, &tFirst);
    parseShiftFromJson(ptSecondShift, &tSecond);

    cJSON_Delete(ptRoot);

    esp_err_t err = Schedule_Data_SaveBells(&tFirst, &tSecond);
    if (err != ESP_OK) return sendError(ptReq, "500 Internal Server Error", "Failed to save");

    Scheduler_ReloadSchedule(ptRsc->hScheduler);

    cJSON* ptResp = cJSON_CreateObject();
    cJSON_AddStringToObject(ptResp, "status", "ok");
    return sendJson(ptReq, ptResp);
}

/* ================================================================== */
/* GET /api/schedule/holidays                                          */
/* ================================================================== */

static esp_err_t
handler_GetHolidays(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    SCHEDULE_DATA_T* ptData = (SCHEDULE_DATA_T*)calloc(1, sizeof(SCHEDULE_DATA_T));
    if (!ptData) return sendError(ptReq, "500 Internal Server Error", "Out of memory");

    Schedule_Data_LoadCalendar(ptData);
    cJSON* ptRoot = Schedule_Data_HolidaysToJson(ptData->atHolidays, ptData->ulHolidayCount);
    free(ptData);

    return sendJson(ptReq, ptRoot);
}

/* ================================================================== */
/* POST /api/schedule/holidays                                         */
/* ================================================================== */

static esp_err_t
handler_PostHolidays(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    SCHEDULE_API_RSC_T* ptRsc = (SCHEDULE_API_RSC_T*)ptReq->user_ctx;

    char* pcBuf = (char*)malloc(REQ_BODY_MAX);
    if (!pcBuf) return sendError(ptReq, "500 Internal Server Error", "Out of memory");

    int iLen = readBody(ptReq, pcBuf, REQ_BODY_MAX);
    if (iLen <= 0) { free(pcBuf); return sendError(ptReq, "400 Bad Request", "Empty body"); }

    cJSON* ptRoot = cJSON_Parse(pcBuf);
    free(pcBuf);
    if (!ptRoot) return sendError(ptReq, "400 Bad Request", "Invalid JSON");

    /* Load existing calendar to preserve exceptions */
    SCHEDULE_DATA_T* ptData = (SCHEDULE_DATA_T*)calloc(1, sizeof(SCHEDULE_DATA_T));
    if (!ptData) { cJSON_Delete(ptRoot); return sendError(ptReq, "500 Internal Server Error", "OOM"); }

    Schedule_Data_LoadCalendar(ptData);

    /* Replace holidays */
    ptData->ulHolidayCount = 0;
    cJSON* ptArr = cJSON_GetObjectItem(ptRoot, "holidays");
    if (ptArr && cJSON_IsArray(ptArr))
    {
        int iSize = cJSON_GetArraySize(ptArr);
        if ((uint32_t)iSize > SCHEDULE_MAX_HOLIDAYS) iSize = SCHEDULE_MAX_HOLIDAYS;

        for (int i = 0; i < iSize; i++)
        {
            cJSON* ptItem = cJSON_GetArrayItem(ptArr, i);
            cJSON* ptS = cJSON_GetObjectItem(ptItem, "startDate");
            cJSON* ptE = cJSON_GetObjectItem(ptItem, "endDate");
            cJSON* ptL = cJSON_GetObjectItem(ptItem, "label");

            if (ptS && cJSON_IsString(ptS) && ptE && cJSON_IsString(ptE))
            {
                HOLIDAY_T* ptH = &ptData->atHolidays[ptData->ulHolidayCount];
                strncpy(ptH->acStartDate, ptS->valuestring, SCHEDULE_DATE_STR_LEN - 1);
                strncpy(ptH->acEndDate, ptE->valuestring, SCHEDULE_DATE_STR_LEN - 1);
                memset(ptH->acLabel, 0, SCHEDULE_LABEL_MAX_LEN);
                if (ptL && cJSON_IsString(ptL))
                {
                    strncpy(ptH->acLabel, ptL->valuestring, SCHEDULE_LABEL_MAX_LEN - 1);
                }
                ptData->ulHolidayCount++;
            }
        }
    }

    cJSON_Delete(ptRoot);

    esp_err_t err = Schedule_Data_SaveCalendar(ptData);
    free(ptData);

    if (err != ESP_OK) return sendError(ptReq, "500 Internal Server Error", "Failed to save");

    Scheduler_ReloadSchedule(ptRsc->hScheduler);

    cJSON* ptResp = cJSON_CreateObject();
    cJSON_AddStringToObject(ptResp, "status", "ok");
    return sendJson(ptReq, ptResp);
}

/* ================================================================== */
/* GET /api/schedule/exceptions                                        */
/* ================================================================== */

static esp_err_t
handler_GetExceptions(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    SCHEDULE_DATA_T* ptData = (SCHEDULE_DATA_T*)calloc(1, sizeof(SCHEDULE_DATA_T));
    if (!ptData) return sendError(ptReq, "500 Internal Server Error", "Out of memory");

    Schedule_Data_LoadCalendar(ptData);
    cJSON* ptRoot = Schedule_Data_ExceptionsToJson(
        ptData->atExceptionWorking, ptData->ulExceptionWorkingCount,
        ptData->atExceptionHoliday, ptData->ulExceptionHolidayCount);
    free(ptData);

    return sendJson(ptReq, ptRoot);
}

/* ================================================================== */
/* POST /api/schedule/exceptions                                       */
/* ================================================================== */

static esp_err_t
handler_PostExceptions(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    SCHEDULE_API_RSC_T* ptRsc = (SCHEDULE_API_RSC_T*)ptReq->user_ctx;

    char* pcBuf = (char*)malloc(REQ_BODY_MAX);
    if (!pcBuf) return sendError(ptReq, "500 Internal Server Error", "Out of memory");

    int iLen = readBody(ptReq, pcBuf, REQ_BODY_MAX);
    if (iLen <= 0) { free(pcBuf); return sendError(ptReq, "400 Bad Request", "Empty body"); }

    cJSON* ptRoot = cJSON_Parse(pcBuf);
    free(pcBuf);
    if (!ptRoot) return sendError(ptReq, "400 Bad Request", "Invalid JSON");

    /* Load existing calendar to preserve holidays */
    SCHEDULE_DATA_T* ptData = (SCHEDULE_DATA_T*)calloc(1, sizeof(SCHEDULE_DATA_T));
    if (!ptData) { cJSON_Delete(ptRoot); return sendError(ptReq, "500 Internal Server Error", "OOM"); }

    Schedule_Data_LoadCalendar(ptData);

    /* Replace exception working days */
    ptData->ulExceptionWorkingCount = 0;
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
                memset(ptEx, 0, sizeof(EXCEPTION_WORKING_T));
                strncpy(ptEx->acDate, ptDate->valuestring, SCHEDULE_DATE_STR_LEN - 1);
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

                cJSON* ptCustom = cJSON_GetObjectItem(ptItem, "customBells");
                if (ptCustom && cJSON_IsArray(ptCustom) && cJSON_GetArraySize(ptCustom) > 0)
                {
                    ptEx->bHasCustomBells = true;
                    ptEx->ulCustomBellCount = 0;
                    int iBellSize = cJSON_GetArraySize(ptCustom);
                    if ((uint32_t)iBellSize > SCHEDULE_MAX_BELLS) iBellSize = SCHEDULE_MAX_BELLS;

                    for (int j = 0; j < iBellSize; j++)
                    {
                        cJSON* ptB = cJSON_GetArrayItem(ptCustom, j);
                        cJSON* ptBH = cJSON_GetObjectItem(ptB, "hour");
                        cJSON* ptBM = cJSON_GetObjectItem(ptB, "minute");
                        cJSON* ptBD = cJSON_GetObjectItem(ptB, "durationSec");
                        cJSON* ptBL = cJSON_GetObjectItem(ptB, "label");

                        if (cJSON_IsNumber(ptBH) && cJSON_IsNumber(ptBM))
                        {
                            BELL_ENTRY_T* ptBell = &ptEx->atCustomBells[ptEx->ulCustomBellCount];
                            ptBell->ucHour = (uint8_t)ptBH->valueint;
                            ptBell->ucMinute = (uint8_t)ptBM->valueint;
                            ptBell->usDurationSec = (ptBD && cJSON_IsNumber(ptBD)) ? (uint16_t)ptBD->valueint : 3;
                            memset(ptBell->acLabel, 0, SCHEDULE_LABEL_MAX_LEN);
                            if (ptBL && cJSON_IsString(ptBL))
                            {
                                strncpy(ptBell->acLabel, ptBL->valuestring, SCHEDULE_LABEL_MAX_LEN - 1);
                            }
                            ptEx->ulCustomBellCount++;
                        }
                    }
                }

                ptData->ulExceptionWorkingCount++;
            }
        }
    }

    /* Replace exception holidays */
    ptData->ulExceptionHolidayCount = 0;
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

    esp_err_t err = Schedule_Data_SaveCalendar(ptData);
    free(ptData);

    if (err != ESP_OK) return sendError(ptReq, "500 Internal Server Error", "Failed to save");

    Scheduler_ReloadSchedule(ptRsc->hScheduler);

    cJSON* ptResp = cJSON_CreateObject();
    cJSON_AddStringToObject(ptResp, "status", "ok");
    return sendJson(ptReq, ptResp);
}

/* ================================================================== */
/* GET /api/bell/status                                                */
/* ================================================================== */

static esp_err_t
handler_GetBellStatus(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    SCHEDULE_API_RSC_T* ptRsc = (SCHEDULE_API_RSC_T*)ptReq->user_ctx;

    SCHEDULER_STATUS_T tStatus;
    Scheduler_GetStatus(ptRsc->hScheduler, &tStatus);

    cJSON* ptRoot = cJSON_CreateObject();

    /* Bell state */
    BELL_STATE_E eState = RingBell_GetState();
    const char* pcState = "idle";
    if (eState == BELL_STATE_RINGING) pcState = "ringing";
    else if (eState == BELL_STATE_PANIC) pcState = "panic";
    cJSON_AddStringToObject(ptRoot, "bellState", pcState);
    cJSON_AddBoolToObject(ptRoot, "panicMode", RingBell_IsPanic());

    /* Day type */
    const char* pcDayTypes[] = { "off", "working", "holiday", "exceptionWorking", "exceptionHoliday" };
    cJSON_AddStringToObject(ptRoot, "dayType", pcDayTypes[tStatus.eDayType]);

    /* Time */
    cJSON_AddBoolToObject(ptRoot, "timeSynced", tStatus.bTimeSynced);
    char acTime[20];
    snprintf(acTime, sizeof(acTime), "%02d:%02d:%02d",
             tStatus.tCurrentTime.tm_hour, tStatus.tCurrentTime.tm_min, tStatus.tCurrentTime.tm_sec);
    cJSON_AddStringToObject(ptRoot, "currentTime", acTime);

    char acDate[SCHEDULE_DATE_STR_LEN];
    snprintf(acDate, sizeof(acDate), "%04d-%02d-%02d",
             tStatus.tCurrentTime.tm_year + 1900, tStatus.tCurrentTime.tm_mon + 1, tStatus.tCurrentTime.tm_mday);
    cJSON_AddStringToObject(ptRoot, "currentDate", acDate);

    /* Next bell */
    if (tStatus.tNextBell.bValid)
    {
        cJSON* ptNext = cJSON_CreateObject();
        char acNextTime[6];
        snprintf(acNextTime, sizeof(acNextTime), "%02d:%02d",
                 tStatus.tNextBell.ucHour, tStatus.tNextBell.ucMinute);
        cJSON_AddStringToObject(ptNext, "time", acNextTime);
        cJSON_AddNumberToObject(ptNext, "durationSec", tStatus.tNextBell.usDurationSec);
        cJSON_AddStringToObject(ptNext, "label", tStatus.tNextBell.acLabel);
        cJSON_AddItemToObject(ptRoot, "nextBell", ptNext);
    }
    else
    {
        cJSON_AddNullToObject(ptRoot, "nextBell");
    }

    return sendJson(ptReq, ptRoot);
}

/* ================================================================== */
/* POST /api/bell/panic                                                */
/* ================================================================== */

static esp_err_t
handler_PostPanic(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    char acBuf[128];
    int iLen = readBody(ptReq, acBuf, sizeof(acBuf));
    if (iLen <= 0) return sendError(ptReq, "400 Bad Request", "Empty body");

    cJSON* ptRoot = cJSON_Parse(acBuf);
    if (!ptRoot) return sendError(ptReq, "400 Bad Request", "Invalid JSON");

    cJSON* ptEnabled = cJSON_GetObjectItem(ptRoot, "enabled");
    if (!ptEnabled || !cJSON_IsBool(ptEnabled))
    {
        cJSON_Delete(ptRoot);
        return sendError(ptReq, "400 Bad Request", "Missing 'enabled' boolean");
    }

    bool bEnable = cJSON_IsTrue(ptEnabled);
    cJSON_Delete(ptRoot);

    ESP_LOGW(TAG, "Panic mode %s by user %s", bEnable ? "ENABLED" : "DISABLED", pcUser);
    RingBell_SetPanic(bEnable);

    cJSON* ptResp = cJSON_CreateObject();
    cJSON_AddStringToObject(ptResp, "status", "ok");
    cJSON_AddBoolToObject(ptResp, "panicMode", bEnable);
    return sendJson(ptReq, ptResp);
}

/* ================================================================== */
/* GET /api/system/time                                                */
/* ================================================================== */

static esp_err_t
handler_GetSystemTime(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    struct tm tNow;
    TimeSync_GetLocalTime(&tNow);

    cJSON* ptRoot = cJSON_CreateObject();

    char acTime[20];
    snprintf(acTime, sizeof(acTime), "%02d:%02d:%02d",
             tNow.tm_hour, tNow.tm_min, tNow.tm_sec);
    cJSON_AddStringToObject(ptRoot, "time", acTime);

    char acDate[SCHEDULE_DATE_STR_LEN];
    snprintf(acDate, sizeof(acDate), "%04d-%02d-%02d",
             tNow.tm_year + 1900, tNow.tm_mon + 1, tNow.tm_mday);
    cJSON_AddStringToObject(ptRoot, "date", acDate);

    cJSON_AddBoolToObject(ptRoot, "synced", TimeSync_IsSynced());

    char acTz[64];
    TimeSync_GetTimezone(acTz, sizeof(acTz));
    cJSON_AddStringToObject(ptRoot, "timezone", acTz);

    return sendJson(ptReq, ptRoot);
}

/* ================================================================== */
/* GET /api/schedule/defaults                                          */
/* ================================================================== */

static esp_err_t
handler_GetDefaults(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    cJSON* ptDefaults = Schedule_Data_ReadDefaultsJson();
    if (NULL == ptDefaults)
    {
        return sendError(ptReq, "404 Not Found", "No default schedule available");
    }

    return sendJson(ptReq, ptDefaults);
}

/* ================================================================== */
/* POST /api/bell/test                                                 */
/* ================================================================== */

static esp_err_t
handler_PostTestBell(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    if (RingBell_IsPanic())
    {
        return sendError(ptReq, "409 Conflict", "Cannot test bell while panic mode is active");
    }

    uint32_t ulDuration = 3; /* default 3 seconds */

    char acBuf[128];
    int iLen = readBody(ptReq, acBuf, sizeof(acBuf));
    if (iLen > 0)
    {
        cJSON* ptRoot = cJSON_Parse(acBuf);
        if (ptRoot)
        {
            cJSON* ptDur = cJSON_GetObjectItem(ptRoot, "durationSec");
            if (ptDur && cJSON_IsNumber(ptDur))
            {
                int iVal = ptDur->valueint;
                if (iVal >= 1 && iVal <= 30) ulDuration = (uint32_t)iVal;
            }
            cJSON_Delete(ptRoot);
        }
    }

    ESP_LOGI(TAG, "Test bell for %lu seconds (by %s)", (unsigned long)ulDuration, pcUser);
    RingBell_RunForDuration(ulDuration);

    cJSON* ptResp = cJSON_CreateObject();
    cJSON_AddStringToObject(ptResp, "status", "ok");
    cJSON_AddNumberToObject(ptResp, "durationSec", (double)ulDuration);
    return sendJson(ptReq, ptResp);
}

/* ================================================================== */
/* GET /api/system/info                                                */
/* ================================================================== */

static esp_err_t
handler_GetSystemInfo(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    cJSON* ptRoot = cJSON_CreateObject();

    /* Uptime */
    int64_t llUptimeUs = esp_timer_get_time();
    uint32_t ulUptimeSec = (uint32_t)(llUptimeUs / 1000000ULL);
    cJSON_AddNumberToObject(ptRoot, "uptimeSec", (double)ulUptimeSec);

    /* Heap */
    cJSON_AddNumberToObject(ptRoot, "freeHeap", (double)esp_get_free_heap_size());
    cJSON_AddNumberToObject(ptRoot, "minFreeHeap", (double)esp_get_minimum_free_heap_size());

    /* Chip info */
    esp_chip_info_t tChip;
    esp_chip_info(&tChip);
    cJSON_AddNumberToObject(ptRoot, "chipCores", tChip.cores);

    /* IDF version */
    cJSON_AddStringToObject(ptRoot, "idfVersion", esp_get_idf_version());

    /* Time info */
    struct tm tNow;
    TimeSync_GetLocalTime(&tNow);
    char acTime[20];
    snprintf(acTime, sizeof(acTime), "%02d:%02d:%02d",
             tNow.tm_hour, tNow.tm_min, tNow.tm_sec);
    cJSON_AddStringToObject(ptRoot, "time", acTime);

    char acDate[SCHEDULE_DATE_STR_LEN];
    snprintf(acDate, sizeof(acDate), "%04d-%02d-%02d",
             tNow.tm_year + 1900, tNow.tm_mon + 1, tNow.tm_mday);
    cJSON_AddStringToObject(ptRoot, "date", acDate);

    cJSON_AddBoolToObject(ptRoot, "timeSynced", TimeSync_IsSynced());

    char acTz[64];
    TimeSync_GetTimezone(acTz, sizeof(acTz));
    cJSON_AddStringToObject(ptRoot, "timezone", acTz);

    return sendJson(ptReq, ptRoot);
}

/* ================================================================== */
/* POST /api/system/reboot                                             */
/* ================================================================== */

static void
rebootTimerCallback(void* pvArg)
{
    (void)pvArg;
    esp_restart();
}

static esp_err_t
handler_PostReboot(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    ESP_LOGW(TAG, "System reboot requested by user %s", pcUser);

    cJSON* ptResp = cJSON_CreateObject();
    cJSON_AddStringToObject(ptResp, "status", "ok");
    cJSON_AddStringToObject(ptResp, "message", "Rebooting in 1 second");
    sendJson(ptReq, ptResp);

    /* Delay reboot so the HTTP response can be sent */
    const esp_timer_create_args_t tTimerArgs = {
        .callback = rebootTimerCallback,
        .name = "reboot_timer",
    };
    esp_timer_handle_t hTimer;
    esp_timer_create(&tTimerArgs, &hTimer);
    esp_timer_start_once(hTimer, 1000000); /* 1 second */

    return ESP_OK;
}

/* ================================================================== */
/* POST /api/system/factory-reset                                      */
/* ================================================================== */

static esp_err_t
handler_PostFactoryReset(httpd_req_t* ptReq)
{
    const char* pcUser; const char* pcRole;
    if (auth_require_bearer(ptReq, &pcUser, &pcRole) != ESP_OK) return ESP_OK;

    SCHEDULE_API_RSC_T* ptRsc = (SCHEDULE_API_RSC_T*)ptReq->user_ctx;

    ESP_LOGW(TAG, "Factory reset requested by user %s", pcUser);

    /* Remove current config files so CreateDefaults will regenerate them */
    remove(SCHEDULE_FILE_SETTINGS);
    remove(SCHEDULE_FILE_BELLS);
    remove(SCHEDULE_FILE_CALENDAR);

    /* Recreate from flashed defaults */
    Schedule_Data_CreateDefaults();

    /* Reload scheduler with fresh data */
    Scheduler_ReloadSchedule(ptRsc->hScheduler);

    /* Reset timezone to what the defaults say */
    SCHEDULE_SETTINGS_T tSettings;
    Schedule_Data_LoadSettings(&tSettings);
    TimeSync_SetTimezone(tSettings.acTimezone);

    cJSON* ptResp = cJSON_CreateObject();
    cJSON_AddStringToObject(ptResp, "status", "ok");
    cJSON_AddStringToObject(ptResp, "message", "Factory defaults restored");
    return sendJson(ptReq, ptResp);
}

/* ================================================================== */
/* Init & Register                                                     */
/* ================================================================== */

esp_err_t
ScheduleAPI_Init(const SCHEDULE_API_PARAMS_T* ptParams, SCHEDULE_API_H* phApi)
{
    if ((NULL == ptParams) || (NULL == phApi)) return ESP_ERR_INVALID_ARG;

    SCHEDULE_API_RSC_T* ptRsc = (SCHEDULE_API_RSC_T*)calloc(1, sizeof(SCHEDULE_API_RSC_T));
    if (NULL == ptRsc) return ESP_ERR_NO_MEM;

    ptRsc->hScheduler = ptParams->hScheduler;
    *phApi = ptRsc;
    return ESP_OK;
}

esp_err_t
ScheduleAPI_Register(SCHEDULE_API_H hApi, httpd_handle_t hHttpServer)
{
    if ((NULL == hApi) || (NULL == hHttpServer)) return ESP_ERR_INVALID_ARG;
    SCHEDULE_API_RSC_T* ptRsc = (SCHEDULE_API_RSC_T*)hApi;
    esp_err_t err = ESP_OK;

    const httpd_uri_t atUris[] = {
        { "/api/schedule/settings",   HTTP_GET,  handler_GetSettings,    ptRsc },
        { "/api/schedule/settings",   HTTP_POST, handler_PostSettings,   ptRsc },
        { "/api/schedule/bells",      HTTP_GET,  handler_GetBells,       ptRsc },
        { "/api/schedule/bells",      HTTP_POST, handler_PostBells,      ptRsc },
        { "/api/schedule/holidays",   HTTP_GET,  handler_GetHolidays,    ptRsc },
        { "/api/schedule/holidays",   HTTP_POST, handler_PostHolidays,   ptRsc },
        { "/api/schedule/exceptions", HTTP_GET,  handler_GetExceptions,  ptRsc },
        { "/api/schedule/exceptions", HTTP_POST, handler_PostExceptions, ptRsc },
        { "/api/bell/status",         HTTP_GET,  handler_GetBellStatus,  ptRsc },
        { "/api/bell/panic",          HTTP_POST, handler_PostPanic,      ptRsc },
        { "/api/bell/test",           HTTP_POST, handler_PostTestBell,   ptRsc },
        { "/api/system/time",         HTTP_GET,  handler_GetSystemTime,  ptRsc },
        { "/api/system/info",         HTTP_GET,  handler_GetSystemInfo,  ptRsc },
        { "/api/system/reboot",       HTTP_POST, handler_PostReboot,     ptRsc },
        { "/api/system/factory-reset",HTTP_POST, handler_PostFactoryReset, ptRsc },
        { "/api/schedule/defaults",   HTTP_GET,  handler_GetDefaults,    ptRsc },
    };

    for (size_t i = 0; i < sizeof(atUris) / sizeof(atUris[0]); i++)
    {
        err = httpd_register_uri_handler(hHttpServer, &atUris[i]);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register %s %s", 
                     atUris[i].method == HTTP_GET ? "GET" : "POST", atUris[i].uri);
            return err;
        }
    }

    ESP_LOGI(TAG, "Schedule API registered (%d endpoints)", (int)(sizeof(atUris) / sizeof(atUris[0])));
    return ESP_OK;
}
