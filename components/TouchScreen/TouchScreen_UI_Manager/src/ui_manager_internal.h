#ifndef TOUCHSCREEN_UI_MANAGER_INTERNAL_H
#define TOUCHSCREEN_UI_MANAGER_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "../include/TouchScreen_UI_Types.h"

/**
 * @brief Internal UI manager state
 */
typedef struct {
    TouchScreen_UI_Screen_t current_screen;
    TouchScreen_WiFi_Setup_Callback_t wifi_callback;
} touchscreen_ui_manager_state_t;

/**
 * @brief Initialize UI manager (internal)
 * 
 * @return true on success, false otherwise
 */
bool touchscreen_ui_manager_init(void);

/**
 * @brief Show splash screen (internal)
 * 
 * @param duration_ms Duration in milliseconds
 */
void touchscreen_ui_show_splash(uint32_t duration_ms);

/**
 * @brief Show WiFi setup screen (internal)
 * 
 * @param callback Callback function
 */
void touchscreen_ui_show_wifi_setup(TouchScreen_WiFi_Setup_Callback_t callback);

/**
 * @brief Deinitialize UI manager (internal)
 */
void touchscreen_ui_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_UI_MANAGER_INTERNAL_H */
