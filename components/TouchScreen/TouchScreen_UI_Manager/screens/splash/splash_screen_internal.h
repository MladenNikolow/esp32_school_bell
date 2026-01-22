#ifndef SPLASH_SCREEN_INTERNAL_H
#define SPLASH_SCREEN_INTERNAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and display the splash screen with logo (internal)
 * @param duration_ms Duration to display the logo in milliseconds
 */
void touchscreen_splash_screen_create(uint32_t duration_ms);

/**
 * @brief Destroy the splash screen (internal)
 */
void touchscreen_splash_screen_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* SPLASH_SCREEN_INTERNAL_H */
