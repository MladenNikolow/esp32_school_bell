#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "AppTask_API.h"
#include "sdkconfig.h"
#include "Definitions/AppErrors.h"

static const char* TAG = "main";

#ifndef CONFIG_FREERTOS_TIMER_TASK_PRIORITY 
    #warning "CONFIG_FREERTOS_TIMER_TASK_PRIORITY not defined, using default 1" 
    #define CONFIG_FREERTOS_TIMER_TASK_PRIORITY 1 
#endif 

#define APP_TASK_PRIORITY   (CONFIG_FREERTOS_TIMER_TASK_PRIORITY + 1)

void app_main() 
{
    int32_t lResult = APP_SUCCESS;
    APP_TASK_H hAppTask;
    APP_TASK_PARAMS_T tAppTaskParams = 
    { 
        .ulTaskPriority = APP_TASK_PRIORITY
     };

    lResult = AppTask_Create(&tAppTaskParams, &hAppTask);
    if(APP_SUCCESS == lResult)
    {
        ESP_LOGE(TAG, "AppTask_Create successful");
        vTaskDelete(NULL);
    }
    else
    {
        ESP_LOGE(TAG, "AppTask_Create failed: %" PRIu32, lResult);
    }
}