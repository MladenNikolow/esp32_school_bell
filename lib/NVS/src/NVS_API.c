#include "nvs_flash.h"
#include "nvs.h"
#include "Definitions/AppErrors.h"

// TODO: Null checks and param validations

esp_err_t
NVS_Init(void)  
{
    return nvs_flash_init();
}   

esp_err_t
NVS_Deinit(void)
{
    return nvs_flash_deinit();
}


esp_err_t
NVS_Open(const char* pcNamespace, uint8_t usOpenMode, nvs_handle_t* phNVSHandle)
{
    return nvs_open(pcNamespace, usOpenMode, phNVSHandle);
}

esp_err_t
NVS_ReadString(nvs_handle_t hNvsHandle, const char* pcKey, void* pvOutBuffer, size_t* pusOutBufferLen)
{
    esp_err_t espError = ESP_OK;
    size_t requiredSize = 0;

    espError = nvs_get_str(hNvsHandle, pcKey, NULL, &requiredSize);

    if(espError == ESP_OK)
    {
        espError = nvs_get_str(hNvsHandle, pcKey, pvOutBuffer, &requiredSize);
    }

    return espError;
}

esp_err_t
NVS_WriteString(nvs_handle_t hNvsHandle, const char* pcKey, const void* pvInBuffer)
{
    return nvs_set_str(hNvsHandle, pcKey, (const char*)pvInBuffer);
}   

esp_err_t
NVS_Read(nvs_handle_t hNvsHandle, const char* pcKey, void* pvOutBuffer, size_t* pusOutBufferLen)
{
    return nvs_get_blob(hNvsHandle, pcKey, pvOutBuffer, pusOutBufferLen);
}

esp_err_t
NVS_Write(nvs_handle_t hNvsHandle, const char* pcKey, const void* pvInBuffer, size_t usInBufferLen)
{
    return nvs_set_blob(hNvsHandle, pcKey, pvInBuffer, usInBufferLen);
}  

esp_err_t
NVS_Commit(nvs_handle_t hNvsHandle)
{
    return nvs_commit(hNvsHandle);
}

esp_err_t
NVS_Close(nvs_handle_t hNVSHandle)
{
    nvs_close(hNVSHandle);
    return APP_SUCCESS;
}

