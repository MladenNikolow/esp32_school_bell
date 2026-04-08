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
#include "TouchScreen_Services.h"
#include "TouchScreen_UI_Manager.h"
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

        esp_err_t espErr = WiFi_Manager_SaveCredentials(ssid, password, NULL);

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

/**
 * @brief Setup wizard callback - marks setup complete, optionally saves WiFi
 *        credentials and restarts, or goes straight to the dashboard.
 */
static void
appTask_SetupWizardCallback(bool completed, bool wifi_configured,
                            const char *ssid, const char *password)
{
    if (!completed)
    {
        ESP_LOGW(TAG, "Setup wizard was not completed");
        return;
    }

    TS_Setup_MarkComplete();
    ESP_LOGI(TAG, "Setup wizard completed, setup marked as done");

    if (wifi_configured && ssid != NULL && password != NULL)
    {
        ESP_LOGI(TAG, "Wizard WiFi configured for SSID '%s', saving and restarting", ssid);

        esp_err_t espErr = WiFi_Manager_SaveCredentials(ssid, password, NULL);
        if (ESP_OK == espErr)
        {
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
        ESP_LOGI(TAG, "No WiFi configured in wizard — showing dashboard");
        /* This callback runs in the TouchScreen task context (via the
         * wrapper in TouchScreen_API.c), so we can navigate directly
         * through the UI manager. */
        TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_DASHBOARD);
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
    ESP_LOGI(TAG, "Finish NVS Initialization with result: %" PRIu32, lResult);

    if(APP_SUCCESS == lResult)
    {
        ESP_LOGI(TAG, "Start Fat FS Initialization.");

        lResult = FatFS_Init();
        ESP_LOGI(TAG, "Finish Fat FS Initialization with result: %" PRIu32, lResult);
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
        ESP_LOGI(TAG, "Start Touch Screen Display Initialization.");

        lResult = TouchScreen_DisplayInit();
        ESP_LOGI(TAG, "Finish Touch Screen Display Initialization with result: %" PRIu32, lResult);
    }

    /* ========== PHASE 2: Asset Verification ========== */

    debug_list_react_assets();

    /* ========== PHASE 3: Connectivity & Services ========== */

    if(APP_SUCCESS == lResult)
    {
        ESP_LOGI(TAG, "Start WiFi Initialization.");

        lResult = WiFi_Manager_Init(&ptAppTaskRsc->hWiFiManager);
        ESP_LOGI(TAG, "Finish WiFi Initialization with result: %" PRIu32, lResult);
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
        ESP_LOGI(TAG, "Start Web Server Initialization.");

        WEB_SERVER_PARAMS_T tWebServerParams = 
        {
            .hWiFiManager = ptAppTaskRsc->hWiFiManager,
            .hScheduler   = ptAppTaskRsc->hScheduler,
        };

        lResult = Ws_Init(&tWebServerParams, &ptAppTaskRsc->hWebServer);
        ESP_LOGI(TAG, "Finish Web Server Initialization with result: %" PRIu32, lResult);
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
        ESP_LOGI(TAG, "Start Touch Screen Task Initialization.");

        /* Initialize touch screen component with priority one level lower than APP_TASK */
        TOUCHSCREEN_PARAMS_T tTouchScreenParams = 
        {
            .ulTaskPriority = ptAppTaskRsc->tParams.ulTaskPriority - 1,
        };

        lResult = TouchScreen_Init(&tTouchScreenParams, &ptAppTaskRsc->hTouchScreen);
        ESP_LOGI(TAG, "Finish Touch Screen Task Initialization with result: %" PRIu32, lResult);
    }

    /* Connect TouchScreen schedule service to the Scheduler backend */
    if(APP_SUCCESS == lResult)
    {
        TS_Schedule_Init(ptAppTaskRsc->hScheduler);
    }

    if(APP_SUCCESS == lResult)
    {
        ESP_LOGI(TAG, "Showing splash screen");
        TouchScreen_ShowSplash(ptAppTaskRsc->hTouchScreen, 3000);

        /* Delay to let splash screen display */
        vTaskDelay(pdMS_TO_TICKS(3100));

        /* Initialize first-time setup service and check for migration */
        TS_Setup_Init();
        TS_Setup_CheckMigration();

        if (!TS_Setup_IsComplete())
        {
            ESP_LOGI(TAG, "First-time setup not complete — showing setup wizard");
            TouchScreen_ShowSetupWizard(ptAppTaskRsc->hTouchScreen, appTask_SetupWizardCallback);
        }
        else
        {
            /* Check if WiFi is already configured */
            uint32_t ulWiFiConfigState = 0;
            esp_err_t espWiFiErr = WiFi_Manager_GetConfigurationState(
                ptAppTaskRsc->hWiFiManager, &ulWiFiConfigState);

            if (ESP_OK == espWiFiErr &&
                WIFI_MANAGER_CONFIGURATION_STATE_CONFIGURED == ulWiFiConfigState)
            {
                ESP_LOGI(TAG, "WiFi configured — showing dashboard");
                TouchScreen_ShowDashboard(ptAppTaskRsc->hTouchScreen);
            }
            else
            {
                ESP_LOGI(TAG, "WiFi not configured — showing WiFi setup");
                TouchScreen_ShowWiFiSetup(ptAppTaskRsc->hTouchScreen, appTask_WiFiSetupCallback);
            }
        }
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
