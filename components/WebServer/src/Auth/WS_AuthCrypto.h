#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define AUTH_CRYPTO_SALT_LEN  16
#define AUTH_CRYPTO_HASH_LEN  32
#define AUTH_CRYPTO_SALT_HEX  (AUTH_CRYPTO_SALT_LEN * 2 + 1)  /* 33 bytes incl. NUL */
#define AUTH_CRYPTO_HASH_HEX  (AUTH_CRYPTO_HASH_LEN * 2 + 1)  /* 65 bytes incl. NUL */

/**
 * @brief  Hash a password with a freshly generated random salt.
 *         Computes SHA-256( salt || password ).
 *
 * @param[in]  password   NUL-terminated plaintext password
 * @param[out] salt_out   16-byte random salt
 * @param[out] hash_out   32-byte SHA-256 digest
 * @return ESP_OK on success
 */
esp_err_t auth_crypto_hash_password(const char* password,
                                    uint8_t salt_out[AUTH_CRYPTO_SALT_LEN],
                                    uint8_t hash_out[AUTH_CRYPTO_HASH_LEN]);

/**
 * @brief  Verify a password against a stored salt + hash (constant-time).
 *
 * @param[in] password  NUL-terminated plaintext password
 * @param[in] salt      16-byte salt that was used when hashing
 * @param[in] hash      32-byte expected SHA-256 digest
 * @return true if the password matches
 */
bool auth_crypto_verify_password(const char* password,
                                 const uint8_t salt[AUTH_CRYPTO_SALT_LEN],
                                 const uint8_t hash[AUTH_CRYPTO_HASH_LEN]);

/**
 * @brief  Convert a binary salt to a hex string (32 chars + NUL).
 */
void auth_crypto_salt_to_hex(const uint8_t salt[AUTH_CRYPTO_SALT_LEN],
                             char hex_out[AUTH_CRYPTO_SALT_HEX]);

/**
 * @brief  Parse a hex string back into a binary salt.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on bad input
 */
esp_err_t auth_crypto_hex_to_salt(const char* hex,
                                  uint8_t salt_out[AUTH_CRYPTO_SALT_LEN]);

/**
 * @brief  Convert a binary hash to a hex string (64 chars + NUL).
 */
void auth_crypto_hash_to_hex(const uint8_t hash[AUTH_CRYPTO_HASH_LEN],
                             char hex_out[AUTH_CRYPTO_HASH_HEX]);

/**
 * @brief  Parse a hex string back into a binary hash.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on bad input
 */
esp_err_t auth_crypto_hex_to_hash(const char* hex,
                                  uint8_t hash_out[AUTH_CRYPTO_HASH_LEN]);
