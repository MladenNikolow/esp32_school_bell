#include "splash_screen_internal.h"
#include "lvgl.h"
#include "../../assets/ringy_logo.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SPLASH_SCREEN";

static lv_obj_t *splash_screen = NULL;

void touchscreen_splash_screen_create(uint32_t duration_ms)
{
    ESP_LOGI(TAG, "Creating splash screen");

    // Get active screen
    splash_screen = lv_scr_act();
    if (!splash_screen) {
        ESP_LOGE(TAG, "Failed to get active screen");
        return;
    }

    // Clear screen
    lv_obj_clean(splash_screen);

    // Create image object for logo
    lv_obj_t *img = lv_image_create(splash_screen);
    if (!img) {
        ESP_LOGE(TAG, "Failed to create image object");
        return;
    }

    // Set the image source to the ringy logo
    lv_image_set_src(img, &ringy_logo);

    // Center the image on screen
    lv_obj_set_align(img, LV_ALIGN_CENTER);

    // Set appropriate size (adjust based on your screen dimensions and logo size)
    lv_obj_set_size(img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    ESP_LOGI(TAG, "Splash screen created, will display for %lu ms", duration_ms);
}

void touchscreen_splash_screen_destroy(void)
{
    ESP_LOGI(TAG, "Destroying splash screen");

    if (splash_screen) {
        lv_obj_clean(splash_screen);
        splash_screen = NULL;
    }
}
