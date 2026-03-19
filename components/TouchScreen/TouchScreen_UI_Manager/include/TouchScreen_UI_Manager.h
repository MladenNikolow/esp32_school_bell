#ifndef TOUCHSCREEN_UI_MANAGER_H
#define TOUCHSCREEN_UI_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "TouchScreen_UI_Types.h"

/**
 * @brief Initialize UI manager — sets up screen registry, theme, and periodic timer
 * @return true on success, false otherwise
 */
bool TouchScreen_UI_ManagerInit(void);

/**
 * @brief Deinitialize UI manager
 */
void TouchScreen_UI_ManagerDeinit(void);

/**
 * @brief Navigate to a screen (replaces current screen)
 * @param screen Screen identifier
 */
void TouchScreen_UI_NavigateTo(TouchScreen_UI_Screen_t screen);

/**
 * @brief Push an overlay screen (e.g. PIN entry) on top of current
 * @param screen Overlay screen identifier
 */
void TouchScreen_UI_PushOverlay(TouchScreen_UI_Screen_t screen);

/**
 * @brief Pop the overlay screen and return to the screen underneath
 */
void TouchScreen_UI_PopOverlay(void);

/**
 * @brief Get the currently active screen
 * @return Current screen identifier
 */
TouchScreen_UI_Screen_t TouchScreen_UI_GetCurrentScreen(void);

/**
 * @brief Trigger a single update cycle (calls active screen\'s update callback)
 */
void TouchScreen_UI_Update(void);

/* === Legacy convenience wrappers (used by TouchScreen_API) === */

void TouchScreen_UI_ShowSplash(uint32_t duration_ms);
void TouchScreen_UI_ShowWiFiSetup(TouchScreen_WiFi_Setup_Callback_t callback);
void TouchScreen_UI_ShowDashboard(void);

/**
 * @brief Show PIN entry overlay — calls callback with result
 * @param callback Called with true on correct PIN, false on cancel
 */
void TouchScreen_UI_ShowPinEntry(TouchScreen_PIN_Result_Callback_t callback);

/**
 * @brief Show the first-time setup wizard — calls callback on completion
 * @param callback Called with completion status, WiFi config, and credentials
 */
void TouchScreen_UI_ShowSetupWizard(TouchScreen_Setup_Wizard_Callback_t callback);

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_UI_MANAGER_H */
