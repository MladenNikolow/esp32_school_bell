#include "button_component.h"
#include "esp_log.h"

static const char *TAG = "BUTTON_COMPONENT";

/* Button color scheme */
#define BUTTON_BG_COLOR         0x3498DB    /* Blue */
#define BUTTON_PRESS_COLOR      0x2980B9    /* Dark Blue */
#define BUTTON_TEXT_COLOR       0xFFFFFF    /* White */
#define BUTTON_DISABLED_COLOR   0xBDC3C7    /* Gray */

lv_obj_t *button_component_create(lv_obj_t *parent, int32_t width, int32_t height,
                                   const char *text, TouchScreen_Button_Callback_t callback)
{
    if (!parent || !text) {
        ESP_LOGE(TAG, "Invalid parent or text");
        return NULL;
    }

    /* Create button */
    lv_obj_t *btn = lv_button_create(parent);
    if (!btn) {
        ESP_LOGE(TAG, "Failed to create button");
        return NULL;
    }

    /* Set size */
    lv_obj_set_size(btn, width, height);

    /* Set background style */
    lv_obj_set_style_bg_color(btn, lv_color_hex(BUTTON_BG_COLOR), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(BUTTON_PRESS_COLOR), LV_STATE_PRESSED);

    /* Set border style */
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);

    /* Set radius for rounded corners */
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);

    /* Create label inside button */
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(BUTTON_TEXT_COLOR), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_center(label);

    /* Add callback if provided */
    if (callback) {
        lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, NULL);
    }

    ESP_LOGI(TAG, "Button created with text: %s", text);

    return btn;
}

void button_component_set_text(lv_obj_t *btn, const char *text)
{
    if (!btn || !text) {
        ESP_LOGE(TAG, "Invalid button or text");
        return;
    }

    /* Find label child and update text */
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (label) {
        lv_label_set_text(label, text);
    }
}

void button_component_set_enabled(lv_obj_t *btn, bool enabled)
{
    if (!btn) {
        ESP_LOGE(TAG, "Invalid button");
        return;
    }

    if (enabled) {
        lv_obj_clear_state(btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(BUTTON_BG_COLOR), LV_PART_MAIN);
    } else {
        lv_obj_add_state(btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(BUTTON_DISABLED_COLOR), LV_PART_MAIN);
    }
}

bool button_component_get_enabled(lv_obj_t *btn)
{
    if (!btn) {
        ESP_LOGE(TAG, "Invalid button");
        return false;
    }

    return !lv_obj_has_state(btn, LV_STATE_DISABLED);
}
