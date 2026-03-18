/* ================================================================== */
/* settings_screen.c — Read-only device settings viewer                */
/* ================================================================== */
#include "settings_screen_internal.h"
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

static const char *TAG = "SETTINGS_SCREEN";

/* ------------------------------------------------------------------ */
/* Layout constants                                                    */
/* ------------------------------------------------------------------ */
#define CONTENT_WIDTH   (480 - 2 * UI_PAD_SCREEN)   /* 456px */

/* ------------------------------------------------------------------ */
/* Module state — LVGL objects                                         */
/* ------------------------------------------------------------------ */
static lv_obj_t *s_content        = NULL;

/* Network card */
static lv_obj_t *s_wifi_ssid_lbl  = NULL;
static lv_obj_t *s_ip_addr_lbl   = NULL;

/* Schedule card */
static lv_obj_t *s_workdays_lbl  = NULL;
static lv_obj_t *s_timezone_lbl  = NULL;

/* System card */
static lv_obj_t *s_ntp_status_lbl = NULL;

/* Language toggle */
static lv_obj_t *s_lang_btn_bg    = NULL;
static lv_obj_t *s_lang_btn_en    = NULL;

/* ------------------------------------------------------------------ */
/* Helper: create a "label + value" info row inside a card             */
/* ------------------------------------------------------------------ */
static lv_obj_t *
create_info_row(lv_obj_t *card, const char *icon, const char *title,
                lv_obj_t **out_value_label)
{
    /* Row container: icon + text column */
    lv_obj_t *row = lv_obj_create(card);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, UI_PAD_SMALL, 0);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

    /* Icon */
    lv_obj_t *ic = lv_label_create(row);
    lv_label_set_text(ic, icon);
    lv_obj_set_style_text_font(ic, UI_FONT_H3, 0);
    lv_obj_set_style_text_color(ic, UI_COLOR_PRIMARY, 0);

    /* Text column */
    lv_obj_t *col = lv_obj_create(row);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(col, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *t = lv_label_create(col);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_font(t, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(t, UI_COLOR_TEXT_SECONDARY, 0);

    lv_obj_t *v = lv_label_create(col);
    lv_label_set_text(v, "-");
    lv_obj_set_style_text_font(v, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(v, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_width(v, CONTENT_WIDTH - 60);          /* prevent overflow */
    lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);     /* ellipsis if too long */

    if (out_value_label) *out_value_label = v;
    return row;
}

/* ------------------------------------------------------------------ */
/* Helper: horizontal divider inside a card                            */
/* ------------------------------------------------------------------ */
static void create_divider(lv_obj_t *parent)
{
    lv_obj_t *div = lv_obj_create(parent);
    lv_obj_set_size(div, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(div, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_set_style_pad_all(div, 0, 0);
    lv_obj_set_scrollbar_mode(div, LV_SCROLLBAR_MODE_OFF);
}

/* ------------------------------------------------------------------ */
/* Card builders                                                       */
/* ------------------------------------------------------------------ */
static void settings_create_network_card(lv_obj_t *parent)
{
    lv_obj_t *card = card_component_create_with_title(
                         parent, CONTENT_WIDTH, LV_SIZE_CONTENT, ui_str(STR_NETWORK));
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 6, 0);

    create_info_row(card, LV_SYMBOL_WIFI,    ui_str(STR_WIFI_NETWORK), &s_wifi_ssid_lbl);
    create_divider(card);
    create_info_row(card, LV_SYMBOL_GPS,     ui_str(STR_IP_ADDRESS),   &s_ip_addr_lbl);
}

static void settings_create_schedule_card(lv_obj_t *parent)
{
    lv_obj_t *card = card_component_create_with_title(
                         parent, CONTENT_WIDTH, LV_SIZE_CONTENT, ui_str(STR_SCHEDULE));
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 6, 0);

    create_info_row(card, LV_SYMBOL_LIST,     ui_str(STR_WORKING_DAYS), &s_workdays_lbl);
    create_divider(card);
    create_info_row(card, LV_SYMBOL_SETTINGS, ui_str(STR_TIMEZONE),     &s_timezone_lbl);
}

static void lang_bg_event_cb(lv_event_t *e);
static void lang_en_event_cb(lv_event_t *e);

/** @brief Apply active/inactive styles to the two language buttons. */
static void update_lang_btn_styles(void)
{
    ui_language_t cur = ui_get_language();
    if (s_lang_btn_bg) {
        if (cur == UI_LANG_BG) {
            lv_obj_set_style_bg_color(s_lang_btn_bg, UI_COLOR_PRIMARY, 0);
            lv_obj_set_style_bg_opa(s_lang_btn_bg, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(s_lang_btn_bg, UI_COLOR_TEXT_ON_PRIMARY, 0);
        } else {
            lv_obj_set_style_bg_color(s_lang_btn_bg, UI_COLOR_SURFACE, 0);
            lv_obj_set_style_bg_opa(s_lang_btn_bg, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(s_lang_btn_bg, UI_COLOR_PRIMARY, 0);
        }
    }
    if (s_lang_btn_en) {
        if (cur == UI_LANG_EN) {
            lv_obj_set_style_bg_color(s_lang_btn_en, UI_COLOR_PRIMARY, 0);
            lv_obj_set_style_bg_opa(s_lang_btn_en, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(s_lang_btn_en, UI_COLOR_TEXT_ON_PRIMARY, 0);
        } else {
            lv_obj_set_style_bg_color(s_lang_btn_en, UI_COLOR_SURFACE, 0);
            lv_obj_set_style_bg_opa(s_lang_btn_en, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(s_lang_btn_en, UI_COLOR_PRIMARY, 0);
        }
    }
}

static void settings_create_system_card(lv_obj_t *parent)
{
    lv_obj_t *card = card_component_create_with_title(
                         parent, CONTENT_WIDTH, LV_SIZE_CONTENT, ui_str(STR_SYSTEM));
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 6, 0);

    create_info_row(card, LV_SYMBOL_REFRESH, ui_str(STR_NTP_SYNC), &s_ntp_status_lbl);
    create_divider(card);

    /* Language toggle row */
    lv_obj_t *lang_row = lv_obj_create(card);
    lv_obj_set_size(lang_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(lang_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lang_row, 0, 0);
    lv_obj_set_style_pad_all(lang_row, 0, 0);
    lv_obj_set_flex_flow(lang_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lang_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(lang_row, LV_SCROLLBAR_MODE_OFF);

    /* Icon + label column */
    lv_obj_t *lang_left = lv_obj_create(lang_row);
    lv_obj_set_size(lang_left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(lang_left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lang_left, 0, 0);
    lv_obj_set_style_pad_all(lang_left, 0, 0);
    lv_obj_set_flex_flow(lang_left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lang_left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lang_left, UI_PAD_SMALL, 0);
    lv_obj_set_scrollbar_mode(lang_left, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *lang_icon = lv_label_create(lang_left);
    lv_label_set_text(lang_icon, LV_SYMBOL_EDIT);
    lv_obj_set_style_text_font(lang_icon, UI_FONT_H3, 0);
    lv_obj_set_style_text_color(lang_icon, UI_COLOR_PRIMARY, 0);

    lv_obj_t *lang_title = lv_label_create(lang_left);
    lv_label_set_text(lang_title, ui_str(STR_LANGUAGE));
    lv_obj_set_style_text_font(lang_title, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(lang_title, UI_COLOR_TEXT_SECONDARY, 0);

    /* Two-button toggle container: [BG] [EN] */
    lv_obj_t *toggle_cont = lv_obj_create(lang_row);
    lv_obj_set_size(toggle_cont, LV_SIZE_CONTENT, 34);
    lv_obj_set_style_bg_opa(toggle_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(toggle_cont, 0, 0);
    lv_obj_set_style_pad_all(toggle_cont, 0, 0);
    lv_obj_set_flex_flow(toggle_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(toggle_cont, 0, 0);
    lv_obj_set_scrollbar_mode(toggle_cont, LV_SCROLLBAR_MODE_OFF);

    /* BG button */
    s_lang_btn_bg = lv_button_create(toggle_cont);
    lv_obj_set_size(s_lang_btn_bg, 50, 34);
    lv_obj_set_style_radius(s_lang_btn_bg, UI_BTN_RADIUS, 0);
    lv_obj_set_style_border_color(s_lang_btn_bg, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(s_lang_btn_bg, 2, 0);
    lv_obj_set_style_shadow_width(s_lang_btn_bg, 0, 0);
    lv_obj_add_event_cb(s_lang_btn_bg, lang_bg_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bg_lbl = lv_label_create(s_lang_btn_bg);
    lv_label_set_text(bg_lbl, "BG");
    lv_obj_set_style_text_font(bg_lbl, UI_FONT_BODY_SMALL, 0);
    lv_obj_center(bg_lbl);

    /* EN button */
    s_lang_btn_en = lv_button_create(toggle_cont);
    lv_obj_set_size(s_lang_btn_en, 50, 34);
    lv_obj_set_style_radius(s_lang_btn_en, UI_BTN_RADIUS, 0);
    lv_obj_set_style_border_color(s_lang_btn_en, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(s_lang_btn_en, 2, 0);
    lv_obj_set_style_shadow_width(s_lang_btn_en, 0, 0);
    lv_obj_add_event_cb(s_lang_btn_en, lang_en_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *en_lbl = lv_label_create(s_lang_btn_en);
    lv_label_set_text(en_lbl, "EN");
    lv_obj_set_style_text_font(en_lbl, UI_FONT_BODY_SMALL, 0);
    lv_obj_center(en_lbl);

    /* Apply initial styles */
    update_lang_btn_styles();
}

/* ------------------------------------------------------------------ */
/* Data refresh                                                        */
/* ------------------------------------------------------------------ */
static const ui_string_id_t s_day_name_ids[] = {
    STR_DAY_SUN, STR_DAY_MON, STR_DAY_TUE, STR_DAY_WED,
    STR_DAY_THU, STR_DAY_FRI, STR_DAY_SAT
};

static void settings_refresh_all(void)
{
    bsp_display_lock(0);

    /* --- Network --- */
    if (s_wifi_ssid_lbl) {
        char ssid[33] = {0};
        if (TS_WiFi_IsConnected() &&
            TS_WiFi_GetConnectedSsid(ssid, sizeof(ssid)) == ESP_OK && ssid[0]) {
            lv_label_set_text(s_wifi_ssid_lbl, ssid);
        } else {
            lv_label_set_text(s_wifi_ssid_lbl, ui_str(STR_NOT_CONNECTED));
        }
    }
    if (s_ip_addr_lbl) {
        char ip[16] = {0};
        if (TS_WiFi_GetIpAddress(ip, sizeof(ip)) == ESP_OK) {
            lv_label_set_text(s_ip_addr_lbl, ip);
        } else {
            lv_label_set_text(s_ip_addr_lbl, "-");
        }
    }

    /* --- Schedule settings --- */
    SCHEDULE_SETTINGS_T tSettings = {0};
    if (TS_Schedule_GetSettings(&tSettings) == ESP_OK) {
        /* Working days: build short-name list like "Mon Tue Wed Thu Fri" */
        if (s_workdays_lbl) {
            char buf[64] = {0};
            size_t pos = 0;
            for (int d = 0; d < 7; d++) {
                if (tSettings.abWorkingDays[d]) {
                    if (pos > 0 && pos < sizeof(buf) - 1) {
                        buf[pos++] = ' ';
                        buf[pos++] = ' ';
                    }
                    size_t left = sizeof(buf) - pos;
                    int n = snprintf(buf + pos, left, "%s", ui_str(s_day_name_ids[d]));
                    if (n > 0) pos += (size_t)n;
                }
            }
            if (pos == 0) {
                lv_label_set_text(s_workdays_lbl, ui_str(STR_NONE));
            } else {
                lv_label_set_text(s_workdays_lbl, buf);
            }
        }
        /* Timezone — show POSIX string as-is (concise enough) */
        if (s_timezone_lbl) {
            if (tSettings.acTimezone[0]) {
                lv_label_set_text(s_timezone_lbl, tSettings.acTimezone);
            } else {
                lv_label_set_text(s_timezone_lbl, "UTC");
            }
        }
    }

    /* --- NTP sync --- */
    if (s_ntp_status_lbl) {
        SCHEDULER_STATUS_T tStatus = {0};
        if (TS_Schedule_GetStatus(&tStatus) == ESP_OK) {
            if (tStatus.bTimeSynced) {
                if (tStatus.ulLastSyncAgeSec < 60) {
                    lv_label_set_text(s_ntp_status_lbl, ui_str(STR_SYNCED_JUST_NOW));
                } else if (tStatus.ulLastSyncAgeSec < 3600) {
                    char buf[48];
                    snprintf(buf, sizeof(buf), "%s (%lum)",
                             ui_str(STR_NTP_SYNC), (unsigned long)(tStatus.ulLastSyncAgeSec / 60));
                    lv_label_set_text(s_ntp_status_lbl, buf);
                } else {
                    char buf[48];
                    snprintf(buf, sizeof(buf), "%s (%luh)",
                             ui_str(STR_NTP_SYNC), (unsigned long)(tStatus.ulLastSyncAgeSec / 3600));
                    lv_label_set_text(s_ntp_status_lbl, buf);
                }
            } else {
                lv_label_set_text(s_ntp_status_lbl, ui_str(STR_NOT_SYNCED));
            }
        } else {
            lv_label_set_text(s_ntp_status_lbl, ui_str(STR_UNKNOWN));
        }
    }

    bsp_display_unlock();
}

/* ------------------------------------------------------------------ */
/* Language button handlers                                            */
/* ------------------------------------------------------------------ */
static void lang_bg_event_cb(lv_event_t *e)
{
    (void)e;
    if (ui_get_language() == UI_LANG_BG) return;  /* Already BG */
    ui_set_language(UI_LANG_BG);
    ESP_LOGI(TAG, "Language changed to BG, rebuilding screen");
    TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_SETTINGS);
}

static void lang_en_event_cb(lv_event_t *e)
{
    (void)e;
    if (ui_get_language() == UI_LANG_EN) return;  /* Already EN */
    ui_set_language(UI_LANG_EN);
    ESP_LOGI(TAG, "Language changed to EN, rebuilding screen");
    TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_SETTINGS);
}

/* ------------------------------------------------------------------ */
/* Screen lifecycle                                                    */
/* ------------------------------------------------------------------ */
void touchscreen_settings_screen_create(void)
{
    ESP_LOGI(TAG, "Creating settings screen");

    lv_obj_t *scr = g_ui_state.screen_obj;

    bsp_display_lock(0);

    /* Main scrollable content area between status bar and navbar */
    s_content = lv_obj_create(scr);
    lv_obj_set_size(s_content, 480, UI_CONTENT_HEIGHT);
    lv_obj_set_pos(s_content, 0, UI_STATUSBAR_HEIGHT);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, UI_PAD_SMALL, 0);
    lv_obj_set_style_pad_row(s_content, 6, 0);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(s_content, LV_SCROLLBAR_MODE_AUTO);

    /* Build cards */
    settings_create_network_card(s_content);
    settings_create_schedule_card(s_content);
    settings_create_system_card(s_content);

    bsp_display_unlock();

    /* Initial data fill */
    settings_refresh_all();

    ESP_LOGI(TAG, "Settings screen created");
}

void touchscreen_settings_screen_destroy(void)
{
    ESP_LOGI(TAG, "Destroying settings screen");

    s_content        = NULL;
    s_wifi_ssid_lbl  = NULL;
    s_ip_addr_lbl    = NULL;
    s_workdays_lbl   = NULL;
    s_timezone_lbl   = NULL;
    s_ntp_status_lbl = NULL;
    s_lang_btn_bg    = NULL;
    s_lang_btn_en    = NULL;
}

void touchscreen_settings_screen_update(void)
{
    if (!s_content) return;
    settings_refresh_all();
}

/* === Convenience: navigate here via UI manager === */
void settings_screen_show(void)
{
    TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_SETTINGS);
}
