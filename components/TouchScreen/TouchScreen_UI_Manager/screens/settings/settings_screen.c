/* ================================================================== */
/* settings_screen.c — Read-only device settings viewer                */
/* ================================================================== */
#include "settings_screen_internal.h"
#include "../../src/ui_manager_internal.h"
#include "../../src/ui_theme.h"
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
                         parent, CONTENT_WIDTH, LV_SIZE_CONTENT, "Network");
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 6, 0);

    create_info_row(card, LV_SYMBOL_WIFI,    "WiFi Network", &s_wifi_ssid_lbl);
    create_divider(card);
    create_info_row(card, LV_SYMBOL_GPS,     "IP Address",   &s_ip_addr_lbl);
}

static void settings_create_schedule_card(lv_obj_t *parent)
{
    lv_obj_t *card = card_component_create_with_title(
                         parent, CONTENT_WIDTH, LV_SIZE_CONTENT, "Schedule");
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 6, 0);

    create_info_row(card, LV_SYMBOL_LIST,     "Working Days", &s_workdays_lbl);
    create_divider(card);
    create_info_row(card, LV_SYMBOL_SETTINGS, "Timezone",     &s_timezone_lbl);
}

static void settings_create_system_card(lv_obj_t *parent)
{
    lv_obj_t *card = card_component_create_with_title(
                         parent, CONTENT_WIDTH, LV_SIZE_CONTENT, "System");
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 6, 0);

    create_info_row(card, LV_SYMBOL_REFRESH, "NTP Sync", &s_ntp_status_lbl);
}

/* ------------------------------------------------------------------ */
/* Data refresh                                                        */
/* ------------------------------------------------------------------ */
static const char * const s_day_names[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
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
            lv_label_set_text(s_wifi_ssid_lbl, "Not connected");
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
                    int n = snprintf(buf + pos, left, "%s", s_day_names[d]);
                    if (n > 0) pos += (size_t)n;
                }
            }
            if (pos == 0) {
                lv_label_set_text(s_workdays_lbl, "None");
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
                    lv_label_set_text(s_ntp_status_lbl, "Synced (just now)");
                } else if (tStatus.ulLastSyncAgeSec < 3600) {
                    char buf[40];
                    snprintf(buf, sizeof(buf), "Synced (%lum ago)",
                             (unsigned long)(tStatus.ulLastSyncAgeSec / 60));
                    lv_label_set_text(s_ntp_status_lbl, buf);
                } else {
                    char buf[40];
                    snprintf(buf, sizeof(buf), "Synced (%luh ago)",
                             (unsigned long)(tStatus.ulLastSyncAgeSec / 3600));
                    lv_label_set_text(s_ntp_status_lbl, buf);
                }
            } else {
                lv_label_set_text(s_ntp_status_lbl, "Not synced");
            }
        } else {
            lv_label_set_text(s_ntp_status_lbl, "Unknown");
        }
    }

    bsp_display_unlock();
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
