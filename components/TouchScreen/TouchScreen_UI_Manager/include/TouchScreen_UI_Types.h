#ifndef TOUCHSCREEN_UI_TYPES_H
#define TOUCHSCREEN_UI_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief UI screen identifiers
 */
typedef enum {
    TOUCHSCREEN_UI_SCREEN_NONE = -1,
    TOUCHSCREEN_UI_SCREEN_SPLASH = 0,
    TOUCHSCREEN_UI_SCREEN_WIFI_SETUP,
    TOUCHSCREEN_UI_SCREEN_DASHBOARD,
    TOUCHSCREEN_UI_SCREEN_SCHEDULE_VIEW,
    TOUCHSCREEN_UI_SCREEN_SETTINGS,
    TOUCHSCREEN_UI_SCREEN_INFO,       /* Device info / about screen */
    TOUCHSCREEN_UI_SCREEN_PIN_ENTRY,  /* Overlay screen */
    TOUCHSCREEN_UI_SCREEN_MAX
} TouchScreen_UI_Screen_t;

/**
 * @brief WiFi setup result types
 */
typedef enum {
    TOUCHSCREEN_WIFI_SETUP_RESULT_SUCCESS = 0,
    TOUCHSCREEN_WIFI_SETUP_RESULT_CANCEL,
    TOUCHSCREEN_WIFI_SETUP_RESULT_ERROR,
} TouchScreen_WiFi_Setup_Result_t;

/**
 * @brief WiFi setup result callback
 */
typedef void (*TouchScreen_WiFi_Setup_Callback_t)(TouchScreen_WiFi_Setup_Result_t result, const char *ssid, const char *password);

/**
 * @brief PIN entry result callback
 * @param success true if correct PIN was entered, false if cancelled
 */
typedef void (*TouchScreen_PIN_Result_Callback_t)(bool success);

/**
 * @brief Screen lifecycle callbacks — each screen must implement these
 */
typedef struct {
    void (*create)(void);              /**< Create screen UI elements */
    void (*destroy)(void);             /**< Clean up screen UI elements */
    void (*update)(void);              /**< Periodic update (called every 1s) — can be NULL */
    bool show_chrome;                  /**< true = show status bar + navbar */
} TouchScreen_Screen_Def_t;

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_UI_TYPES_H */
