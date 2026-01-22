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
 * 
 * @note This function must be called before TouchScreen_Init() to properly
 *       initialize the display and rendering pipeline
 * 
 * @return ESP_OK on success, error code otherwise
 */
int32_t TouchScreen_DisplayInit(void);

/**
 * @brief Initialize and start the touch screen component
 * 
 * @note This function creates the touch screen task which manages:
 *       - Display rendering and refresh
 *       - Touch input handling
 *       - UI screens (splash, WiFi setup, etc.)
 *       - LVGL event loop integration
 * 
 * @note TouchScreen_DisplayInit() must be called before this function
 * 
 * @param ptParams Initialization parameters (must not be NULL)
 * @param phTouchScreen Output handle for touch screen component (must not be NULL)
 * 
 * @return ESP_OK on success, error code otherwise
 */
int32_t TouchScreen_Init(TOUCHSCREEN_PARAMS_T* ptParams, TOUCHSCREEN_H* phTouchScreen);

/**
 * @brief Deinitialize and stop the touch screen component
 * 
 * @param hTouchScreen Touch screen handle
 * 
 * @return ESP_OK on success, error code otherwise
 */
int32_t TouchScreen_Deinit(TOUCHSCREEN_H hTouchScreen);

/**
 * @brief Show splash screen on the touch screen
 * 
 * @param hTouchScreen Touch screen handle
 * @param duration_ms Duration to display splash screen in milliseconds
 * 
 * @return ESP_OK on success, error code otherwise
 */
int32_t TouchScreen_ShowSplash(TOUCHSCREEN_H hTouchScreen, uint32_t duration_ms);

/**
 * @brief Show WiFi setup screen on the touch screen
 * 
 * @param hTouchScreen Touch screen handle
 * @param wifi_setup_callback Callback function when WiFi setup is complete (can be NULL)
 * 
 * @return ESP_OK on success, error code otherwise
 */
int32_t TouchScreen_ShowWiFiSetup(TOUCHSCREEN_H hTouchScreen, void (*wifi_setup_callback)(int result, const char *ssid, const char *password));

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_API_H */
