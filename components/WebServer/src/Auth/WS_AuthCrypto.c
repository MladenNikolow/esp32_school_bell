#include "WS_AuthCrypto.h"

#include <string.h>
#include <stdio.h>

#include "esp_random.h"
#include "mbedtls/sha256.h"
#include "mbedtls/constant_time.h"

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */
static void bin_to_hex(const uint8_t* bin, size_t len, char* hex_out)
{
    for (size_t i = 0; i < len; i++) {
        sprintf(hex_out + (i * 2), "%02x", bin[i]);
    }
    hex_out[len * 2] = '\0';
}

static esp_err_t hex_to_bin(const char* hex, uint8_t* bin_out, size_t bin_len)
{
    if (NULL == hex) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t hex_len = strlen(hex);
    if (hex_len != bin_len * 2) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < bin_len; i++) {
        unsigned int byte;
        if (1 != sscanf(hex + (i * 2), "%02x", &byte)) {
            return ESP_ERR_INVALID_ARG;
        }
        bin_out[i] = (uint8_t)byte;
    }

    return ESP_OK;
}

/**
 * @brief Compute SHA-256( salt || password ) into hash_out.
 */
static esp_err_t compute_hash(const uint8_t salt[AUTH_CRYPTO_SALT_LEN],
                               const char* password,
                               uint8_t hash_out[AUTH_CRYPTO_HASH_LEN])
{
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);

    int ret = mbedtls_sha256_starts(&ctx, 0);  /* 0 = SHA-256 (not 224) */
    if (0 != ret) { goto cleanup; }

    ret = mbedtls_sha256_update(&ctx, salt, AUTH_CRYPTO_SALT_LEN);
    if (0 != ret) { goto cleanup; }

    ret = mbedtls_sha256_update(&ctx, (const uint8_t*)password, strlen(password));
    if (0 != ret) { goto cleanup; }

    ret = mbedtls_sha256_finish(&ctx, hash_out);

cleanup:
    mbedtls_sha256_free(&ctx);
    return (0 == ret) ? ESP_OK : ESP_FAIL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t auth_crypto_hash_password(const char* password,
                                    uint8_t salt_out[AUTH_CRYPTO_SALT_LEN],
                                    uint8_t hash_out[AUTH_CRYPTO_HASH_LEN])
{
    if ((NULL == password) || (NULL == salt_out) || (NULL == hash_out)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_fill_random(salt_out, AUTH_CRYPTO_SALT_LEN);
    return compute_hash(salt_out, password, hash_out);
}

bool auth_crypto_verify_password(const char* password,
                                 const uint8_t salt[AUTH_CRYPTO_SALT_LEN],
                                 const uint8_t hash[AUTH_CRYPTO_HASH_LEN])
{
    if ((NULL == password) || (NULL == salt) || (NULL == hash)) {
        return false;
    }

    uint8_t computed[AUTH_CRYPTO_HASH_LEN];
    if (ESP_OK != compute_hash(salt, password, computed)) {
        return false;
    }

    /* Constant-time comparison — prevents timing attacks */
    return (0 == mbedtls_ct_memcmp(computed, hash, AUTH_CRYPTO_HASH_LEN));
}

void auth_crypto_salt_to_hex(const uint8_t salt[AUTH_CRYPTO_SALT_LEN],
                             char hex_out[AUTH_CRYPTO_SALT_HEX])
{
    bin_to_hex(salt, AUTH_CRYPTO_SALT_LEN, hex_out);
}

esp_err_t auth_crypto_hex_to_salt(const char* hex,
                                  uint8_t salt_out[AUTH_CRYPTO_SALT_LEN])
{
    return hex_to_bin(hex, salt_out, AUTH_CRYPTO_SALT_LEN);
}

void auth_crypto_hash_to_hex(const uint8_t hash[AUTH_CRYPTO_HASH_LEN],
                             char hex_out[AUTH_CRYPTO_HASH_HEX])
{
    bin_to_hex(hash, AUTH_CRYPTO_HASH_LEN, hex_out);
}

esp_err_t auth_crypto_hex_to_hash(const char* hex,
                                  uint8_t hash_out[AUTH_CRYPTO_HASH_LEN])
{
    return hex_to_bin(hex, hash_out, AUTH_CRYPTO_HASH_LEN);
}
