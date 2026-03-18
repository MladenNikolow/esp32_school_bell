#ifndef INFO_SCREEN_INTERNAL_H
#define INFO_SCREEN_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the info/about screen content (internal)
 */
void touchscreen_info_screen_create(void);

/**
 * @brief Destroy the info/about screen (internal)
 */
void touchscreen_info_screen_destroy(void);

/**
 * @brief Periodic 1s update: refresh QR code if IP changes
 */
void touchscreen_info_screen_update(void);

#ifdef __cplusplus
}
#endif

#endif /* INFO_SCREEN_INTERNAL_H */
