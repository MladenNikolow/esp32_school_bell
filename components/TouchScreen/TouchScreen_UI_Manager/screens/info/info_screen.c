/* ================================================================== */
/* info_screen.c — Device info, version, QR code for web access        */
/* ================================================================== */
#include "info_screen_internal.h"
#include "../../src/ui_manager_internal.h"
#include "../../src/ui_theme.h"
#include "../../src/ui_strings.h"
#include "../../components/card/card_component.h"
#include "TouchScreen_Services.h"
#include "TouchScreen_UI_Manager.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_mac.h"
#include "esp_app_desc.h"
#include "esp_idf_version.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "INFO_SCREEN";

/* ------------------------------------------------------------------ */
/* Layout constants                                                    */
/* ------------------------------------------------------------------ */
#define CONTENT_WIDTH   (480 - 2 * UI_PAD_SCREEN)   /* 456px */
#define QR_CODE_SIZE    150

/* ------------------------------------------------------------------ */
/* Module state — LVGL objects                                         */
/* ------------------------------------------------------------------ */
static lv_obj_t *s_content         = NULL;

/* Device card */
static lv_obj_t *s_fw_version_lbl  = NULL;
static lv_obj_t *s_idf_version_lbl = NULL;
static lv_obj_t *s_chip_info_lbl   = NULL;
static lv_obj_t *s_mac_addr_lbl    = NULL;

/* Web access card */
static lv_obj_t *s_qr_code         = NULL;
static lv_obj_t *s_web_url_lbl     = NULL;

/* Track last IP to avoid unnecessary QR regeneration */
static char s_last_ip[16] = {0};

/* ------------------------------------------------------------------ */
/* Helper: create a "label + value" info row inside a card             */
/* ------------------------------------------------------------------ */
static lv_obj_t *
create_info_row(lv_obj_t *card, const char *icon, const char *title,
                lv_obj_t **out_value_label)
{
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

    lv_obj_t *ic = lv_label_create(row);
    lv_label_set_text(ic, icon);
    lv_obj_set_style_text_font(ic, UI_FONT_H3, 0);
    lv_obj_set_style_text_color(ic, UI_COLOR_PRIMARY, 0);

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
    lv_obj_set_width(v, CONTENT_WIDTH - 60);
    lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);

    if (out_value_label) *out_value_label = v;
    return row;
}

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
static void info_create_device_card(lv_obj_t *parent)
{
    lv_obj_t *card = card_component_create_with_title(
                         parent, CONTENT_WIDTH, LV_SIZE_CONTENT, ui_str(STR_DEVICE));
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 6, 0);

    create_info_row(card, LV_SYMBOL_HOME,     ui_str(STR_FIRMWARE_VERSION), &s_fw_version_lbl);
    create_divider(card);
    create_info_row(card, LV_SYMBOL_SETTINGS, ui_str(STR_IDF_VERSION),  &s_idf_version_lbl);
    create_divider(card);
    create_info_row(card, LV_SYMBOL_CHARGE,   ui_str(STR_CHIP),             &s_chip_info_lbl);
    create_divider(card);
    create_info_row(card, LV_SYMBOL_GPS,      ui_str(STR_MAC_ADDRESS),      &s_mac_addr_lbl);
}

static void info_create_web_access_card(lv_obj_t *parent)
{
    lv_obj_t *card = card_component_create_with_title(
                         parent, CONTENT_WIDTH, LV_SIZE_CONTENT, ui_str(STR_WEB_INTERFACE));
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    /* Instruction label */
    lv_obj_t *hint = lv_label_create(card);
    lv_label_set_text(hint, ui_str(STR_SCAN_QR));
    lv_obj_set_style_text_font(hint, UI_FONT_CAPTION, 0);
    lv_obj_set_style_text_color(hint, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_width(hint, LV_PCT(100));
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

#if LV_USE_QRCODE
    /* QR code widget */
    s_qr_code = lv_qrcode_create(card);
    lv_qrcode_set_size(s_qr_code, QR_CODE_SIZE);
    lv_qrcode_set_dark_color(s_qr_code, lv_color_hex(0x000000));
    lv_qrcode_set_light_color(s_qr_code, lv_color_hex(0xFFFFFF));
    lv_obj_set_style_border_width(s_qr_code, 4, 0);
    lv_obj_set_style_border_color(s_qr_code, lv_color_hex(0xFFFFFF), 0);
#endif

    /* URL label below QR */
    s_web_url_lbl = lv_label_create(card);
    lv_label_set_text(s_web_url_lbl, ui_str(STR_NOT_CONNECTED));
    lv_obj_set_style_text_font(s_web_url_lbl, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(s_web_url_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_width(s_web_url_lbl, LV_PCT(100));
    lv_obj_set_style_text_align(s_web_url_lbl, LV_TEXT_ALIGN_CENTER, 0);
}



/* ------------------------------------------------------------------ */
/* Data refresh                                                        */
/* ------------------------------------------------------------------ */
static void info_refresh_all(void)
{
    bsp_display_lock(0);

    /* Firmware version */
    if (s_fw_version_lbl) {
        const esp_app_desc_t *app = esp_app_get_description();
        if (app && app->version[0]) {
            lv_label_set_text(s_fw_version_lbl, app->version);
        } else {
            lv_label_set_text(s_fw_version_lbl, "N/A");
        }
    }

    /* IDF version */
    if (s_idf_version_lbl) {
        lv_label_set_text(s_idf_version_lbl, esp_get_idf_version());
    }

    /* Chip info */
    if (s_chip_info_lbl) {
        esp_chip_info_t chip;
        esp_chip_info(&chip);
        char buf[48];
        snprintf(buf, sizeof(buf), "ESP32-S3, %d core(s), rev %d.%d",
                 chip.cores, chip.revision / 100, chip.revision % 100);
        lv_label_set_text(s_chip_info_lbl, buf);
    }

    /* MAC address */
    if (s_mac_addr_lbl) {
        uint8_t mac[6];
        if (esp_efuse_mac_get_default(mac) == ESP_OK) {
            char buf[24];
            snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            lv_label_set_text(s_mac_addr_lbl, buf);
        } else {
            lv_label_set_text(s_mac_addr_lbl, ui_str(STR_UNKNOWN));
        }
    }

    /* Web access: QR code + URL */
    char ip[16] = {0};
    bool has_ip = (TS_WiFi_GetIpAddress(ip, sizeof(ip)) == ESP_OK && ip[0]);

    if (s_web_url_lbl) {
        if (has_ip) {
            char url[40];
            snprintf(url, sizeof(url), "http://%s", ip);
            lv_label_set_text(s_web_url_lbl, url);
        } else {
            lv_label_set_text(s_web_url_lbl, ui_str(STR_WIFI_NOT_CONNECTED));
        }
    }

#if LV_USE_QRCODE
    if (s_qr_code) {
        if (has_ip && strcmp(ip, s_last_ip) != 0) {
            /* IP changed — regenerate QR code */
            char url[40];
            snprintf(url, sizeof(url), "http://%s", ip);
            lv_qrcode_update(s_qr_code, url, strlen(url));
            strncpy(s_last_ip, ip, sizeof(s_last_ip) - 1);
            s_last_ip[sizeof(s_last_ip) - 1] = '\0';
            lv_obj_clear_flag(s_qr_code, LV_OBJ_FLAG_HIDDEN);
        } else if (!has_ip) {
            lv_obj_add_flag(s_qr_code, LV_OBJ_FLAG_HIDDEN);
            s_last_ip[0] = '\0';
        }
    }
#endif

    bsp_display_unlock();
}

/* ------------------------------------------------------------------ */
/* Screen lifecycle                                                    */
/* ------------------------------------------------------------------ */
void touchscreen_info_screen_create(void)
{
    ESP_LOGI(TAG, "Creating info screen");

    lv_obj_t *scr = g_ui_state.screen_obj;

    bsp_display_lock(0);

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

    info_create_device_card(s_content);
    info_create_web_access_card(s_content);

    bsp_display_unlock();

    s_last_ip[0] = '\0';
    info_refresh_all();

    ESP_LOGI(TAG, "Info screen created");
}

void touchscreen_info_screen_destroy(void)
{
    ESP_LOGI(TAG, "Destroying info screen");

    s_content         = NULL;
    s_fw_version_lbl  = NULL;
    s_idf_version_lbl = NULL;
    s_chip_info_lbl   = NULL;
    s_mac_addr_lbl    = NULL;
    s_qr_code         = NULL;
    s_web_url_lbl     = NULL;
    s_last_ip[0]      = '\0';
}

void touchscreen_info_screen_update(void)
{
    if (!s_content) return;
    info_refresh_all();
}
