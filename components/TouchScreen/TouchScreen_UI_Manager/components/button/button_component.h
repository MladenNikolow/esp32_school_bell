#ifndef TOUCHSCREEN_BUTTON_COMPONENT_H
#define TOUCHSCREEN_BUTTON_COMPONENT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Button callback function type
 */
typedef void (*TouchScreen_Button_Callback_t)(lv_event_t *e);

/**
 * @brief Create a button component (internal)
 * @param parent Parent LVGL object
 * @param width Button width in pixels
 * @param height Button height in pixels
 * @param text Button label text
 * @param callback Function to call on button click (can be NULL)
 * @return Pointer to created button object, or NULL on error
 */
lv_obj_t *button_component_create(lv_obj_t *parent, int32_t width, int32_t height,
                                   const char *text, TouchScreen_Button_Callback_t callback);

/**
 * @brief Set button text (internal)
 * @param btn Button object
 * @param text New button text
 */
void button_component_set_text(lv_obj_t *btn, const char *text);

/**
 * @brief Set button enabled/disabled state (internal)
 * @param btn Button object
 * @param enabled True to enable, false to disable
 */
void button_component_set_enabled(lv_obj_t *btn, bool enabled);

/**
 * @brief Get button enabled/disabled state (internal)
 * @param btn Button object
 * @return True if enabled, false if disabled
 */
bool button_component_get_enabled(lv_obj_t *btn);

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_BUTTON_COMPONENT_H */
