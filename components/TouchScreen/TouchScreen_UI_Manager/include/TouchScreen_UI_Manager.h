#ifndef TOUCHSCREEN_UI_MANAGER_H
#define TOUCHSCREEN_UI_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "TouchScreen_UI_Types.h"

/**
 * @brief Initialize UI manager (internal use)
 * 
 * @return true on success, false otherwise
 */
bool TouchScreen_UI_ManagerInit(void);

/**
 * @brief Show splash screen
 * 
 * @param duration_ms Duration to display splash screen in milliseconds
 */
void TouchScreen_UI_ShowSplash(uint32_t duration_ms);

/**
 * @brief Show WiFi setup screen
 * 
 * @param callback Callback function when WiFi setup completes (can be NULL)
 */
void TouchScreen_UI_ShowWiFiSetup(TouchScreen_WiFi_Setup_Callback_t callback);

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_UI_MANAGER_H */
