#ifndef SCHEDULE_VIEW_SCREEN_INTERNAL_H
#define SCHEDULE_VIEW_SCREEN_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the schedule view screen content (internal)
 */
void touchscreen_schedule_view_screen_create(void);

/**
 * @brief Destroy the schedule view screen (internal)
 */
void touchscreen_schedule_view_screen_destroy(void);

/**
 * @brief Periodic 1s update: refresh bell list highlighting
 */
void touchscreen_schedule_view_screen_update(void);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULE_VIEW_SCREEN_INTERNAL_H */
