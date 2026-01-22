#ifndef WIFI_SETUP_SCREEN_H
#define WIFI_SETUP_SCREEN_H

#include "../../ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and display the WiFi setup screen
 * @param callback Function to call when user completes setup (connect or skip)
 */
void wifi_setup_screen_create(wifi_setup_callback_t callback);

/**
 * @brief Destroy the WiFi setup screen and cleanup resources
 */
void wifi_setup_screen_destroy(void);

/**
 * @brief Get current SSID value from input field
 * @return Pointer to SSID string (valid until next screen operation)
 */
const char *wifi_setup_screen_get_ssid(void);

/**
 * @brief Get current password value from input field
 * @return Pointer to password string (valid until next screen operation)
 */
const char *wifi_setup_screen_get_password(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_SETUP_SCREEN_H */
