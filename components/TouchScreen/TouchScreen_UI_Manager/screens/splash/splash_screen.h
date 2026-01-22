#ifndef SPLASH_SCREEN_H
#define SPLASH_SCREEN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and display the splash screen with logo
 * @param duration_ms Duration to display the logo in milliseconds
 * @note This function will block for the specified duration
 */
void splash_screen_create(uint32_t duration_ms);

/**
 * @brief Destroy the splash screen and cleanup resources
 */
void splash_screen_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* SPLASH_SCREEN_H */
