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
    TouchScreen_UI_Screen_t      current_screen;     /**< Active screen */
    TouchScreen_UI_Screen_t      overlay_screen;      /**< Overlay on top (-1 if none) */
    TouchScreen_WiFi_Setup_Callback_t wifi_callback;  /**< WiFi callback (set before showing wifi screen) */
    TouchScreen_PIN_Result_Callback_t    pin_callback;   /**< PIN result callback (set before showing PIN overlay) */
    lv_obj_t                    *screen_obj;          /**< Current screen LVGL object (set before create) */
    lv_obj_t                    *statusbar;           /**< Status bar object (persistent) */
    lv_obj_t                    *navbar;              /**< Navbar object (persistent) */
    lv_timer_t                  *update_timer;        /**< 1-second periodic update timer */
    bool                         initialized;
    bool                         wifi_is_initial_setup; /**< true = boot-time WiFi setup, false = runtime reconfiguration */
} touchscreen_ui_manager_state_t;

/* Screen definitions are registered in ui_manager.c */
extern touchscreen_ui_manager_state_t g_ui_state;

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_UI_MANAGER_INTERNAL_H */
