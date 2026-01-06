#include <stdint.h>
#include "WiFI_Manager_Public.h"
#include "esp_err.h"

/* WiFi manager handler */
typedef struct _WIFI_MANAGER_RSC_T*     WIFI_MANAGER_H;

/* Initialize the WiFi manager */
int32_t 
WiFi_Manager_Init(WIFI_MANAGER_H* phWifiManager);

int32_t
WiFi_Manager_GetSsid(WIFI_MANAGER_H hWifiManager,
                     uint8_t* pbSsid,
                     size_t* pSsidLen);

int32_t
WiFi_Manager_GetPassword(WIFI_MANAGER_H hWifiManager,
                         uint8_t* pbPass,
                         size_t* pPassLen);

esp_err_t
WiFi_Manager_GetConfigurationState(WIFI_MANAGER_H hWifiManager, 
                                   uint32_t* pulConfigurationState);

esp_err_t
WiFi_Manager_SaveCredentials(const char* ssid,
                             const char* pass);
