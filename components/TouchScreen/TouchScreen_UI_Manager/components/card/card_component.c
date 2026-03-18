#include "card_component.h"
#include "../../src/ui_theme.h"
#include "esp_log.h"

static const char *TAG = "CARD_COMPONENT";

lv_obj_t *card_component_create(lv_obj_t *parent, int32_t width, int32_t height)
{
    if (!parent) {
        ESP_LOGE(TAG, "Invalid parent");
        return NULL;
    }

    lv_obj_t *card = lv_obj_create(parent);
    if (!card) {
        ESP_LOGE(TAG, "Failed to create card");
        return NULL;
    }

    lv_obj_set_size(card, width, height);
    ui_theme_apply_card(card);

    return card;
}

lv_obj_t *card_component_create_with_title(lv_obj_t *parent, int32_t width, int32_t height,
                                            const char *title)
{
    lv_obj_t *card = card_component_create(parent, width, height);
    if (!card || !title) return card;

    /* Set card to column flex layout */
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);

    /* Title label */
    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_width(lbl, lv_pct(100));

    /* Divider line */
    lv_obj_t *divider = lv_obj_create(card);
    lv_obj_set_size(divider, lv_pct(100), 1);
    lv_obj_set_style_bg_color(divider, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_pad_all(divider, 0, 0);

    return card;
}
