#ifndef TOUCHSCREEN_UI_CONFIG_H
#define TOUCHSCREEN_UI_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* UI Configuration constants — Waveshare ESP32-S3-Touch-LCD-4 (480x480 square) */
#define TOUCHSCREEN_UI_SCREEN_WIDTH             480
#define TOUCHSCREEN_UI_SCREEN_HEIGHT            480

/* Splash screen configuration */
#define TOUCHSCREEN_UI_SPLASH_DEFAULT_DURATION  3000  /* milliseconds */

/* WiFi setup screen configuration */
#define TOUCHSCREEN_UI_WIFI_SSID_MAX_LEN        32
#define TOUCHSCREEN_UI_WIFI_PASSWORD_MAX_LEN    64

/* Periodic update interval */
#define TOUCHSCREEN_UI_UPDATE_PERIOD_MS         1000  /* 1 second */

/* Screen idle timeout for brightness dimming */
#define TOUCHSCREEN_UI_DIM_TIMEOUT_MS           60000 /* 60 seconds */
#define TOUCHSCREEN_UI_DIM_BRIGHTNESS           30    /* percent */
#define TOUCHSCREEN_UI_FULL_BRIGHTNESS          100   /* percent */

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_UI_CONFIG_H */
