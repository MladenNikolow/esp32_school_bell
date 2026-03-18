#ifndef TOUCHSCREEN_UI_STATUSBAR_H
#define TOUCHSCREEN_UI_STATUSBAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdbool.h>

/**
 * @brief Create the status bar on the given parent screen
 * @param parent The screen object to attach the status bar to
 * @return The status bar container object, or NULL on failure
 */
lv_obj_t *ui_statusbar_create(lv_obj_t *parent);

/**
 * @brief Destroy the status bar
 */
void ui_statusbar_destroy(void);

/**
 * @brief Update the clock label with current time string
 * @param time_str Time string in "HH:MM:SS" format
 */
void ui_statusbar_update_time(const char *time_str);

/**
 * @brief Set WiFi connection indicator
 * @param connected true if WiFi is connected
 */
void ui_statusbar_set_wifi_connected(bool connected);

/**
 * @brief Register a click callback on the WiFi status icon
 * @param cb    LVGL event callback invoked on LV_EVENT_CLICKED
 * @param user_data Opaque pointer forwarded to the callback
 */
void ui_statusbar_set_wifi_click_cb(lv_event_cb_t cb, void *user_data);

/**
 * @brief Set NTP sync indicator
 * @param synced true if NTP is synchronized
 */
void ui_statusbar_set_ntp_synced(bool synced);

/**
 * @brief Set bell state indicator
 * @param state 0=idle, 1=ringing, 2=panic
 */
void ui_statusbar_set_bell_state(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_UI_STATUSBAR_H */
