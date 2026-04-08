#ifndef WIFI_LIST_ITEM_COMPONENT_H
#define WIFI_LIST_ITEM_COMPONENT_H

#include "lvgl.h"
#include "TouchScreen_Services.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback when a WiFi list item is tapped
 * @param ssid The SSID of the tapped network
 * @param bssid The BSSID (6-byte MAC) of the tapped network
 * @param secured true if network requires password
 */
typedef void (*wifi_list_item_click_cb_t)(const char *ssid, const uint8_t *bssid, bool secured);

/**
 * @brief Create a WiFi network list item row
 * @param parent Parent LVGL object (the scrollable list)
 * @param ap WiFi AP info
 * @param click_cb Callback when item is tapped
 * @return Created list item object, or NULL on error
 */
lv_obj_t *wifi_list_item_create(lv_obj_t *parent, const TS_WiFi_AP_t *ap,
                                 wifi_list_item_click_cb_t click_cb);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_LIST_ITEM_COMPONENT_H */
