#include "nvs_flash.h"
#include "nvs.h"

esp_err_t
NVS_Init(void);

esp_err_t 
NVS_Deinit(void);

esp_err_t
NVS_Open(const char* pcNamespace, uint8_t usOpenMode, nvs_handle_t* phNvsHandle);

esp_err_t
NVS_ReadString(nvs_handle_t hNvsHandle, const char* pcKey, void* pvOutBuffer, size_t* pusOutBufferLen);

esp_err_t
NVS_WriteString(nvs_handle_t hNvsHandle, const char* pcKey, const void* pvInBuffer);

esp_err_t
NVS_Read(nvs_handle_t hNvsHandle, const char* pcKey, void* pvOutBuffer, size_t* pusOutBufferLen);

esp_err_t
NVS_Write(nvs_handle_t hNvsHandle, const char* pcKey, const void* pvInBuffer, size_t usInBufferLen);

esp_err_t
NVS_Commit(nvs_handle_t hNvsHandle);

esp_err_t
NVS_Close(nvs_handle_t hNvsHandle);