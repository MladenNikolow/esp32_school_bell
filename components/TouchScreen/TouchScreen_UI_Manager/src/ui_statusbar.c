#include "ui_statusbar.h"
#include "ui_theme.h"
#include "ui_config.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "UI_STATUSBAR";

/* Status bar widget references */
static lv_obj_t *s_bar = NULL;
static lv_obj_t *s_bell_icon = NULL;
static lv_obj_t *s_app_label = NULL;
static lv_obj_t *s_time_label = NULL;

/* WiFi icon is now a clickable container with WiFi glyph + "X" overlay */
static lv_obj_t *s_wifi_container = NULL;
static lv_obj_t *s_wifi_icon = NULL;
static lv_obj_t *s_wifi_x = NULL;

/* Bell ringing animation state */
static lv_anim_t s_bell_anim;
static bool      s_bell_anim_running = false;

static void bell_anim_opa_cb(void *obj, int32_t val)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)val, 0);
}

lv_obj_t *ui_statusbar_create(lv_obj_t *parent)
{
    if (!parent) return NULL;

    /* Container bar at top */
    s_bar = lv_obj_create(parent);
    lv_obj_set_size(s_bar, TOUCHSCREEN_UI_SCREEN_WIDTH, UI_STATUSBAR_HEIGHT);
    lv_obj_align(s_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s_bar, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_bar, 0, 0);
    lv_obj_set_style_radius(s_bar, 0, 0);
    lv_obj_set_style_pad_left(s_bar, 12, 0);
    lv_obj_set_style_pad_right(s_bar, 12, 0);
    lv_obj_set_style_pad_top(s_bar, 0, 0);
    lv_obj_set_style_pad_bottom(s_bar, 0, 0);
    lv_obj_set_scrollbar_mode(s_bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(s_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Left group: bell icon + app name */
    lv_obj_t *left_group = lv_obj_create(s_bar);
    lv_obj_set_size(left_group, LV_SIZE_CONTENT, UI_STATUSBAR_HEIGHT);
    lv_obj_set_style_bg_opa(left_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_group, 0, 0);
    lv_obj_set_style_pad_all(left_group, 0, 0);
    lv_obj_set_scrollbar_mode(left_group, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(left_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left_group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(left_group, 6, 0);

    /* Bell icon (🔔) */
    s_bell_icon = lv_label_create(left_group);
    lv_label_set_text(s_bell_icon, LV_SYMBOL_BELL);
    lv_obj_set_style_text_color(s_bell_icon, UI_COLOR_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(s_bell_icon, UI_FONT_BODY, 0);

    /* App name */
    s_app_label = lv_label_create(left_group);
    lv_label_set_text(s_app_label, "Ringy");
    lv_obj_set_style_text_color(s_app_label, UI_COLOR_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(s_app_label, UI_FONT_H3, 0);

    /* Center: clock — floating so it's always perfectly centered
     * regardless of left/right group widths */
    s_time_label = lv_label_create(s_bar);
    lv_label_set_text(s_time_label, "--:--:--");
    lv_obj_set_style_text_color(s_time_label, UI_COLOR_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(s_time_label, UI_FONT_H3, 0);
    lv_obj_add_flag(s_time_label, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(s_time_label, LV_ALIGN_CENTER, 0, 0);

    /* Right group: WiFi + NTP icons */
    lv_obj_t *right_group = lv_obj_create(s_bar);
    lv_obj_set_size(right_group, LV_SIZE_CONTENT, UI_STATUSBAR_HEIGHT);
    lv_obj_set_style_bg_opa(right_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_group, 0, 0);
    lv_obj_set_style_pad_all(right_group, 0, 0);
    lv_obj_set_scrollbar_mode(right_group, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(right_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_group, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right_group, 8, 0);

    /* WiFi clickable container — holds the WiFi glyph and an "X" overlay.
     * Enlarged to 48×48 for comfortable finger tap (~7.2mm on 4" 480px display).
     * Extended click area adds 8px padding for ~64px effective touch zone. */
    s_wifi_container = lv_obj_create(right_group);
    lv_obj_set_size(s_wifi_container, 48, 40);
    lv_obj_set_style_bg_color(s_wifi_container, UI_COLOR_DANGER, 0);
    lv_obj_set_style_bg_opa(s_wifi_container, LV_OPA_TRANSP, 0);  /* Updated in set_wifi_connected */
    lv_obj_set_style_border_width(s_wifi_container, 0, 0);
    lv_obj_set_style_radius(s_wifi_container, 6, 0);
    lv_obj_set_style_pad_all(s_wifi_container, 0, 0);
    lv_obj_set_scrollbar_mode(s_wifi_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(s_wifi_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_wifi_container, 8);

    /* WiFi glyph */
    s_wifi_icon = lv_label_create(s_wifi_container);
    lv_label_set_text(s_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_wifi_icon, lv_color_hex(0x80FFFFFF), 0);  /* Dimmed by default */
    lv_obj_set_style_text_font(s_wifi_icon, UI_FONT_BODY, 0);
    lv_obj_center(s_wifi_icon);

    /* Small "X" overlay — positioned bottom-right within container */
    s_wifi_x = lv_label_create(s_wifi_container);
    lv_label_set_text(s_wifi_x, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(s_wifi_x, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(s_wifi_x, UI_COLOR_TEXT_ON_PRIMARY, 0);
    lv_obj_align(s_wifi_x, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_flag(s_wifi_x, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "Status bar created");
    return s_bar;
}

void ui_statusbar_destroy(void)
{
    if (s_bell_anim_running && s_bell_icon) {
        lv_anim_delete(s_bell_icon, bell_anim_opa_cb);
        s_bell_anim_running = false;
    }
    s_bar = NULL;
    s_bell_icon = NULL;
    s_app_label = NULL;
    s_time_label = NULL;
    s_wifi_container = NULL;
    s_wifi_icon = NULL;
    s_wifi_x = NULL;
}

void ui_statusbar_update_time(const char *time_str)
{
    if (s_time_label && time_str) {
        lv_label_set_text(s_time_label, time_str);
    }
}

void ui_statusbar_set_wifi_connected(bool connected)
{
    if (s_wifi_icon) {
        lv_obj_set_style_text_color(s_wifi_icon,
            connected ? UI_COLOR_TEXT_ON_PRIMARY : UI_COLOR_TEXT_ON_PRIMARY, 0);
    }
    if (s_wifi_x) {
        if (connected) {
            lv_obj_add_flag(s_wifi_x, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(s_wifi_x, LV_OBJ_FLAG_HIDDEN);
        }
    }
    /* Subtle red tint on container when disconnected — visually hints "tap to fix" */
    if (s_wifi_container) {
        lv_obj_set_style_bg_opa(s_wifi_container,
            connected ? LV_OPA_TRANSP : LV_OPA_50, 0);
    }
}

void ui_statusbar_set_wifi_click_cb(lv_event_cb_t cb, void *user_data)
{
    if (s_wifi_container && cb) {
        lv_obj_add_event_cb(s_wifi_container, cb, LV_EVENT_CLICKED, user_data);
    }
}

void ui_statusbar_set_ntp_synced(bool synced)
{
    (void)synced;  /* NTP status shown via warning banner on dashboard */
}

void ui_statusbar_set_bell_state(uint8_t state)
{
    if (!s_bell_icon) return;

    switch (state) {
        case 0:  /* Idle */
            lv_obj_set_style_text_color(s_bell_icon, UI_COLOR_TEXT_ON_PRIMARY, 0);
            break;
        case 1:  /* Ringing */
            lv_obj_set_style_text_color(s_bell_icon, UI_COLOR_WARNING, 0);
            break;
        case 2:  /* Panic */
            lv_obj_set_style_text_color(s_bell_icon, UI_COLOR_DANGER, 0);
            break;
        default:
            break;
    }

    /* Start/stop bell pulsing animation for ringing state */
    if (state == 1 && !s_bell_anim_running) {
        lv_anim_init(&s_bell_anim);
        lv_anim_set_var(&s_bell_anim, s_bell_icon);
        lv_anim_set_values(&s_bell_anim, LV_OPA_COVER, LV_OPA_30);
        lv_anim_set_duration(&s_bell_anim, 400);
        lv_anim_set_playback_duration(&s_bell_anim, 400);
        lv_anim_set_repeat_count(&s_bell_anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&s_bell_anim, bell_anim_opa_cb);
        lv_anim_start(&s_bell_anim);
        s_bell_anim_running = true;
    } else if (state != 1 && s_bell_anim_running) {
        lv_anim_delete(s_bell_icon, bell_anim_opa_cb);
        lv_obj_set_style_opa(s_bell_icon, LV_OPA_COVER, 0);
        s_bell_anim_running = false;
    }
}
