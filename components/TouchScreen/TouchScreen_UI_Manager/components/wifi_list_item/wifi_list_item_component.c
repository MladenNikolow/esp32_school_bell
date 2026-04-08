/* ================================================================== */
/* wifi_list_item_component.c — WiFi network row for scan results      */
/* Signal icon + SSID + lock icon + RSSI value                         */
/* ================================================================== */
#include "wifi_list_item_component.h"
#include "../../src/ui_theme.h"
#include "lvgl.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Signal-strength icon (UTF-8 bars via built-in LVGL symbols)         */
/* We use simple text labels:  ▂▄▆█ style using block characters      */
/* ------------------------------------------------------------------ */
static const char *
wifi_signal_text(int8_t rssi)
{
    if (rssi >= -50) return LV_SYMBOL_WIFI;       /* Excellent */
    if (rssi >= -60) return LV_SYMBOL_WIFI;       /* Good      */
    if (rssi >= -70) return LV_SYMBOL_WIFI;       /* Fair      */
    return LV_SYMBOL_WIFI;                         /* Weak      */
}

static lv_color_t
wifi_signal_color(int8_t rssi)
{
    if (rssi >= -50) return lv_color_hex(0x4CAF50);  /* Green — excellent  */
    if (rssi >= -60) return lv_color_hex(0x8BC34A);  /* Light green — good */
    if (rssi >= -70) return lv_color_hex(0xFF9800);  /* Orange — fair      */
    return lv_color_hex(0xF44336);                    /* Red — weak         */
}

/* ------------------------------------------------------------------ */
/* Event handler — forward click to app callback                       */
/* ------------------------------------------------------------------ */
typedef struct {
    char                      ssid[33];
    uint8_t                   bssid[6];
    bool                      secured;
    wifi_list_item_click_cb_t cb;
} wifi_item_data_t;

static void
wifi_item_click_handler(lv_event_t *e)
{
    wifi_item_data_t *data = (wifi_item_data_t *)lv_event_get_user_data(e);
    if (data && data->cb) {
        data->cb(data->ssid, data->bssid, data->secured);
    }
}

/* Clean up the allocated user data when the item is deleted */
static void
wifi_item_delete_handler(lv_event_t *e)
{
    wifi_item_data_t *data = (wifi_item_data_t *)lv_event_get_user_data(e);
    if (data) {
        free(data);
    }
}

/* ------------------------------------------------------------------ */
/* Public: create one row                                              */
/* ------------------------------------------------------------------ */
lv_obj_t *
wifi_list_item_create(lv_obj_t *parent, const TS_WiFi_AP_t *ap,
                      wifi_list_item_click_cb_t click_cb)
{
    if (!parent || !ap) return NULL;

    /* --- Row container --- */
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), 52);
    lv_obj_set_style_bg_color(row, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(row, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_hor(row, 12, 0);
    lv_obj_set_style_pad_ver(row, 6, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 10, 0);

    /* Pressed feedback */
    lv_obj_set_style_bg_color(row, lv_color_hex(0xE3F2FD), LV_STATE_PRESSED);

    /* --- Signal icon --- */
    lv_obj_t *signal_lbl = lv_label_create(row);
    lv_label_set_text(signal_lbl, wifi_signal_text(ap->cRssi));
    lv_obj_set_style_text_color(signal_lbl, wifi_signal_color(ap->cRssi), 0);
    lv_obj_set_style_text_font(signal_lbl, UI_FONT_BODY, 0);
    lv_obj_set_style_min_width(signal_lbl, 24, 0);

    /* --- SSID label (grows to fill) --- */
    lv_obj_t *ssid_lbl = lv_label_create(row);
    lv_label_set_text(ssid_lbl, ap->acSsid[0] ? ap->acSsid : "(Hidden)");
    lv_obj_set_style_text_color(ssid_lbl, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(ssid_lbl, UI_FONT_BODY, 0);
    lv_label_set_long_mode(ssid_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(ssid_lbl, 1);

    /* --- Lock icon (if secured) --- */
    if (ap->bSecured) {
        lv_obj_t *lock_lbl = lv_label_create(row);
        lv_label_set_text(lock_lbl, LV_SYMBOL_EYE_CLOSE);
        lv_obj_set_style_text_color(lock_lbl, UI_COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(lock_lbl, UI_FONT_CAPTION, 0);
    }

    /* --- RSSI value --- */
    lv_obj_t *rssi_lbl = lv_label_create(row);
    char rssi_buf[12];
    snprintf(rssi_buf, sizeof(rssi_buf), "%d dBm", (int)ap->cRssi);
    lv_label_set_text(rssi_lbl, rssi_buf);
    lv_obj_set_style_text_color(rssi_lbl, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(rssi_lbl, UI_FONT_SMALL, 0);
    lv_obj_set_style_min_width(rssi_lbl, 60, 0);

    /* --- Click handler with data --- */
    if (click_cb) {
        wifi_item_data_t *data = calloc(1, sizeof(wifi_item_data_t));
        if (data) {
            strncpy(data->ssid, ap->acSsid, sizeof(data->ssid) - 1);
            memcpy(data->bssid, ap->abBssid, 6);
            data->secured = ap->bSecured;
            data->cb = click_cb;
            lv_obj_add_event_cb(row, wifi_item_click_handler, LV_EVENT_CLICKED, data);
            lv_obj_add_event_cb(row, wifi_item_delete_handler, LV_EVENT_DELETE, data);
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        }
    }

    return row;
}
