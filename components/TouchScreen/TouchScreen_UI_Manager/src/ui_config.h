#ifndef TOUCHSCREEN_UI_CONFIG_H
#define TOUCHSCREEN_UI_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* UI Configuration constants */
#define TOUCHSCREEN_UI_SCREEN_WIDTH             480
#define TOUCHSCREEN_UI_SCREEN_HEIGHT            320

/* Splash screen configuration */
#define TOUCHSCREEN_UI_SPLASH_DEFAULT_DURATION  2000  /* milliseconds */

/* WiFi setup screen configuration */
#define TOUCHSCREEN_UI_WIFI_SSID_MAX_LEN        32
#define TOUCHSCREEN_UI_WIFI_PASSWORD_MAX_LEN    64

/* Colors */
#define TOUCHSCREEN_UI_COLOR_PRIMARY            0x3498DB
#define TOUCHSCREEN_UI_COLOR_SECONDARY          0x95A5A6
#define TOUCHSCREEN_UI_COLOR_BACKGROUND         0xF8FAFB
#define TOUCHSCREEN_UI_COLOR_TEXT_PRIMARY       0x2C3E50

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_UI_CONFIG_H */
