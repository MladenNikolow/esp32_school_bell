/* ================================================================== */
/* dashboard_screen.c — Dashboard: status cards, test bell, panic FAB  */
/* ================================================================== */
#include "dashboard_screen_internal.h"
#include "../../src/ui_manager_internal.h"
#include "../../src/ui_theme.h"
#include "../../src/ui_strings.h"
#include "../../components/card/card_component.h"
#include "TouchScreen_Services.h"
#include "TouchScreen_UI_Manager.h"
#include "Schedule_Data.h"
#include "esp_log.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <stdint.h>

static const char *TAG = "DASHBOARD";

/* ------------------------------------------------------------------ */
/* Layout constants                                                    */
/* ------------------------------------------------------------------ */
#define CONTENT_WIDTH       (480 - 2 * UI_PAD_SCREEN)          /* 456px */
#define TEST_BELL_SEC       3

/* ------------------------------------------------------------------ */
/* Module state — all LVGL objects we need to update                   */
/* ------------------------------------------------------------------ */
static lv_obj_t *s_content       = NULL;   /* Scrollable content container */

/* Bell status card */
static lv_obj_t *s_bell_dot      = NULL;
static lv_obj_t *s_bell_state_lbl = NULL;
static lv_obj_t *s_next_bell_lbl = NULL;
static lv_obj_t *s_next_bell_time = NULL;
static lv_obj_t *s_next_bell_info = NULL;

/* Info cards */
static lv_obj_t *s_day_date_lbl  = NULL;
static lv_obj_t *s_day_type_lbl  = NULL;

/* Error/warning banner */
static lv_obj_t *s_warning_banner = NULL;
static lv_obj_t *s_warning_icon   = NULL;
static lv_obj_t *s_warning_label  = NULL;

/* Test bell button */
static lv_obj_t *s_test_btn      = NULL;

/* Panic button (inline) */
static lv_obj_t *s_panic_btn     = NULL;
static lv_obj_t *s_panic_label   = NULL;
static lv_anim_t s_panic_anim;
static bool      s_panic_anim_running = false;

/* Day override buttons */
static lv_obj_t *s_day_off_btn   = NULL;
static lv_obj_t *s_day_on_btn    = NULL;
static lv_obj_t *s_day_off_label = NULL;
static lv_obj_t *s_day_on_label  = NULL;

/* ------------------------------------------------------------------ */
/* Forward declarations                                                */
/* ------------------------------------------------------------------ */
static void dashboard_create_bell_status_card(lv_obj_t *parent);
static void dashboard_create_info_cards(lv_obj_t *parent);
static void dashboard_create_action_buttons(lv_obj_t *parent);
static void dashboard_create_day_override_buttons(lv_obj_t *parent);
static void dashboard_create_warning_banner(lv_obj_t *parent);
static void dashboard_refresh_all(void);
static void dashboard_refresh_bell_status(void);
static void dashboard_refresh_info_cards(void);
static void dashboard_refresh_panic_fab(void);
static void dashboard_refresh_warning_banner(void);
static void dashboard_refresh_day_override_buttons(void);
static void dashboard_start_panic_pulse(void);
static void dashboard_stop_panic_pulse(void);

/* PIN callback targets */
static void pin_result_test_bell(bool success);
static void pin_result_panic_toggle(bool success);
static void pin_result_day_off(bool success);
static void pin_result_day_on(bool success);

/* Event handlers */
static void test_bell_event_cb(lv_event_t *e);
static void panic_fab_event_cb(lv_event_t *e);
static void day_off_event_cb(lv_event_t *e);
static void day_on_event_cb(lv_event_t *e);

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static const char *day_type_str(int eDayType)
{
    switch (eDayType) {
        case 0: return ui_str(STR_DAY_OFF);
        case 1: return ui_str(STR_WORKING_DAY);
        case 2: return ui_str(STR_HOLIDAY);
        case 3: return ui_str(STR_EXCEPTION_WORKING);
        case 4: return ui_str(STR_EXCEPTION_OFF);
        default: return ui_str(STR_UNKNOWN);
    }
}

static lv_color_t bell_state_color(BELL_STATE_E state)
{
    switch (state) {
        case BELL_STATE_RINGING: return UI_COLOR_WARNING;
        case BELL_STATE_PANIC:   return UI_COLOR_DANGER;
        default:                 return UI_COLOR_SUCCESS;
    }
}

/* Lighter border ring color for the status dot */
static lv_color_t bell_state_border_color(BELL_STATE_E state)
{
    switch (state) {
        case BELL_STATE_RINGING: return lv_color_hex(0xFFCC80);  /* Orange 200 */
        case BELL_STATE_PANIC:   return lv_color_hex(0xEF9A9A);  /* Red 200 */
        default:                 return lv_color_hex(0xA5D6A7);  /* Green 200 */
    }
}

static const char *bell_state_str(BELL_STATE_E state)
{
    switch (state) {
        case BELL_STATE_RINGING: return ui_str(STR_BELL_RINGING);
        case BELL_STATE_PANIC:   return ui_str(STR_BELL_PANIC);
        default:                 return ui_str(STR_BELL_IDLE);
    }
}

/* ------------------------------------------------------------------ */
/* Screen lifecycle                                                    */
/* ------------------------------------------------------------------ */
void touchscreen_dashboard_screen_create(void)
{
    ESP_LOGI(TAG, "Creating dashboard screen");

    bsp_display_lock(0);

    lv_obj_t *scr = g_ui_state.screen_obj;

    /* Scrollable content container between statusbar and navbar */
    s_content = lv_obj_create(scr);
    lv_obj_set_size(s_content, 480, UI_CONTENT_HEIGHT);
    lv_obj_set_pos(s_content, 0, UI_STATUSBAR_HEIGHT);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, UI_PAD_SMALL, 0);
    lv_obj_set_style_pad_row(s_content, 4, 0);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(s_content, LV_SCROLLBAR_MODE_AUTO);

    /* Warning banner (hidden by default, shown when WiFi/NTP issues) */
    dashboard_create_warning_banner(s_content);

    /* Build sections */
    dashboard_create_bell_status_card(s_content);
    dashboard_create_info_cards(s_content);
    dashboard_create_action_buttons(s_content);
    dashboard_create_day_override_buttons(s_content);

    bsp_display_unlock();

    /* Initial data fill */
    dashboard_refresh_all();

    ESP_LOGI(TAG, "Dashboard screen created");
}

void touchscreen_dashboard_screen_destroy(void)
{
    ESP_LOGI(TAG, "Destroying dashboard screen");

    dashboard_stop_panic_pulse();

    s_content = NULL;
    s_bell_dot = NULL;
    s_bell_state_lbl = NULL;
    s_next_bell_lbl = NULL;
    s_next_bell_time = NULL;
    s_next_bell_info = NULL;
    s_day_date_lbl = NULL;
    s_day_type_lbl = NULL;
    s_warning_banner = NULL;
    s_warning_icon = NULL;
    s_warning_label = NULL;
    s_test_btn = NULL;
    s_panic_btn = NULL;
    s_panic_label = NULL;
    s_day_off_btn = NULL;
    s_day_on_btn = NULL;
    s_day_off_label = NULL;
    s_day_on_label = NULL;
}

void touchscreen_dashboard_screen_update(void)
{
    if (!s_content) return;
    dashboard_refresh_all();
}

/* ------------------------------------------------------------------ */
/* Bell Status Card                                                    */
/* ------------------------------------------------------------------ */
static void dashboard_create_bell_status_card(lv_obj_t *parent)
{
    lv_obj_t *card = card_component_create(parent, CONTENT_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 4, 0);

    /* Row: dot + state text */
    lv_obj_t *row = lv_obj_create(card);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, UI_PAD_MEDIUM, 0);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

    /* State dot — 24px circle with subtle lighter border ring */
    s_bell_dot = lv_obj_create(row);
    lv_obj_set_size(s_bell_dot, 24, 24);
    lv_obj_set_style_radius(s_bell_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_bell_dot, UI_COLOR_SUCCESS, 0);
    lv_obj_set_style_bg_opa(s_bell_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_bell_dot, 2, 0);
    lv_obj_set_style_border_color(s_bell_dot, lv_color_hex(0xA5D6A7), 0);
    lv_obj_set_style_border_opa(s_bell_dot, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(s_bell_dot, LV_SCROLLBAR_MODE_OFF);

    /* "Bell Status" title + state */
    lv_obj_t *col = lv_obj_create(row);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(col, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title = lv_label_create(col);
    lv_label_set_text(title, ui_str(STR_BELL_STATUS));
    lv_obj_set_style_text_font(title, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT_SECONDARY, 0);

    s_bell_state_lbl = lv_label_create(col);
    lv_label_set_text(s_bell_state_lbl, ui_str(STR_BELL_IDLE));
    lv_obj_set_style_text_font(s_bell_state_lbl, UI_FONT_H3, 0);
    lv_obj_set_style_text_color(s_bell_state_lbl, UI_COLOR_TEXT_PRIMARY, 0);

    /* Divider */
    lv_obj_t *divider = lv_obj_create(card);
    lv_obj_set_size(divider, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(divider, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_pad_all(divider, 0, 0);
    lv_obj_set_scrollbar_mode(divider, LV_SCROLLBAR_MODE_OFF);

    /* Next bell section */
    s_next_bell_lbl = lv_label_create(card);
    lv_label_set_text(s_next_bell_lbl, ui_str(STR_NEXT_BELL));
    lv_obj_set_style_text_font(s_next_bell_lbl, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(s_next_bell_lbl, UI_COLOR_TEXT_SECONDARY, 0);

    /* Large prominent time display */
    s_next_bell_time = lv_label_create(card);
    lv_label_set_text(s_next_bell_time, "--:--");
    lv_obj_set_style_text_font(s_next_bell_time, UI_FONT_H1, 0);
    lv_obj_set_style_text_color(s_next_bell_time, UI_COLOR_PRIMARY, 0);

    /* Bell label + duration detail line */
    s_next_bell_info = lv_label_create(card);
    lv_label_set_text(s_next_bell_info, "");
    lv_obj_set_style_text_font(s_next_bell_info, UI_FONT_BODY_SMALL, 0);
    lv_obj_set_style_text_color(s_next_bell_info, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_width(s_next_bell_info, LV_PCT(100));
    lv_label_set_long_mode(s_next_bell_info, LV_LABEL_LONG_DOT);
}

/* ------------------------------------------------------------------ */
/* Info Card (full-width: Date + Day Type combined)                     */
/* ------------------------------------------------------------------ */
static void dashboard_create_info_cards(lv_obj_t *parent)
{
    lv_obj_t *card = card_component_create(parent, CONTENT_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 4, 0);

    /* Date row: icon + text column */
    lv_obj_t *date_row = lv_obj_create(card);
    lv_obj_set_size(date_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(date_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(date_row, 0, 0);
    lv_obj_set_style_pad_all(date_row, 0, 0);
    lv_obj_set_flex_flow(date_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(date_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(date_row, UI_PAD_SMALL, 0);
    lv_obj_set_scrollbar_mode(date_row, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *date_icon = lv_label_create(date_row);
    lv_label_set_text(date_icon, LV_SYMBOL_LOOP);
    lv_obj_set_style_text_font(date_icon, UI_FONT_H3, 0);
    lv_obj_set_style_text_color(date_icon, UI_COLOR_PRIMARY, 0);

    lv_obj_t *date_col = lv_obj_create(date_row);
    lv_obj_set_size(date_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(date_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(date_col, 0, 0);
    lv_obj_set_style_pad_all(date_col, 0, 0);
    lv_obj_set_flex_flow(date_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(date_col, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *date_title = lv_label_create(date_col);
    lv_label_set_text(date_title, ui_str(STR_DATE));
    lv_obj_set_style_text_font(date_title, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(date_title, UI_COLOR_TEXT_SECONDARY, 0);

    s_day_date_lbl = lv_label_create(date_col);
    lv_label_set_text(s_day_date_lbl, "-");
    lv_obj_set_style_text_font(s_day_date_lbl, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(s_day_date_lbl, UI_COLOR_TEXT_PRIMARY, 0);

    /* Divider */
    lv_obj_t *divider = lv_obj_create(card);
    lv_obj_set_size(divider, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(divider, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_pad_all(divider, 0, 0);
    lv_obj_set_scrollbar_mode(divider, LV_SCROLLBAR_MODE_OFF);

    /* Day Type row: icon + text column */
    lv_obj_t *type_row = lv_obj_create(card);
    lv_obj_set_size(type_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(type_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(type_row, 0, 0);
    lv_obj_set_style_pad_all(type_row, 0, 0);
    lv_obj_set_flex_flow(type_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(type_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(type_row, UI_PAD_SMALL, 0);
    lv_obj_set_scrollbar_mode(type_row, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *type_icon = lv_label_create(type_row);
    lv_label_set_text(type_icon, LV_SYMBOL_LIST);
    lv_obj_set_style_text_font(type_icon, UI_FONT_H3, 0);
    lv_obj_set_style_text_color(type_icon, UI_COLOR_PRIMARY, 0);

    lv_obj_t *type_col = lv_obj_create(type_row);
    lv_obj_set_size(type_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(type_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(type_col, 0, 0);
    lv_obj_set_style_pad_all(type_col, 0, 0);
    lv_obj_set_flex_flow(type_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(type_col, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *type_title = lv_label_create(type_col);
    lv_label_set_text(type_title, ui_str(STR_DAY_TYPE));
    lv_obj_set_style_text_font(type_title, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(type_title, UI_COLOR_TEXT_SECONDARY, 0);

    s_day_type_lbl = lv_label_create(type_col);
    lv_label_set_text(s_day_type_lbl, "-");
    lv_obj_set_style_text_font(s_day_type_lbl, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(s_day_type_lbl, UI_COLOR_TEXT_PRIMARY, 0);
}

/* ------------------------------------------------------------------ */
/* Action Buttons: Test Bell + Panic side by side                      */
/* ------------------------------------------------------------------ */
#define ACTION_BTN_H    42
#define ACTION_GAP      UI_PAD_SMALL
#define ACTION_BTN_W    ((CONTENT_WIDTH - ACTION_GAP) / 2)

static void dashboard_create_action_buttons(lv_obj_t *parent)
{
    /* Row container for the two buttons */
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, CONTENT_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

    /* Test Bell button */
    s_test_btn = lv_button_create(row);
    lv_obj_set_size(s_test_btn, ACTION_BTN_W, ACTION_BTN_H);
    ui_theme_apply_btn_primary(s_test_btn);
    lv_obj_set_flex_flow(s_test_btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_test_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_test_btn, 6, 0);

    lv_obj_t *test_icon = lv_label_create(s_test_btn);
    lv_label_set_text(test_icon, LV_SYMBOL_BELL);
    lv_obj_set_style_text_color(test_icon, UI_COLOR_TEXT_ON_PRIMARY, 0);

    lv_obj_t *test_text = lv_label_create(s_test_btn);
    lv_label_set_text(test_text, ui_str(STR_RING));
    lv_obj_set_style_text_color(test_text, UI_COLOR_TEXT_ON_PRIMARY, 0);

    lv_obj_add_event_cb(s_test_btn, test_bell_event_cb, LV_EVENT_CLICKED, NULL);

    /* Panic button */
    s_panic_btn = lv_button_create(row);
    lv_obj_set_size(s_panic_btn, ACTION_BTN_W, ACTION_BTN_H);
    lv_obj_set_style_bg_color(s_panic_btn, UI_COLOR_DANGER, 0);
    lv_obj_set_style_bg_opa(s_panic_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_panic_btn, UI_BTN_RADIUS, 0);
    lv_obj_set_style_border_width(s_panic_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_panic_btn, 4, 0);
    lv_obj_set_style_shadow_color(s_panic_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(s_panic_btn, LV_OPA_20, 0);
    lv_obj_set_style_shadow_offset_y(s_panic_btn, 2, 0);

    lv_obj_set_flex_flow(s_panic_btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_panic_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_panic_btn, 6, 0);

    lv_obj_t *panic_icon = lv_label_create(s_panic_btn);
    lv_label_set_text(panic_icon, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(panic_icon, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(panic_icon, UI_COLOR_TEXT_ON_PRIMARY, 0);

    s_panic_label = lv_label_create(s_panic_btn);
    lv_label_set_text(s_panic_label, ui_str(STR_PANIC));
    lv_obj_set_style_text_font(s_panic_label, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(s_panic_label, UI_COLOR_TEXT_ON_PRIMARY, 0);

    lv_obj_add_event_cb(s_panic_btn, panic_fab_event_cb, LV_EVENT_CLICKED, NULL);
}

/* ------------------------------------------------------------------ */
/* Day Override Buttons: Day Off + Day On side by side                  */
/* ------------------------------------------------------------------ */
static void dashboard_create_day_override_buttons(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, CONTENT_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

    /* Day Off button (warning/orange) */
    s_day_off_btn = lv_button_create(row);
    lv_obj_set_size(s_day_off_btn, ACTION_BTN_W, ACTION_BTN_H);
    lv_obj_set_style_bg_color(s_day_off_btn, UI_COLOR_WARNING, 0);
    lv_obj_set_style_bg_opa(s_day_off_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_day_off_btn, UI_BTN_RADIUS, 0);
    lv_obj_set_style_border_width(s_day_off_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_day_off_btn, 4, 0);
    lv_obj_set_style_shadow_color(s_day_off_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(s_day_off_btn, LV_OPA_20, 0);
    lv_obj_set_style_shadow_offset_y(s_day_off_btn, 2, 0);

    lv_obj_set_flex_flow(s_day_off_btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_day_off_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_day_off_btn, 6, 0);

    lv_obj_t *off_icon = lv_label_create(s_day_off_btn);
    lv_label_set_text(off_icon, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(off_icon, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(off_icon, UI_COLOR_TEXT_ON_PRIMARY, 0);

    s_day_off_label = lv_label_create(s_day_off_btn);
    lv_label_set_text(s_day_off_label, ui_str(STR_DAY_OFF_BTN));
    lv_obj_set_style_text_font(s_day_off_label, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(s_day_off_label, UI_COLOR_TEXT_ON_PRIMARY, 0);

    lv_obj_add_event_cb(s_day_off_btn, day_off_event_cb, LV_EVENT_CLICKED, NULL);

    /* Day On button (success/green) */
    s_day_on_btn = lv_button_create(row);
    lv_obj_set_size(s_day_on_btn, ACTION_BTN_W, ACTION_BTN_H);
    lv_obj_set_style_bg_color(s_day_on_btn, UI_COLOR_SUCCESS, 0);
    lv_obj_set_style_bg_opa(s_day_on_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_day_on_btn, UI_BTN_RADIUS, 0);
    lv_obj_set_style_border_width(s_day_on_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_day_on_btn, 4, 0);
    lv_obj_set_style_shadow_color(s_day_on_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(s_day_on_btn, LV_OPA_20, 0);
    lv_obj_set_style_shadow_offset_y(s_day_on_btn, 2, 0);

    lv_obj_set_flex_flow(s_day_on_btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_day_on_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_day_on_btn, 6, 0);

    lv_obj_t *on_icon = lv_label_create(s_day_on_btn);
    lv_label_set_text(on_icon, LV_SYMBOL_OK);
    lv_obj_set_style_text_font(on_icon, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(on_icon, UI_COLOR_TEXT_ON_PRIMARY, 0);

    s_day_on_label = lv_label_create(s_day_on_btn);
    lv_label_set_text(s_day_on_label, ui_str(STR_DAY_ON_BTN));
    lv_obj_set_style_text_font(s_day_on_label, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(s_day_on_label, UI_COLOR_TEXT_ON_PRIMARY, 0);

    lv_obj_add_event_cb(s_day_on_btn, day_on_event_cb, LV_EVENT_CLICKED, NULL);
}

/* ------------------------------------------------------------------ */
/* Event handlers                                                      */
/* ------------------------------------------------------------------ */
static void test_bell_event_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Test bell pressed — requesting PIN");
    TouchScreen_UI_ShowPinEntry(pin_result_test_bell);
}

static void panic_fab_event_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Panic FAB pressed — requesting PIN");
    TouchScreen_UI_ShowPinEntry(pin_result_panic_toggle);
}

static void day_off_event_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Day Off pressed — requesting PIN");
    TouchScreen_UI_ShowPinEntry(pin_result_day_off);
}

static void day_on_event_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Day On pressed — requesting PIN");
    TouchScreen_UI_ShowPinEntry(pin_result_day_on);
}

/* ------------------------------------------------------------------ */
/* PIN result callbacks                                                */
/* ------------------------------------------------------------------ */
static void pin_result_test_bell(bool success)
{
    if (success) {
        ESP_LOGI(TAG, "PIN OK — ringing test bell for %ds", TEST_BELL_SEC);
        TS_Bell_TestRing(TEST_BELL_SEC);
        dashboard_refresh_bell_status();
    } else {
        ESP_LOGI(TAG, "PIN cancelled for test bell");
    }
}

static void pin_result_panic_toggle(bool success)
{
    if (success) {
        bool currently_panic = TS_Bell_IsPanic();
        ESP_LOGI(TAG, "PIN OK — toggling panic: %s → %s",
                 currently_panic ? "ON" : "OFF",
                 currently_panic ? "OFF" : "ON");
        TS_Bell_SetPanic(!currently_panic);
        dashboard_refresh_bell_status();
        dashboard_refresh_panic_fab();
    } else {
        ESP_LOGI(TAG, "PIN cancelled for panic toggle");
    }
}

static void pin_result_day_off(bool success)
{
    if (success) {
        int current = TS_Schedule_GetTodayOverrideAction();
        if (current == (int)EXCEPTION_ACTION_DAY_OFF) {
            ESP_LOGI(TAG, "PIN OK — cancelling Day Off override");
            TS_Schedule_CancelTodayOverride();
        } else {
            ESP_LOGI(TAG, "PIN OK — setting today as Day Off");
            TS_Schedule_SetTodayOverride(EXCEPTION_ACTION_DAY_OFF);
        }
        dashboard_refresh_info_cards();
        dashboard_refresh_day_override_buttons();
    } else {
        ESP_LOGI(TAG, "PIN cancelled for Day Off");
    }
}

static void pin_result_day_on(bool success)
{
    if (success) {
        int current = TS_Schedule_GetTodayOverrideAction();
        if (current == (int)EXCEPTION_ACTION_NORMAL) {
            ESP_LOGI(TAG, "PIN OK — cancelling Day On override");
            TS_Schedule_CancelTodayOverride();
        } else {
            ESP_LOGI(TAG, "PIN OK — setting today as Day On (default schedule)");
            TS_Schedule_SetTodayOverride(EXCEPTION_ACTION_NORMAL);
        }
        dashboard_refresh_info_cards();
        dashboard_refresh_day_override_buttons();
    } else {
        ESP_LOGI(TAG, "PIN cancelled for Day On");
    }
}

/* ------------------------------------------------------------------ */
/* Data refresh                                                        */
/* ------------------------------------------------------------------ */
static void dashboard_refresh_all(void)
{
    dashboard_refresh_warning_banner();
    dashboard_refresh_bell_status();
    dashboard_refresh_info_cards();
    dashboard_refresh_panic_fab();
    dashboard_refresh_day_override_buttons();
}

static void dashboard_refresh_bell_status(void)
{
    bsp_display_lock(0);

    /* Bell state dot + label */
    BELL_STATE_E state = TS_Bell_GetState();
    if (s_bell_dot) {
        lv_obj_set_style_bg_color(s_bell_dot, bell_state_color(state), 0);
        lv_obj_set_style_border_color(s_bell_dot, bell_state_border_color(state), 0);
    }
    if (s_bell_state_lbl) {
        lv_label_set_text(s_bell_state_lbl, bell_state_str(state));
    }

    /* Next bell info */
    NEXT_BELL_INFO_T tNext = {0};
    esp_err_t err = TS_Schedule_GetNextBell(&tNext);

    if (err == ESP_OK && tNext.bValid) {
        if (s_next_bell_time) {
            char tbuf[8];
            snprintf(tbuf, sizeof(tbuf), "%02u:%02u", tNext.ucHour, tNext.ucMinute);
            lv_label_set_text(s_next_bell_time, tbuf);
        }
        if (s_next_bell_info) {
            char dbuf[64];
            snprintf(dbuf, sizeof(dbuf), "%s (%us)", tNext.acLabel, tNext.usDurationSec);
            lv_label_set_text(s_next_bell_info, dbuf);
        }
    } else {
        if (s_next_bell_time) {
            lv_label_set_text(s_next_bell_time, "--:--");
        }
        if (s_next_bell_info) {
            lv_label_set_text(s_next_bell_info, ui_str(STR_NO_UPCOMING_BELLS));
        }
    }

    bsp_display_unlock();
}

static void dashboard_refresh_info_cards(void)
{
    SCHEDULER_STATUS_T tStatus = {0};
    esp_err_t err = TS_Schedule_GetStatus(&tStatus);

    bsp_display_lock(0);

    if (s_day_date_lbl) {
        if (err == ESP_OK && tStatus.bTimeSynced) {
            char buf[32];
            const char *days[] = {
                ui_str(STR_DAY_SUN), ui_str(STR_DAY_MON), ui_str(STR_DAY_TUE),
                ui_str(STR_DAY_WED), ui_str(STR_DAY_THU), ui_str(STR_DAY_FRI),
                ui_str(STR_DAY_SAT)
            };
            const char *months[] = {
                ui_str(STR_MON_JAN), ui_str(STR_MON_FEB), ui_str(STR_MON_MAR),
                ui_str(STR_MON_APR), ui_str(STR_MON_MAY), ui_str(STR_MON_JUN),
                ui_str(STR_MON_JUL), ui_str(STR_MON_AUG), ui_str(STR_MON_SEP),
                ui_str(STR_MON_OCT), ui_str(STR_MON_NOV), ui_str(STR_MON_DEC)
            };
            int wday = tStatus.tCurrentTime.tm_wday;
            int mon  = tStatus.tCurrentTime.tm_mon;
            if (wday < 0 || wday > 6) wday = 0;
            if (mon < 0 || mon > 11)  mon = 0;
            snprintf(buf, sizeof(buf), "%s, %s %d",
                     days[wday],
                     months[mon],
                     tStatus.tCurrentTime.tm_mday);
            lv_label_set_text(s_day_date_lbl, buf);
        } else {
            lv_label_set_text(s_day_date_lbl, ui_str(STR_NO_TIME_SYNC));
        }
    }

    if (s_day_type_lbl) {
        if (err == ESP_OK) {
            lv_label_set_text(s_day_type_lbl, day_type_str(tStatus.eDayType));
        } else {
            lv_label_set_text(s_day_type_lbl, "-");
        }
    }

    bsp_display_unlock();
}

static void dashboard_refresh_panic_fab(void)
{
    bool is_panic = TS_Bell_IsPanic();

    bsp_display_lock(0);

    if (s_panic_label) {
        if (is_panic) {
            lv_label_set_text(s_panic_label, ui_str(STR_STOP));
        } else {
            lv_label_set_text(s_panic_label, ui_str(STR_PANIC));
        }
    }

    if (s_panic_btn) {
        if (is_panic) {
            lv_obj_set_style_bg_color(s_panic_btn, UI_COLOR_DANGER_DARK, 0);
        } else {
            lv_obj_set_style_bg_color(s_panic_btn, UI_COLOR_DANGER, 0);
        }
    }

    bsp_display_unlock();

    if (is_panic) {
        dashboard_start_panic_pulse();
    } else {
        dashboard_stop_panic_pulse();
    }
}

/* ------------------------------------------------------------------ */
/* Day override button refresh (toggle labels based on active override)*/
/* ------------------------------------------------------------------ */
static void dashboard_refresh_day_override_buttons(void)
{
    int override = TS_Schedule_GetTodayOverrideAction();

    bsp_display_lock(0);

    if (s_day_off_label) {
        if (override == (int)EXCEPTION_ACTION_DAY_OFF) {
            lv_label_set_text(s_day_off_label, ui_str(STR_CANCEL_OFF));
        } else {
            lv_label_set_text(s_day_off_label, ui_str(STR_DAY_OFF_BTN));
        }
    }
    if (s_day_off_btn) {
        if (override == (int)EXCEPTION_ACTION_DAY_OFF) {
            lv_obj_set_style_bg_color(s_day_off_btn, UI_COLOR_TEXT_SECONDARY, 0);
        } else {
            lv_obj_set_style_bg_color(s_day_off_btn, UI_COLOR_WARNING, 0);
        }
    }

    if (s_day_on_label) {
        if (override == (int)EXCEPTION_ACTION_NORMAL) {
            lv_label_set_text(s_day_on_label, ui_str(STR_CANCEL_ON));
        } else {
            lv_label_set_text(s_day_on_label, ui_str(STR_DAY_ON_BTN));
        }
    }
    if (s_day_on_btn) {
        if (override == (int)EXCEPTION_ACTION_NORMAL) {
            lv_obj_set_style_bg_color(s_day_on_btn, UI_COLOR_TEXT_SECONDARY, 0);
        } else {
            lv_obj_set_style_bg_color(s_day_on_btn, UI_COLOR_SUCCESS, 0);
        }
    }

    bsp_display_unlock();
}

/* ------------------------------------------------------------------ */
/* Warning banner (WiFi / NTP error states)                            */
/* ------------------------------------------------------------------ */
static void dashboard_create_warning_banner(lv_obj_t *parent)
{
    s_warning_banner = lv_obj_create(parent);
    lv_obj_set_size(s_warning_banner, CONTENT_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_warning_banner, UI_COLOR_WARNING, 0);
    lv_obj_set_style_bg_opa(s_warning_banner, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_warning_banner, UI_BTN_RADIUS, 0);
    lv_obj_set_style_border_width(s_warning_banner, 0, 0);
    lv_obj_set_style_pad_all(s_warning_banner, UI_PAD_SMALL, 0);
    lv_obj_set_scrollbar_mode(s_warning_banner, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(s_warning_banner, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_warning_banner, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_warning_banner, 6, 0);

    s_warning_icon = lv_label_create(s_warning_banner);
    lv_label_set_text(s_warning_icon, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_color(s_warning_icon, UI_COLOR_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(s_warning_icon, UI_FONT_CAPTION, 0);

    s_warning_label = lv_label_create(s_warning_banner);
    lv_obj_set_style_text_color(s_warning_label, UI_COLOR_TEXT_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(s_warning_label, UI_FONT_CAPTION, 0);
    lv_label_set_text(s_warning_label, "");
    lv_obj_set_flex_grow(s_warning_label, 1);
    lv_label_set_long_mode(s_warning_label, LV_LABEL_LONG_WRAP);

    /* Hidden by default */
    lv_obj_add_flag(s_warning_banner, LV_OBJ_FLAG_HIDDEN);
}

static void dashboard_refresh_warning_banner(void)
{
    if (!s_warning_banner || !s_warning_label) return;

    bool wifi_ok = TS_WiFi_IsConnected();
    SCHEDULER_STATUS_T tStatus = {0};
    esp_err_t err = TS_Schedule_GetStatus(&tStatus);
    bool ntp_ok = (err == ESP_OK && tStatus.bTimeSynced);

    bsp_display_lock(0);

    if (!wifi_ok && !ntp_ok) {
        lv_label_set_text(s_warning_label, ui_str(STR_WARN_WIFI_AND_TIME));
        lv_obj_set_style_bg_color(s_warning_banner, UI_COLOR_DANGER, 0);
        lv_obj_clear_flag(s_warning_banner, LV_OBJ_FLAG_HIDDEN);
    } else if (!wifi_ok) {
        lv_label_set_text(s_warning_label, ui_str(STR_WARN_WIFI_DISCONNECTED));
        lv_obj_set_style_bg_color(s_warning_banner, UI_COLOR_WARNING, 0);
        lv_obj_clear_flag(s_warning_banner, LV_OBJ_FLAG_HIDDEN);
    } else if (!ntp_ok && tStatus.ulLastSyncAgeSec != UINT32_MAX
               && tStatus.ulLastSyncAgeSec > 0) {
        char warn_buf[96];
        uint32_t mins = tStatus.ulLastSyncAgeSec / 60;
        snprintf(warn_buf, sizeof(warn_buf),
                 ui_str(STR_WARN_TIME_STALE), (unsigned long)mins);
        lv_label_set_text(s_warning_label, warn_buf);
        lv_obj_set_style_bg_color(s_warning_banner, UI_COLOR_WARNING, 0);
        lv_obj_clear_flag(s_warning_banner, LV_OBJ_FLAG_HIDDEN);
    } else if (!ntp_ok) {
        lv_label_set_text(s_warning_label, ui_str(STR_WARN_TIME_NOT_SYNCED));
        lv_obj_set_style_bg_color(s_warning_banner, UI_COLOR_WARNING, 0);
        lv_obj_clear_flag(s_warning_banner, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_warning_banner, LV_OBJ_FLAG_HIDDEN);
    }

    bsp_display_unlock();
}

/* ------------------------------------------------------------------ */
/* Panic pulsing animation                                             */
/* ------------------------------------------------------------------ */
static void panic_anim_opa_cb(void *obj, int32_t val)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)obj, (lv_opa_t)val, 0);
}

static void dashboard_start_panic_pulse(void)
{
    if (s_panic_anim_running || !s_panic_btn) return;

    lv_anim_init(&s_panic_anim);
    lv_anim_set_var(&s_panic_anim, s_panic_btn);
    lv_anim_set_values(&s_panic_anim, LV_OPA_COVER, LV_OPA_50);
    lv_anim_set_duration(&s_panic_anim, 500);
    lv_anim_set_playback_duration(&s_panic_anim, 500);
    lv_anim_set_repeat_count(&s_panic_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&s_panic_anim, panic_anim_opa_cb);

    bsp_display_lock(0);
    lv_anim_start(&s_panic_anim);
    bsp_display_unlock();

    s_panic_anim_running = true;
}

static void dashboard_stop_panic_pulse(void)
{
    if (!s_panic_anim_running || !s_panic_btn) return;

    bsp_display_lock(0);
    lv_anim_delete(s_panic_btn, panic_anim_opa_cb);
    if (s_panic_btn) {
        lv_obj_set_style_bg_opa(s_panic_btn, LV_OPA_COVER, 0);
    }
    bsp_display_unlock();

    s_panic_anim_running = false;
}
