/* ================================================================== */
/* wifi_setup_screen.c — WiFi Setup: Scan → Select → Password → Save  */
/*                                                                      */
/* Views on one screen object:                                          */
/*   VIEW_LIST       – header + scan btn + scrollable network list      */
/*   VIEW_PASSWORD   – selected SSID + password field + keyboard        */
/*   VIEW_MANUAL     – SSID + password input fields + keyboard          */
/*   VIEW_CONNECTING – spinner + status while connecting                */
/*                                                                      */
/* The callback (set by AppTask) receives SSID+password on "Connect"    */
/* or a CANCEL on "Skip".  AppTask then saves to NVS and reboots.      */
/* The WiFi retry / AP-mode logic in WebServer is NOT touched here.     */
/* ================================================================== */
#include "wifi_setup_screen_internal.h"
#include "../../src/ui_manager_internal.h"
#include "../../components/wifi_list_item/wifi_list_item_component.h"
#include "../../components/input_field/input_field_component.h"
#include "../../src/ui_theme.h"
#include "../../src/ui_strings.h"
#include "../../src/ui_config.h"
#include "TouchScreen_Services.h"
#include "TouchScreen_UI_Manager.h"
#include "TouchScreen_UI_Types.h"
#include "lvgl.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "WIFI_SETUP";

/* ------------------------------------------------------------------ */
/* Layout constants for 480×480 display                                */
/* ------------------------------------------------------------------ */
#define WIFI_HEADER_H       60
#define WIFI_BODY_H         (480 - WIFI_HEADER_H)          /* 420 */
#define WIFI_KB_H           200  /* 50px per key row ≈ 7.5mm on 4" display */
#define WIFI_FORM_GAP       8    /* Gap between form and keyboard */
#define WIFI_FORM_W         440
/* Height of form when keyboard is visible */
#define WIFI_FORM_H_KB      (WIFI_BODY_H - WIFI_KB_H - WIFI_FORM_GAP - 8)  /* ~204 */
/* Heights when keyboard is hidden */
#define WIFI_PW_FORM_H      230
#define WIFI_MAN_FORM_H     280

/* Connecting view constants */
#define WIFI_CONNECT_CHECK_MS    500   /* Check interval for connection status */
#define WIFI_CONNECT_TIMEOUT_MS  6000  /* Max time to wait for connection */

/* ------------------------------------------------------------------ */
/* View states                                                         */
/* ------------------------------------------------------------------ */
typedef enum {
    VIEW_LIST,
    VIEW_PASSWORD,
    VIEW_MANUAL,
    VIEW_CONNECTING,
} wifi_view_t;

/* ------------------------------------------------------------------ */
/* Module state                                                        */
/* ------------------------------------------------------------------ */
static lv_obj_t  *s_screen        = NULL;   /* lv_scr_act() ref            */
static lv_obj_t  *s_header        = NULL;   /* Blue header bar             */
static lv_obj_t  *s_body          = NULL;   /* Content area below header   */

/* LIST view widgets */
static lv_obj_t  *s_top_row       = NULL;
static lv_obj_t  *s_scan_btn      = NULL;
static lv_obj_t  *s_scan_btn_lbl  = NULL;   /* Scan button label (for text updates) */
static lv_obj_t  *s_list          = NULL;   /* Scrollable network list     */
static lv_obj_t  *s_status_label  = NULL;   /* "Scanning…" / "N networks"  */
static lv_obj_t  *s_bot_row       = NULL;
static lv_obj_t  *s_manual_btn    = NULL;   /* "Enter manually" label-btn  */
static lv_obj_t  *s_skip_btn      = NULL;

/* PASSWORD view widgets */
static lv_obj_t  *s_pw_container  = NULL;
static lv_obj_t  *s_pw_ssid_label = NULL;
static lv_obj_t  *s_pw_input      = NULL;
static lv_obj_t  *s_pw_show_cb    = NULL;   /* Show-password checkbox       */
static lv_obj_t  *s_pw_connect    = NULL;
static lv_obj_t  *s_pw_back       = NULL;
static lv_obj_t  *s_keyboard      = NULL;

/* MANUAL view widgets */
static lv_obj_t  *s_man_container = NULL;
static lv_obj_t  *s_man_ssid_inp  = NULL;
static lv_obj_t  *s_man_pw_inp    = NULL;
static lv_obj_t  *s_man_show_cb   = NULL;   /* Show-password checkbox       */
static lv_obj_t  *s_man_connect   = NULL;
static lv_obj_t  *s_man_back      = NULL;

/* CONNECTING view widgets */
static lv_obj_t  *s_conn_container = NULL;
static lv_obj_t  *s_conn_spinner   = NULL;
static lv_obj_t  *s_conn_label     = NULL;
static lv_obj_t  *s_conn_status    = NULL;
static lv_obj_t  *s_conn_back      = NULL;
static lv_timer_t *s_conn_timer    = NULL;
static uint32_t   s_conn_elapsed   = 0;

static wifi_view_t s_view = VIEW_LIST;
static bool s_keyboard_visible = false;   /* Track keyboard visibility */
static bool s_scan_in_progress = false;   /* Guard against concurrent scans */
static bool s_is_initial_setup = false;   /* true = boot-time, false = runtime reconfiguration */

/* Callback and selected network info */
static TouchScreen_WiFi_Setup_Callback_t s_callback = NULL;
static char s_selected_ssid[33]  = {0};
static uint8_t s_selected_bssid[6] = {0};
static char s_selected_pw[65]    = {0};
static bool s_selected_secured   = false;

/* Scan results buffer */
static TS_WiFi_AP_t s_ap_list[TS_WIFI_SCAN_MAX_AP];
static uint16_t     s_ap_count = 0;

/* Async scan poll timer */
static lv_timer_t  *s_scan_poll_timer = NULL;
#define SCAN_POLL_INTERVAL_MS  200

/* ------------------------------------------------------------------ */
/* Forward declarations                                                */
/* ------------------------------------------------------------------ */
static void view_show_list(void);
static void view_show_password(const char *ssid, bool secured);
static void view_show_manual(void);
static void view_show_connecting(const char *ssid);
static void do_scan(void);
static void on_scan_poll_timer(lv_timer_t *timer);
static void populate_list(void);
static void hide_keyboard(void);
static void show_keyboard_for(lv_obj_t *ta);
static void adjust_form_for_keyboard(bool kb_visible);

/* ------------------------------------------------------------------ */
/* Helper: create the blue header bar                                  */
/* ------------------------------------------------------------------ */
static void
create_header(lv_obj_t *parent, const char *title)
{
    s_header = lv_obj_create(parent);
    lv_obj_set_size(s_header, 480, WIFI_HEADER_H);
    lv_obj_align(s_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s_header, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(s_header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_header, 0, 0);
    lv_obj_set_style_radius(s_header, 0, 0);
    lv_obj_set_style_pad_hor(s_header, 16, 0);
    lv_obj_clear_flag(s_header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(s_header);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_H2, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
}

/* ------------------------------------------------------------------ */
/* Helper: create the body container below header                      */
/* ------------------------------------------------------------------ */
static void
create_body(lv_obj_t *parent)
{
    s_body = lv_obj_create(parent);
    lv_obj_set_size(s_body, 480, WIFI_BODY_H);
    lv_obj_align(s_body, LV_ALIGN_TOP_MID, 0, WIFI_HEADER_H);
    lv_obj_set_style_bg_color(s_body, UI_COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(s_body, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_body, 0, 0);
    lv_obj_set_style_radius(s_body, 0, 0);
    lv_obj_set_style_pad_all(s_body, 0, 0);
    lv_obj_clear_flag(s_body, LV_OBJ_FLAG_SCROLLABLE);

    /* Click on body background dismisses keyboard */
    lv_obj_add_flag(s_body, LV_OBJ_FLAG_CLICKABLE);
}

/* ------------------------------------------------------------------ */
/* EVENT HANDLERS                                                      */
/* ------------------------------------------------------------------ */

static void
on_scan_btn(lv_event_t *e)
{
    (void)e;
    if (s_scan_in_progress) return;  /* Guard against double-tap */
    do_scan();
}

static void
on_skip_btn(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Skip pressed");
    if (s_callback) {
        s_callback(TOUCHSCREEN_WIFI_SETUP_RESULT_CANCEL, NULL, NULL);
    }
}

static void
on_network_selected(const char *ssid, const uint8_t *bssid, bool secured)
{
    ESP_LOGI(TAG, "Network selected: %s (secured=%d)", ssid, secured);
    strncpy(s_selected_ssid, ssid, sizeof(s_selected_ssid) - 1);
    s_selected_ssid[sizeof(s_selected_ssid) - 1] = '\0';
    if (bssid != NULL) {
        memcpy(s_selected_bssid, bssid, 6);
    } else {
        memset(s_selected_bssid, 0, 6);
    }
    s_selected_secured = secured;

    if (secured) {
        view_show_password(ssid, secured);
    } else {
        /* Open network — go through connecting view */
        s_selected_pw[0] = '\0';
        view_show_connecting(ssid);
    }
}

static void
on_manual_btn(lv_event_t *e)
{
    (void)e;
    view_show_manual();
}

/* Password view handlers */
static void
on_pw_connect(lv_event_t *e)
{
    (void)e;
    if (!s_pw_input) return;
    const char *pw = lv_textarea_get_text(s_pw_input);
    strncpy(s_selected_pw, pw ? pw : "", sizeof(s_selected_pw) - 1);
    s_selected_pw[sizeof(s_selected_pw) - 1] = '\0';
    view_show_connecting(s_selected_ssid);
}

static void
on_pw_back(lv_event_t *e)
{
    (void)e;
    view_show_list();
}

static void
on_pw_show_toggle(lv_event_t *e)
{
    (void)e;
    if (!s_pw_input || !s_pw_show_cb) return;
    bool checked = lv_obj_has_state(s_pw_show_cb, LV_STATE_CHECKED);
    lv_textarea_set_password_mode(s_pw_input, !checked);
}

/* Manual view show password toggle */
static void
on_man_show_toggle(lv_event_t *e)
{
    (void)e;
    if (!s_man_pw_inp || !s_man_show_cb) return;
    bool checked = lv_obj_has_state(s_man_show_cb, LV_STATE_CHECKED);
    lv_textarea_set_password_mode(s_man_pw_inp, !checked);
}

/* ------------------------------------------------------------------ */
/* Keyboard helpers                                                    */
/* ------------------------------------------------------------------ */
static void
hide_keyboard(void)
{
    if (!s_keyboard || !s_keyboard_visible) return;

    bsp_display_lock(0);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(s_keyboard, NULL);
    s_keyboard_visible = false;

    adjust_form_for_keyboard(false);
    bsp_display_unlock();
}

static void
show_keyboard_for(lv_obj_t *ta)
{
    if (!s_keyboard || !ta) return;
    if (s_keyboard_visible) {
        /* Just re-target the keyboard */
        bsp_display_lock(0);
        lv_keyboard_set_textarea(s_keyboard, ta);
        bsp_display_unlock();
        return;
    }

    bsp_display_lock(0);
    lv_keyboard_set_textarea(s_keyboard, ta);
    lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    s_keyboard_visible = true;
    adjust_form_for_keyboard(true);
    bsp_display_unlock();
}

/* Resize form containers and hide/show checkboxes to avoid keyboard overlap */
static void
adjust_form_for_keyboard(bool kb_visible)
{
    if (s_view == VIEW_PASSWORD && s_pw_container) {
        lv_obj_set_height(s_pw_container,
            kb_visible ? WIFI_FORM_H_KB : WIFI_PW_FORM_H);
        /* Hide show-password checkbox when keyboard visible to prevent overlap with buttons */
        if (s_pw_show_cb) {
            if (kb_visible) lv_obj_add_flag(s_pw_show_cb, LV_OBJ_FLAG_HIDDEN);
            else            lv_obj_clear_flag(s_pw_show_cb, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (s_view == VIEW_MANUAL && s_man_container) {
        lv_obj_set_height(s_man_container,
            kb_visible ? WIFI_FORM_H_KB : WIFI_MAN_FORM_H);
        if (s_man_show_cb) {
            if (kb_visible) lv_obj_add_flag(s_man_show_cb, LV_OBJ_FLAG_HIDDEN);
            else            lv_obj_clear_flag(s_man_show_cb, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* Keyboard events */
static void
on_keyboard_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CANCEL) {
        hide_keyboard();
    } else if (code == LV_EVENT_READY) {
        /* Enter pressed — trigger connect */
        if (s_view == VIEW_PASSWORD && s_pw_input) {
            on_pw_connect(NULL);
        } else if (s_view == VIEW_MANUAL && s_man_ssid_inp) {
            /* Simulate manual connect button press */
            const char *ssid = lv_textarea_get_text(s_man_ssid_inp);
            const char *pw   = s_man_pw_inp ? lv_textarea_get_text(s_man_pw_inp) : "";
            if (ssid && ssid[0] != '\0') {
                strncpy(s_selected_ssid, ssid, sizeof(s_selected_ssid) - 1);
                s_selected_ssid[sizeof(s_selected_ssid) - 1] = '\0';
                strncpy(s_selected_pw, pw ? pw : "", sizeof(s_selected_pw) - 1);
                s_selected_pw[sizeof(s_selected_pw) - 1] = '\0';
                memset(s_selected_bssid, 0, 6);  /* Manual entry — no BSSID */
                view_show_connecting(s_selected_ssid);
            }
        }
    }
}

/* Show keyboard when textarea is focused */
static void
on_textarea_focus(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    show_keyboard_for(ta);
}

/* Dismiss keyboard when tapping outside input fields */
static void
on_body_click(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    /* Only dismiss if tap was on the body itself (not on a child widget) */
    if (target == s_body && s_keyboard_visible) {
        hide_keyboard();
    }
}

/* Manual view handlers */
static void
on_man_connect(lv_event_t *e)
{
    (void)e;
    if (!s_man_ssid_inp) return;
    const char *ssid = lv_textarea_get_text(s_man_ssid_inp);
    const char *pw   = s_man_pw_inp ? lv_textarea_get_text(s_man_pw_inp) : "";
    if (ssid && ssid[0] != '\0') {
        strncpy(s_selected_ssid, ssid, sizeof(s_selected_ssid) - 1);
        s_selected_ssid[sizeof(s_selected_ssid) - 1] = '\0';
        strncpy(s_selected_pw, pw ? pw : "", sizeof(s_selected_pw) - 1);
        s_selected_pw[sizeof(s_selected_pw) - 1] = '\0';
        memset(s_selected_bssid, 0, 6);  /* Manual entry — no BSSID */
        view_show_connecting(s_selected_ssid);
    }
}

static void
on_man_back(lv_event_t *e)
{
    (void)e;
    view_show_list();
}

/* Connecting view handlers */
static void
on_conn_back(lv_event_t *e)
{
    (void)e;
    /* Cancel connecting — stop timer and go back to list */
    if (s_conn_timer) {
        lv_timer_delete(s_conn_timer);
        s_conn_timer = NULL;
    }
    view_show_list();
}

static void
on_conn_timer(lv_timer_t *timer)
{
    (void)timer;
    s_conn_elapsed += WIFI_CONNECT_CHECK_MS;

    bsp_display_lock(0);

    if (TS_WiFi_IsConnected()) {
        /* Connected! Show success briefly */
        if (s_conn_timer) {
            lv_timer_delete(s_conn_timer);
            s_conn_timer = NULL;
        }
        if (s_conn_spinner) lv_obj_add_flag(s_conn_spinner, LV_OBJ_FLAG_HIDDEN);
        if (s_conn_status) {
            lv_label_set_text_fmt(s_conn_status, LV_SYMBOL_OK "  %s", ui_str(STR_CONNECTED));
            lv_obj_set_style_text_color(s_conn_status, UI_COLOR_SUCCESS, 0);
        }
        if (s_conn_back) lv_obj_add_flag(s_conn_back, LV_OBJ_FLAG_HIDDEN);
        bsp_display_unlock();

        /* Brief delay then navigate to dashboard */
        vTaskDelay(pdMS_TO_TICKS(800));
        if (s_callback) {
            s_callback(TOUCHSCREEN_WIFI_SETUP_RESULT_SUCCESS,
                       s_selected_ssid, s_selected_pw);
        }
        return;
    }

    if (s_conn_elapsed >= WIFI_CONNECT_TIMEOUT_MS) {
        /* Timeout — connection was initiated, retry mechanism handles the rest */
        if (s_conn_timer) {
            lv_timer_delete(s_conn_timer);
            s_conn_timer = NULL;
        }
        if (s_conn_spinner) lv_obj_add_flag(s_conn_spinner, LV_OBJ_FLAG_HIDDEN);
        if (s_conn_status) {
            lv_label_set_text(s_conn_status, ui_str(STR_CONNECTING_BG));
            lv_obj_set_style_text_color(s_conn_status, UI_COLOR_TEXT_SECONDARY, 0);
        }
        if (s_conn_back) lv_obj_add_flag(s_conn_back, LV_OBJ_FLAG_HIDDEN);
        bsp_display_unlock();

        vTaskDelay(pdMS_TO_TICKS(1200));
        if (s_callback) {
            s_callback(TOUCHSCREEN_WIFI_SETUP_RESULT_SUCCESS,
                       s_selected_ssid, s_selected_pw);
        }
        return;
    }

    bsp_display_unlock();
}

/* ------------------------------------------------------------------ */
/* Create shared keyboard (hidden initially)                           */
/* ------------------------------------------------------------------ */
static void
create_keyboard(void)
{
    if (s_keyboard) return;
    s_keyboard = lv_keyboard_create(s_screen);
    lv_obj_set_size(s_keyboard, 480, WIFI_KB_H);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_keyboard, on_keyboard_event, LV_EVENT_ALL, NULL);
    s_keyboard_visible = false;
}

/* ------------------------------------------------------------------ */
/* Helper: clear list views for password/manual/connecting transition   */
/* ------------------------------------------------------------------ */
static void
cleanup_password_manual_views(void)
{
    if (s_pw_container)   { lv_obj_delete(s_pw_container);   s_pw_container = NULL;  }
    if (s_man_container)  { lv_obj_delete(s_man_container);  s_man_container = NULL; }
    if (s_conn_back)      { lv_obj_delete(s_conn_back);      s_conn_back = NULL;     }
    if (s_conn_container) { lv_obj_delete(s_conn_container); s_conn_container = NULL; }
    s_pw_input = NULL; s_pw_ssid_label = NULL; s_pw_show_cb = NULL;
    s_pw_connect = NULL; s_pw_back = NULL;
    s_man_ssid_inp = NULL; s_man_pw_inp = NULL; s_man_show_cb = NULL;
    s_man_connect = NULL; s_man_back = NULL;
    s_conn_spinner = NULL; s_conn_label = NULL; s_conn_status = NULL;
    if (s_conn_timer) { lv_timer_delete(s_conn_timer); s_conn_timer = NULL; }
}

/* ------------------------------------------------------------------ */
/* VIEW_LIST: scan button + network list + skip                        */
/* ------------------------------------------------------------------ */
static void
view_show_list(void)
{
    bsp_display_lock(0);

    s_view = VIEW_LIST;

    /* Remove password / manual / connecting views if present */
    cleanup_password_manual_views();

    if (s_keyboard) {
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
        s_keyboard_visible = false;
    }

    /* If list view widgets already exist, just unhide them */
    if (s_list) {
        if (s_top_row) lv_obj_clear_flag(s_top_row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_list, LV_OBJ_FLAG_HIDDEN);
        if (s_bot_row) lv_obj_clear_flag(s_bot_row, LV_OBJ_FLAG_HIDDEN);
        bsp_display_unlock();
        return;
    }

    /* --- Top row: Scan button + status --- */
    s_top_row = lv_obj_create(s_body);
    lv_obj_set_size(s_top_row, 460, 48);
    lv_obj_align(s_top_row, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_opa(s_top_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_top_row, 0, 0);
    lv_obj_set_style_pad_all(s_top_row, 0, 0);
    lv_obj_clear_flag(s_top_row, LV_OBJ_FLAG_SCROLLABLE);

    s_scan_btn = lv_button_create(s_top_row);
    lv_obj_set_size(s_scan_btn, 140, 40);
    lv_obj_align(s_scan_btn, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(s_scan_btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(s_scan_btn, UI_BTN_RADIUS, 0);
    /* Pressed state feedback */
    lv_obj_set_style_bg_color(s_scan_btn, UI_COLOR_PRIMARY_DARK, LV_STATE_PRESSED);
    /* Disabled state (while scanning) */
    lv_obj_set_style_bg_color(s_scan_btn, lv_color_hex(0x90CAF9), LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(s_scan_btn, LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_add_event_cb(s_scan_btn, on_scan_btn, LV_EVENT_CLICKED, NULL);

    s_scan_btn_lbl = lv_label_create(s_scan_btn);
    lv_label_set_text_fmt(s_scan_btn_lbl, LV_SYMBOL_REFRESH " %s", ui_str(STR_SCAN));
    lv_obj_set_style_text_color(s_scan_btn_lbl, UI_COLOR_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(s_scan_btn_lbl, UI_FONT_BODY_SMALL, 0);
    lv_obj_center(s_scan_btn_lbl);

    s_status_label = lv_label_create(s_top_row);
    lv_label_set_text(s_status_label, ui_str(STR_TAP_SCAN));
    lv_obj_set_style_text_color(s_status_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(s_status_label, UI_FONT_CAPTION, 0);
    lv_obj_align(s_status_label, LV_ALIGN_RIGHT_MID, -4, 0);

    /* --- Scrollable network list --- */
    s_list = lv_obj_create(s_body);
    lv_obj_set_size(s_list, 460, 280);
    lv_obj_align(s_list, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(s_list, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_list, UI_CARD_RADIUS, 0);
    lv_obj_set_style_border_color(s_list, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(s_list, 1, 0);
    lv_obj_set_style_shadow_width(s_list, 4, 0);
    lv_obj_set_style_shadow_color(s_list, lv_color_hex(0xD0D0D0), 0);
    lv_obj_set_style_shadow_ofs_y(s_list, 2, 0);
    lv_obj_set_style_pad_all(s_list, 4, 0);
    lv_obj_set_style_pad_row(s_list, 0, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_AUTO);

    /* Placeholder message in list */
    lv_obj_t *placeholder = lv_label_create(s_list);
    lv_label_set_text(placeholder, ui_str(STR_NO_NETWORKS_SCANNED));
    lv_obj_set_style_text_color(placeholder, UI_COLOR_TEXT_DISABLED, 0);
    lv_obj_set_style_text_font(placeholder, UI_FONT_BODY_SMALL, 0);
    lv_obj_set_style_text_align(placeholder, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(placeholder, lv_pct(100));
    lv_obj_set_style_pad_top(placeholder, 40, 0);

    /* --- Bottom row: "Enter SSID manually" button + Skip/Back --- */
    s_bot_row = lv_obj_create(s_body);
    lv_obj_set_size(s_bot_row, 460, 50);
    lv_obj_align(s_bot_row, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_opa(s_bot_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_bot_row, 0, 0);
    lv_obj_set_style_pad_all(s_bot_row, 0, 0);
    lv_obj_clear_flag(s_bot_row, LV_OBJ_FLAG_SCROLLABLE);

    /* "Enter SSID manually" — outlined button style for proper touch area */
    s_manual_btn = lv_button_create(s_bot_row);
    lv_obj_set_size(s_manual_btn, 200, 40);
    lv_obj_align(s_manual_btn, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_opa(s_manual_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(s_manual_btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(s_manual_btn, 2, 0);
    lv_obj_set_style_radius(s_manual_btn, UI_BTN_RADIUS, 0);
    lv_obj_set_style_bg_color(s_manual_btn, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(s_manual_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_add_event_cb(s_manual_btn, on_manual_btn, LV_EVENT_CLICKED, NULL);
    lv_obj_t *man_lbl = lv_label_create(s_manual_btn);
    lv_label_set_text(man_lbl, ui_str(STR_ENTER_SSID_MANUALLY));
    lv_obj_set_style_text_color(man_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(man_lbl, UI_FONT_BODY_SMALL, 0);
    lv_obj_center(man_lbl);

    /* Skip (initial setup) or Back (runtime reconfiguration) */
    s_skip_btn = lv_button_create(s_bot_row);
    lv_obj_set_size(s_skip_btn, 110, 40);
    lv_obj_align(s_skip_btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(s_skip_btn, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_radius(s_skip_btn, UI_BTN_RADIUS, 0);
    lv_obj_add_event_cb(s_skip_btn, on_skip_btn, LV_EVENT_CLICKED, NULL);
    lv_obj_t *skip_lbl = lv_label_create(s_skip_btn);
    lv_label_set_text_fmt(skip_lbl, s_is_initial_setup ? "%s" : LV_SYMBOL_LEFT " %s",
                           ui_str(s_is_initial_setup ? STR_SKIP : STR_BACK));
    lv_obj_set_style_text_color(skip_lbl, UI_COLOR_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(skip_lbl, UI_FONT_BODY_SMALL, 0);
    lv_obj_center(skip_lbl);

    bsp_display_unlock();

    /* Show connection status if connected, then always scan.
     * The scan temporarily disconnects STA, but auto-reconnect resumes
     * after the scan completes.  Connection is only changed when the
     * user explicitly presses Connect with new credentials. */
    if (!s_is_initial_setup && TS_WiFi_IsConnected()) {
        char ssid_buf[33] = {0};
        if (TS_WiFi_GetConnectedSsid(ssid_buf, sizeof(ssid_buf)) == ESP_OK && ssid_buf[0]) {
            char status_buf[80];
            snprintf(status_buf, sizeof(status_buf), LV_SYMBOL_WIFI "  Connected to %s", ssid_buf);
            bsp_display_lock(0);
            if (s_status_label) lv_label_set_text(s_status_label, status_buf);
            bsp_display_unlock();
        }
    }
    do_scan();
}

/* ------------------------------------------------------------------ */
/* do_scan(): trigger WiFi scan & rebuild list                         */
/* ------------------------------------------------------------------ */
static void
do_scan(void)
{
    s_scan_in_progress = true;

    bsp_display_lock(0);

    /* Update button to scanning state */
    if (s_scan_btn_lbl) lv_label_set_text_fmt(s_scan_btn_lbl, LV_SYMBOL_REFRESH " %s", ui_str(STR_SCANNING));
    if (s_scan_btn) lv_obj_add_state(s_scan_btn, LV_STATE_DISABLED);
    if (s_status_label) lv_label_set_text(s_status_label, "");

    /* Clear list immediately so stale results don't persist */
    if (s_list) {
        lv_obj_clean(s_list);
        lv_obj_t *scanning = lv_label_create(s_list);
        lv_label_set_text_fmt(scanning, LV_SYMBOL_REFRESH "  %s", ui_str(STR_SCANNING_NETWORKS));
        lv_obj_set_style_text_color(scanning, UI_COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(scanning, UI_FONT_BODY_SMALL, 0);
        lv_obj_set_style_text_align(scanning, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(scanning, lv_pct(100));
        lv_obj_set_style_pad_top(scanning, 40, 0);
    }

    bsp_display_unlock();

    /* Start async scan in background task — UI stays responsive */
    s_ap_count = 0;
    esp_err_t err = TS_WiFi_ScanAsync();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start async scan: %s", esp_err_to_name(err));

        bsp_display_lock(0);
        if (s_scan_btn_lbl) lv_label_set_text_fmt(s_scan_btn_lbl, LV_SYMBOL_REFRESH " %s", ui_str(STR_SCAN));
        if (s_scan_btn) lv_obj_clear_state(s_scan_btn, LV_STATE_DISABLED);
        if (s_status_label) lv_label_set_text(s_status_label, ui_str(STR_SCAN_FAILED));
        if (s_list) {
            lv_obj_clean(s_list);
            lv_obj_t *errlab = lv_label_create(s_list);
            lv_label_set_text(errlab, ui_str(STR_SCAN_FAILED_RETRY));
            lv_obj_set_style_text_color(errlab, UI_COLOR_DANGER, 0);
            lv_obj_set_style_text_font(errlab, UI_FONT_BODY_SMALL, 0);
            lv_obj_set_style_text_align(errlab, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_width(errlab, lv_pct(100));
            lv_obj_set_style_pad_top(errlab, 40, 0);
        }
        s_scan_in_progress = false;
        bsp_display_unlock();
        return;
    }

    /* Start LVGL timer to poll for scan completion */
    bsp_display_lock(0);
    if (s_scan_poll_timer) {
        lv_timer_delete(s_scan_poll_timer);
    }
    s_scan_poll_timer = lv_timer_create(on_scan_poll_timer, SCAN_POLL_INTERVAL_MS, NULL);
    bsp_display_unlock();
}

/* ------------------------------------------------------------------ */
/* on_scan_poll_timer(): checks async scan completion                   */
/* ------------------------------------------------------------------ */
static void
on_scan_poll_timer(lv_timer_t *timer)
{
    (void)timer;

    if (!TS_WiFi_IsScanComplete()) return;  /* Still scanning */

    /* Scan done — clean up timer */
    if (s_scan_poll_timer) {
        lv_timer_delete(s_scan_poll_timer);
        s_scan_poll_timer = NULL;
    }

    /* Retrieve results */
    s_ap_count = 0;
    esp_err_t err = TS_WiFi_ScanGetResults(s_ap_list, &s_ap_count);

    /* Restore button label */
    if (s_scan_btn_lbl) lv_label_set_text_fmt(s_scan_btn_lbl, LV_SYMBOL_REFRESH " %s", ui_str(STR_SCAN));
    if (s_scan_btn) lv_obj_clear_state(s_scan_btn, LV_STATE_DISABLED);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Scan failed: %s", esp_err_to_name(err));
        if (s_status_label) lv_label_set_text(s_status_label, ui_str(STR_SCAN_FAILED));

        if (s_list) {
            lv_obj_clean(s_list);
            lv_obj_t *errlab = lv_label_create(s_list);
            lv_label_set_text(errlab, ui_str(STR_SCAN_FAILED_RETRY));
            lv_obj_set_style_text_color(errlab, UI_COLOR_DANGER, 0);
            lv_obj_set_style_text_font(errlab, UI_FONT_BODY_SMALL, 0);
            lv_obj_set_style_text_align(errlab, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_width(errlab, lv_pct(100));
            lv_obj_set_style_pad_top(errlab, 40, 0);
        }

        s_scan_in_progress = false;
        return;
    }

    /* Update status text */
    if (s_status_label) {
        char buf[40];
        snprintf(buf, sizeof(buf), "%u network%s found",
                 s_ap_count, s_ap_count == 1 ? "" : "s");
        lv_label_set_text(s_status_label, buf);
    }

    s_scan_in_progress = false;

    populate_list();
}

/* ------------------------------------------------------------------ */
/* populate_list(): fill the scrollable list with wifi_list_items       */
/* ------------------------------------------------------------------ */
static void
populate_list(void)
{
    bsp_display_lock(0);

    if (!s_list) { bsp_display_unlock(); return; }

    /* Clear existing children */
    lv_obj_clean(s_list);

    if (s_ap_count == 0) {
        lv_obj_t *empty = lv_label_create(s_list);
        lv_label_set_text(empty, ui_str(STR_NO_NETWORKS_FOUND));
        lv_obj_set_style_text_color(empty, UI_COLOR_TEXT_DISABLED, 0);
        lv_obj_set_style_text_font(empty, UI_FONT_BODY_SMALL, 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(empty, lv_pct(100));
        lv_obj_set_style_pad_top(empty, 40, 0);
    } else {
        for (uint16_t i = 0; i < s_ap_count; i++) {
            /* Skip hidden/empty SSIDs */
            if (s_ap_list[i].acSsid[0] == '\0') continue;
            wifi_list_item_create(s_list, &s_ap_list[i], on_network_selected);
        }
    }

    bsp_display_unlock();
}

/* ------------------------------------------------------------------ */
/* VIEW_PASSWORD: SSID display + password input + connect/back          */
/* ------------------------------------------------------------------ */
static void
view_show_password(const char *ssid, bool secured)
{
    bsp_display_lock(0);

    s_view = VIEW_PASSWORD;

    /* Hide list view widgets */
    if (s_top_row) lv_obj_add_flag(s_top_row, LV_OBJ_FLAG_HIDDEN);
    if (s_list)    lv_obj_add_flag(s_list, LV_OBJ_FLAG_HIDDEN);
    if (s_bot_row) lv_obj_add_flag(s_bot_row, LV_OBJ_FLAG_HIDDEN);

    /* Container for password entry — starts at full height, shrinks when keyboard shows */
    s_pw_container = lv_obj_create(s_body);
    lv_obj_set_size(s_pw_container, WIFI_FORM_W, WIFI_PW_FORM_H);
    lv_obj_align(s_pw_container, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_color(s_pw_container, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(s_pw_container, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_pw_container, UI_CARD_RADIUS, 0);
    lv_obj_set_style_border_color(s_pw_container, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(s_pw_container, 1, 0);
    lv_obj_set_style_shadow_width(s_pw_container, UI_CARD_SHADOW_WIDTH, 0);
    lv_obj_set_style_shadow_color(s_pw_container, lv_color_hex(0xD0D0D0), 0);
    lv_obj_set_style_shadow_ofs_y(s_pw_container, UI_CARD_SHADOW_OFS_Y, 0);
    lv_obj_set_style_pad_all(s_pw_container, UI_PAD_MEDIUM, 0);
    lv_obj_clear_flag(s_pw_container, LV_OBJ_FLAG_SCROLLABLE);

    /* Network name */
    lv_obj_t *net_lbl = lv_label_create(s_pw_container);
    lv_label_set_text(net_lbl, ui_str(STR_CONNECT_TO));
    lv_obj_set_style_text_color(net_lbl, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(net_lbl, UI_FONT_CAPTION, 0);
    lv_obj_align(net_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    s_pw_ssid_label = lv_label_create(s_pw_container);
    lv_label_set_text(s_pw_ssid_label, ssid);
    lv_obj_set_style_text_color(s_pw_ssid_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(s_pw_ssid_label, UI_FONT_H3, 0);
    lv_label_set_long_mode(s_pw_ssid_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_pw_ssid_label, 400);
    lv_obj_align(s_pw_ssid_label, LV_ALIGN_TOP_LEFT, 0, 18);

    /* Password label */
    lv_obj_t *pw_lbl = lv_label_create(s_pw_container);
    lv_label_set_text(pw_lbl, ui_str(STR_PASSWORD));
    lv_obj_set_style_text_color(pw_lbl, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(pw_lbl, UI_FONT_CAPTION, 0);
    lv_obj_align(pw_lbl, LV_ALIGN_TOP_LEFT, 0, 48);

    /* Password input */
    s_pw_input = input_field_component_create(s_pw_container, 400, 42, ui_str(STR_ENTER_PASSWORD));
    if (s_pw_input) {
        lv_obj_align(s_pw_input, LV_ALIGN_TOP_MID, 0, 66);
        input_field_component_set_password_mode(s_pw_input, true);
        input_field_component_set_max_length(s_pw_input, 63);
        lv_obj_add_event_cb(s_pw_input, on_textarea_focus, LV_EVENT_FOCUSED, NULL);
    }

    /* Show password checkbox */
    s_pw_show_cb = lv_checkbox_create(s_pw_container);
    lv_checkbox_set_text(s_pw_show_cb, ui_str(STR_SHOW_PASSWORD));
    lv_obj_set_style_text_font(s_pw_show_cb, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(s_pw_show_cb, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(s_pw_show_cb, LV_ALIGN_TOP_LEFT, 0, 116);
    lv_obj_add_event_cb(s_pw_show_cb, on_pw_show_toggle, LV_EVENT_VALUE_CHANGED, NULL);

    /* Buttons: Back + Connect */
    s_pw_back = lv_button_create(s_pw_container);
    lv_obj_set_size(s_pw_back, 120, 42);
    lv_obj_align(s_pw_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_pw_back, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_radius(s_pw_back, UI_BTN_RADIUS, 0);
    lv_obj_add_event_cb(s_pw_back, on_pw_back, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(s_pw_back);
    lv_label_set_text_fmt(back_lbl, LV_SYMBOL_LEFT " %s", ui_str(STR_BACK));
    lv_obj_set_style_text_color(back_lbl, UI_COLOR_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(back_lbl, UI_FONT_BODY_SMALL, 0);
    lv_obj_center(back_lbl);

    s_pw_connect = lv_button_create(s_pw_container);
    lv_obj_set_size(s_pw_connect, 150, 42);
    lv_obj_align(s_pw_connect, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(s_pw_connect, UI_COLOR_SUCCESS, 0);
    lv_obj_set_style_radius(s_pw_connect, UI_BTN_RADIUS, 0);
    lv_obj_add_event_cb(s_pw_connect, on_pw_connect, LV_EVENT_CLICKED, NULL);
    lv_obj_t *conn_lbl = lv_label_create(s_pw_connect);
    lv_label_set_text_fmt(conn_lbl, "%s " LV_SYMBOL_RIGHT, ui_str(STR_CONNECT));
    lv_obj_set_style_text_color(conn_lbl, UI_COLOR_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(conn_lbl, UI_FONT_BODY_SMALL, 0);
    lv_obj_center(conn_lbl);

    /* Show keyboard immediately for password entry */
    create_keyboard();
    s_keyboard_visible = false;  /* Force show_keyboard_for to run the full path */
    bsp_display_unlock();
    show_keyboard_for(s_pw_input);
}

/* ------------------------------------------------------------------ */
/* VIEW_MANUAL: SSID + password inputs                                 */
/* ------------------------------------------------------------------ */
static void
view_show_manual(void)
{
    bsp_display_lock(0);

    s_view = VIEW_MANUAL;

    /* Hide list view widgets */
    if (s_top_row) lv_obj_add_flag(s_top_row, LV_OBJ_FLAG_HIDDEN);
    if (s_list)    lv_obj_add_flag(s_list, LV_OBJ_FLAG_HIDDEN);
    if (s_bot_row) lv_obj_add_flag(s_bot_row, LV_OBJ_FLAG_HIDDEN);

    /* Container — uses flex column so content reflows when keyboard resizes it */
    s_man_container = lv_obj_create(s_body);
    lv_obj_set_size(s_man_container, WIFI_FORM_W, WIFI_MAN_FORM_H);
    lv_obj_align(s_man_container, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_color(s_man_container, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(s_man_container, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_man_container, UI_CARD_RADIUS, 0);
    lv_obj_set_style_border_color(s_man_container, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(s_man_container, 1, 0);
    lv_obj_set_style_shadow_width(s_man_container, UI_CARD_SHADOW_WIDTH, 0);
    lv_obj_set_style_shadow_color(s_man_container, lv_color_hex(0xD0D0D0), 0);
    lv_obj_set_style_shadow_ofs_y(s_man_container, UI_CARD_SHADOW_OFS_Y, 0);
    lv_obj_set_style_pad_all(s_man_container, 10, 0);
    lv_obj_set_style_pad_row(s_man_container, 2, 0);
    lv_obj_set_flex_flow(s_man_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s_man_container, LV_OBJ_FLAG_SCROLLABLE);

    /* SSID label */
    lv_obj_t *ssid_lbl = lv_label_create(s_man_container);
    lv_label_set_text(ssid_lbl, ui_str(STR_NETWORK_NAME_SSID));
    lv_obj_set_style_text_color(ssid_lbl, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(ssid_lbl, UI_FONT_CAPTION, 0);

    /* SSID input */
    s_man_ssid_inp = input_field_component_create(s_man_container, 400, 38, ui_str(STR_ENTER_SSID));
    if (s_man_ssid_inp) {
        lv_obj_set_width(s_man_ssid_inp, lv_pct(100));
        input_field_component_set_max_length(s_man_ssid_inp, 32);
        lv_obj_add_event_cb(s_man_ssid_inp, on_textarea_focus, LV_EVENT_FOCUSED, NULL);
    }

    /* Password label */
    lv_obj_t *pw_lbl = lv_label_create(s_man_container);
    lv_label_set_text(pw_lbl, ui_str(STR_PASSWORD));
    lv_obj_set_style_text_color(pw_lbl, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(pw_lbl, UI_FONT_CAPTION, 0);
    lv_obj_set_style_pad_top(pw_lbl, 4, 0);

    /* Password input */
    s_man_pw_inp = input_field_component_create(s_man_container, 400, 38, ui_str(STR_PASSWORD_OR_EMPTY));
    if (s_man_pw_inp) {
        lv_obj_set_width(s_man_pw_inp, lv_pct(100));
        input_field_component_set_password_mode(s_man_pw_inp, true);
        input_field_component_set_max_length(s_man_pw_inp, 63);
        lv_obj_add_event_cb(s_man_pw_inp, on_textarea_focus, LV_EVENT_FOCUSED, NULL);
    }

    /* Show password checkbox */
    s_man_show_cb = lv_checkbox_create(s_man_container);
    lv_checkbox_set_text(s_man_show_cb, ui_str(STR_SHOW_PASSWORD));
    lv_obj_set_style_text_font(s_man_show_cb, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(s_man_show_cb, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_add_event_cb(s_man_show_cb, on_man_show_toggle, LV_EVENT_VALUE_CHANGED, NULL);

    /* Spacer to push buttons to bottom */
    lv_obj_t *spacer = lv_obj_create(s_man_container);
    lv_obj_set_width(spacer, lv_pct(100));
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_style_pad_all(spacer, 0, 0);
    lv_obj_set_style_min_height(spacer, 2, 0);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

    /* Button row: Back + Connect */
    lv_obj_t *btn_row = lv_obj_create(s_man_container);
    lv_obj_set_size(btn_row, lv_pct(100), 42);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    s_man_back = lv_button_create(btn_row);
    lv_obj_set_size(s_man_back, 120, 42);
    lv_obj_align(s_man_back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(s_man_back, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_radius(s_man_back, UI_BTN_RADIUS, 0);
    lv_obj_add_event_cb(s_man_back, on_man_back, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(s_man_back);
    lv_label_set_text_fmt(back_lbl, LV_SYMBOL_LEFT " %s", ui_str(STR_BACK));
    lv_obj_set_style_text_color(back_lbl, UI_COLOR_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(back_lbl, UI_FONT_BODY_SMALL, 0);
    lv_obj_center(back_lbl);

    s_man_connect = lv_button_create(btn_row);
    lv_obj_set_size(s_man_connect, 150, 42);
    lv_obj_align(s_man_connect, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(s_man_connect, UI_COLOR_SUCCESS, 0);
    lv_obj_set_style_radius(s_man_connect, UI_BTN_RADIUS, 0);
    lv_obj_add_event_cb(s_man_connect, on_man_connect, LV_EVENT_CLICKED, NULL);
    lv_obj_t *conn_lbl = lv_label_create(s_man_connect);
    lv_label_set_text_fmt(conn_lbl, "%s " LV_SYMBOL_RIGHT, ui_str(STR_CONNECT));
    lv_obj_set_style_text_color(conn_lbl, UI_COLOR_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(conn_lbl, UI_FONT_BODY_SMALL, 0);
    lv_obj_center(conn_lbl);

    /* Show keyboard, start with SSID focused */
    create_keyboard();
    s_keyboard_visible = false;
    bsp_display_unlock();
    show_keyboard_for(s_man_ssid_inp);
}

/* ------------------------------------------------------------------ */
/* VIEW_CONNECTING: spinner + status while WiFi connects               */
/* ------------------------------------------------------------------ */
static void
view_show_connecting(const char *ssid)
{
    bsp_display_lock(0);

    s_view = VIEW_CONNECTING;

    /* Hide any previous views */
    if (s_top_row)   lv_obj_add_flag(s_top_row, LV_OBJ_FLAG_HIDDEN);
    if (s_list)      lv_obj_add_flag(s_list, LV_OBJ_FLAG_HIDDEN);
    if (s_bot_row)   lv_obj_add_flag(s_bot_row, LV_OBJ_FLAG_HIDDEN);
    if (s_pw_container)  { lv_obj_delete(s_pw_container);  s_pw_container = NULL; }
    if (s_man_container) { lv_obj_delete(s_man_container); s_man_container = NULL; }
    s_pw_input = NULL; s_pw_ssid_label = NULL; s_pw_show_cb = NULL;
    s_pw_connect = NULL; s_pw_back = NULL;
    s_man_ssid_inp = NULL; s_man_pw_inp = NULL; s_man_show_cb = NULL;
    s_man_connect = NULL; s_man_back = NULL;

    if (s_keyboard) {
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
        s_keyboard_visible = false;
    }

    /* Connecting container — centered card (content only, no button) */
    s_conn_container = lv_obj_create(s_body);
    lv_obj_set_size(s_conn_container, WIFI_FORM_W, 160);
    lv_obj_align(s_conn_container, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(s_conn_container, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(s_conn_container, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_conn_container, UI_CARD_RADIUS, 0);
    lv_obj_set_style_border_color(s_conn_container, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(s_conn_container, 1, 0);
    lv_obj_set_style_shadow_width(s_conn_container, UI_CARD_SHADOW_WIDTH, 0);
    lv_obj_set_style_shadow_color(s_conn_container, lv_color_hex(0xD0D0D0), 0);
    lv_obj_set_style_shadow_ofs_y(s_conn_container, UI_CARD_SHADOW_OFS_Y, 0);
    lv_obj_set_style_pad_all(s_conn_container, UI_PAD_LARGE, 0);
    lv_obj_clear_flag(s_conn_container, LV_OBJ_FLAG_SCROLLABLE);

    /* Spinner */
    s_conn_spinner = lv_spinner_create(s_conn_container);
    lv_obj_set_size(s_conn_spinner, 48, 48);
    lv_obj_align(s_conn_spinner, LV_ALIGN_TOP_MID, 0, 0);
    lv_spinner_set_anim_params(s_conn_spinner, 1000, 270);

    /* "Connecting to <SSID>..." label */
    s_conn_label = lv_label_create(s_conn_container);
    char conn_text[80];
    snprintf(conn_text, sizeof(conn_text), ui_str(STR_CONNECTING_TO), ssid);
    lv_label_set_text(s_conn_label, conn_text);
    lv_obj_set_style_text_color(s_conn_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(s_conn_label, UI_FONT_BODY, 0);
    lv_obj_set_style_text_align(s_conn_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_conn_label, 380);
    lv_obj_align(s_conn_label, LV_ALIGN_TOP_MID, 0, 56);

    /* Status sub-label */
    s_conn_status = lv_label_create(s_conn_container);
    lv_label_set_text(s_conn_status, ui_str(STR_PLEASE_WAIT));
    lv_obj_set_style_text_color(s_conn_status, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(s_conn_status, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_align(s_conn_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_conn_status, 380);
    lv_obj_align(s_conn_status, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* Cancel button — placed below the card on the body */
    s_conn_back = lv_button_create(s_body);
    lv_obj_set_size(s_conn_back, 140, 42);
    lv_obj_align_to(s_conn_back, s_conn_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);
    lv_obj_set_style_bg_color(s_conn_back, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_radius(s_conn_back, UI_BTN_RADIUS, 0);
    lv_obj_add_event_cb(s_conn_back, on_conn_back, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bck_lbl = lv_label_create(s_conn_back);
    lv_label_set_text_fmt(bck_lbl, LV_SYMBOL_LEFT " %s", ui_str(STR_CANCEL));
    lv_obj_set_style_text_color(bck_lbl, UI_COLOR_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(bck_lbl, UI_FONT_BODY_SMALL, 0);
    lv_obj_center(bck_lbl);

    bsp_display_unlock();

    /* Initiate actual WiFi connection */
    ESP_LOGI(TAG, "Initiating connection to '%s'", ssid);
    TS_WiFi_Connect(ssid, s_selected_pw, s_selected_bssid);

    /* Start periodic timer to check connection status */
    s_conn_elapsed = 0;
    bsp_display_lock(0);
    s_conn_timer = lv_timer_create(on_conn_timer, WIFI_CONNECT_CHECK_MS, NULL);
    bsp_display_unlock();
}

/* ================================================================== */
/* Public API                                                          */
/* ================================================================== */

void
touchscreen_wifi_setup_screen_create(TouchScreen_WiFi_Setup_Callback_t callback, bool is_initial_setup)
{
    ESP_LOGI(TAG, "Creating WiFi setup screen (initial=%d)", is_initial_setup);

    s_callback = callback;
    s_is_initial_setup = is_initial_setup;

    /* Reset any stale scan state from a previous session */
    s_scan_in_progress = false;
    TS_WiFi_ScanAbort();

    bsp_display_lock(0);

    s_screen = g_ui_state.screen_obj;
    if (!s_screen) {
        ESP_LOGE(TAG, "Failed to get screen object");
        bsp_display_unlock();
        return;
    }

    lv_obj_clean(s_screen);
    lv_obj_set_style_bg_color(s_screen, UI_COLOR_BACKGROUND, 0);

    char header_buf[64];
    snprintf(header_buf, sizeof(header_buf), LV_SYMBOL_WIFI " %s", ui_str(STR_WIFI_SETUP));
    create_header(s_screen, header_buf);
    create_body(s_screen);

    /* Register body click handler — dismiss keyboard on tap outside */
    lv_obj_add_event_cb(s_body, on_body_click, LV_EVENT_CLICKED, NULL);

    bsp_display_unlock();

    /* Show the list view (also triggers auto-scan) */
    view_show_list();

    ESP_LOGI(TAG, "WiFi setup screen created");
}

void
touchscreen_wifi_setup_screen_destroy(void)
{
    ESP_LOGI(TAG, "Destroying WiFi setup screen");

    /* Stop connecting timer if active */
    if (s_conn_timer) {
        bsp_display_lock(0);
        lv_timer_delete(s_conn_timer);
        bsp_display_unlock();
        s_conn_timer = NULL;
    }

    /* Stop scan poll timer if active */
    if (s_scan_poll_timer) {
        bsp_display_lock(0);
        lv_timer_delete(s_scan_poll_timer);
        bsp_display_unlock();
        s_scan_poll_timer = NULL;
    }

    /* Abort any in-progress async scan */
    TS_WiFi_ScanAbort();

    /* NULL all static pointers — the screen's lv_obj_clean() will free LVGL objects */
    s_screen        = NULL;
    s_header        = NULL;
    s_body          = NULL;
    s_top_row       = NULL;
    s_scan_btn      = NULL;
    s_scan_btn_lbl  = NULL;
    s_list          = NULL;
    s_status_label  = NULL;
    s_bot_row       = NULL;
    s_manual_btn    = NULL;
    s_skip_btn      = NULL;
    s_pw_container  = NULL;
    s_pw_ssid_label = NULL;
    s_pw_input      = NULL;
    s_pw_show_cb    = NULL;
    s_pw_connect    = NULL;
    s_pw_back       = NULL;
    s_man_container = NULL;
    s_man_ssid_inp  = NULL;
    s_man_pw_inp    = NULL;
    s_man_show_cb   = NULL;
    s_man_connect   = NULL;
    s_man_back      = NULL;
    s_conn_container = NULL;
    s_conn_spinner   = NULL;
    s_conn_label     = NULL;
    s_conn_status    = NULL;
    s_conn_back      = NULL;
    s_keyboard      = NULL;
    s_callback      = NULL;
    s_view          = VIEW_LIST;
    s_keyboard_visible = false;
    s_scan_in_progress = false;
    s_ap_count      = 0;
    memset(s_selected_ssid, 0, sizeof(s_selected_ssid));
    memset(s_selected_pw, 0, sizeof(s_selected_pw));
}

/* ------------------------------------------------------------------ */
/* Periodic update — auto-navigate to dashboard on reconnect           */
/* ------------------------------------------------------------------ */
void
touchscreen_wifi_setup_screen_update(void)
{
    /* If the background retry mechanism reconnected while the user
       is on this screen during initial (boot-time) setup, navigate
       straight to the dashboard.  In runtime reconfiguration mode the
       user deliberately entered via PIN, so let them stay until they
       press Back or Connect.                                         */
    if (s_is_initial_setup && s_view != VIEW_CONNECTING && TS_WiFi_IsConnected()) {
        ESP_LOGI(TAG, "WiFi reconnected — leaving WiFi setup for dashboard");
        TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_DASHBOARD);
    }
}
