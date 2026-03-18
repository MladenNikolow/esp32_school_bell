/* ================================================================== */
/* schedule_view_screen.c — Today's bell list (read-only), shift tabs  */
/* ================================================================== */
#include "schedule_view_screen_internal.h"
#include "../../src/ui_manager_internal.h"
#include "../../src/ui_theme.h"
#include "../../components/card/card_component.h"
#include "TouchScreen_Services.h"
#include "TouchScreen_UI_Manager.h"
#include "Scheduler_API.h"
#include "esp_log.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "SCHEDULE_VIEW";

/* ------------------------------------------------------------------ */
/* Layout constants                                                    */
/* ------------------------------------------------------------------ */
#define CONTENT_WIDTH   (480 - 2 * UI_PAD_SCREEN)   /* 456px */
#define TAB_HEIGHT      40
#define ROW_HEIGHT      52
#define DOT_SIZE        10
#define TIME_COL_W      70
#define DUR_COL_W       50

/* ------------------------------------------------------------------ */
/* Module state                                                        */
/* ------------------------------------------------------------------ */
static lv_obj_t *s_content       = NULL;  /* Container between chrome */
static lv_obj_t *s_tab_1st       = NULL;  /* 1st Shift tab button */
static lv_obj_t *s_tab_2nd       = NULL;  /* 2nd Shift tab button */
static lv_obj_t *s_list_cont     = NULL;  /* Scrollable bell list area */
static lv_obj_t *s_empty_label   = NULL;  /* "No bells" message */
static uint8_t   s_active_shift  = 0;     /* 0 = 1st, 1 = 2nd */

/* Cached bell data for current shift */
static BELL_ENTRY_T s_bells[SCHEDULE_MAX_BELLS_PER_SHIFT];
static uint32_t     s_bell_count  = 0;
static bool         s_shift_enabled = false;

/* Current time cache (updated each refresh) */
static uint8_t s_cur_hour   = 0;
static uint8_t s_cur_minute = 0;

/* ------------------------------------------------------------------ */
/* Forward declarations                                                */
/* ------------------------------------------------------------------ */
static void schedule_load_shift(uint8_t shift);
static void schedule_build_list(void);
static void schedule_update_highlight(void);
static void schedule_scroll_to_next(void);
static void tab_1st_event_cb(lv_event_t *e);
static void tab_2nd_event_cb(lv_event_t *e);
static void update_tab_styles(void);

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Determine bell row state relative to current time.
 *  -1 = past, 0 = next (first future bell), 1 = future
 */
static int bell_time_state(uint8_t idx)
{
    if (idx >= s_bell_count) return 1;

    uint16_t bell_min = (uint16_t)s_bells[idx].ucHour * 60 + s_bells[idx].ucMinute;
    uint16_t now_min  = (uint16_t)s_cur_hour * 60 + s_cur_minute;

    if (bell_min < now_min) return -1;  /* Past */
    if (bell_min == now_min) return 0;  /* Current / next */

    /* Check if this is the first future bell */
    for (uint32_t i = 0; i < idx; i++) {
        uint16_t prev_min = (uint16_t)s_bells[i].ucHour * 60 + s_bells[i].ucMinute;
        if (prev_min >= now_min) {
            return 1;  /* An earlier entry is already the "next" */
        }
    }
    return 0;  /* This is the first future bell = "next" */
}

/* ------------------------------------------------------------------ */
/* Tab bar                                                             */
/* ------------------------------------------------------------------ */
static void update_tab_styles(void)
{
    bsp_display_lock(0);

    if (s_tab_1st) {
        if (s_active_shift == 0) {
            lv_obj_set_style_bg_color(s_tab_1st, UI_COLOR_PRIMARY, 0);
            lv_obj_set_style_bg_opa(s_tab_1st, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(s_tab_1st, UI_COLOR_TEXT_ON_PRIMARY, 0);
        } else {
            lv_obj_set_style_bg_color(s_tab_1st, UI_COLOR_SURFACE, 0);
            lv_obj_set_style_bg_opa(s_tab_1st, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(s_tab_1st, UI_COLOR_PRIMARY, 0);
        }
    }
    if (s_tab_2nd) {
        if (s_active_shift == 1) {
            lv_obj_set_style_bg_color(s_tab_2nd, UI_COLOR_PRIMARY, 0);
            lv_obj_set_style_bg_opa(s_tab_2nd, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(s_tab_2nd, UI_COLOR_TEXT_ON_PRIMARY, 0);
        } else {
            lv_obj_set_style_bg_color(s_tab_2nd, UI_COLOR_SURFACE, 0);
            lv_obj_set_style_bg_opa(s_tab_2nd, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(s_tab_2nd, UI_COLOR_PRIMARY, 0);
        }
    }

    bsp_display_unlock();
}

static void tab_1st_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_active_shift == 0) return;
    s_active_shift = 0;
    update_tab_styles();
    schedule_load_shift(0);
    schedule_build_list();
    schedule_scroll_to_next();
}

static void tab_2nd_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_active_shift == 1) return;
    s_active_shift = 1;
    update_tab_styles();
    schedule_load_shift(1);
    schedule_build_list();
    schedule_scroll_to_next();
}

/* ------------------------------------------------------------------ */
/* Data loading                                                        */
/* ------------------------------------------------------------------ */
static void schedule_load_shift(uint8_t shift)
{
    s_bell_count = 0;
    s_shift_enabled = false;

    esp_err_t err = TS_Schedule_GetShiftBells(shift, s_bells, &s_bell_count, &s_shift_enabled);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load shift %u bells: %s", shift, esp_err_to_name(err));
        s_bell_count = 0;
    }

    /* Update current time */
    SCHEDULER_STATUS_T st = {0};
    if (TS_Schedule_GetStatus(&st) == ESP_OK && st.bTimeSynced) {
        s_cur_hour   = (uint8_t)st.tCurrentTime.tm_hour;
        s_cur_minute = (uint8_t)st.tCurrentTime.tm_min;
    }

    ESP_LOGI(TAG, "Shift %u: %s, %lu bells",
             shift, s_shift_enabled ? "enabled" : "disabled", (unsigned long)s_bell_count);
}

/* ------------------------------------------------------------------ */
/* Build the scrollable bell list                                      */
/* ------------------------------------------------------------------ */
static void schedule_build_list(void)
{
    if (!s_list_cont) return;

    bsp_display_lock(0);

    /* Clear existing children */
    lv_obj_clean(s_list_cont);

    /* Empty state */
    if (!s_shift_enabled || s_bell_count == 0) {
        s_empty_label = lv_label_create(s_list_cont);
        const char *msg = !s_shift_enabled ? "Shift disabled" : "No bells today";
        lv_label_set_text(s_empty_label, msg);
        lv_obj_set_style_text_font(s_empty_label, UI_FONT_BODY, 0);
        lv_obj_set_style_text_color(s_empty_label, UI_COLOR_TEXT_SECONDARY, 0);
        lv_obj_center(s_empty_label);
        bsp_display_unlock();
        return;
    }
    s_empty_label = NULL;

    /* Build rows */
    bool next_found = false;
    for (uint32_t i = 0; i < s_bell_count; i++) {
        int state = bell_time_state((uint8_t)i);
        bool is_next = false;
        if (state >= 0 && !next_found) {
            is_next = true;
            next_found = true;
        }
        bool is_past = (state < 0);

        /* Row container */
        lv_obj_t *row = lv_obj_create(s_list_cont);
        lv_obj_set_size(row, LV_PCT(100), ROW_HEIGHT);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_left(row, UI_PAD_SMALL, 0);
        lv_obj_set_style_pad_right(row, UI_PAD_SMALL, 0);
        lv_obj_set_style_pad_top(row, 4, 0);
        lv_obj_set_style_pad_bottom(row, 4, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, UI_PAD_SMALL, 0);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

        /* Highlight next bell row */
        if (is_next) {
            lv_obj_set_style_bg_color(row, UI_COLOR_PRIMARY_LIGHT, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_40, 0);
            lv_obj_set_style_radius(row, UI_BTN_RADIUS, 0);
        }

        /* Status indicator (dot or checkmark) */
        lv_obj_t *indicator = lv_label_create(row);
        if (is_past) {
            lv_label_set_text(indicator, LV_SYMBOL_OK);
            lv_obj_set_style_text_color(indicator, UI_COLOR_TEXT_DISABLED, 0);
        } else if (is_next) {
            lv_label_set_text(indicator, LV_SYMBOL_RIGHT);
            lv_obj_set_style_text_color(indicator, UI_COLOR_PRIMARY, 0);
        } else {
            lv_label_set_text(indicator, LV_SYMBOL_MINUS);
            lv_obj_set_style_text_color(indicator, UI_COLOR_TEXT_DISABLED, 0);
        }
        lv_obj_set_style_text_font(indicator, UI_FONT_CAPTION, 0);
        lv_obj_set_style_min_width(indicator, 20, 0);

        /* Time */
        lv_obj_t *time_lbl = lv_label_create(row);
        char time_buf[8];
        snprintf(time_buf, sizeof(time_buf), "%02u:%02u",
                 s_bells[i].ucHour, s_bells[i].ucMinute);
        lv_label_set_text(time_lbl, time_buf);
        lv_obj_set_style_text_font(time_lbl, UI_FONT_BODY, 0);
        lv_obj_set_style_min_width(time_lbl, TIME_COL_W, 0);

        if (is_past) {
            lv_obj_set_style_text_color(time_lbl, UI_COLOR_TEXT_DISABLED, 0);
        } else if (is_next) {
            lv_obj_set_style_text_color(time_lbl, UI_COLOR_PRIMARY_DARK, 0);
        } else {
            lv_obj_set_style_text_color(time_lbl, UI_COLOR_TEXT_PRIMARY, 0);
        }

        /* Label (bell name) — takes remaining space */
        lv_obj_t *name_lbl = lv_label_create(row);
        lv_label_set_text(name_lbl, s_bells[i].acLabel);
        lv_obj_set_flex_grow(name_lbl, 1);
        lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(name_lbl, UI_FONT_BODY_SMALL, 0);

        if (is_past) {
            lv_obj_set_style_text_color(name_lbl, UI_COLOR_TEXT_DISABLED, 0);
        } else if (is_next) {
            lv_obj_set_style_text_color(name_lbl, UI_COLOR_PRIMARY_DARK, 0);
        } else {
            lv_obj_set_style_text_color(name_lbl, UI_COLOR_TEXT_PRIMARY, 0);
        }

        /* Duration */
        lv_obj_t *dur_lbl = lv_label_create(row);
        char dur_buf[12];
        snprintf(dur_buf, sizeof(dur_buf), "%us", s_bells[i].usDurationSec);
        lv_label_set_text(dur_lbl, dur_buf);
        lv_obj_set_style_text_font(dur_lbl, UI_FONT_SMALL, 0);
        lv_obj_set_style_text_color(dur_lbl, UI_COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_min_width(dur_lbl, DUR_COL_W, 0);
        lv_obj_set_style_text_align(dur_lbl, LV_TEXT_ALIGN_RIGHT, 0);

        /* Bottom divider (except last row) */
        if (i < s_bell_count - 1) {
            lv_obj_t *div = lv_obj_create(s_list_cont);
            lv_obj_set_size(div, LV_PCT(100), 1);
            lv_obj_set_style_bg_color(div, UI_COLOR_DIVIDER, 0);
            lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(div, 0, 0);
            lv_obj_set_style_pad_all(div, 0, 0);
            lv_obj_set_scrollbar_mode(div, LV_SCROLLBAR_MODE_OFF);
        }
    }

    bsp_display_unlock();
}

/* ------------------------------------------------------------------ */
/* Scroll to next bell row                                             */
/* ------------------------------------------------------------------ */
static void schedule_scroll_to_next(void)
{
    if (!s_list_cont || s_bell_count == 0) return;

    bsp_display_lock(0);

    /* Find the next-bell row and scroll to it.
       Each bell row is followed by a divider, so the row index i
       maps to child index i * 2 (row, divider pairs). */
    bool found = false;
    for (uint32_t i = 0; i < s_bell_count; i++) {
        int state = bell_time_state((uint8_t)i);
        if (state >= 0) {
            uint32_t child_idx = i * 2;  /* row + divider pairs */
            if (child_idx > 0 && child_idx > 1) child_idx -= 2;  /* Scroll 1 above for context */
            lv_obj_t *target = lv_obj_get_child(s_list_cont, (int32_t)child_idx);
            if (target) {
                lv_obj_scroll_to_view(target, LV_ANIM_ON);
            }
            found = true;
            break;
        }
    }

    /* If all past, scroll to bottom */
    if (!found) {
        lv_obj_scroll_to_y(s_list_cont, LV_COORD_MAX, LV_ANIM_ON);
    }

    bsp_display_unlock();
}

/* ------------------------------------------------------------------ */
/* Highlight refresh (called periodically)                             */
/* ------------------------------------------------------------------ */
static void schedule_update_highlight(void)
{
    /* Re-read current time */
    SCHEDULER_STATUS_T st = {0};
    if (TS_Schedule_GetStatus(&st) == ESP_OK && st.bTimeSynced) {
        uint8_t new_hour = (uint8_t)st.tCurrentTime.tm_hour;
        uint8_t new_min  = (uint8_t)st.tCurrentTime.tm_min;

        /* Only rebuild if minute changed */
        if (new_hour != s_cur_hour || new_min != s_cur_minute) {
            s_cur_hour   = new_hour;
            s_cur_minute = new_min;
            schedule_build_list();
        }
    }
}

/* ------------------------------------------------------------------ */
/* Screen lifecycle                                                    */
/* ------------------------------------------------------------------ */
void touchscreen_schedule_view_screen_create(void)
{
    ESP_LOGI(TAG, "Creating schedule view screen");

    s_active_shift = 0;

    bsp_display_lock(0);

    lv_obj_t *scr = g_ui_state.screen_obj;

    /* Content container */
    s_content = lv_obj_create(scr);
    lv_obj_set_size(s_content, 480, UI_CONTENT_HEIGHT);
    lv_obj_set_pos(s_content, 0, UI_STATUSBAR_HEIGHT);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, UI_PAD_SCREEN, 0);
    lv_obj_set_style_pad_row(s_content, UI_PAD_SMALL, 0);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(s_content, LV_SCROLLBAR_MODE_OFF);

    /* === Shift toggle tabs === */
    lv_obj_t *tab_row = lv_obj_create(s_content);
    lv_obj_set_size(tab_row, CONTENT_WIDTH, TAB_HEIGHT);
    lv_obj_set_style_bg_opa(tab_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tab_row, 0, 0);
    lv_obj_set_style_pad_all(tab_row, 0, 0);
    lv_obj_set_flex_flow(tab_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tab_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(tab_row, 0, 0);
    lv_obj_set_scrollbar_mode(tab_row, LV_SCROLLBAR_MODE_OFF);

    /* 1st Shift tab */
    s_tab_1st = lv_button_create(tab_row);
    lv_obj_set_size(s_tab_1st, CONTENT_WIDTH / 2, TAB_HEIGHT);
    lv_obj_set_style_radius(s_tab_1st, UI_BTN_RADIUS, 0);
    lv_obj_set_style_border_color(s_tab_1st, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(s_tab_1st, 2, 0);
    lv_obj_set_style_shadow_width(s_tab_1st, 0, 0);
    lv_obj_t *t1_lbl = lv_label_create(s_tab_1st);
    lv_label_set_text(t1_lbl, "1st Shift");
    lv_obj_set_style_text_font(t1_lbl, UI_FONT_BODY_SMALL, 0);
    lv_obj_center(t1_lbl);
    lv_obj_add_event_cb(s_tab_1st, tab_1st_event_cb, LV_EVENT_CLICKED, NULL);

    /* 2nd Shift tab */
    s_tab_2nd = lv_button_create(tab_row);
    lv_obj_set_size(s_tab_2nd, CONTENT_WIDTH / 2, TAB_HEIGHT);
    lv_obj_set_style_radius(s_tab_2nd, UI_BTN_RADIUS, 0);
    lv_obj_set_style_border_color(s_tab_2nd, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(s_tab_2nd, 2, 0);
    lv_obj_set_style_shadow_width(s_tab_2nd, 0, 0);
    lv_obj_t *t2_lbl = lv_label_create(s_tab_2nd);
    lv_label_set_text(t2_lbl, "2nd Shift");
    lv_obj_set_style_text_font(t2_lbl, UI_FONT_BODY_SMALL, 0);
    lv_obj_center(t2_lbl);
    lv_obj_add_event_cb(s_tab_2nd, tab_2nd_event_cb, LV_EVENT_CLICKED, NULL);

    /* === Scrollable bell list container === */
    s_list_cont = lv_obj_create(s_content);
    lv_obj_set_size(s_list_cont, CONTENT_WIDTH,
                    UI_CONTENT_HEIGHT - TAB_HEIGHT - 3 * UI_PAD_SCREEN);
    lv_obj_set_style_bg_color(s_list_cont, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(s_list_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_list_cont, UI_CARD_RADIUS, 0);
    lv_obj_set_style_border_width(s_list_cont, 0, 0);
    lv_obj_set_style_pad_all(s_list_cont, UI_PAD_SMALL, 0);
    lv_obj_set_style_pad_row(s_list_cont, 0, 0);
    lv_obj_set_flex_flow(s_list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(s_list_cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_shadow_width(s_list_cont, UI_CARD_SHADOW_WIDTH, 0);
    lv_obj_set_style_shadow_color(s_list_cont, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(s_list_cont, UI_CARD_SHADOW_OPA, 0);
    lv_obj_set_style_shadow_offset_y(s_list_cont, UI_CARD_SHADOW_OFS_Y, 0);

    bsp_display_unlock();

    /* Style active tab */
    update_tab_styles();

    /* Load data and build list */
    schedule_load_shift(s_active_shift);
    schedule_build_list();
    schedule_scroll_to_next();

    ESP_LOGI(TAG, "Schedule view screen created");
}

void touchscreen_schedule_view_screen_destroy(void)
{
    ESP_LOGI(TAG, "Destroying schedule view screen");

    s_content     = NULL;
    s_tab_1st     = NULL;
    s_tab_2nd     = NULL;
    s_list_cont   = NULL;
    s_empty_label = NULL;
    s_bell_count  = 0;
}

void touchscreen_schedule_view_screen_update(void)
{
    if (!s_list_cont) return;
    schedule_update_highlight();
}
