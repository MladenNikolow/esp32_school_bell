#include "input_field_component.h"
#include "esp_log.h"

static const char *TAG = "INPUT_FIELD_COMPONENT";

/* Input field color scheme */
#define INPUT_BG_COLOR          0xFFFFFF    /* White */
#define INPUT_BORDER_COLOR      0x7F8C8D    /* Gray */
#define INPUT_TEXT_COLOR        0x000000    /* Black */
#define INPUT_PLACEHOLDER_COLOR 0xBDC3C7    /* Light Gray */

lv_obj_t *input_field_component_create(lv_obj_t *parent, int32_t width, int32_t height,
                                        const char *placeholder)
{
    if (!parent || !placeholder) {
        ESP_LOGE(TAG, "Invalid parent or placeholder");
        return NULL;
    }

    /* Create textarea */
    lv_obj_t *textarea = lv_textarea_create(parent);
    if (!textarea) {
        ESP_LOGE(TAG, "Failed to create textarea");
        return NULL;
    }

    /* Set size */
    lv_obj_set_size(textarea, width, height);

    /* Set background style */
    lv_obj_set_style_bg_color(textarea, lv_color_hex(INPUT_BG_COLOR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(textarea, LV_OPA_100, LV_PART_MAIN);

    /* Set border style */
    lv_obj_set_style_border_color(textarea, lv_color_hex(INPUT_BORDER_COLOR), LV_PART_MAIN);
    lv_obj_set_style_border_width(textarea, 2, LV_PART_MAIN);

    /* Set radius for rounded corners */
    lv_obj_set_style_radius(textarea, 6, LV_PART_MAIN);

    /* Set text color */
    lv_obj_set_style_text_color(textarea, lv_color_hex(INPUT_TEXT_COLOR), LV_PART_MAIN);

    /* Set padding inside textarea */
    lv_obj_set_style_pad_all(textarea, 10, LV_PART_MAIN);

    /* Set placeholder text (placeholder part styled separately) */
    lv_textarea_set_placeholder_text(textarea, placeholder);
    lv_obj_set_style_text_color(textarea, lv_color_hex(INPUT_PLACEHOLDER_COLOR), LV_PART_TEXTAREA_PLACEHOLDER);

    /* Single line mode */
    lv_textarea_set_one_line(textarea, true);

    /* No scrollbar needed for single line */
    lv_obj_set_scrollbar_mode(textarea, LV_SCROLLBAR_MODE_OFF);

    ESP_LOGI(TAG, "Input field created with placeholder: %s", placeholder);

    return textarea;
}

const char *input_field_component_get_text(lv_obj_t *input_field)
{
    if (!input_field) {
        ESP_LOGE(TAG, "Invalid input field");
        return NULL;
    }

    return lv_textarea_get_text(input_field);
}

void input_field_component_set_text(lv_obj_t *input_field, const char *text)
{
    if (!input_field || !text) {
        ESP_LOGE(TAG, "Invalid input field or text");
        return;
    }

    lv_textarea_set_text(input_field, text);
}

void input_field_component_clear(lv_obj_t *input_field)
{
    if (!input_field) {
        ESP_LOGE(TAG, "Invalid input field");
        return;
    }

    lv_textarea_set_text(input_field, "");
}

void input_field_component_set_password_mode(lv_obj_t *input_field, bool is_password)
{
    if (!input_field) {
        ESP_LOGE(TAG, "Invalid input field");
        return;
    }

    lv_textarea_set_password_mode(input_field, is_password);
}

void input_field_component_set_max_length(lv_obj_t *input_field, uint32_t max_length)
{
    if (!input_field) {
        ESP_LOGE(TAG, "Invalid input field");
        return;
    }

    lv_textarea_set_max_length(input_field, max_length);
}
