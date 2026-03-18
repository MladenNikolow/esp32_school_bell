#ifndef DASHBOARD_SCREEN_INTERNAL_H
#define DASHBOARD_SCREEN_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the dashboard screen content (internal — called from ui_manager)
 */
void touchscreen_dashboard_screen_create(void);

/**
 * @brief Destroy the dashboard screen (internal — called from ui_manager)
 */
void touchscreen_dashboard_screen_destroy(void);

/**
 * @brief Periodic 1s update: refresh clock, bell state, next bell info
 */
void touchscreen_dashboard_screen_update(void);

#ifdef __cplusplus
}
#endif

#endif /* DASHBOARD_SCREEN_INTERNAL_H */
