#ifndef UI_TYPES_H
#define UI_TYPES_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UI Screen types for the application flow
 */
typedef enum {
    UI_SCREEN_SPLASH,
    UI_SCREEN_WIFI_SETUP,
    UI_SCREEN_MAX
} ui_screen_t;

/**
 * @brief WiFi Setup Result types
 */
typedef enum {
    WIFI_RESULT_CONNECT,
    WIFI_RESULT_SKIP
} wifi_setup_result_t;

/**
 * @brief Button callback function type
 */
typedef void (*button_callback_t)(lv_event_t *e);

/**
 * @brief WiFi Setup callback function type
 */
typedef void (*wifi_setup_callback_t)(wifi_setup_result_t result, const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif

#endif /* UI_TYPES_H */
