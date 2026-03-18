#include "ui_theme.h"
#include "esp_log.h"

static const char *TAG = "UI_THEME";

void ui_theme_init(void)
{
    ESP_LOGI(TAG, "Initializing Material Design theme");

    /* Set up font fallback chain: custom Cyrillic fonts -> built-in LVGL
     * Montserrat fonts (same size) that include FontAwesome symbol glyphs.
     * This lets LV_SYMBOL_* render correctly from the fallback font while
     * Cyrillic text renders from the primary custom font. */
    font_montserrat_cyrillic_12.fallback = &lv_font_montserrat_12;
    font_montserrat_cyrillic_14.fallback = &lv_font_montserrat_14;
    font_montserrat_cyrillic_16.fallback = &lv_font_montserrat_16;
    font_montserrat_cyrillic_18.fallback = &lv_font_montserrat_18;
    font_montserrat_cyrillic_20.fallback = &lv_font_montserrat_20;
    font_montserrat_cyrillic_24.fallback = &lv_font_montserrat_24;
    font_montserrat_cyrillic_28.fallback = &lv_font_montserrat_28;

    lv_display_t *disp = lv_display_get_default();
    if (!disp) {
        ESP_LOGE(TAG, "No display found");
        return;
    }

    /* Set default font for the display */
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    if (scr) {
        lv_obj_set_style_text_font(scr, UI_FONT_BODY, 0);
        lv_obj_set_style_text_color(scr, UI_COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_bg_color(scr, UI_COLOR_BACKGROUND, 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    }

    ESP_LOGI(TAG, "Theme initialized");
}

void ui_theme_apply_card(lv_obj_t *obj)
{
    if (!obj) return;

    lv_obj_set_style_bg_color(obj, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, UI_CARD_RADIUS, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, UI_CARD_PAD, 0);

    /* Material elevation / shadow */
    lv_obj_set_style_shadow_width(obj, UI_CARD_SHADOW_WIDTH, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(obj, UI_CARD_SHADOW_OPA, 0);
    lv_obj_set_style_shadow_offset_y(obj, UI_CARD_SHADOW_OFS_Y, 0);

    /* Remove scrollbar */
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

void ui_theme_apply_btn_primary(lv_obj_t *btn)
{
    if (!btn) return;

    lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY_DARK, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, UI_BTN_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, UI_COLOR_TEXT_ON_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(btn, UI_FONT_BODY, LV_PART_MAIN);
}

void ui_theme_apply_btn_danger(lv_obj_t *btn)
{
    if (!btn) return;

    lv_obj_set_style_bg_color(btn, UI_COLOR_DANGER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, UI_COLOR_DANGER_DARK, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, UI_BTN_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, UI_COLOR_TEXT_ON_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(btn, UI_FONT_BODY, LV_PART_MAIN);
}

void ui_theme_apply_btn_secondary(lv_obj_t *btn)
{
    if (!btn) return;

    lv_obj_set_style_bg_color(btn, UI_COLOR_SURFACE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, UI_COLOR_BACKGROUND, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, UI_COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, UI_BTN_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, UI_COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(btn, UI_FONT_BODY, LV_PART_MAIN);
}

void ui_theme_apply_fab(lv_obj_t *btn, lv_color_t bg_color)
{
    if (!btn) return;

    lv_obj_set_size(btn, UI_FAB_SIZE, UI_FAB_SIZE);
    lv_obj_set_style_bg_color(btn, bg_color, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, UI_FAB_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);

    /* Elevated shadow for FAB */
    lv_obj_set_style_shadow_width(btn, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(btn, 4, LV_PART_MAIN);

    lv_obj_set_style_text_color(btn, UI_COLOR_TEXT_ON_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(btn, UI_FONT_BODY_SMALL, LV_PART_MAIN);
}
