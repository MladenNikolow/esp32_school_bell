#include "ui_navbar.h"
#include "ui_theme.h"
#include "ui_config.h"
#include "esp_log.h"

static const char *TAG = "UI_NAVBAR";

#define NAV_TAB_COUNT   4

static lv_obj_t *s_navbar = NULL;
static lv_obj_t *s_dashboard_btn = NULL;
static lv_obj_t *s_schedule_btn = NULL;
static lv_obj_t *s_settings_btn = NULL;
static lv_obj_t *s_info_btn = NULL;
static lv_obj_t *s_dashboard_icon = NULL;
static lv_obj_t *s_dashboard_label = NULL;
static lv_obj_t *s_schedule_icon = NULL;
static lv_obj_t *s_schedule_label = NULL;
static lv_obj_t *s_settings_icon = NULL;
static lv_obj_t *s_settings_label = NULL;
static lv_obj_t *s_info_icon = NULL;
static lv_obj_t *s_info_label = NULL;
static lv_obj_t *s_indicator = NULL;
static ui_navbar_tab_cb_t s_tab_cb = NULL;
static uint8_t s_active_tab = UI_NAV_TAB_DASHBOARD;

static void nav_btn_event_cb(lv_event_t *e)
{
    uint8_t tab_index = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (tab_index != s_active_tab) {
        ui_navbar_set_active(tab_index);
        if (s_tab_cb) {
            s_tab_cb(tab_index);
        }
    }
}

static lv_obj_t *create_nav_item(lv_obj_t *parent, const char *icon, const char *label_text,
                                  uint8_t tab_index, lv_obj_t **out_icon, lv_obj_t **out_label)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, TOUCHSCREEN_UI_SCREEN_WIDTH / NAV_TAB_COUNT, UI_NAVBAR_HEIGHT);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_scrollbar_mode(btn, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn, 2, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *ic = lv_label_create(btn);
    lv_label_set_text(ic, icon);
    lv_obj_set_style_text_font(ic, UI_FONT_H3, 0);
    lv_obj_set_style_text_color(ic, UI_COLOR_TEXT_SECONDARY, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_font(lbl, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_SECONDARY, 0);

    lv_obj_add_event_cb(btn, nav_btn_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)tab_index);

    *out_icon = ic;
    *out_label = lbl;
    return btn;
}

lv_obj_t *ui_navbar_create(lv_obj_t *parent, ui_navbar_tab_cb_t cb)
{
    if (!parent) return NULL;
    s_tab_cb = cb;

    /* Navbar container at bottom */
    s_navbar = lv_obj_create(parent);
    lv_obj_set_size(s_navbar, TOUCHSCREEN_UI_SCREEN_WIDTH, UI_NAVBAR_HEIGHT);
    lv_obj_align(s_navbar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(s_navbar, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(s_navbar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(s_navbar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(s_navbar, 1, 0);
    lv_obj_set_style_border_color(s_navbar, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_radius(s_navbar, 0, 0);
    lv_obj_set_style_pad_all(s_navbar, 0, 0);
    lv_obj_set_scrollbar_mode(s_navbar, LV_SCROLLBAR_MODE_OFF);

    /* Elevation shadow above navbar */
    lv_obj_set_style_shadow_width(s_navbar, 8, 0);
    lv_obj_set_style_shadow_color(s_navbar, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(s_navbar, LV_OPA_10, 0);
    lv_obj_set_style_shadow_offset_y(s_navbar, -2, 0);

    lv_obj_set_flex_flow(s_navbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_navbar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Dashboard tab */
    s_dashboard_btn = create_nav_item(s_navbar, LV_SYMBOL_HOME, "Dashboard",
                                       UI_NAV_TAB_DASHBOARD, &s_dashboard_icon, &s_dashboard_label);

    /* Schedule tab */
    s_schedule_btn = create_nav_item(s_navbar, LV_SYMBOL_LIST, "Schedule",
                                      UI_NAV_TAB_SCHEDULE, &s_schedule_icon, &s_schedule_label);

    /* Settings tab */
    s_settings_btn = create_nav_item(s_navbar, LV_SYMBOL_SETTINGS, "Settings",
                                      UI_NAV_TAB_SETTINGS, &s_settings_icon, &s_settings_label);

    /* Info tab */
    s_info_btn = create_nav_item(s_navbar, LV_SYMBOL_EYE_OPEN, "Info",
                                  UI_NAV_TAB_INFO, &s_info_icon, &s_info_label);

    /* Active indicator line (3px bar under active tab) — floating to avoid
       disrupting the flex row layout of the nav buttons */
    s_indicator = lv_obj_create(s_navbar);
    lv_obj_set_size(s_indicator, TOUCHSCREEN_UI_SCREEN_WIDTH / NAV_TAB_COUNT - 30, 3);
    lv_obj_set_style_bg_color(s_indicator, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(s_indicator, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_indicator, 0, 0);
    lv_obj_set_style_radius(s_indicator, 2, 0);
    lv_obj_add_flag(s_indicator, LV_OBJ_FLAG_FLOATING);

    /* Set initial active tab */
    ui_navbar_set_active(UI_NAV_TAB_DASHBOARD);

    ESP_LOGI(TAG, "Navbar created");
    return s_navbar;
}

void ui_navbar_destroy(void)
{
    s_navbar = NULL;
    s_dashboard_btn = NULL;
    s_schedule_btn = NULL;
    s_settings_btn = NULL;
    s_info_btn = NULL;
    s_dashboard_icon = NULL;
    s_dashboard_label = NULL;
    s_schedule_icon = NULL;
    s_schedule_label = NULL;
    s_settings_icon = NULL;
    s_settings_label = NULL;
    s_info_icon = NULL;
    s_info_label = NULL;
    s_indicator = NULL;
    s_tab_cb = NULL;
    s_active_tab = UI_NAV_TAB_DASHBOARD;
}

void ui_navbar_set_active(uint8_t tab_index)
{
    s_active_tab = tab_index;

    lv_color_t active_color = UI_COLOR_PRIMARY;
    lv_color_t inactive_color = UI_COLOR_TEXT_SECONDARY;

    /* Dashboard tab styling */
    if (s_dashboard_icon) {
        lv_obj_set_style_text_color(s_dashboard_icon,
            (tab_index == UI_NAV_TAB_DASHBOARD) ? active_color : inactive_color, 0);
    }
    if (s_dashboard_label) {
        lv_obj_set_style_text_color(s_dashboard_label,
            (tab_index == UI_NAV_TAB_DASHBOARD) ? active_color : inactive_color, 0);
    }

    /* Schedule tab styling */
    if (s_schedule_icon) {
        lv_obj_set_style_text_color(s_schedule_icon,
            (tab_index == UI_NAV_TAB_SCHEDULE) ? active_color : inactive_color, 0);
    }
    if (s_schedule_label) {
        lv_obj_set_style_text_color(s_schedule_label,
            (tab_index == UI_NAV_TAB_SCHEDULE) ? active_color : inactive_color, 0);
    }

    /* Settings tab styling */
    if (s_settings_icon) {
        lv_obj_set_style_text_color(s_settings_icon,
            (tab_index == UI_NAV_TAB_SETTINGS) ? active_color : inactive_color, 0);
    }
    if (s_settings_label) {
        lv_obj_set_style_text_color(s_settings_label,
            (tab_index == UI_NAV_TAB_SETTINGS) ? active_color : inactive_color, 0);
    }

    /* Info tab styling */
    if (s_info_icon) {
        lv_obj_set_style_text_color(s_info_icon,
            (tab_index == UI_NAV_TAB_INFO) ? active_color : inactive_color, 0);
    }
    if (s_info_label) {
        lv_obj_set_style_text_color(s_info_label,
            (tab_index == UI_NAV_TAB_INFO) ? active_color : inactive_color, 0);
    }

    /* Move indicator under active tab — 4 tabs divide the 480px bar into 120px slots.
       Center of each slot: 60, 180, 300, 420.  Center of bar: 240.
       Offsets from center: -180, -60, +60, +180. */
    if (s_indicator) {
        int32_t tab_w   = TOUCHSCREEN_UI_SCREEN_WIDTH / NAV_TAB_COUNT;  /* 120 */
        int32_t center  = TOUCHSCREEN_UI_SCREEN_WIDTH / 2;   /* 240 */
        int32_t tab_ctr = tab_w / 2 + tab_index * tab_w;     /* 80 / 240 / 400 */
        int32_t x_ofs   = tab_ctr - center;
        lv_obj_align(s_indicator, LV_ALIGN_BOTTOM_MID, x_ofs, -2);
    }
}
