#include "TouchScreen_API.h"
#include "TouchScreen_UI_Manager.h"
#include "AppErrors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

static const char* TAG = "TOUCHSCREEN";

/* Touch screen task stack size - larger than UI_TASK since it runs continuously */
#define TOUCHSCREEN_TASK_STACK_BYTES        8192UL
#define TOUCHSCREEN_TASK_STACK_WORDS        ((TOUCHSCREEN_TASK_STACK_BYTES + sizeof(StackType_t) - 1) / sizeof(StackType_t))
#define TOUCHSCREEN_TASK_QUEUE_LENGTH       8UL

/**
 * @brief Touch screen events
 */
typedef enum {
    TOUCHSCREEN_EVENT_NONE = 0,
    TOUCHSCREEN_EVENT_SHOW_SPLASH,
    TOUCHSCREEN_EVENT_SHOW_WIFI_SETUP,
    TOUCHSCREEN_EVENT_UPDATE_SCREEN,
} TOUCHSCREEN_EVENT_ID_T;

/**
 * @brief Touch screen event structure
 */
typedef struct {
    TOUCHSCREEN_EVENT_ID_T ulEvent;
    uint32_t ulParam1;
    uint32_t ulParam2;
    void* pvParam;
} TOUCHSCREEN_EVENT_T;

/**
 * @brief Touch screen resource structure
 */
typedef struct {
    TOUCHSCREEN_PARAMS_T    tParams;              /* Initialization parameters */
    TaskHandle_t            hTouchScreenTask;     /* Task handle */
    QueueHandle_t           hEventQueue;          /* Event queue */
    bool                    bInitialized;         /* Initialization flag */
    bool                    bRunning;             /* Running flag */
    void (*pfWiFiCallback)(int result, const char *ssid, const char *password);  /* WiFi callback */
} TOUCHSCREEN_RSC_T;

/* Forward declarations */
static void touchScreen_TaskEntry(void* pvArg);
static int32_t touchScreen_HandleSplashEvent(TOUCHSCREEN_RSC_T* ptRsc, uint32_t duration_ms);
static int32_t touchScreen_HandleWiFiSetupEvent(TOUCHSCREEN_RSC_T* ptRsc);

/**
 * @brief Initialize the BSP display hardware
 */
int32_t TouchScreen_DisplayInit(void)
{
    int32_t lResult = APP_SUCCESS;
    
    ESP_LOGI(TAG, "Initializing display");
    
    bsp_display_start();
    
    return lResult;
}

/**
 * @brief Initialize touch screen component
 */
int32_t TouchScreen_Init(TOUCHSCREEN_PARAMS_T* ptParams, TOUCHSCREEN_H* phTouchScreen)
{
    int32_t lResult = APP_SUCCESS;
    TOUCHSCREEN_RSC_T* ptRsc = NULL;
    BaseType_t xTaskResult = pdFAIL;

    if ((NULL == ptParams) || (NULL == phTouchScreen)) {
        ESP_LOGE(TAG, "Invalid parameters");
        return APP_ERROR_INVALID_PARAM;
    }

    ptRsc = (TOUCHSCREEN_RSC_T*)calloc(1, sizeof(TOUCHSCREEN_RSC_T));
    if (NULL == ptRsc) {
        ESP_LOGE(TAG, "Failed to allocate memory for touch screen resource");
        return APP_ERROR_OUT_OF_MEMORY;
    }

    ptRsc->tParams = *ptParams;
    ptRsc->bRunning = true;

    /* Create event queue for touch screen communications */
    ptRsc->hEventQueue = xQueueCreate(TOUCHSCREEN_TASK_QUEUE_LENGTH, sizeof(TOUCHSCREEN_EVENT_T));
    if (NULL == ptRsc->hEventQueue) {
        ESP_LOGE(TAG, "Failed to create event queue");
        free(ptRsc);
        return APP_ERROR_QUEUE_CREATE_FAILED;
    }

    /* Create touch screen task */
    xTaskResult = xTaskCreate(
        touchScreen_TaskEntry,
        "TOUCHSCREEN_TASK",
        TOUCHSCREEN_TASK_STACK_WORDS,
        (void*)ptRsc,
        ptParams->ulTaskPriority,
        &ptRsc->hTouchScreenTask
    );

    if (xTaskResult != pdPASS) 
    {
        ESP_LOGE(TAG, "Failed to create touch screen task");
        vQueueDelete(ptRsc->hEventQueue);
        free(ptRsc);
        return APP_ERROR_TASK_CREATE_FAILED;
    }

    ptRsc->bInitialized = true;
    *phTouchScreen = (TOUCHSCREEN_H)ptRsc;

    ESP_LOGI(TAG, "Touch screen component initialized successfully");
    return APP_SUCCESS;
}

/**
 * @brief Deinitialize touch screen component
 */
int32_t TouchScreen_Deinit(TOUCHSCREEN_H hTouchScreen)
{
    TOUCHSCREEN_RSC_T* ptRsc = (TOUCHSCREEN_RSC_T*)hTouchScreen;

    if (NULL == ptRsc) {
        return APP_ERROR_INVALID_PARAM;
    }

    ptRsc->bRunning = false;

    /* Give task time to clean up */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Delete queue and task will be cleaned up by FreeRTOS */
    if (ptRsc->hEventQueue) {
        vQueueDelete(ptRsc->hEventQueue);
    }

    free(ptRsc);
    ESP_LOGI(TAG, "Touch screen component deinitialized");
    return APP_SUCCESS;
}

/**
 * @brief Show splash screen
 */
int32_t TouchScreen_ShowSplash(TOUCHSCREEN_H hTouchScreen, uint32_t duration_ms)
{
    TOUCHSCREEN_RSC_T* ptRsc = (TOUCHSCREEN_RSC_T*)hTouchScreen;
    TOUCHSCREEN_EVENT_T tEvent = {
        .ulEvent = TOUCHSCREEN_EVENT_SHOW_SPLASH,
        .ulParam1 = duration_ms,
    };

    if (NULL == ptRsc) {
        return APP_ERROR_INVALID_PARAM;
    }

    if (pdPASS == xQueueSend(ptRsc->hEventQueue, &tEvent, pdMS_TO_TICKS(100))) {
        return APP_SUCCESS;
    }

    ESP_LOGW(TAG, "Failed to send splash event to queue");
    return APP_ERROR_QUEUE_SEND_FAILED;
}

/**
 * @brief Show WiFi setup screen
 */
int32_t TouchScreen_ShowWiFiSetup(TOUCHSCREEN_H hTouchScreen, void (*wifi_setup_callback)(int result, const char *ssid, const char *password))
{
    TOUCHSCREEN_RSC_T* ptRsc = (TOUCHSCREEN_RSC_T*)hTouchScreen;
    TOUCHSCREEN_EVENT_T tEvent = {
        .ulEvent = TOUCHSCREEN_EVENT_SHOW_WIFI_SETUP,
        .pvParam = (void*)wifi_setup_callback,
    };

    if (NULL == ptRsc) {
        return APP_ERROR_INVALID_PARAM;
    }

    ptRsc->pfWiFiCallback = wifi_setup_callback;

    if (pdPASS == xQueueSend(ptRsc->hEventQueue, &tEvent, pdMS_TO_TICKS(100))) {
        return APP_SUCCESS;
    }

    ESP_LOGW(TAG, "Failed to send WiFi setup event to queue");
    return APP_ERROR_QUEUE_SEND_FAILED;
}

/**
 * @brief Touch screen task main entry point
 */
static void touchScreen_TaskEntry(void* pvArg)
{
    TOUCHSCREEN_RSC_T* ptRsc = (TOUCHSCREEN_RSC_T*)pvArg;
    TOUCHSCREEN_EVENT_T tEvent = {0};

    if (NULL == ptRsc) {
        ESP_LOGE(TAG, "Invalid task argument");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Touch screen task started");

    /* Initialize UI manager - display is already initialized by bsp_display_start() */
    if (!TouchScreen_UI_ManagerInit()) {
        ESP_LOGW(TAG, "UI manager init returned false, but continuing");
    }

    /* Main event loop */
    while (ptRsc->bRunning) {
        /* Wait for events with timeout to allow continuous LVGL rendering */
        if (xQueueReceive(ptRsc->hEventQueue, &tEvent, pdMS_TO_TICKS(10))) {
            switch (tEvent.ulEvent) {
                case TOUCHSCREEN_EVENT_SHOW_SPLASH:
                    ESP_LOGI(TAG, "Showing splash screen for %lu ms", tEvent.ulParam1);
                    touchScreen_HandleSplashEvent(ptRsc, tEvent.ulParam1);
                    break;

                case TOUCHSCREEN_EVENT_SHOW_WIFI_SETUP:
                    ESP_LOGI(TAG, "Showing WiFi setup screen");
                    touchScreen_HandleWiFiSetupEvent(ptRsc);
                    break;

                case TOUCHSCREEN_EVENT_NONE:
                default:
                    /* Low priority event or unknown */
                    break;
            }
        }

        /* Allow LVGL task and other higher priority tasks to run */
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGI(TAG, "Touch screen task exiting");
    vTaskDelete(NULL);
}

/**
 * @brief Handle splash screen event
 */
static int32_t touchScreen_HandleSplashEvent(TOUCHSCREEN_RSC_T* ptRsc, uint32_t duration_ms)
{
    if (NULL == ptRsc) {
        return APP_ERROR_INVALID_PARAM;
    }

    /* Show splash screen using UI manager */
    TouchScreen_UI_ShowSplash(duration_ms);

    return APP_SUCCESS;
}

/**
 * @brief Handle WiFi setup screen event
 */
static int32_t touchScreen_HandleWiFiSetupEvent(TOUCHSCREEN_RSC_T* ptRsc)
{
    if (NULL == ptRsc) {
        return APP_ERROR_INVALID_PARAM;
    }

    /* Show WiFi setup screen with callback */
    TouchScreen_UI_ShowWiFiSetup(ptRsc->pfWiFiCallback);

    return APP_SUCCESS;
}
