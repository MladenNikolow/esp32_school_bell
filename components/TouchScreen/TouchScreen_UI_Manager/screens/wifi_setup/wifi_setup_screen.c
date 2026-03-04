#include "wifi_setup_screen_internal.h"
#include "../../components/input_field/input_field_component.h"
#include "../../components/button/button_component.h"
#include "TouchScreen_UI_Types.h"
#include "lvgl.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "WIFI_SETUP_SCREEN";

/* Screen objects */
static lv_obj_t *wifi_setup_screen = NULL;
static lv_obj_t *title_label = NULL;
static lv_obj_t *ssid_input = NULL;
static lv_obj_t *password_input = NULL;
static lv_obj_t *connect_btn = NULL;
static lv_obj_t *skip_btn = NULL;

/* Callback reference */
static TouchScreen_WiFi_Setup_Callback_t screen_callback = NULL;

/**
 * @brief Connect button event handler
 */
static void connect_btn_event_handler(lv_event_t *e)
{
    ESP_LOGI(TAG, "Connect button pressed");

    if (screen_callback) {
        const char *ssid = touchscreen_wifi_setup_screen_get_ssid();
        const char *password = touchscreen_wifi_setup_screen_get_password();

        if (ssid && password && ssid[0] != '\0') {
            screen_callback(TOUCHSCREEN_WIFI_SETUP_RESULT_SUCCESS, ssid, password);
        } else {
            ESP_LOGW(TAG, "SSID is empty, ignoring connect");
        }
    }
}

/**
 * @brief Skip button event handler
 */
static void skip_btn_event_handler(lv_event_t *e)
{
    ESP_LOGI(TAG, "Skip button pressed");

    if (screen_callback) {
        screen_callback(TOUCHSCREEN_WIFI_SETUP_RESULT_CANCEL, NULL, NULL);
    }
}

/* On-screen keyboard and helper state */
static lv_obj_t *keyboard = NULL;
static lv_obj_t *last_textarea = NULL;

/* Keyboard event handler: hide on cancel/ready */
static void keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CANCEL || code == LV_EVENT_READY) {
        if (keyboard) {
            lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_textarea(keyboard, NULL);
        }
    }
}

/* Textarea focus event: show keyboard and attach to textarea */
static void textarea_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED) {
        lv_obj_t *ta = lv_event_get_target(e);
        last_textarea = ta;
        if (keyboard) {
            lv_keyboard_set_textarea(keyboard, ta);
            lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void touchscreen_wifi_setup_screen_create(TouchScreen_WiFi_Setup_Callback_t callback)
{
    ESP_LOGI(TAG, "Creating WiFi setup screen");

    screen_callback = callback;

    // Lock display to prevent rendering while creating
    bsp_display_lock(0);

    // Get active screen
    wifi_setup_screen = lv_scr_act();
    if (!wifi_setup_screen) {
        ESP_LOGE(TAG, "Failed to get active screen");
        bsp_display_unlock();
        return;
    }

    // Clear screen
    lv_obj_clean(wifi_setup_screen);

    // Set background color - light gradient feel
    lv_obj_set_style_bg_color(wifi_setup_screen, lv_color_hex(0xF8FAFB), 0);

    /* ===== HEADER PANEL ===== */
    lv_obj_t *header_panel = lv_obj_create(wifi_setup_screen);
    lv_obj_set_size(header_panel, 480, 70);
    lv_obj_align(header_panel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header_panel, lv_color_hex(0x3498DB), 0);  // Nice blue
    lv_obj_set_style_border_width(header_panel, 0, 0);
    lv_obj_set_style_pad_all(header_panel, 15, 0);

    /* Title in header */
    title_label = lv_label_create(header_panel);
    lv_label_set_text(title_label, "WiFi Setup");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_28, 0);
    lv_obj_set_height(title_label, 45);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

    /* ===== FORM CONTAINER ===== */
    lv_obj_t *form_container = lv_obj_create(wifi_setup_screen);
    lv_obj_set_size(form_container, 420, 280);
    lv_obj_align(form_container, LV_ALIGN_TOP_MID, 0, 85);
    lv_obj_set_style_bg_color(form_container, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(form_container, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_border_width(form_container, 1, 0);
    lv_obj_set_style_radius(form_container, 10, 0);
    lv_obj_set_style_pad_all(form_container, 20, 0);
    lv_obj_set_style_shadow_width(form_container, 4, 0);
    lv_obj_set_style_shadow_color(form_container, lv_color_hex(0xD0D0D0), 0);
    lv_obj_set_style_shadow_ofs_y(form_container, 2, 0);

    /* SSID Label */
    lv_obj_t *ssid_label = lv_label_create(form_container);
    lv_label_set_text(ssid_label, "Network Name");
    lv_obj_set_style_text_color(ssid_label, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_text_opa(ssid_label, LV_OPA_100, 0);
    lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 0, 0);

    /* SSID Input Field */
    ssid_input = input_field_component_create(form_container, 380, 45, "Enter SSID");
    if (ssid_input) {
        lv_obj_align(ssid_input, LV_ALIGN_TOP_MID, 0, 28);
    } else {
        ESP_LOGE(TAG, "Failed to create SSID input field");
    }

    /* Password Label */
    lv_obj_t *password_label = lv_label_create(form_container);
    lv_label_set_text(password_label, "Password");
    lv_obj_set_style_text_color(password_label, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_text_opa(password_label, LV_OPA_100, 0);
    lv_obj_align(password_label, LV_ALIGN_TOP_LEFT, 0, 95);

    /* Password Input Field */
    password_input = input_field_component_create(form_container, 380, 45, "Enter Password");
    if (password_input) {
        lv_obj_align(password_input, LV_ALIGN_TOP_MID, 0, 123);
    } else {
        ESP_LOGE(TAG, "Failed to create password input field");
    }

    // Create on-screen keyboard (hidden by default)
    keyboard = lv_keyboard_create(lv_scr_act());
    lv_obj_set_size(keyboard, lv_pct(100), 160);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);

    // Connect Button - Blue/Primary (inside form container) - RIGHT
    connect_btn = lv_btn_create(form_container);
    lv_obj_set_size(connect_btn, 190, 50);
    lv_obj_align(connect_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x3498DB), 0);
    lv_obj_set_style_border_width(connect_btn, 0, 0);
    lv_obj_set_style_radius(connect_btn, 8, 0);
    lv_obj_add_event_cb(connect_btn, connect_btn_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t *connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_set_style_text_color(connect_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(connect_label);

    // Skip Button - Gray/Secondary (inside form container) - LEFT
    skip_btn = lv_btn_create(form_container);
    lv_obj_set_size(skip_btn, 190, 50);
    lv_obj_align(skip_btn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_set_style_bg_color(skip_btn, lv_color_hex(0x95A5A6), 0);
    lv_obj_set_style_border_width(skip_btn, 0, 0);
    lv_obj_set_style_radius(skip_btn, 8, 0);
    lv_obj_add_event_cb(skip_btn, skip_btn_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t *skip_label = lv_label_create(skip_btn);
    lv_label_set_text(skip_label, "Skip");
    lv_obj_set_style_text_color(skip_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(skip_label);

    // Add event callbacks to textareas to show keyboard when focused
    if (ssid_input) lv_obj_add_event_cb(ssid_input, textarea_event_cb, LV_EVENT_FOCUSED, NULL);
    if (password_input) lv_obj_add_event_cb(password_input, textarea_event_cb, LV_EVENT_FOCUSED, NULL);

    // Unlock display to allow rendering
    bsp_display_unlock();

    ESP_LOGI(TAG, "WiFi setup screen created successfully");
}

void touchscreen_wifi_setup_screen_destroy(void)
{
    ESP_LOGI(TAG, "Destroying WiFi setup screen");

    if (wifi_setup_screen) {
        lv_obj_clean(wifi_setup_screen);
        wifi_setup_screen = NULL;
        title_label = NULL;
        ssid_input = NULL;
        password_input = NULL;
        connect_btn = NULL;
        skip_btn = NULL;
        screen_callback = NULL;
    }
}

const char *touchscreen_wifi_setup_screen_get_ssid(void)
{
    if (ssid_input) {
        return lv_textarea_get_text(ssid_input);
    }
    return NULL;
}

const char *touchscreen_wifi_setup_screen_get_password(void)
{
    if (password_input) {
        return lv_textarea_get_text(password_input);
    }
    return NULL;
}
