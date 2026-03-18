#ifndef SETTINGS_SCREEN_INTERNAL_H
#define SETTINGS_SCREEN_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the settings screen content (internal)
 */
void touchscreen_settings_screen_create(void);

/**
 * @brief Destroy the settings screen (internal)
 */
void touchscreen_settings_screen_destroy(void);

/**
 * @brief Periodic 1s update: refresh displayed values
 */
void touchscreen_settings_screen_update(void);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_SCREEN_INTERNAL_H */
