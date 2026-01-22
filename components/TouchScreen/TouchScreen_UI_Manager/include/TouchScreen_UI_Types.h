#ifndef TOUCHSCREEN_UI_TYPES_H
#define TOUCHSCREEN_UI_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UI screen types
 */
typedef enum {
    TOUCHSCREEN_UI_SCREEN_SPLASH = 0,
    TOUCHSCREEN_UI_SCREEN_WIFI_SETUP,
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
 * 
 * @param result Result code
 * @param ssid SSID string (may be NULL on error)
 * @param password Password string (may be NULL on error)
 */
typedef void (*TouchScreen_WiFi_Setup_Callback_t)(TouchScreen_WiFi_Setup_Result_t result, const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_UI_TYPES_H */
