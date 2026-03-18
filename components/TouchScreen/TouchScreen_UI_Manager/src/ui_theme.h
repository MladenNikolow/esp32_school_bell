#ifndef TOUCHSCREEN_UI_THEME_H
#define TOUCHSCREEN_UI_THEME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* =========================================================================
 * Material Design Color Palette
 * ========================================================================= */

/* Primary */
#define UI_COLOR_PRIMARY            lv_color_hex(0x2196F3)  /* Material Blue 500 */
#define UI_COLOR_PRIMARY_DARK       lv_color_hex(0x1976D2)  /* Material Blue 700 */
#define UI_COLOR_PRIMARY_LIGHT      lv_color_hex(0xBBDEFB)  /* Material Blue 100 */

/* Accent / Danger */
#define UI_COLOR_ACCENT             lv_color_hex(0xFF5722)  /* Deep Orange */
#define UI_COLOR_DANGER             lv_color_hex(0xF44336)  /* Red 500 */
#define UI_COLOR_DANGER_DARK        lv_color_hex(0xD32F2F)  /* Red 700 */

/* Success / Warning */
#define UI_COLOR_SUCCESS            lv_color_hex(0x4CAF50)  /* Green 500 */
#define UI_COLOR_WARNING            lv_color_hex(0xFF9800)  /* Orange 500 */

/* Surfaces */
#define UI_COLOR_SURFACE            lv_color_hex(0xFFFFFF)  /* White */
#define UI_COLOR_BACKGROUND         lv_color_hex(0xF5F5F5)  /* Grey 100 */
#define UI_COLOR_CARD               lv_color_hex(0xFFFFFF)  /* White */

/* Text */
#define UI_COLOR_TEXT_PRIMARY       lv_color_hex(0x212121)  /* Grey 900 */
#define UI_COLOR_TEXT_SECONDARY     lv_color_hex(0x757575)  /* Grey 600 */
#define UI_COLOR_TEXT_DISABLED      lv_color_hex(0x9E9E9E)  /* Grey 500 */
#define UI_COLOR_TEXT_ON_PRIMARY    lv_color_hex(0xFFFFFF)  /* White */

/* Dividers & Borders */
#define UI_COLOR_DIVIDER            lv_color_hex(0xE0E0E0)  /* Grey 300 */
#define UI_COLOR_BORDER             lv_color_hex(0xBDBDBD)  /* Grey 400 */

/* =========================================================================
 * Typography — Custom Montserrat with Latin + Cyrillic (BG/EN support)
 * ========================================================================= */
extern lv_font_t font_montserrat_cyrillic_28;
extern lv_font_t font_montserrat_cyrillic_24;
extern lv_font_t font_montserrat_cyrillic_20;
extern lv_font_t font_montserrat_cyrillic_18;
extern lv_font_t font_montserrat_cyrillic_16;
extern lv_font_t font_montserrat_cyrillic_14;
extern lv_font_t font_montserrat_cyrillic_12;

#define UI_FONT_H1                  (&font_montserrat_cyrillic_28)
#define UI_FONT_H2                  (&font_montserrat_cyrillic_24)
#define UI_FONT_H3                  (&font_montserrat_cyrillic_20)
#define UI_FONT_BODY                (&font_montserrat_cyrillic_18)
#define UI_FONT_BODY_SMALL          (&font_montserrat_cyrillic_16)
#define UI_FONT_CAPTION             (&font_montserrat_cyrillic_14)
#define UI_FONT_SMALL               (&font_montserrat_cyrillic_12)

/* =========================================================================
 * Layout Constants
 * ========================================================================= */

#define UI_STATUSBAR_HEIGHT         40
#define UI_NAVBAR_HEIGHT            56
#define UI_CONTENT_Y_START          UI_STATUSBAR_HEIGHT
#define UI_CONTENT_HEIGHT           (480 - UI_STATUSBAR_HEIGHT - UI_NAVBAR_HEIGHT)  /* 384px */

#define UI_CARD_RADIUS              12
#define UI_CARD_PAD                 16
#define UI_CARD_SHADOW_WIDTH        8
#define UI_CARD_SHADOW_OFS_Y        2
#define UI_CARD_SHADOW_OPA          LV_OPA_20

#define UI_BTN_RADIUS               8
#define UI_FAB_RADIUS               32
#define UI_FAB_SIZE                 64

#define UI_PAD_SCREEN               12
#define UI_PAD_SMALL                8
#define UI_PAD_MEDIUM               16
#define UI_PAD_LARGE                24

/* =========================================================================
 * Theme API
 * ========================================================================= */

/**
 * @brief Initialize the Material Design theme and apply to active display
 */
void ui_theme_init(void);

/**
 * @brief Apply card style to an object (rounded corners, shadow, white bg)
 */
void ui_theme_apply_card(lv_obj_t *obj);

/**
 * @brief Apply primary button style
 */
void ui_theme_apply_btn_primary(lv_obj_t *btn);

/**
 * @brief Apply danger/red button style
 */
void ui_theme_apply_btn_danger(lv_obj_t *btn);

/**
 * @brief Apply secondary/outline button style
 */
void ui_theme_apply_btn_secondary(lv_obj_t *btn);

/**
 * @brief Apply FAB (floating action button) style - circular, elevated
 */
void ui_theme_apply_fab(lv_obj_t *btn, lv_color_t bg_color);

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_UI_THEME_H */
