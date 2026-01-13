#include "AppTask_API.h"
#include "Definitions/AppErrors.h"
#include "freertos/FreeRTOS.h"
#include "WiFi_Manager_API.h"
#include "NVS_API.h"
#include "Ws_API.h"
#include "FatFS/FatFS_API.h"
#include "esp_log.h"

static const char* TAG = "APP_TASK";

#define APP_TASK_STACK_BYTES        4096UL
#define APP_TASK_STACK_WORDS        ((APP_TASK_STACK_BYTES + sizeof(StackType_t) - 1) / sizeof(StackType_t))
#define APP_TASK_QUEUE_LENGTH       4UL

typedef struct _APP_TASK_RSC_T
{
    APP_TASK_PARAMS_T    tParams;               /* Init params */
    TaskHandle_t         hAppTask;              /* FreeRTOS task handle */
    QueueHandle_t        hAppTaskQueue;         /* Event queue handle */

    WIFI_MANAGER_H       hWiFiManager;          /* WiFi manager handle */
    WEB_SERVER_H         hWebServer;            /* Web server handle */
} APP_TASK_RSC_T;

static void 
appTask_Enter(void* pvArg);

static uint32_t
appTask_Init(APP_TASK_RSC_T* ptAppTaskRsc);

static uint32_t
appTask_Process(APP_TASK_RSC_T* ptAppTaskRsc);

int32_t 
AppTask_Create(APP_TASK_PARAMS_T* ptParams, APP_TASK_H* phAppTask)
{
    int32_t lResult = APP_SUCCESS;
    APP_TASK_RSC_T* ptAppTaskRsc = NULL;
    BaseType_t xTaskCreateResult = pdFAIL;

    if ((NULL == ptParams) || 
        (NULL == phAppTask))
    {
        return APP_ERROR_INVALID_PARAM;
    }

    ptAppTaskRsc = (APP_TASK_RSC_T*)calloc(1, sizeof(APP_TASK_RSC_T));
    if (NULL == ptAppTaskRsc)
    {
        lResult = APP_ERROR_OUT_OF_MEMORY;
    }
    else
    {
        // TODO : Refactor *phAppTask = (APP_TASK_H)ptAppTaskRsc; at the end only if successful
        *phAppTask = (APP_TASK_H)ptAppTaskRsc;
        ptAppTaskRsc->tParams = *ptParams;

        ptAppTaskRsc->hAppTaskQueue = xQueueCreate(APP_TASK_QUEUE_LENGTH, sizeof(APP_TASK_EVENT_T));
        if(NULL == ptAppTaskRsc->hAppTaskQueue)
        {
            lResult = APP_ERROR_QUEUE_CREATE_FAILED;
            free(ptAppTaskRsc);
            ptAppTaskRsc = NULL;
        }   

        xTaskCreateResult = xTaskCreate(appTask_Enter,       
                                        "APP_TASK",         
                                        APP_TASK_STACK_WORDS,         
                                        ptAppTaskRsc,             
                                        ptParams->ulTaskPriority,            
                                        &ptAppTaskRsc->hAppTask);

        if(pdPASS != xTaskCreateResult)
        {
            lResult = APP_ERROR_TASK_CREATE_FAILED;
            free(ptAppTaskRsc);
            ptAppTaskRsc = NULL;
        }
    }

    return lResult;
}

static void 
appTask_Enter(void* pvArg)
{
    uint32_t lResult = APP_SUCCESS;
    APP_TASK_RSC_T* ptAppTaskRsc = (APP_TASK_RSC_T*)pvArg;

    assert(NULL != ptAppTaskRsc);

    lResult = appTask_Init(ptAppTaskRsc);

    if(APP_SUCCESS == lResult)
    {
        lResult = appTask_Process(ptAppTaskRsc);
    }

    assert(false);
}

static uint32_t
appTask_Init(APP_TASK_RSC_T* ptAppTaskRsc) 
{
    uint32_t lResult = APP_SUCCESS;

    assert(NULL != ptAppTaskRsc);

    lResult = NVS_Init();

    ESP_LOGE(TAG, "Finish Nvs Initialization with result: %" PRIu32, lResult);

    if (APP_SUCCESS == lResult)
    {
        ESP_LOGE(TAG, "Start WiFi Initialization.");

        lResult = WiFi_Manager_Init(&ptAppTaskRsc->hWiFiManager);

        ESP_LOGE(TAG, "Finish WiFi Initialization with result: %" PRIu32, lResult);
    }

    if(APP_SUCCESS == lResult)
    {
        ESP_LOGE(TAG, "Start Fat FS Initialization.");

        lResult = FatFS_Init();

        ESP_LOGE(TAG, "Finish Fat FS Initialization with result: %" PRIu32, lResult);
    }

    debug_list_react_assets();

    if(APP_SUCCESS == lResult)
    {
        ESP_LOGE(TAG, "Start Web Server Initialization.");

        WEB_SERVER_PARAMS_T tWebServerParams = 
        {
            .hWiFiManager = ptAppTaskRsc->hWiFiManager,
        };

        lResult = Ws_Init(&tWebServerParams, &ptAppTaskRsc->hWebServer);
        ESP_LOGE(TAG, "Finish Web Server Initialization with result: %" PRIu32, lResult);
    }
    
    return lResult;
}

static uint32_t
appTask_Process(APP_TASK_RSC_T* ptAppTaskRsc)
{
    uint32_t lResult = APP_SUCCESS;

    assert(NULL != ptAppTaskRsc);

    while (true)
    {
        APP_TASK_EVENT_T tEvent = { 0 };

        if(pdPASS == xQueueReceive(ptAppTaskRsc->hAppTaskQueue, &tEvent, portMAX_DELAY))
        {
            switch(tEvent.ulEvent)
            {
                case APP_TASK_EVENTS_EVENT_NONE:
                default:
                {
                    break;
                }
                    
            }
        }
    }

    return lResult;
}