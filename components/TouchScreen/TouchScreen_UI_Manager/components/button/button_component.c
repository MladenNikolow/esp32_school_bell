#include "button_component.h"
#include "../../src/ui_theme.h"
#include "esp_log.h"

static const char *TAG = "BUTTON_COMPONENT";

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

    /* Apply Material primary button style */
    ui_theme_apply_btn_primary(btn);

    /* Create label inside button */
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT_ON_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, UI_FONT_BODY, LV_PART_MAIN);
    lv_obj_center(label);

    /* Add callback if provided */
    if (callback) {
        lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, NULL);
    }

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
        lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, LV_PART_MAIN);
    } else {
        lv_obj_add_state(btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(btn, UI_COLOR_TEXT_DISABLED, LV_PART_MAIN);
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
