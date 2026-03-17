#include "AppTask_API.h"
#include "AppErrors.h"
#include "freertos/FreeRTOS.h"
#include "WiFi_Manager_API.h"
#include "NVS_API.h"
#include "Ws_API.h"
#include "FatFS_API.h"
#include "SPIFFS_API.h"
#include "TimeSync_API.h"
#include "Scheduler_API.h"
#include "RingBell_API.h"
#include "TouchScreen_API.h"
#include "esp_log.h"
#include "esp_system.h"

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
    TOUCHSCREEN_H        hTouchScreen;          /* Touch screen handle */
    SCHEDULER_H          hScheduler;            /* Scheduler handle */
} APP_TASK_RSC_T;

static void 
appTask_Enter(void* pvArg);

/**
 * @brief WiFi setup callback - saves credentials and restarts the device.
 *        Mirrors the behavior of Ws_WifiConfigPage_Post.
 */
static void
appTask_WiFiSetupCallback(int result, const char *ssid, const char *password)
{
    if (result == 0 && ssid != NULL && password != NULL)  /* SUCCESS */
    {
        ESP_LOGI(TAG, "WiFi setup: saving credentials for SSID '%s'", ssid);

        esp_err_t espErr = WiFi_Manager_SaveCredentials(ssid, password);

        if (ESP_OK == espErr)
        {
            ESP_LOGI(TAG, "Credentials saved. Restarting...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        }
        else
        {
            ESP_LOGE(TAG, "Failed to save WiFi credentials: %s", esp_err_to_name(espErr));
        }
    }
    else
    {
        ESP_LOGI(TAG, "WiFi setup cancelled or failed (result=%d)", result);
    }
}

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

    /* ========== PHASE 1: Hardware & Storage Initialization ========== */

    lResult = NVS_Init();
    ESP_LOGE(TAG, "Finish NVS Initialization with result: %" PRIu32, lResult);

    if(APP_SUCCESS == lResult)
    {
        ESP_LOGE(TAG, "Start Fat FS Initialization.");

        lResult = FatFS_Init();
        ESP_LOGE(TAG, "Finish Fat FS Initialization with result: %" PRIu32, lResult);
    }

    if(APP_SUCCESS == lResult)
    {
        ESP_LOGI(TAG, "Start SPIFFS Initialization.");

        esp_err_t spiffsErr = SPIFFS_Init();
        if (ESP_OK != spiffsErr)
        {
            ESP_LOGE(TAG, "SPIFFS init failed: %s", esp_err_to_name(spiffsErr));
            lResult = APP_ERROR_INIT_FAILED;
        }

        ESP_LOGI(TAG, "Finish SPIFFS Initialization.");
    }

    if(APP_SUCCESS == lResult)
    {
        ESP_LOGE(TAG, "Start Touch Screen Display Initialization.");

        lResult = TouchScreen_DisplayInit();
        ESP_LOGE(TAG, "Finish Touch Screen Display Initialization with result: %" PRIu32, lResult);
    }

    /* ========== PHASE 2: Asset Verification ========== */

    debug_list_react_assets();

    /* ========== PHASE 3: Connectivity & Services ========== */

    if(APP_SUCCESS == lResult)
    {
        ESP_LOGE(TAG, "Start WiFi Initialization.");

        lResult = WiFi_Manager_Init(&ptAppTaskRsc->hWiFiManager);
        ESP_LOGE(TAG, "Finish WiFi Initialization with result: %" PRIu32, lResult);
    }

    /* RingBell: Initialize bell GPIO and duration timer */
    if(APP_SUCCESS == lResult)
    {
        ESP_LOGI(TAG, "Start RingBell Initialization.");

        esp_err_t bellErr = RingBell_Init();
        if (ESP_OK != bellErr)
        {
            ESP_LOGE(TAG, "RingBell init failed: %s", esp_err_to_name(bellErr));
            lResult = APP_ERROR_INIT_FAILED;
        }

        ESP_LOGI(TAG, "Finish RingBell Initialization.");
    }

    /* Scheduler: Load schedule data and start background task */
    if(APP_SUCCESS == lResult)
    {
        ESP_LOGI(TAG, "Start Scheduler Initialization.");

        esp_err_t schedErr = Scheduler_Init(&ptAppTaskRsc->hScheduler);
        if (ESP_OK != schedErr)
        {
            ESP_LOGE(TAG, "Scheduler init failed: %s", esp_err_to_name(schedErr));
            lResult = APP_ERROR_INIT_FAILED;
        }

        ESP_LOGI(TAG, "Finish Scheduler Initialization.");
    }

    if(APP_SUCCESS == lResult)
    {
        ESP_LOGE(TAG, "Start Web Server Initialization.");

        WEB_SERVER_PARAMS_T tWebServerParams = 
        {
            .hWiFiManager = ptAppTaskRsc->hWiFiManager,
            .hScheduler   = ptAppTaskRsc->hScheduler,
        };

        lResult = Ws_Init(&tWebServerParams, &ptAppTaskRsc->hWebServer);
        ESP_LOGE(TAG, "Finish Web Server Initialization with result: %" PRIu32, lResult);
    }

    /* TimeSync: Initialize SNTP after Ws_Init (requires TCP/IP stack) */
    if(APP_SUCCESS == lResult)
    {
        ESP_LOGI(TAG, "Start TimeSync Initialization.");
        TimeSync_Init();
        ESP_LOGI(TAG, "Finish TimeSync Initialization.");
    }

    /* ========== PHASE 4: UI Initialization & Presentation ========== */

    if(APP_SUCCESS == lResult)
    {
        ESP_LOGE(TAG, "Start Touch Screen Task Initialization.");

        /* Initialize touch screen component with priority one level lower than APP_TASK */
        TOUCHSCREEN_PARAMS_T tTouchScreenParams = 
        {
            .ulTaskPriority = ptAppTaskRsc->tParams.ulTaskPriority - 1,
        };

        lResult = TouchScreen_Init(&tTouchScreenParams, &ptAppTaskRsc->hTouchScreen);
        ESP_LOGE(TAG, "Finish Touch Screen Task Initialization with result: %" PRIu32, lResult);
    }

    if(APP_SUCCESS == lResult)
    {
        ESP_LOGE(TAG, "Showing splash screen");
        TouchScreen_ShowSplash(ptAppTaskRsc->hTouchScreen, 5000);

        /* Delay to let splash screen display */
        vTaskDelay(pdMS_TO_TICKS(5100));

        ESP_LOGE(TAG, "Showing WiFi setup screen");
        TouchScreen_ShowWiFiSetup(ptAppTaskRsc->hTouchScreen, appTask_WiFiSetupCallback);
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
