/* ================================================================== */
/* pin_entry_screen.c — 4-digit PIN numeric keypad overlay             */
/* ================================================================== */
#include "pin_entry_screen_internal.h"
#include "../../src/ui_manager_internal.h"
#include "../../src/ui_theme.h"
#include "TouchScreen_Services.h"
#include "TouchScreen_UI_Manager.h"
#include "esp_log.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "PIN_ENTRY";

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */
#define PIN_LENGTH          4
#define KEY_COLS            3
#define KEY_ROWS            4
#define KEY_WIDTH           80
#define KEY_HEIGHT          56
#define KEY_GAP             10
#define DOT_SIZE            20
#define DOT_GAP             16
#define PANEL_WIDTH         320
#define PANEL_PAD           24
#define SHAKE_OFFSET        12
#define SHAKE_DURATION_MS   60

/* ------------------------------------------------------------------ */
/* Module state                                                        */
/* ------------------------------------------------------------------ */
static TouchScreen_PIN_Result_Callback_t s_callback    = NULL;
static lv_obj_t *s_backdrop    = NULL;  /* Full-screen dark overlay */
static lv_obj_t *s_panel       = NULL;  /* White card containing keypad */
static lv_obj_t *s_dots[PIN_LENGTH]    = {0};
static lv_obj_t *s_status_label        = NULL;
static lv_obj_t *s_lockout_label       = NULL;
static lv_timer_t *s_lockout_timer     = NULL;
static char      s_entered[PIN_LENGTH + 1] = {0};
static uint8_t   s_entered_count = 0;

/* ------------------------------------------------------------------ */
/* Forward declarations                                                */
/* ------------------------------------------------------------------ */
static void pin_update_dots(void);
static void pin_clear_entry(void);
static void pin_on_digit(uint8_t digit);
static void pin_on_backspace(void);
static void pin_on_clear(void);
static void pin_on_cancel(void);
static void pin_try_validate(void);
static void pin_shake_animation(void);
static void pin_show_lockout(void);
static void pin_hide_lockout(void);
static void pin_lockout_timer_cb(lv_timer_t *timer);

/* ------------------------------------------------------------------ */
/* Event handlers                                                      */
/* ------------------------------------------------------------------ */
static void key_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    intptr_t key_val = (intptr_t)lv_event_get_user_data(e);

    (void)btn;

    if (key_val >= 0 && key_val <= 9) {
        pin_on_digit((uint8_t)key_val);
    } else if (key_val == 10) {
        pin_on_clear();
    } else if (key_val == 11) {
        pin_on_backspace();
    }
}

static void cancel_event_cb(lv_event_t *e)
{
    (void)e;
    pin_on_cancel();
}

/* ------------------------------------------------------------------ */
/* Dot indicators                                                      */
/* ------------------------------------------------------------------ */
static void pin_update_dots(void)
{
    for (int i = 0; i < PIN_LENGTH; i++) {
        if (!s_dots[i]) continue;
        if (i < s_entered_count) {
            /* Filled dot */
            lv_obj_set_style_bg_color(s_dots[i], UI_COLOR_PRIMARY, 0);
            lv_obj_set_style_bg_opa(s_dots[i], LV_OPA_COVER, 0);
        } else {
            /* Empty dot */
            lv_obj_set_style_bg_color(s_dots[i], UI_COLOR_DIVIDER, 0);
            lv_obj_set_style_bg_opa(s_dots[i], LV_OPA_COVER, 0);
        }
    }
}

/* ------------------------------------------------------------------ */
/* PIN logic                                                           */
/* ------------------------------------------------------------------ */
static void pin_clear_entry(void)
{
    memset(s_entered, 0, sizeof(s_entered));
    s_entered_count = 0;
    pin_update_dots();
    if (s_status_label) {
        lv_label_set_text(s_status_label, "");
    }
}

static void pin_on_digit(uint8_t digit)
{
    if (TS_Pin_IsLockedOut()) {
        pin_show_lockout();
        return;
    }

    if (s_entered_count >= PIN_LENGTH) return;

    s_entered[s_entered_count] = '0' + digit;
    s_entered_count++;
    s_entered[s_entered_count] = '\0';

    pin_update_dots();

    if (s_entered_count == PIN_LENGTH) {
        pin_try_validate();
    }
}

static void pin_on_backspace(void)
{
    if (s_entered_count == 0) return;

    s_entered_count--;
    s_entered[s_entered_count] = '\0';
    pin_update_dots();

    if (s_status_label) {
        lv_label_set_text(s_status_label, "");
    }
}

static void pin_on_clear(void)
{
    pin_clear_entry();
}

static void pin_on_cancel(void)
{
    ESP_LOGI(TAG, "PIN entry cancelled");
    TouchScreen_PIN_Result_Callback_t cb = s_callback;
    TouchScreen_UI_PopOverlay();
    if (cb) {
        cb(false);
    }
}

static void pin_try_validate(void)
{
    bool valid = TS_Pin_Validate(s_entered);

    if (valid) {
        ESP_LOGI(TAG, "PIN correct");
        TouchScreen_PIN_Result_Callback_t cb = s_callback;
        TouchScreen_UI_PopOverlay();
        if (cb) {
            cb(true);
        }
    } else {
        ESP_LOGW(TAG, "Wrong PIN");

        if (TS_Pin_IsLockedOut()) {
            pin_shake_animation();
            pin_clear_entry();
            pin_show_lockout();
        } else {
            pin_shake_animation();
            if (s_status_label) {
                lv_obj_set_style_text_color(s_status_label, UI_COLOR_DANGER, 0);
                lv_label_set_text(s_status_label, "Wrong PIN");
            }
            pin_clear_entry();
        }
    }
}

/* ------------------------------------------------------------------ */
/* Shake animation (horizontal wobble on wrong PIN)                    */
/* ------------------------------------------------------------------ */
static void shake_anim_x_cb(void *obj, int32_t val)
{
    lv_obj_set_style_translate_x((lv_obj_t *)obj, val, 0);
}

static void pin_shake_animation(void)
{
    if (!s_panel) return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_panel);
    lv_anim_set_values(&a, -SHAKE_OFFSET, SHAKE_OFFSET);
    lv_anim_set_duration(&a, SHAKE_DURATION_MS);
    lv_anim_set_repeat_count(&a, 3);
    lv_anim_set_playback_duration(&a, SHAKE_DURATION_MS);
    lv_anim_set_exec_cb(&a, shake_anim_x_cb);
    lv_anim_start(&a);
}

/* ------------------------------------------------------------------ */
/* Lockout display                                                     */
/* ------------------------------------------------------------------ */
static void pin_show_lockout(void)
{
    if (s_status_label) {
        lv_obj_set_style_text_color(s_status_label, UI_COLOR_DANGER, 0);
    }

    /* Start timer if not already running */
    if (!s_lockout_timer) {
        bsp_display_lock(0);
        s_lockout_timer = lv_timer_create(pin_lockout_timer_cb, 1000, NULL);
        bsp_display_unlock();
    }
    pin_lockout_timer_cb(NULL); /* Update immediately */
}

static void pin_hide_lockout(void)
{
    if (s_lockout_timer) {
        bsp_display_lock(0);
        lv_timer_delete(s_lockout_timer);
        bsp_display_unlock();
        s_lockout_timer = NULL;
    }
    if (s_status_label) {
        lv_label_set_text(s_status_label, "");
    }
}

static void pin_lockout_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!TS_Pin_IsLockedOut()) {
        pin_hide_lockout();
        return;
    }

    uint32_t remaining = TS_Pin_GetLockoutRemaining();
    if (s_status_label) {
        char buf[40];
        snprintf(buf, sizeof(buf), "Locked for %lus", (unsigned long)remaining);
        lv_label_set_text(s_status_label, buf);
    }
}

/* ------------------------------------------------------------------ */
/* Screen create / destroy                                             */
/* ------------------------------------------------------------------ */
void touchscreen_pin_entry_screen_create(TouchScreen_PIN_Result_Callback_t callback)
{
    ESP_LOGI(TAG, "Creating PIN entry overlay");
    s_callback = callback;
    pin_clear_entry();

    bsp_display_lock(0);

    lv_obj_t *scr = g_ui_state.screen_obj;

    /* === Backdrop (semi-transparent dark overlay) === */
    s_backdrop = lv_obj_create(scr);
    lv_obj_set_size(s_backdrop, 480, 480);
    lv_obj_set_pos(s_backdrop, 0, 0);
    lv_obj_set_style_bg_color(s_backdrop, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_backdrop, LV_OPA_50, 0);
    lv_obj_set_style_border_width(s_backdrop, 0, 0);
    lv_obj_set_style_radius(s_backdrop, 0, 0);
    lv_obj_set_style_pad_all(s_backdrop, 0, 0);
    lv_obj_set_scrollbar_mode(s_backdrop, LV_SCROLLBAR_MODE_OFF);
    /* Absorb events so taps don't pass through */
    lv_obj_add_flag(s_backdrop, LV_OBJ_FLAG_CLICKABLE);

    /* === White panel (card) === */
    s_panel = lv_obj_create(s_backdrop);
    lv_obj_set_size(s_panel, PANEL_WIDTH, LV_SIZE_CONTENT);
    lv_obj_center(s_panel);
    ui_theme_apply_card(s_panel);
    lv_obj_set_style_pad_all(s_panel, PANEL_PAD, 0);
    lv_obj_set_flex_flow(s_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_panel, 12, 0);
    lv_obj_set_scrollbar_mode(s_panel, LV_SCROLLBAR_MODE_OFF);

    /* Title */
    lv_obj_t *title = lv_label_create(s_panel);
    lv_label_set_text(title, "Enter PIN");
    lv_obj_set_style_text_font(title, UI_FONT_H2, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT_PRIMARY, 0);

    /* Subtitle */
    lv_obj_t *subtitle = lv_label_create(s_panel);
    lv_label_set_text(subtitle, "Enter 4-digit PIN to continue");
    lv_obj_set_style_text_font(subtitle, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(subtitle, UI_COLOR_TEXT_SECONDARY, 0);

    /* === Dot indicators row === */
    lv_obj_t *dot_row = lv_obj_create(s_panel);
    lv_obj_set_size(dot_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(dot_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dot_row, 0, 0);
    lv_obj_set_style_pad_all(dot_row, 4, 0);
    lv_obj_set_flex_flow(dot_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dot_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dot_row, DOT_GAP, 0);
    lv_obj_set_scrollbar_mode(dot_row, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < PIN_LENGTH; i++) {
        s_dots[i] = lv_obj_create(dot_row);
        lv_obj_set_size(s_dots[i], DOT_SIZE, DOT_SIZE);
        lv_obj_set_style_radius(s_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_dots[i], UI_COLOR_DIVIDER, 0);
        lv_obj_set_style_bg_opa(s_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(s_dots[i], UI_COLOR_PRIMARY, 0);
        lv_obj_set_style_border_width(s_dots[i], 2, 0);
        lv_obj_set_scrollbar_mode(s_dots[i], LV_SCROLLBAR_MODE_OFF);
    }

    /* === Status label (wrong PIN / lockout) === */
    s_status_label = lv_label_create(s_panel);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_font(s_status_label, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(s_status_label, UI_COLOR_DANGER, 0);
    lv_obj_set_style_min_height(s_status_label, 18, 0);

    /* === Numeric keypad grid === */
    /*  Layout: [1][2][3] / [4][5][6] / [7][8][9] / [C][0][⌫]  */
    static const char *key_labels[KEY_ROWS][KEY_COLS] = {
        {"1", "2", "3"},
        {"4", "5", "6"},
        {"7", "8", "9"},
        {"C", "0", LV_SYMBOL_BACKSPACE},
    };
    /* key_val: 0-9 = digit, 10 = clear, 11 = backspace */
    static const int key_values[KEY_ROWS][KEY_COLS] = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9},
        {10, 0, 11},
    };

    lv_obj_t *grid = lv_obj_create(s_panel);
    int grid_w = KEY_COLS * KEY_WIDTH + (KEY_COLS - 1) * KEY_GAP;
    int grid_h = KEY_ROWS * KEY_HEIGHT + (KEY_ROWS - 1) * KEY_GAP;
    lv_obj_set_size(grid, grid_w, grid_h);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_OFF);

    for (int row = 0; row < KEY_ROWS; row++) {
        for (int col = 0; col < KEY_COLS; col++) {
            lv_obj_t *btn = lv_button_create(grid);
            lv_obj_set_size(btn, KEY_WIDTH, KEY_HEIGHT);
            int x = col * (KEY_WIDTH + KEY_GAP);
            int y = row * (KEY_HEIGHT + KEY_GAP);
            lv_obj_set_pos(btn, x, y);

            int val = key_values[row][col];

            if (val == 10) {
                /* Clear button — secondary style */
                ui_theme_apply_btn_secondary(btn);
            } else if (val == 11) {
                /* Backspace — secondary style */
                ui_theme_apply_btn_secondary(btn);
            } else {
                /* Digit button — primary style */
                ui_theme_apply_btn_primary(btn);
            }

            lv_obj_t *label = lv_label_create(btn);
            lv_label_set_text(label, key_labels[row][col]);
            lv_obj_center(label);

            lv_obj_add_event_cb(btn, key_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)val);
        }
    }

    /* === Cancel button === */
    lv_obj_t *cancel_btn = lv_button_create(s_panel);
    lv_obj_set_size(cancel_btn, grid_w, 44);
    ui_theme_apply_btn_secondary(cancel_btn);

    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);

    lv_obj_add_event_cb(cancel_btn, cancel_event_cb, LV_EVENT_CLICKED, NULL);

    bsp_display_unlock();

    /* If already locked out from previous attempts, show immediately */
    if (TS_Pin_IsLockedOut()) {
        pin_show_lockout();
    }

    ESP_LOGI(TAG, "PIN entry overlay created");
}

void touchscreen_pin_entry_screen_destroy(void)
{
    ESP_LOGI(TAG, "Destroying PIN entry overlay");

    if (s_lockout_timer) {
        bsp_display_lock(0);
        lv_timer_delete(s_lockout_timer);
        bsp_display_unlock();
        s_lockout_timer = NULL;
    }

    bsp_display_lock(0);
    if (s_backdrop) {
        lv_obj_delete(s_backdrop);
        s_backdrop = NULL;
    }
    bsp_display_unlock();

    s_panel = NULL;
    s_status_label = NULL;
    s_lockout_label = NULL;
    s_callback = NULL;
    memset(s_dots, 0, sizeof(s_dots));
    memset(s_entered, 0, sizeof(s_entered));
    s_entered_count = 0;
}
