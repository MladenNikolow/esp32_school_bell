#ifndef WIFI_SETUP_SCREEN_INTERNAL_H
#define WIFI_SETUP_SCREEN_INTERNAL_H

#include "../../include/TouchScreen_UI_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and display the WiFi setup screen (internal)
 * @param callback Function to call when user completes setup
 */
void touchscreen_wifi_setup_screen_create(TouchScreen_WiFi_Setup_Callback_t callback);

/**
 * @brief Destroy the WiFi setup screen (internal)
 */
void touchscreen_wifi_setup_screen_destroy(void);

/**
 * @brief Get current SSID value from input field (internal)
 * @return Pointer to SSID string
 */
const char *touchscreen_wifi_setup_screen_get_ssid(void);

/**
 * @brief Get current password value from input field (internal)
 * @return Pointer to password string
 */
const char *touchscreen_wifi_setup_screen_get_password(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_SETUP_SCREEN_INTERNAL_H */
