#ifndef WIFI_SETUP_SCREEN_INTERNAL_H
#define WIFI_SETUP_SCREEN_INTERNAL_H

#include "../../include/TouchScreen_UI_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and display the WiFi setup screen (internal)
 *        Shows network scan list → tap to select → password entry → connect
 * @param callback Function to call when user completes setup
 * @param is_initial_setup true if first-time boot WiFi config, false if runtime reconfiguration
 */
void touchscreen_wifi_setup_screen_create(TouchScreen_WiFi_Setup_Callback_t callback, bool is_initial_setup);

/**
 * @brief Destroy the WiFi setup screen and free resources (internal)
 */
void touchscreen_wifi_setup_screen_destroy(void);

/**
 * @brief Periodic update for WiFi setup screen (called every 1s).
 *        Auto-navigates to dashboard if WiFi reconnects via retry mechanism.
 */
void touchscreen_wifi_setup_screen_update(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_SETUP_SCREEN_INTERNAL_H */
