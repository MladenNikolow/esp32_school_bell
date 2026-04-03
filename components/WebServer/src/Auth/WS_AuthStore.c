#include "WS_AuthStore.h"
#include "WS_AuthCrypto.h"

#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "sdkconfig.h"

static const char* TAG = "AUTH_STORE";

/* ------------------------------------------------------------------ */
/* NVS namespace and keys                                              */
/* ------------------------------------------------------------------ */
#define AUTH_NVS_NAMESPACE  "auth"

#define KEY_SVC_SALT    "svc_salt"
#define KEY_SVC_HASH    "svc_hash"
#define KEY_CLI_USER    "cli_user"
#define KEY_CLI_SALT    "cli_salt"
#define KEY_CLI_HASH    "cli_hash"
#define KEY_CLI_EXISTS  "cli_exists"

/* Service username — compile-time constant from Kconfig */
#define SVC_USERNAME    CONFIG_WS_AUTH_USERNAME
#define SVC_PASSWORD    CONFIG_WS_AUTH_PASSWORD

/* ------------------------------------------------------------------ */
/* NVS helpers                                                         */
/* ------------------------------------------------------------------ */
static esp_err_t nvs_write_str(nvs_handle_t h, const char* key, const char* val)
{
    esp_err_t err = nvs_set_str(h, key, val);
    return err;
}

static esp_err_t nvs_read_str(nvs_handle_t h, const char* key, char* buf, size_t buf_len)
{
    size_t required = buf_len;
    return nvs_get_str(h, key, buf, &required);
}

/* ------------------------------------------------------------------ */
/* Init — provision service credentials on first boot                  */
/* ------------------------------------------------------------------ */
esp_err_t auth_store_init(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(AUTH_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (ESP_OK != err) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Check if service hash already exists */
    char existing[AUTH_CRYPTO_HASH_HEX];
    size_t len = sizeof(existing);
    err = nvs_get_str(h, KEY_SVC_HASH, existing, &len);

    if (ESP_ERR_NVS_NOT_FOUND == err) {
        /* First boot — hash the Kconfig default password */
        ESP_LOGI(TAG, "First boot: provisioning service account hash");

        uint8_t salt[AUTH_CRYPTO_SALT_LEN];
        uint8_t hash[AUTH_CRYPTO_HASH_LEN];
        err = auth_crypto_hash_password(SVC_PASSWORD, salt, hash);
        if (ESP_OK != err) {
            nvs_close(h);
            return err;
        }

        char salt_hex[AUTH_CRYPTO_SALT_HEX];
        char hash_hex[AUTH_CRYPTO_HASH_HEX];
        auth_crypto_salt_to_hex(salt, salt_hex);
        auth_crypto_hash_to_hex(hash, hash_hex);

        err = nvs_write_str(h, KEY_SVC_SALT, salt_hex);
        if (ESP_OK == err) {
            err = nvs_write_str(h, KEY_SVC_HASH, hash_hex);
        }
        if (ESP_OK == err) {
            err = nvs_commit(h);
        }

        if (ESP_OK != err) {
            ESP_LOGE(TAG, "Failed to provision service hash: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Service account provisioned");
        }
    } else if (ESP_OK == err) {
        ESP_LOGI(TAG, "Service account already provisioned");
    } else {
        ESP_LOGE(TAG, "Error reading service hash: %s", esp_err_to_name(err));
    }

    nvs_close(h);
    return (err == ESP_ERR_NVS_NOT_FOUND) ? ESP_FAIL : err;
}

/* ------------------------------------------------------------------ */
/* Service account verification                                        */
/* ------------------------------------------------------------------ */
bool auth_store_verify_service(const char* username, const char* password)
{
    if ((NULL == username) || (NULL == password)) {
        return false;
    }

    /* Username must match the compile-time constant */
    if (0 != strcmp(username, SVC_USERNAME)) {
        return false;
    }

    nvs_handle_t h;
    if (ESP_OK != nvs_open(AUTH_NVS_NAMESPACE, NVS_READONLY, &h)) {
        return false;
    }

    char salt_hex[AUTH_CRYPTO_SALT_HEX];
    char hash_hex[AUTH_CRYPTO_HASH_HEX];
    bool result = false;

    if (ESP_OK == nvs_read_str(h, KEY_SVC_SALT, salt_hex, sizeof(salt_hex))
        && ESP_OK == nvs_read_str(h, KEY_SVC_HASH, hash_hex, sizeof(hash_hex)))
    {
        uint8_t salt[AUTH_CRYPTO_SALT_LEN];
        uint8_t hash[AUTH_CRYPTO_HASH_LEN];

        if (ESP_OK == auth_crypto_hex_to_salt(salt_hex, salt)
            && ESP_OK == auth_crypto_hex_to_hash(hash_hex, hash))
        {
            result = auth_crypto_verify_password(password, salt, hash);
        }
    }

    nvs_close(h);
    return result;
}

/* ------------------------------------------------------------------ */
/* Client account                                                      */
/* ------------------------------------------------------------------ */
bool auth_store_client_exists(void)
{
    nvs_handle_t h;
    if (ESP_OK != nvs_open(AUTH_NVS_NAMESPACE, NVS_READONLY, &h)) {
        return false;
    }

    uint8_t flag = 0;
    esp_err_t err = nvs_get_u8(h, KEY_CLI_EXISTS, &flag);
    nvs_close(h);

    return (ESP_OK == err) && (1 == flag);
}

bool auth_store_verify_client(const char* username, const char* password)
{
    if ((NULL == username) || (NULL == password)) {
        return false;
    }

    if (!auth_store_client_exists()) {
        return false;
    }

    nvs_handle_t h;
    if (ESP_OK != nvs_open(AUTH_NVS_NAMESPACE, NVS_READONLY, &h)) {
        return false;
    }

    char stored_user[32] = {0};
    char salt_hex[AUTH_CRYPTO_SALT_HEX];
    char hash_hex[AUTH_CRYPTO_HASH_HEX];
    bool result = false;

    if (ESP_OK == nvs_read_str(h, KEY_CLI_USER, stored_user, sizeof(stored_user))
        && ESP_OK == nvs_read_str(h, KEY_CLI_SALT, salt_hex, sizeof(salt_hex))
        && ESP_OK == nvs_read_str(h, KEY_CLI_HASH, hash_hex, sizeof(hash_hex)))
    {
        if (0 == strcmp(username, stored_user)) {
            uint8_t salt[AUTH_CRYPTO_SALT_LEN];
            uint8_t hash[AUTH_CRYPTO_HASH_LEN];

            if (ESP_OK == auth_crypto_hex_to_salt(salt_hex, salt)
                && ESP_OK == auth_crypto_hex_to_hash(hash_hex, hash))
            {
                result = auth_crypto_verify_password(password, salt, hash);
            }
        }
    }

    nvs_close(h);
    return result;
}

esp_err_t auth_store_set_client(const char* username, const char* password)
{
    if ((NULL == username) || (NULL == password)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strlen(username) == 0 || strlen(username) > 31) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(password) < 8) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t salt[AUTH_CRYPTO_SALT_LEN];
    uint8_t hash[AUTH_CRYPTO_HASH_LEN];
    esp_err_t err = auth_crypto_hash_password(password, salt, hash);
    if (ESP_OK != err) {
        return err;
    }

    char salt_hex[AUTH_CRYPTO_SALT_HEX];
    char hash_hex[AUTH_CRYPTO_HASH_HEX];
    auth_crypto_salt_to_hex(salt, salt_hex);
    auth_crypto_hash_to_hex(hash, hash_hex);

    nvs_handle_t h;
    err = nvs_open(AUTH_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (ESP_OK != err) {
        return err;
    }

    err = nvs_write_str(h, KEY_CLI_USER, username);
    if (ESP_OK == err) { err = nvs_write_str(h, KEY_CLI_SALT, salt_hex); }
    if (ESP_OK == err) { err = nvs_write_str(h, KEY_CLI_HASH, hash_hex); }
    if (ESP_OK == err) { err = nvs_set_u8(h, KEY_CLI_EXISTS, 1); }
    if (ESP_OK == err) { err = nvs_commit(h); }

    nvs_close(h);

    if (ESP_OK == err) {
        ESP_LOGI(TAG, "Client account set: %s", username);
    } else {
        ESP_LOGE(TAG, "Failed to set client account: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t auth_store_delete_client(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(AUTH_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (ESP_OK != err) {
        return err;
    }

    /* Erase all client keys — ignore NOT_FOUND errors */
    nvs_erase_key(h, KEY_CLI_USER);
    nvs_erase_key(h, KEY_CLI_SALT);
    nvs_erase_key(h, KEY_CLI_HASH);
    nvs_erase_key(h, KEY_CLI_EXISTS);
    err = nvs_commit(h);

    nvs_close(h);

    ESP_LOGI(TAG, "Client account deleted");
    return err;
}

esp_err_t auth_store_get_client_username(char* out, size_t len)
{
    if ((NULL == out) || (0 == len)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!auth_store_client_exists()) {
        return ESP_ERR_NOT_FOUND;
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open(AUTH_NVS_NAMESPACE, NVS_READONLY, &h);
    if (ESP_OK != err) {
        return err;
    }

    err = nvs_read_str(h, KEY_CLI_USER, out, len);
    nvs_close(h);
    return err;
}
