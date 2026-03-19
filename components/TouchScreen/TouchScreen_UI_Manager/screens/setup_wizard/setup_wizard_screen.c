/* ================================================================== */
/* setup_wizard_screen.c — First-time setup wizard (3 steps)           */
/*   Step 1: Language selection (BG/EN)                                */
/*   Step 2: PIN creation (4-6 digits, enter + confirm)                */
/*   Step 3: WiFi setup (optional — scan/connect or skip)              */
/* ================================================================== */
#include "setup_wizard_screen_internal.h"
#include "../../src/ui_manager_internal.h"
#include "../../src/ui_theme.h"
#include "../../src/ui_strings.h"
#include "../wifi_setup/wifi_setup_screen_internal.h"
#include "TouchScreen_Services.h"
#include "TouchScreen_UI_Manager.h"
#include "esp_log.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "SETUP_WIZARD";

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */
#define WIZARD_STEPS            3
#define PIN_MIN_LEN             4
#define PIN_MAX_LEN             6
#define PIN_DEFAULT_LEN         4
#define KEY_COLS                3
#define KEY_ROWS                4
#define KEY_WIDTH               72
#define KEY_HEIGHT              50
#define KEY_GAP                 8
#define DOT_SIZE                18
#define DOT_GAP                 14
#define STEP_DOT_SIZE           10
#define STEP_DOT_GAP            12
#define STEP_DOT_ACTIVE_SIZE    14
#define SHAKE_OFFSET            12
#define SHAKE_DURATION_MS       60
#define BOTTOM_BAR_HEIGHT       64
#define TOP_AREA_HEIGHT         90

/* ------------------------------------------------------------------ */
/* Module state                                                        */
/* ------------------------------------------------------------------ */
static uint8_t s_step = 0;  /* 0=language, 1=PIN, 2=WiFi */

/* Language step */
static ui_language_t s_selected_lang = UI_LANG_BG;

/* PIN step */
static uint8_t  s_pin_length      = PIN_DEFAULT_LEN;
static char     s_pin_first[PIN_MAX_LEN + 1]  = {0};
static char     s_pin_confirm[PIN_MAX_LEN + 1] = {0};
static uint8_t  s_pin_entered     = 0;
static bool     s_pin_confirming  = false;  /* false=entering, true=confirming */
static bool     s_pin_done        = false;  /* PIN successfully set */

/* WiFi step */
static bool     s_wifi_done       = false;
static char     s_wifi_ssid[33]   = {0};
static char     s_wifi_pass[64]   = {0};

/* LVGL objects */
static lv_obj_t *s_content_area    = NULL;  /* Middle area container */
static lv_obj_t *s_step_dots[WIZARD_STEPS] = {0};
static lv_obj_t *s_btn_next       = NULL;
static lv_obj_t *s_btn_back       = NULL;
static lv_obj_t *s_next_label     = NULL;

/* PIN step objects */
static lv_obj_t *s_pin_dots[PIN_MAX_LEN] = {0};
static lv_obj_t *s_pin_status     = NULL;
static lv_obj_t *s_pin_title      = NULL;
static lv_obj_t *s_pin_panel      = NULL;
static lv_obj_t *s_pin_dot_row    = NULL;
static lv_obj_t *s_pin_len_btns[3]= {0};   /* [4] [5] [6] */

/* Language step objects */
static lv_obj_t *s_lang_btn_bg    = NULL;
static lv_obj_t *s_lang_btn_en    = NULL;

/* WiFi step objects — uses embedded wifi setup */
static lv_obj_t *s_wifi_container = NULL;
static bool      s_wifi_embedded  = false;  /* true when wifi setup sub-screen is active */

/* ------------------------------------------------------------------ */
/* Forward declarations                                                */
/* ------------------------------------------------------------------ */
static void wizard_build_step(void);
static void wizard_build_language_step(void);
static void wizard_build_pin_step(void);
static void wizard_build_wifi_step(void);
static void wizard_update_step_dots(void);
static void wizard_update_nav_buttons(void);
static void wizard_clear_content(void);
static void wizard_finish(void);

/* PIN helpers */
static void pin_clear_entry(void);
static void pin_update_dots(void);
static void pin_on_digit(uint8_t digit);
static void pin_on_backspace(void);
static void pin_on_clear(void);
static void pin_try_complete(void);
static void pin_shake_animation(void);
static void pin_rebuild_dots(void);
static void pin_update_len_btn_styles(void);

/* WiFi result callback (used in wifi_setup_btn_cb before its definition) */
static void wizard_wifi_result_cb(TouchScreen_WiFi_Setup_Result_t result,
                                   const char *ssid, const char *password);

/* ------------------------------------------------------------------ */
/* Event callbacks                                                     */
/* ------------------------------------------------------------------ */

/* --- Navigation buttons --- */
static void next_btn_cb(lv_event_t *e)
{
    (void)e;
    if (s_step == 0)
    {
        /* Language step → go to PIN step */
        ui_set_language(s_selected_lang);
        s_step = 1;
        wizard_build_step();
    }
    else if (s_step == 1)
    {
        /* PIN step → go to WiFi step (only if PIN done) */
        if (s_pin_done)
        {
            s_step = 2;
            wizard_build_step();
        }
    }
    else if (s_step == 2)
    {
        /* WiFi step → finish */
        wizard_finish();
    }
}

static void back_btn_cb(lv_event_t *e)
{
    (void)e;
    if (s_step == 1)
    {
        s_step = 0;
        /* Reset PIN state */
        s_pin_done = false;
        s_pin_confirming = false;
        pin_clear_entry();
        wizard_build_step();
    }
    else if (s_step == 2)
    {
        /* Tear down embedded WiFi setup if active */
        if (s_wifi_embedded)
        {
            touchscreen_wifi_setup_screen_destroy();
            s_wifi_embedded = false;
        }
        s_wifi_done = false;
        s_step = 1;
        wizard_build_step();
    }
}

/* --- Language buttons --- */
static void lang_bg_cb(lv_event_t *e)
{
    (void)e;
    s_selected_lang = UI_LANG_BG;
    ui_set_language(UI_LANG_BG);
    /* Rebuild the whole step to refresh all text */
    wizard_build_step();
}

static void lang_en_cb(lv_event_t *e)
{
    (void)e;
    s_selected_lang = UI_LANG_EN;
    ui_set_language(UI_LANG_EN);
    wizard_build_step();
}

/* --- PIN length buttons --- */
static void pin_len_cb(lv_event_t *e)
{
    intptr_t len = (intptr_t)lv_event_get_user_data(e);
    if (len >= PIN_MIN_LEN && len <= PIN_MAX_LEN)
    {
        s_pin_length = (uint8_t)len;
        s_pin_confirming = false;
        s_pin_done = false;
        pin_clear_entry();
        pin_update_len_btn_styles();
        pin_rebuild_dots();
        wizard_update_nav_buttons();

        if (s_pin_title)
        {
            bsp_display_lock(0);
            lv_label_set_text(s_pin_title, ui_str(STR_WIZARD_ENTER_PIN));
            bsp_display_unlock();
        }
    }
}

/* --- PIN keypad --- */
static void pin_key_cb(lv_event_t *e)
{
    intptr_t key_val = (intptr_t)lv_event_get_user_data(e);
    if (key_val >= 0 && key_val <= 9)
    {
        pin_on_digit((uint8_t)key_val);
    }
    else if (key_val == 10)
    {
        pin_on_clear();
    }
    else if (key_val == 11)
    {
        pin_on_backspace();
    }
}

/* --- WiFi step buttons --- */
static void wifi_setup_btn_cb(lv_event_t *e)
{
    (void)e;
    /* Show the embedded WiFi setup inside the wizard content area */
    if (!s_wifi_embedded)
    {
        ESP_LOGI(TAG, "Opening WiFi setup within wizard");
        s_wifi_embedded = true;
        wizard_clear_content();
        wizard_update_step_dots();

        /* Reuse the existing WiFi setup screen's create function.
         * We set the callback so we get notified when WiFi connects. */
        g_ui_state.wifi_callback = NULL;  /* We handle completion differently */
        g_ui_state.wifi_is_initial_setup = true;

        bsp_display_lock(0);
        touchscreen_wifi_setup_screen_create(wizard_wifi_result_cb, true);
        bsp_display_unlock();
    }
}

static void wifi_skip_btn_cb(lv_event_t *e)
{
    (void)e;
    wizard_finish();
}

/* ------------------------------------------------------------------ */
/* WiFi result callback implementation                                 */
/* ------------------------------------------------------------------ */
static void
wizard_wifi_result_cb(TouchScreen_WiFi_Setup_Result_t result,
                      const char *ssid, const char *password)
{
    s_wifi_embedded = false;

    if (result == TOUCHSCREEN_WIFI_SETUP_RESULT_SUCCESS && ssid && ssid[0] != '\0')
    {
        ESP_LOGI(TAG, "WiFi configured in wizard: SSID='%s'", ssid);
        s_wifi_done = true;
        strncpy(s_wifi_ssid, ssid, sizeof(s_wifi_ssid) - 1);
        s_wifi_ssid[sizeof(s_wifi_ssid) - 1] = '\0';
        if (password)
        {
            strncpy(s_wifi_pass, password, sizeof(s_wifi_pass) - 1);
            s_wifi_pass[sizeof(s_wifi_pass) - 1] = '\0';
        }
        else
        {
            s_wifi_pass[0] = '\0';
        }

        /* Auto-finish after WiFi success */
        wizard_finish();
    }
    else if (result == TOUCHSCREEN_WIFI_SETUP_RESULT_CANCEL)
    {
        ESP_LOGI(TAG, "WiFi setup cancelled in wizard — showing WiFi step choices");
        /* Go back to WiFi step choices (not back to PIN) */
        wizard_build_step();
    }
    else
    {
        ESP_LOGW(TAG, "WiFi setup failed/error in wizard");
        wizard_build_step();
    }
}

/* ------------------------------------------------------------------ */
/* PIN logic                                                           */
/* ------------------------------------------------------------------ */
static void pin_clear_entry(void)
{
    if (s_pin_confirming)
    {
        memset(s_pin_confirm, 0, sizeof(s_pin_confirm));
    }
    else
    {
        memset(s_pin_first, 0, sizeof(s_pin_first));
    }
    s_pin_entered = 0;
    pin_update_dots();
}

static void pin_update_dots(void)
{
    bsp_display_lock(0);
    for (int i = 0; i < PIN_MAX_LEN; i++)
    {
        if (!s_pin_dots[i]) continue;
        if (i < s_pin_length)
        {
            lv_obj_clear_flag(s_pin_dots[i], LV_OBJ_FLAG_HIDDEN);
            if (i < s_pin_entered)
            {
                lv_obj_set_style_bg_color(s_pin_dots[i], UI_COLOR_PRIMARY, 0);
            }
            else
            {
                lv_obj_set_style_bg_color(s_pin_dots[i], UI_COLOR_DIVIDER, 0);
            }
        }
        else
        {
            lv_obj_add_flag(s_pin_dots[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    bsp_display_unlock();
}

static void pin_on_digit(uint8_t digit)
{
    if (s_pin_done) return;
    if (s_pin_entered >= s_pin_length) return;

    char *buf = s_pin_confirming ? s_pin_confirm : s_pin_first;
    buf[s_pin_entered] = '0' + digit;
    s_pin_entered++;
    buf[s_pin_entered] = '\0';
    pin_update_dots();

    if (s_pin_entered == s_pin_length)
    {
        pin_try_complete();
    }
}

static void pin_on_backspace(void)
{
    if (s_pin_entered == 0) return;
    s_pin_entered--;
    char *buf = s_pin_confirming ? s_pin_confirm : s_pin_first;
    buf[s_pin_entered] = '\0';
    pin_update_dots();

    if (s_pin_status)
    {
        bsp_display_lock(0);
        lv_label_set_text(s_pin_status, "");
        bsp_display_unlock();
    }
}

static void pin_on_clear(void)
{
    pin_clear_entry();
    if (s_pin_status)
    {
        bsp_display_lock(0);
        lv_label_set_text(s_pin_status, "");
        bsp_display_unlock();
    }
}

static void pin_try_complete(void)
{
    if (!s_pin_confirming)
    {
        /* First entry done — switch to confirm mode */
        s_pin_confirming = true;
        s_pin_entered = 0;
        memset(s_pin_confirm, 0, sizeof(s_pin_confirm));
        pin_update_dots();

        bsp_display_lock(0);
        if (s_pin_title)
        {
            lv_label_set_text(s_pin_title, ui_str(STR_WIZARD_CONFIRM_PIN));
        }
        if (s_pin_status)
        {
            lv_label_set_text(s_pin_status, "");
        }
        bsp_display_unlock();
    }
    else
    {
        /* Confirm entry done — check match */
        if (strncmp(s_pin_first, s_pin_confirm, s_pin_length) == 0)
        {
            /* Match! Save PIN */
            esp_err_t err = TS_Pin_Set(s_pin_first);
            if (ESP_OK == err)
            {
                s_pin_done = true;
                ESP_LOGI(TAG, "PIN set successfully (%d digits)", s_pin_length);

                bsp_display_lock(0);
                if (s_pin_status)
                {
                    lv_obj_set_style_text_color(s_pin_status, UI_COLOR_SUCCESS, 0);
                    lv_label_set_text(s_pin_status, ui_str(STR_WIZARD_PIN_SET_OK));
                }
                if (s_pin_title)
                {
                    lv_label_set_text(s_pin_title, ui_str(STR_WIZARD_PIN_SET_OK));
                }
                bsp_display_unlock();

                wizard_update_nav_buttons();
            }
            else
            {
                ESP_LOGE(TAG, "Failed to save PIN: %s", esp_err_to_name(err));
            }
        }
        else
        {
            /* Mismatch — shake and reset */
            ESP_LOGW(TAG, "PIN mismatch");
            pin_shake_animation();

            bsp_display_lock(0);
            if (s_pin_status)
            {
                lv_obj_set_style_text_color(s_pin_status, UI_COLOR_DANGER, 0);
                lv_label_set_text(s_pin_status, ui_str(STR_WIZARD_PIN_MISMATCH));
            }
            bsp_display_unlock();

            /* Reset to first entry */
            s_pin_confirming = false;
            s_pin_entered = 0;
            memset(s_pin_first, 0, sizeof(s_pin_first));
            memset(s_pin_confirm, 0, sizeof(s_pin_confirm));
            pin_update_dots();

            bsp_display_lock(0);
            if (s_pin_title)
            {
                lv_label_set_text(s_pin_title, ui_str(STR_WIZARD_ENTER_PIN));
            }
            bsp_display_unlock();
        }
    }
}

/* Shake animation */
static void shake_anim_x_cb(void *obj, int32_t val)
{
    lv_obj_set_style_translate_x((lv_obj_t *)obj, val, 0);
}

static void pin_shake_animation(void)
{
    if (!s_pin_panel) return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_pin_panel);
    lv_anim_set_values(&a, -SHAKE_OFFSET, SHAKE_OFFSET);
    lv_anim_set_duration(&a, SHAKE_DURATION_MS);
    lv_anim_set_repeat_count(&a, 3);
    lv_anim_set_playback_duration(&a, SHAKE_DURATION_MS);
    lv_anim_set_exec_cb(&a, shake_anim_x_cb);
    lv_anim_start(&a);
}

static void pin_rebuild_dots(void)
{
    if (!s_pin_dot_row) return;

    bsp_display_lock(0);
    /* Remove existing dots */
    lv_obj_clean(s_pin_dot_row);
    memset(s_pin_dots, 0, sizeof(s_pin_dots));

    for (int i = 0; i < s_pin_length; i++)
    {
        s_pin_dots[i] = lv_obj_create(s_pin_dot_row);
        lv_obj_set_size(s_pin_dots[i], DOT_SIZE, DOT_SIZE);
        lv_obj_set_style_radius(s_pin_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_pin_dots[i], UI_COLOR_DIVIDER, 0);
        lv_obj_set_style_bg_opa(s_pin_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(s_pin_dots[i], UI_COLOR_PRIMARY, 0);
        lv_obj_set_style_border_width(s_pin_dots[i], 2, 0);
        lv_obj_set_scrollbar_mode(s_pin_dots[i], LV_SCROLLBAR_MODE_OFF);
    }
    bsp_display_unlock();
}

static void pin_update_len_btn_styles(void)
{
    bsp_display_lock(0);
    for (int i = 0; i < 3; i++)
    {
        if (!s_pin_len_btns[i]) continue;
        uint8_t len = PIN_MIN_LEN + i;
        if (len == s_pin_length)
        {
            lv_obj_set_style_bg_color(s_pin_len_btns[i], UI_COLOR_PRIMARY, 0);
            lv_obj_set_style_bg_opa(s_pin_len_btns[i], LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(lv_obj_get_child(s_pin_len_btns[i], 0), UI_COLOR_TEXT_ON_PRIMARY, 0);
        }
        else
        {
            lv_obj_set_style_bg_color(s_pin_len_btns[i], UI_COLOR_SURFACE, 0);
            lv_obj_set_style_bg_opa(s_pin_len_btns[i], LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(lv_obj_get_child(s_pin_len_btns[i], 0), UI_COLOR_TEXT_PRIMARY, 0);
        }
    }
    bsp_display_unlock();
}

/* ------------------------------------------------------------------ */
/* Step builders                                                       */
/* ------------------------------------------------------------------ */
static void wizard_clear_content(void)
{
    bsp_display_lock(0);
    if (s_content_area)
    {
        lv_obj_clean(s_content_area);
    }
    bsp_display_unlock();

    /* Reset pointers */
    s_pin_panel = NULL;
    s_pin_dot_row = NULL;
    s_pin_status = NULL;
    s_pin_title = NULL;
    s_lang_btn_bg = NULL;
    s_lang_btn_en = NULL;
    s_wifi_container = NULL;
    memset(s_pin_dots, 0, sizeof(s_pin_dots));
    memset(s_pin_len_btns, 0, sizeof(s_pin_len_btns));
}

static void wizard_build_step(void)
{
    wizard_clear_content();
    wizard_update_step_dots();
    wizard_update_nav_buttons();

    switch (s_step)
    {
        case 0: wizard_build_language_step(); break;
        case 1: wizard_build_pin_step(); break;
        case 2: wizard_build_wifi_step(); break;
    }
}

/* --- Step 1: Language Selection --- */
static void wizard_build_language_step(void)
{
    bsp_display_lock(0);

    /* Globe icon placeholder — use text */
    lv_obj_t *icon = lv_label_create(s_content_area);
    lv_label_set_text(icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(icon, UI_FONT_H1, 0);
    lv_obj_set_style_text_color(icon, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_pad_top(icon, 16, 0);

    /* Title — shown in BOTH languages since this is the language choice */
    lv_obj_t *title = lv_label_create(s_content_area);
    lv_label_set_text(title, ui_str(STR_WIZARD_CHOOSE_LANG_DESC));
    lv_obj_set_style_text_font(title, UI_FONT_H2, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, 400);
    lv_obj_set_style_pad_top(title, 12, 0);

    /* Button row */
    lv_obj_t *btn_row = lv_obj_create(s_content_area);
    lv_obj_set_size(btn_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 20, 0);
    lv_obj_set_style_pad_top(btn_row, 24, 0);
    lv_obj_set_scrollbar_mode(btn_row, LV_SCROLLBAR_MODE_OFF);

    /* BG button */
    s_lang_btn_bg = lv_button_create(btn_row);
    lv_obj_set_size(s_lang_btn_bg, 180, 70);
    lv_obj_set_style_radius(s_lang_btn_bg, 35, 0);  /* Pill shape */
    lv_obj_t *bg_lbl = lv_label_create(s_lang_btn_bg);
    lv_label_set_text(bg_lbl, "Български");
    lv_obj_set_style_text_font(bg_lbl, UI_FONT_H3, 0);
    lv_obj_center(bg_lbl);
    lv_obj_add_event_cb(s_lang_btn_bg, lang_bg_cb, LV_EVENT_CLICKED, NULL);

    /* EN button */
    s_lang_btn_en = lv_button_create(btn_row);
    lv_obj_set_size(s_lang_btn_en, 180, 70);
    lv_obj_set_style_radius(s_lang_btn_en, 35, 0);
    lv_obj_t *en_lbl = lv_label_create(s_lang_btn_en);
    lv_label_set_text(en_lbl, "English");
    lv_obj_set_style_text_font(en_lbl, UI_FONT_H3, 0);
    lv_obj_center(en_lbl);
    lv_obj_add_event_cb(s_lang_btn_en, lang_en_cb, LV_EVENT_CLICKED, NULL);

    /* Style active/inactive */
    if (s_selected_lang == UI_LANG_BG)
    {
        ui_theme_apply_btn_primary(s_lang_btn_bg);
        ui_theme_apply_btn_secondary(s_lang_btn_en);
    }
    else
    {
        ui_theme_apply_btn_secondary(s_lang_btn_bg);
        ui_theme_apply_btn_primary(s_lang_btn_en);
    }
    /* Re-apply pill radius after theme applies its own radius */
    lv_obj_set_style_radius(s_lang_btn_bg, 35, 0);
    lv_obj_set_style_radius(s_lang_btn_en, 35, 0);

    bsp_display_unlock();
}

/* --- Step 2: PIN Setup --- */
static void wizard_build_pin_step(void)
{
    bsp_display_lock(0);

    /* Lock icon */
    lv_obj_t *icon = lv_label_create(s_content_area);
    lv_label_set_text(icon, LV_SYMBOL_EYE_CLOSE);
    lv_obj_set_style_text_font(icon, UI_FONT_H1, 0);
    lv_obj_set_style_text_color(icon, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_pad_top(icon, 4, 0);

    /* Title (changes between "Enter PIN" and "Confirm PIN") */
    s_pin_title = lv_label_create(s_content_area);
    if (s_pin_done)
    {
        lv_label_set_text(s_pin_title, ui_str(STR_WIZARD_PIN_SET_OK));
    }
    else if (s_pin_confirming)
    {
        lv_label_set_text(s_pin_title, ui_str(STR_WIZARD_CONFIRM_PIN));
    }
    else
    {
        lv_label_set_text(s_pin_title, ui_str(STR_WIZARD_ENTER_PIN));
    }
    lv_obj_set_style_text_font(s_pin_title, UI_FONT_H3, 0);
    lv_obj_set_style_text_color(s_pin_title, UI_COLOR_TEXT_PRIMARY, 0);

    s_pin_panel = lv_obj_create(s_content_area);
    lv_obj_set_size(s_pin_panel, 430, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_pin_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_pin_panel, 0, 0);
    lv_obj_set_style_pad_all(s_pin_panel, 0, 0);
    lv_obj_set_flex_flow(s_pin_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_pin_panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_pin_panel, 6, 0);
    lv_obj_set_scrollbar_mode(s_pin_panel, LV_SCROLLBAR_MODE_OFF);

    /* PIN length selector row: [4] [5] [6] */
    lv_obj_t *len_row = lv_obj_create(s_pin_panel);
    lv_obj_set_size(len_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(len_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(len_row, 0, 0);
    lv_obj_set_style_pad_all(len_row, 0, 0);
    lv_obj_set_flex_flow(len_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(len_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(len_row, 8, 0);
    lv_obj_set_scrollbar_mode(len_row, LV_SCROLLBAR_MODE_OFF);

    /* Label before length buttons */
    lv_obj_t *len_label = lv_label_create(len_row);
    lv_label_set_text(len_label, ui_str(STR_WIZARD_PIN_LENGTH));
    lv_obj_set_style_text_font(len_label, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(len_label, UI_COLOR_TEXT_SECONDARY, 0);

    for (int i = 0; i < 3; i++)
    {
        uint8_t len = PIN_MIN_LEN + i;
        s_pin_len_btns[i] = lv_button_create(len_row);
        lv_obj_set_size(s_pin_len_btns[i], 44, 36);
        lv_obj_set_style_radius(s_pin_len_btns[i], 8, 0);
        lv_obj_set_style_border_color(s_pin_len_btns[i], UI_COLOR_PRIMARY, 0);
        lv_obj_set_style_border_width(s_pin_len_btns[i], 2, 0);
        lv_obj_set_style_pad_all(s_pin_len_btns[i], 0, 0);

        lv_obj_t *lbl = lv_label_create(s_pin_len_btns[i]);
        char txt[4];
        snprintf(txt, sizeof(txt), "%d", len);
        lv_label_set_text(lbl, txt);
        lv_obj_set_style_text_font(lbl, UI_FONT_BODY, 0);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(s_pin_len_btns[i], pin_len_cb, LV_EVENT_CLICKED, (void *)(intptr_t)len);
    }
    pin_update_len_btn_styles();

    /* Dot indicators row */
    s_pin_dot_row = lv_obj_create(s_pin_panel);
    lv_obj_set_size(s_pin_dot_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_pin_dot_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_pin_dot_row, 0, 0);
    lv_obj_set_style_pad_all(s_pin_dot_row, 4, 0);
    lv_obj_set_flex_flow(s_pin_dot_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_pin_dot_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_pin_dot_row, DOT_GAP, 0);
    lv_obj_set_scrollbar_mode(s_pin_dot_row, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < s_pin_length; i++)
    {
        s_pin_dots[i] = lv_obj_create(s_pin_dot_row);
        lv_obj_set_size(s_pin_dots[i], DOT_SIZE, DOT_SIZE);
        lv_obj_set_style_radius(s_pin_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_pin_dots[i], UI_COLOR_DIVIDER, 0);
        lv_obj_set_style_bg_opa(s_pin_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(s_pin_dots[i], UI_COLOR_PRIMARY, 0);
        lv_obj_set_style_border_width(s_pin_dots[i], 2, 0);
        lv_obj_set_scrollbar_mode(s_pin_dots[i], LV_SCROLLBAR_MODE_OFF);
    }

    /* Status label */
    s_pin_status = lv_label_create(s_pin_panel);
    lv_label_set_text(s_pin_status, "");
    lv_obj_set_style_text_font(s_pin_status, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(s_pin_status, UI_COLOR_DANGER, 0);
    lv_obj_set_style_min_height(s_pin_status, 16, 0);

    /* Numeric keypad grid */
    static const char *key_labels[KEY_ROWS][KEY_COLS] = {
        {"1", "2", "3"},
        {"4", "5", "6"},
        {"7", "8", "9"},
        {"C", "0", LV_SYMBOL_BACKSPACE},
    };
    static const int key_values[KEY_ROWS][KEY_COLS] = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9},
        {10, 0, 11},
    };

    lv_obj_t *grid = lv_obj_create(s_pin_panel);
    int grid_w = KEY_COLS * KEY_WIDTH + (KEY_COLS - 1) * KEY_GAP;
    int grid_h = KEY_ROWS * KEY_HEIGHT + (KEY_ROWS - 1) * KEY_GAP;
    lv_obj_set_size(grid, grid_w, grid_h);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_OFF);

    for (int row = 0; row < KEY_ROWS; row++)
    {
        for (int col = 0; col < KEY_COLS; col++)
        {
            lv_obj_t *btn = lv_button_create(grid);
            lv_obj_set_size(btn, KEY_WIDTH, KEY_HEIGHT);
            int x = col * (KEY_WIDTH + KEY_GAP);
            int y = row * (KEY_HEIGHT + KEY_GAP);
            lv_obj_set_pos(btn, x, y);

            int val = key_values[row][col];

            if (val == 10 || val == 11)
            {
                ui_theme_apply_btn_secondary(btn);
            }
            else
            {
                ui_theme_apply_btn_primary(btn);
            }

            lv_obj_t *label = lv_label_create(btn);
            lv_label_set_text(label, key_labels[row][col]);
            lv_obj_center(label);

            lv_obj_add_event_cb(btn, pin_key_cb, LV_EVENT_CLICKED, (void *)(intptr_t)val);
        }
    }

    bsp_display_unlock();
}

/* --- Step 3: WiFi Setup (optional) --- */
static void wizard_build_wifi_step(void)
{
    bsp_display_lock(0);

    /* WiFi icon */
    lv_obj_t *icon = lv_label_create(s_content_area);
    lv_label_set_text(icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(icon, UI_FONT_H1, 0);
    lv_obj_set_style_text_color(icon, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_pad_top(icon, 20, 0);

    /* Title */
    lv_obj_t *title = lv_label_create(s_content_area);
    lv_label_set_text(title, ui_str(STR_WIZARD_WIFI_TITLE));
    lv_obj_set_style_text_font(title, UI_FONT_H2, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_pad_top(title, 12, 0);

    /* Subtitle */
    lv_obj_t *desc = lv_label_create(s_content_area);
    lv_label_set_text(desc, ui_str(STR_WIZARD_WIFI_DESC));
    lv_obj_set_style_text_font(desc, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(desc, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_align(desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(desc, 8, 0);

    /* Button row */
    lv_obj_t *btn_row = lv_obj_create(s_content_area);
    lv_obj_set_size(btn_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn_row, 16, 0);
    lv_obj_set_style_pad_top(btn_row, 32, 0);
    lv_obj_set_scrollbar_mode(btn_row, LV_SCROLLBAR_MODE_OFF);

    /* Setup WiFi button */
    lv_obj_t *setup_btn = lv_button_create(btn_row);
    lv_obj_set_size(setup_btn, 260, 56);
    ui_theme_apply_btn_primary(setup_btn);
    lv_obj_set_style_radius(setup_btn, 28, 0);

    lv_obj_t *setup_lbl = lv_label_create(setup_btn);
    lv_label_set_text(setup_lbl, ui_str(STR_WIZARD_SETUP_WIFI));
    lv_obj_set_style_text_font(setup_lbl, UI_FONT_H3, 0);
    lv_obj_center(setup_lbl);

    lv_obj_add_event_cb(setup_btn, wifi_setup_btn_cb, LV_EVENT_CLICKED, NULL);

    /* Skip button */
    lv_obj_t *skip_btn = lv_button_create(btn_row);
    lv_obj_set_size(skip_btn, 260, 56);
    ui_theme_apply_btn_secondary(skip_btn);
    lv_obj_set_style_radius(skip_btn, 28, 0);

    lv_obj_t *skip_lbl = lv_label_create(skip_btn);
    lv_label_set_text(skip_lbl, ui_str(STR_WIZARD_SKIP_WIFI));
    lv_obj_set_style_text_font(skip_lbl, UI_FONT_H3, 0);
    lv_obj_center(skip_lbl);

    lv_obj_add_event_cb(skip_btn, wifi_skip_btn_cb, LV_EVENT_CLICKED, NULL);

    bsp_display_unlock();
}

/* ------------------------------------------------------------------ */
/* Step dot indicators                                                 */
/* ------------------------------------------------------------------ */
static void wizard_update_step_dots(void)
{
    bsp_display_lock(0);
    for (int i = 0; i < WIZARD_STEPS; i++)
    {
        if (!s_step_dots[i]) continue;
        if (i == s_step)
        {
            lv_obj_set_size(s_step_dots[i], STEP_DOT_ACTIVE_SIZE, STEP_DOT_ACTIVE_SIZE);
            lv_obj_set_style_bg_color(s_step_dots[i], UI_COLOR_PRIMARY, 0);
        }
        else if (i < s_step)
        {
            lv_obj_set_size(s_step_dots[i], STEP_DOT_SIZE, STEP_DOT_SIZE);
            lv_obj_set_style_bg_color(s_step_dots[i], UI_COLOR_PRIMARY_LIGHT, 0);
        }
        else
        {
            lv_obj_set_size(s_step_dots[i], STEP_DOT_SIZE, STEP_DOT_SIZE);
            lv_obj_set_style_bg_color(s_step_dots[i], UI_COLOR_DIVIDER, 0);
        }
    }
    bsp_display_unlock();
}

/* ------------------------------------------------------------------ */
/* Navigation buttons                                                  */
/* ------------------------------------------------------------------ */
static void wizard_update_nav_buttons(void)
{
    bsp_display_lock(0);

    /* Back button: hidden on step 0, visible on steps 1-2 */
    if (s_btn_back)
    {
        if (s_step == 0)
        {
            lv_obj_add_flag(s_btn_back, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_clear_flag(s_btn_back, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Next button logic */
    if (s_btn_next && s_next_label)
    {
        if (s_step == 0)
        {
            /* Language step — always enabled */
            lv_obj_clear_state(s_btn_next, LV_STATE_DISABLED);
            lv_label_set_text(s_next_label, ui_str(STR_WIZARD_NEXT));
            lv_obj_clear_flag(s_btn_next, LV_OBJ_FLAG_HIDDEN);
        }
        else if (s_step == 1)
        {
            /* PIN step — enabled only after PIN is confirmed */
            if (s_pin_done)
            {
                lv_obj_clear_state(s_btn_next, LV_STATE_DISABLED);
            }
            else
            {
                lv_obj_add_state(s_btn_next, LV_STATE_DISABLED);
            }
            lv_label_set_text(s_next_label, ui_str(STR_WIZARD_NEXT));
            lv_obj_clear_flag(s_btn_next, LV_OBJ_FLAG_HIDDEN);
        }
        else if (s_step == 2)
        {
            /* WiFi step — hide Next since WiFi step has its own buttons */
            lv_obj_add_flag(s_btn_next, LV_OBJ_FLAG_HIDDEN);
        }
    }

    bsp_display_unlock();
}

/* ------------------------------------------------------------------ */
/* Wizard completion                                                   */
/* ------------------------------------------------------------------ */
static void wizard_finish(void)
{
    ESP_LOGI(TAG, "Setup wizard completed (wifi=%s)", s_wifi_done ? "yes" : "no");

    TouchScreen_Setup_Wizard_Callback_t cb = g_ui_state.wizard_callback;

    if (cb)
    {
        cb(true, s_wifi_done,
           s_wifi_done ? s_wifi_ssid : NULL,
           s_wifi_done ? s_wifi_pass : NULL);
    }
}

/* ------------------------------------------------------------------ */
/* Screen create / destroy / update                                    */
/* ------------------------------------------------------------------ */
void touchscreen_setup_wizard_screen_create(void)
{
    ESP_LOGI(TAG, "Creating setup wizard screen");

    /* Reset state */
    s_step = 0;
    s_selected_lang = ui_get_language();
    s_pin_length = PIN_DEFAULT_LEN;
    s_pin_confirming = false;
    s_pin_done = false;
    s_pin_entered = 0;
    s_wifi_done = false;
    s_wifi_embedded = false;
    memset(s_pin_first, 0, sizeof(s_pin_first));
    memset(s_pin_confirm, 0, sizeof(s_pin_confirm));
    memset(s_wifi_ssid, 0, sizeof(s_wifi_ssid));
    memset(s_wifi_pass, 0, sizeof(s_wifi_pass));
    memset(s_pin_dots, 0, sizeof(s_pin_dots));
    memset(s_pin_len_btns, 0, sizeof(s_pin_len_btns));
    memset(s_step_dots, 0, sizeof(s_step_dots));

    bsp_display_lock(0);

    lv_obj_t *scr = g_ui_state.screen_obj;

    /* Main layout: column flex (top area | content | bottom bar) */
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    /* === Content area (scrollable middle) === */
    s_content_area = lv_obj_create(scr);
    lv_obj_set_size(s_content_area, 480, 480 - BOTTOM_BAR_HEIGHT);
    lv_obj_set_style_bg_opa(s_content_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_content_area, 0, 0);
    lv_obj_set_style_pad_all(s_content_area, 8, 0);
    lv_obj_set_flex_flow(s_content_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content_area, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_content_area, 4, 0);
    lv_obj_set_scrollbar_mode(s_content_area, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_grow(s_content_area, 1);

    /* === Bottom bar (step dots + nav buttons) === */
    lv_obj_t *bottom_bar = lv_obj_create(scr);
    lv_obj_set_size(bottom_bar, 480, BOTTOM_BAR_HEIGHT);
    lv_obj_set_style_bg_color(bottom_bar, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(bottom_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(bottom_bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(bottom_bar, 1, 0);
    lv_obj_set_style_border_color(bottom_bar, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_pad_left(bottom_bar, 16, 0);
    lv_obj_set_style_pad_right(bottom_bar, 16, 0);
    lv_obj_set_style_pad_top(bottom_bar, 0, 0);
    lv_obj_set_style_pad_bottom(bottom_bar, 0, 0);
    lv_obj_set_style_radius(bottom_bar, 0, 0);
    lv_obj_set_scrollbar_mode(bottom_bar, LV_SCROLLBAR_MODE_OFF);

    /* Back button (left) */
    s_btn_back = lv_button_create(bottom_bar);
    lv_obj_set_size(s_btn_back, 100, 40);
    lv_obj_align(s_btn_back, LV_ALIGN_LEFT_MID, 0, 0);
    ui_theme_apply_btn_secondary(s_btn_back);

    lv_obj_t *back_lbl = lv_label_create(s_btn_back);
    lv_label_set_text(back_lbl, ui_str(STR_BACK));
    lv_obj_set_style_text_font(back_lbl, UI_FONT_BODY_SMALL, 0);
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(s_btn_back, back_btn_cb, LV_EVENT_CLICKED, NULL);

    /* Step dots (center) */
    lv_obj_t *dot_row = lv_obj_create(bottom_bar);
    lv_obj_set_size(dot_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(dot_row);
    lv_obj_set_style_bg_opa(dot_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dot_row, 0, 0);
    lv_obj_set_style_pad_all(dot_row, 0, 0);
    lv_obj_set_flex_flow(dot_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dot_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dot_row, STEP_DOT_GAP, 0);
    lv_obj_set_scrollbar_mode(dot_row, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < WIZARD_STEPS; i++)
    {
        s_step_dots[i] = lv_obj_create(dot_row);
        lv_obj_set_size(s_step_dots[i], STEP_DOT_SIZE, STEP_DOT_SIZE);
        lv_obj_set_style_radius(s_step_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_step_dots[i], UI_COLOR_DIVIDER, 0);
        lv_obj_set_style_bg_opa(s_step_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_step_dots[i], 0, 0);
        lv_obj_set_scrollbar_mode(s_step_dots[i], LV_SCROLLBAR_MODE_OFF);
    }

    /* Next button (right) */
    s_btn_next = lv_button_create(bottom_bar);
    lv_obj_set_size(s_btn_next, 100, 40);
    lv_obj_align(s_btn_next, LV_ALIGN_RIGHT_MID, 0, 0);
    ui_theme_apply_btn_primary(s_btn_next);

    s_next_label = lv_label_create(s_btn_next);
    lv_label_set_text(s_next_label, ui_str(STR_WIZARD_NEXT));
    lv_obj_set_style_text_font(s_next_label, UI_FONT_BODY_SMALL, 0);
    lv_obj_center(s_next_label);
    lv_obj_add_event_cb(s_btn_next, next_btn_cb, LV_EVENT_CLICKED, NULL);

    bsp_display_unlock();

    /* Build initial step */
    wizard_build_step();

    ESP_LOGI(TAG, "Setup wizard screen created");
}

void touchscreen_setup_wizard_screen_destroy(void)
{
    ESP_LOGI(TAG, "Destroying setup wizard screen");

    if (s_wifi_embedded)
    {
        touchscreen_wifi_setup_screen_destroy();
        s_wifi_embedded = false;
    }

    /* All LVGL objects are children of the screen and will be auto-deleted
     * when the screen is deleted by the UI manager. Just NULL out pointers. */
    s_content_area = NULL;
    s_btn_next = NULL;
    s_btn_back = NULL;
    s_next_label = NULL;
    s_pin_panel = NULL;
    s_pin_dot_row = NULL;
    s_pin_status = NULL;
    s_pin_title = NULL;
    s_lang_btn_bg = NULL;
    s_lang_btn_en = NULL;
    s_wifi_container = NULL;
    memset(s_pin_dots, 0, sizeof(s_pin_dots));
    memset(s_pin_len_btns, 0, sizeof(s_pin_len_btns));
    memset(s_step_dots, 0, sizeof(s_step_dots));

    /* Clear sensitive data */
    memset(s_pin_first, 0, sizeof(s_pin_first));
    memset(s_pin_confirm, 0, sizeof(s_pin_confirm));
    memset(s_wifi_pass, 0, sizeof(s_wifi_pass));
}

void touchscreen_setup_wizard_screen_update(void)
{
    /* Forward update to embedded WiFi setup if active */
    if (s_wifi_embedded)
    {
        touchscreen_wifi_setup_screen_update();
    }
}
