#ifndef PIN_ENTRY_SCREEN_H
#define PIN_ENTRY_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "TouchScreen_UI_Types.h"

/**
 * @brief Show the PIN entry overlay screen
 *
 * Pushes a semi-transparent overlay with a 4-digit numeric keypad.
 * Calls the result callback with true on correct PIN, false on cancel.
 *
 * @param callback Result callback (success=true, cancelled=false)
 */
void pin_entry_screen_show(TouchScreen_PIN_Result_Callback_t callback);

#ifdef __cplusplus
}
#endif

#endif /* PIN_ENTRY_SCREEN_H */
