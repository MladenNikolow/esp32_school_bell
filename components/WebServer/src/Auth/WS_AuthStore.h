#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief  Initialise the credential store.
 *         On first boot (no service hash in NVS), hashes the Kconfig default
 *         password and writes the salt + hash to the "auth" NVS namespace.
 *         Must be called once at startup before any login attempt.
 *
 * @return ESP_OK on success
 */
esp_err_t auth_store_init(void);

/**
 * @brief  Verify service account credentials.
 *         Service username is the compile-time CONFIG_WS_AUTH_USERNAME.
 *
 * @param[in] username  Submitted username
 * @param[in] password  Submitted plaintext password
 * @return true if credentials are valid
 */
bool auth_store_verify_service(const char* username, const char* password);

/**
 * @brief  Check whether a client account has been provisioned.
 * @return true if a client account exists in NVS
 */
bool auth_store_client_exists(void);

/**
 * @brief  Verify client account credentials.
 *
 * @param[in] username  Submitted username
 * @param[in] password  Submitted plaintext password
 * @return true if credentials are valid
 */
bool auth_store_verify_client(const char* username, const char* password);

/**
 * @brief  Create or update the client account.
 *         Generates a fresh salt, hashes the password, and stores everything
 *         in the "auth" NVS namespace.
 *
 * @param[in] username  Client username (max 31 chars)
 * @param[in] password  Client plaintext password (min 8 chars)
 * @return ESP_OK on success
 */
esp_err_t auth_store_set_client(const char* username, const char* password);

/**
 * @brief  Delete the client account from NVS.
 * @return ESP_OK on success
 */
esp_err_t auth_store_delete_client(void);

/**
 * @brief  Get the stored client username.
 *
 * @param[out] out  Buffer to receive the username
 * @param[in]  len  Size of the output buffer
 * @return ESP_OK if a client account exists and the username was copied
 */
esp_err_t auth_store_get_client_username(char* out, size_t len);
