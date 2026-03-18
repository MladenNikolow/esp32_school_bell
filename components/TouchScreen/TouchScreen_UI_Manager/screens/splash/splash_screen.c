#include "splash_screen_internal.h"
#include "../../src/ui_manager_internal.h"
#include "lvgl.h"
#include "../../assets/ringy_logo.h"
#include "../../src/ui_theme.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "SPLASH_SCREEN";

static lv_obj_t *splash_screen = NULL;

void touchscreen_splash_screen_create(uint32_t duration_ms)
{
    ESP_LOGI(TAG, "Creating splash screen");

    bsp_display_lock(0);

    splash_screen = g_ui_state.screen_obj;
    if (!splash_screen) {
        ESP_LOGE(TAG, "Failed to get screen object");
        bsp_display_unlock();
        return;
    }

    lv_obj_clean(splash_screen);

    /* Background colour matches the logo bitmap edge colour */
    lv_obj_set_style_bg_color(splash_screen, lv_color_hex(0xBDDBD6), 0);
    lv_obj_set_style_bg_opa(splash_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(splash_screen, 0, 0);
    lv_obj_set_scrollbar_mode(splash_screen, LV_SCROLLBAR_MODE_OFF);

    /* Logo image — 500×500 source, centred on 480×480 screen (10px clipped/side) */
    lv_obj_t *img = lv_image_create(splash_screen);
    if (img) {
        lv_image_set_src(img, &ringy_logo);
        lv_image_set_scale(img, 256);  /* 100% = native 500×500 */
        lv_obj_set_size(img, 480, 480);
        lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CENTER);
        lv_obj_center(img);
    }

    bsp_display_unlock();

    ESP_LOGI(TAG, "Splash screen created, will display for %lu ms", (unsigned long)duration_ms);
}

void touchscreen_splash_screen_destroy(void)
{
    ESP_LOGI(TAG, "Destroying splash screen");

    bsp_display_lock(0);
    if (splash_screen) {
        lv_obj_clean(splash_screen);
        splash_screen = NULL;
    }
    bsp_display_unlock();
}
