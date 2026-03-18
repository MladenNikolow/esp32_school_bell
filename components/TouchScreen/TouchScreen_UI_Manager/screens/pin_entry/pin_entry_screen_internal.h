#ifndef PIN_ENTRY_SCREEN_INTERNAL_H
#define PIN_ENTRY_SCREEN_INTERNAL_H

#include "TouchScreen_UI_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the PIN entry overlay (internal — called from ui_manager)
 * @param callback PIN result callback
 */
void touchscreen_pin_entry_screen_create(TouchScreen_PIN_Result_Callback_t callback);

/**
 * @brief Destroy the PIN entry overlay (internal — called from ui_manager)
 */
void touchscreen_pin_entry_screen_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* PIN_ENTRY_SCREEN_INTERNAL_H */
