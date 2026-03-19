#ifndef SETUP_WIZARD_SCREEN_INTERNAL_H
#define SETUP_WIZARD_SCREEN_INTERNAL_H

#include "../../include/TouchScreen_UI_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the setup wizard screen (internal — called from ui_manager)
 */
void touchscreen_setup_wizard_screen_create(void);

/**
 * @brief Destroy the setup wizard screen and free resources (internal)
 */
void touchscreen_setup_wizard_screen_destroy(void);

/**
 * @brief Periodic update for wizard screen (called every 1s)
 */
void touchscreen_setup_wizard_screen_update(void);

#ifdef __cplusplus
}
#endif

#endif /* SETUP_WIZARD_SCREEN_INTERNAL_H */
