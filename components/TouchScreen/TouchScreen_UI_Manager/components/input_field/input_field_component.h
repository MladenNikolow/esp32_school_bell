#ifndef TOUCHSCREEN_INPUT_FIELD_COMPONENT_H
#define TOUCHSCREEN_INPUT_FIELD_COMPONENT_H

#include "lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an input field component (textarea) (internal)
 * @param parent Parent LVGL object
 * @param width Input field width in pixels
 * @param height Input field height in pixels
 * @param placeholder Placeholder text to display
 * @return Pointer to created textarea object, or NULL on error
 */
lv_obj_t *input_field_component_create(lv_obj_t *parent, int32_t width, int32_t height,
                                        const char *placeholder);

/**
 * @brief Get the text from input field (internal)
 * @param input_field Input field object
 * @return Pointer to text string
 */
const char *input_field_component_get_text(lv_obj_t *input_field);

/**
 * @brief Set the text in input field (internal)
 * @param input_field Input field object
 * @param text Text to set
 */
void input_field_component_set_text(lv_obj_t *input_field, const char *text);

/**
 * @brief Clear input field (internal)
 * @param input_field Input field object
 */
void input_field_component_clear(lv_obj_t *input_field);

/**
 * @brief Set input field as password field (internal)
 * @param input_field Input field object
 * @param is_password True to hide characters, false to show
 */
void input_field_component_set_password_mode(lv_obj_t *input_field, bool is_password);

/**
 * @brief Set maximum length for input field (internal)
 * @param input_field Input field object
 * @param max_length Maximum number of characters
 */
void input_field_component_set_max_length(lv_obj_t *input_field, uint32_t max_length);

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_INPUT_FIELD_COMPONENT_H */
