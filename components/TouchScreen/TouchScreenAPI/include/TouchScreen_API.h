#ifndef TOUCHSCREEN_API_H
#define TOUCHSCREEN_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Touch screen initialization parameters
 */
typedef struct {
    uint32_t ulTaskPriority;  /*!< Touch screen task priority */
} TOUCHSCREEN_PARAMS_T;

/**
 * @brief Touch screen handle (opaque)
 */
typedef void* TOUCHSCREEN_H;

/**
 * @brief Initialize the BSP display hardware
 */
int32_t TouchScreen_DisplayInit(void);

/**
 * @brief Initialize and start the touch screen component
 */
int32_t TouchScreen_Init(TOUCHSCREEN_PARAMS_T* ptParams, TOUCHSCREEN_H* phTouchScreen);

/**
 * @brief Deinitialize and stop the touch screen component
 */
int32_t TouchScreen_Deinit(TOUCHSCREEN_H hTouchScreen);

/**
 * @brief Show splash screen on the touch screen
 */
int32_t TouchScreen_ShowSplash(TOUCHSCREEN_H hTouchScreen, uint32_t duration_ms);

/**
 * @brief Show WiFi setup screen
 */
int32_t TouchScreen_ShowWiFiSetup(TOUCHSCREEN_H hTouchScreen, void (*wifi_setup_callback)(int result, const char *ssid, const char *password));

/**
 * @brief Show the main dashboard screen
 */
int32_t TouchScreen_ShowDashboard(TOUCHSCREEN_H hTouchScreen);

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_API_H */
