#ifndef TOUCHSCREEN_UI_NAVBAR_H
#define TOUCHSCREEN_UI_NAVBAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdint.h>

/* Navigation tab indices */
#define UI_NAV_TAB_DASHBOARD    0
#define UI_NAV_TAB_SCHEDULE     1
#define UI_NAV_TAB_SETTINGS     2
#define UI_NAV_TAB_INFO         3

/**
 * @brief Callback invoked when a navigation tab is tapped
 * @param tab_index UI_NAV_TAB_DASHBOARD, UI_NAV_TAB_SCHEDULE, or UI_NAV_TAB_SETTINGS
 */
typedef void (*ui_navbar_tab_cb_t)(uint8_t tab_index);

/**
 * @brief Create the bottom navigation bar
 * @param parent The screen object to attach the navbar to
 * @param cb Callback for tab selection changes
 * @return The navbar container object, or NULL on failure
 */
lv_obj_t *ui_navbar_create(lv_obj_t *parent, ui_navbar_tab_cb_t cb);

/**
 * @brief Destroy the navbar (clear references)
 */
void ui_navbar_destroy(void);

/**
 * @brief Set the active tab visually
 * @param tab_index UI_NAV_TAB_DASHBOARD or UI_NAV_TAB_SCHEDULE
 */
void ui_navbar_set_active(uint8_t tab_index);

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_UI_NAVBAR_H */
