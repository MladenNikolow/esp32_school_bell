#include "ui_manager_internal.h"
#include "../screens/splash/splash_screen_internal.h"
#include "../screens/wifi_setup/wifi_setup_screen_internal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "TOUCHSCREEN_UI_MGR";

static touchscreen_ui_manager_state_t ui_state = {
    .current_screen = TOUCHSCREEN_UI_SCREEN_SPLASH,
    .wifi_callback = NULL
};

bool touchscreen_ui_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing UI Manager");
    ui_state.current_screen = TOUCHSCREEN_UI_SCREEN_SPLASH;
    ui_state.wifi_callback = NULL;
    return true;
}

void touchscreen_ui_show_splash(uint32_t duration_ms)
{
    ESP_LOGI(TAG, "Showing splash screen for %lu ms", duration_ms);
    ui_state.current_screen = TOUCHSCREEN_UI_SCREEN_SPLASH;
    touchscreen_splash_screen_create(duration_ms);
    
    // Display the splash screen for the specified duration
    // Break the delay into smaller chunks to allow other tasks to run and prevent watchdog timeout
    uint32_t chunk_size = 100;  // 100ms chunks
    uint32_t remaining = duration_ms;
    
    while (remaining > 0) {
        uint32_t delay = (remaining > chunk_size) ? chunk_size : remaining;
        vTaskDelay(pdMS_TO_TICKS(delay));
        remaining -= delay;
    }
    
    ESP_LOGI(TAG, "Splash screen display time completed");
    vTaskDelay(pdMS_TO_TICKS(100));
    lv_refr_now(NULL);
}

void touchscreen_ui_show_wifi_setup(TouchScreen_WiFi_Setup_Callback_t callback)
{
    ESP_LOGI(TAG, "Showing WiFi setup screen");
    ui_state.current_screen = TOUCHSCREEN_UI_SCREEN_WIFI_SETUP;
    ui_state.wifi_callback = callback;
    
    // Small delay to ensure previous screen is cleaned up
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Explicitly clean the active screen before creating new one
    lv_obj_t *scr = lv_scr_act();
    if (scr) {
        lv_obj_clean(scr);
        ESP_LOGI(TAG, "Cleaned active screen before WiFi setup");
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
    
    touchscreen_wifi_setup_screen_create(callback);
    
    // Let LVGL handle refresh naturally - don't force it
    ESP_LOGI(TAG, "WiFi setup screen displayed");
}

void touchscreen_ui_manager_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing UI Manager");
    // Cleanup if needed
}

/* Public API wrapper functions */
bool TouchScreen_UI_ManagerInit(void)
{
    return touchscreen_ui_manager_init();
}

void TouchScreen_UI_ShowSplash(uint32_t duration_ms)
{
    touchscreen_ui_show_splash(duration_ms);
}

void TouchScreen_UI_ShowWiFiSetup(TouchScreen_WiFi_Setup_Callback_t callback)
{
    touchscreen_ui_show_wifi_setup(callback);
}
