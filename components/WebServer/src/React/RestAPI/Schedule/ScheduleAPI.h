#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "Scheduler_API.h"

typedef struct _SCHEDULE_API_RSC_T* SCHEDULE_API_H;

typedef struct
{
    SCHEDULER_H hScheduler;
} SCHEDULE_API_PARAMS_T;

/**
 * @brief Initialise ScheduleAPI resource.
 */
esp_err_t ScheduleAPI_Init(const SCHEDULE_API_PARAMS_T* ptParams, SCHEDULE_API_H* phApi);

/**
 * @brief Register all schedule-related HTTP endpoints.
 */
esp_err_t ScheduleAPI_Register(SCHEDULE_API_H hApi, httpd_handle_t hHttpServer);
