#ifndef TOUCHSCREEN_CARD_COMPONENT_H
#define TOUCHSCREEN_CARD_COMPONENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief Create a Material Design card container
 * @param parent Parent LVGL object
 * @param width Card width in pixels
 * @param height Card height in pixels (LV_SIZE_CONTENT for auto)
 * @return Pointer to created card object, or NULL on failure
 */
lv_obj_t *card_component_create(lv_obj_t *parent, int32_t width, int32_t height);

/**
 * @brief Create a card with a title bar
 * @param parent Parent LVGL object
 * @param width Card width in pixels
 * @param height Card height in pixels (LV_SIZE_CONTENT for auto)
 * @param title Title text displayed at top of card
 * @return Pointer to created card object, or NULL on failure
 */
lv_obj_t *card_component_create_with_title(lv_obj_t *parent, int32_t width, int32_t height,
                                            const char *title);

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_CARD_COMPONENT_H */
