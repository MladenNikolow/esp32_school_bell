#include <stdlib.h>
#include <string.h>
#include "WiFi_Manager_API.h"
#include "WiFI_Manager_Public.h"
#include "Definitions/AppErrors.h"
#include "NVS_API.h"

#define WIFI_MANAGER_DEFAULT_SSID              "ESP32_Setup"
#define WIFI_MANAGER_DEFAULT_PASS              "12345678"

#define WIFI_MANAGER_NVS_NAMESPACE              "wifi_manager"
#define WIFI_MANAGER_NVS_KEY_SSID               "ssid"
#define WIFI_MANAGER_NVS_KEY_PASS               "pass"


typedef struct _WIFI_MANAGER_RSC_T
{
    uint32_t ulConfigurationState;                /* WIFI_MANAGER_CONFIGURATION_STATE_E */
    uint8_t  abSsid[WIFI_MANAGER_MAX_SSID_LENGTH + 1];
    uint8_t  abPass[WIFI_MANAGER_MAX_PASS_LENGTH + 1];
} WIFI_MANAGER_RSC_T;

int32_t
wiFi_Manager_InitCredentials(WIFI_MANAGER_RSC_T* ptRsc);

int32_t 
WiFi_Manager_Init(WIFI_MANAGER_H* phWifiManager)
{
    int32_t lResult = APP_SUCCESS;
    WIFI_MANAGER_RSC_T* ptRsc = NULL;

    if (NULL == phWifiManager)
    {
        return APP_ERROR_INVALID_PARAM;
    }

    ptRsc = (WIFI_MANAGER_RSC_T*)calloc(1, sizeof(WIFI_MANAGER_RSC_T));
    if (NULL == ptRsc)
    {
        lResult = APP_ERROR_OUT_OF_MEMORY;
    }
    else
    {
        lResult = wiFi_Manager_InitCredentials(ptRsc);

        if(APP_SUCCESS == lResult)
        {
            *phWifiManager = (WIFI_MANAGER_H)ptRsc;
        }
        else
        {
            free(ptRsc);
            ptRsc = NULL;
        }
    }

    return lResult;
}

int32_t
WiFi_Manager_GetConfigurationState(WIFI_MANAGER_H hWifiManager, 
                                   uint32_t* pulConfigurationState)
{
    WIFI_MANAGER_RSC_T* ptRsc = (WIFI_MANAGER_RSC_T*)hWifiManager;

    if ((NULL == ptRsc) || 
        (NULL == pulConfigurationState))
    {
        return APP_ERROR_INVALID_PARAM;
    }

    *pulConfigurationState = ptRsc->ulConfigurationState;

    return APP_SUCCESS;
}

int32_t
WiFi_Manager_GetSsid(WIFI_MANAGER_H hWifiManager,
                     uint8_t* pbSsid,
                     size_t* pSsidLen)
{
    int32_t lResult = APP_SUCCESS;
    WIFI_MANAGER_RSC_T* ptRsc = (WIFI_MANAGER_RSC_T*)hWifiManager;

    if ((NULL == ptRsc) ||
        (NULL == pbSsid) ||
        (NULL == pSsidLen))
    {
        return APP_ERROR_INVALID_PARAM;
    }

    size_t len = strnlen((char*)ptRsc->abSsid, WIFI_MANAGER_MAX_SSID_LENGTH);

    if (*pSsidLen <= len)
    {
        lResult = APP_ERROR_BUFFER_TOO_SMALL;
    }
    else
    {
        strncpy((char*)pbSsid, (char*)ptRsc->abSsid, *pSsidLen);
        *pSsidLen = len;
    }

    return lResult;
}

int32_t
WiFi_Manager_GetPassword(WIFI_MANAGER_H hWifiManager,
                         uint8_t* pbPass,
                         size_t* pPassLen)
{
    int32_t lResult = APP_SUCCESS;
    WIFI_MANAGER_RSC_T* ptRsc = (WIFI_MANAGER_RSC_T*)hWifiManager;

    if ((NULL == ptRsc)     ||
        (NULL == pbPass)    ||
        (NULL == pPassLen))
    {
        return APP_ERROR_INVALID_PARAM;
    }

    size_t len = strnlen((char*)ptRsc->abPass, WIFI_MANAGER_MAX_PASS_LENGTH);

    if (*pPassLen <= len)
    {
        lResult = APP_ERROR_BUFFER_TOO_SMALL;
    }
    else
    {
        strncpy((char*)pbPass, (char*)ptRsc->abPass, *pPassLen);
        *pPassLen = len;
    }

    return lResult;
}

esp_err_t
WiFi_Manager_SaveCredentials(const char* ssid,const char* pass)
{
    esp_err_t espErr = ESP_OK;
    nvs_handle_t hNvs = 0;

    espErr = NVS_Open(WIFI_MANAGER_NVS_NAMESPACE, NVS_READWRITE, &hNvs);

    if(ESP_OK == espErr)
    {
        espErr = NVS_WriteString(hNvs, WIFI_MANAGER_NVS_KEY_SSID, ssid);
    }

    if(ESP_OK == espErr)
    {
        espErr = NVS_WriteString(hNvs, WIFI_MANAGER_NVS_KEY_PASS, pass);
    }

    if(ESP_OK == espErr)
    {
        espErr = NVS_Commit(hNvs);
    }

    if(ESP_OK == espErr)
    {
        espErr = NVS_Close(hNvs);
    }
    
    return espErr;
}
    
int32_t
wiFi_Manager_InitCredentials(WIFI_MANAGER_RSC_T* ptRsc)
{    
    int32_t lResult = APP_ERROR;
    esp_err_t espErr = ESP_OK;
    nvs_handle_t hNvs = 0;
    size_t szSsidLen = sizeof(ptRsc->abSsid);
    size_t szPassLen = sizeof(ptRsc->abPass);

    assert(NULL != ptRsc);

    espErr = NVS_Open(WIFI_MANAGER_NVS_NAMESPACE, NVS_READWRITE, &hNvs);

    if(ESP_OK == espErr)
    {
        espErr = NVS_ReadString(hNvs, WIFI_MANAGER_NVS_KEY_SSID, (char*)ptRsc->abSsid, &szSsidLen);

        if (ESP_OK == espErr)
        {
            espErr = NVS_ReadString(hNvs, WIFI_MANAGER_NVS_KEY_PASS, (char*)ptRsc->abPass, &szPassLen);
        }

        if (ESP_OK == espErr)
        {
            ptRsc->ulConfigurationState = WIFI_MANAGER_CONFIGURATION_STATE_CONFIGURED;
        }
        else if(ESP_ERR_NVS_NOT_FOUND == espErr)
        {
            ptRsc->ulConfigurationState = WIFI_MANAGER_CONFIGURATION_STATE_WAITING_CONFIGURATION;
            strncpy((char*)ptRsc->abSsid, WIFI_MANAGER_DEFAULT_SSID, sizeof(WIFI_MANAGER_DEFAULT_SSID));
            strncpy((char*)ptRsc->abPass, WIFI_MANAGER_DEFAULT_PASS, sizeof(WIFI_MANAGER_DEFAULT_PASS));
            lResult = APP_SUCCESS;
        }

        (void)NVS_Close(hNvs);
    }
    else
    {
        // TODO: It should not happen, but if NVS open fails, use default values
        // Add diagnosis  / critical error handling as needed      
    }

    return lResult;
}