#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "AppTask_API.h"
#include "sdkconfig.h"
#include "AppErrors.h"

static const char* TAG = "main";

#ifndef CONFIG_FREERTOS_TIMER_TASK_PRIORITY 
    #warning "CONFIG_FREERTOS_TIMER_TASK_PRIORITY not defined, using default 1" 
    #define CONFIG_FREERTOS_TIMER_TASK_PRIORITY 1 
#endif 

#define APP_TASK_PRIORITY   (CONFIG_FREERTOS_TIMER_TASK_PRIORITY + 2)

/**
 * @brief Main application entry point
 * 
 * Initialization order:
 * 1. APP_TASK (priority 3) - manages WiFi, NVS, WebServer, Display, TouchScreen
 * 2. Display/BSP (via TouchScreen_DisplayInit in APP_TASK)
 * 3. TouchScreen (priority 2) - manages UI and display updates
 * 4. LVGL rendering (priority 4) - runs continuously
 * 5. Touch input (priority 5) - highest priority for responsiveness
 */
void app_main(void) 
{
    int32_t lResult = APP_SUCCESS;
    APP_TASK_H hAppTask = NULL;
    
    APP_TASK_PARAMS_T tAppTaskParams = 
    { 
        .ulTaskPriority = APP_TASK_PRIORITY
    };

    lResult = AppTask_Create(&tAppTaskParams, &hAppTask);
    if(APP_SUCCESS == lResult)
    {
        ESP_LOGI(TAG, "AppTask_Create successful");
    }
    else
    {
        ESP_LOGE(TAG, "AppTask_Create failed: %" PRIu32, lResult);
    }
    
    /* main task completes - all functionality is handled by AppTask */
    vTaskDelete(NULL);
}

// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "lvgl.h"
// #include "bsp/esp-bsp.h"
// #include "bsp/display.h"
// #include "ringy_logo.h"

// static void create_buttons(void);


// void app_main(void)
// {
//     bsp_display_start();

//     // --- Show logo ---
//     bsp_display_lock(0);

//     lv_obj_t *img = lv_image_create(lv_scr_act());
//     lv_image_set_src(img, &ringy_logo);

//     // Force the image to fill the screen
//     lv_obj_set_size(img, 480, 480);
//     lv_obj_center(img);

//     // Scale the image to match the new size
//     lv_image_set_scale(img, LV_SCALE_NONE);   // disable proportional scaling

//     bsp_display_unlock();

//     vTaskDelay(pdMS_TO_TICKS(5000));

//     // --- Remove logo and create buttons ---
//     bsp_display_lock(0);

//     lv_obj_del(img);
//     create_buttons();

//     bsp_display_unlock();
// }

// static void create_buttons(void)
// {
//     lv_obj_t *scr = lv_scr_act();

//     //
//     // LEFT BUTTON — "Ring"
//     //
//     lv_obj_t *btn_left = lv_button_create(scr);
//     lv_obj_set_size(btn_left, lv_pct(50), lv_pct(100));
//     lv_obj_set_align(btn_left, LV_ALIGN_LEFT_MID);
    
//     // Style
//     lv_obj_set_style_bg_color(btn_left, lv_color_hex(0x28B463), 0);  // Green
//     lv_obj_set_style_border_color(btn_left, lv_color_hex(0xFFFFFF), 0);
//     lv_obj_set_style_border_width(btn_left, 4, 0);
//     lv_obj_set_style_radius(btn_left, 0, 0);

//     // Label
//     lv_obj_t *lbl_left = lv_label_create(btn_left);
//     lv_label_set_text(lbl_left, "Ring");
//     lv_obj_set_style_text_font(lbl_left, &lv_font_montserrat_28, 0);
//     lv_obj_set_style_text_color(lbl_left, lv_color_hex(0xFFFFFF), 0);
//     lv_obj_center(lbl_left);

//     //
//     // RIGHT BUTTON — "Configure"
//     //
//     lv_obj_t *btn_right = lv_button_create(scr);
//     lv_obj_set_size(btn_right, lv_pct(50), lv_pct(100));
//     lv_obj_set_align(btn_right, LV_ALIGN_RIGHT_MID);

//     // Style
//     lv_obj_set_style_bg_color(btn_right, lv_color_hex(0x2E86C1), 0);   // Blue
//     lv_obj_set_style_border_color(btn_right, lv_color_hex(0xFFFFFF), 0);
//     lv_obj_set_style_border_width(btn_right, 4, 0);
//     lv_obj_set_style_radius(btn_right, 0, 0); // Square edges

//     // Label
//     lv_obj_t *lbl_right = lv_label_create(btn_right);
//     lv_label_set_text(lbl_right, "Configure");
//     lv_obj_set_style_text_font(lbl_right, &lv_font_montserrat_28, 0);
//     lv_obj_set_style_text_color(lbl_right, lv_color_hex(0xFFFFFF), 0);
//     lv_obj_center(lbl_right);
// }

// // #include <stdio.h>
// // #include "freertos/FreeRTOS.h"
// // #include "freertos/task.h"
// // #include "nvs_flash.h"
// // #include "nvs.h"
// // #include "esp_log.h"
// // #include "esp_err.h"
// // #include "esp_check.h"
// // #include "esp_memory_utils.h"
// // #include "lvgl.h"
// // #include "bsp/esp-bsp.h"
// // #include "bsp/display.h"
// // #include "lv_demos.h"

// // void app_main(void)
// // {
// //     // Initialize display, touch, LVGL task, etc.
// //     bsp_display_start();

// //     // Create a screen object
// //     lv_obj_t *scr = lv_scr_act();

// //     // Create a label
// //     lv_obj_t *label = lv_label_create(scr);
// //     lv_label_set_text(label, "Hello World");
// //     lv_obj_center(label);

// //     // bsp_display_start();

// //     // bsp_display_lock(0);

// //     // // lv_demo_music();
// //     // lv_demo_benchmark();
// //     // // lv_demo_widgets();

// //     // bsp_display_unlock();
// // }