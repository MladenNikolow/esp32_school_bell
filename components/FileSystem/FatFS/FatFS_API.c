
#include <dirent.h> 
#include <sys/stat.h> 
#include <string.h>
#include <errno.h>
#include "FatFS_API.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "esp_log.h"
#include "wear_levelling.h"


static const char *TAG = "fatfs";
static wl_handle_t hWearLevelling = WL_INVALID_HANDLE;

esp_err_t
FatFS_Init()
{
    esp_err_t espRslt = ESP_OK;
    const char* pcBasePath = "/react";

    esp_vfs_fat_mount_config_t tFatFsConfig =
    {
        .format_if_mount_failed = true,
        .max_files = 10,
        .allocation_unit_size = 4096
    };

    espRslt = esp_vfs_fat_spiflash_mount_rw_wl(
        pcBasePath,          // Mount point
        "fatfs-react",       // Partition label
        &tFatFsConfig,       // FATFS config
        &hWearLevelling      // Wear leveling handle
    );

    if (espRslt != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(espRslt));
    }
    else
    {
        ESP_LOGI(TAG, "FATFS mounted at %s", pcBasePath);
    }

    return espRslt;
}

void debug_list_react_assets(void)
{
    const char *base = "/react";
    const char *assets = "/react/assets";
    ESP_LOGI(TAG, "Checking mount point %s", base);

    struct stat st;
    if (stat(base, &st) != 0) {
        ESP_LOGE(TAG, "stat(%s) failed: %s", base, strerror(errno));
        return;
    }
    ESP_LOGI(TAG, "%s exists, mode=0%o", base, st.st_mode);

    /* Check assets directory */
    if (stat(assets, &st) != 0) {
        ESP_LOGE(TAG, "stat(%s) failed: %s", assets, strerror(errno));
        return;
    }
    ESP_LOGI(TAG, "%s exists, mode=0%o", assets, st.st_mode);

    DIR *d = opendir(assets);
    if (!d) {
        ESP_LOGE(TAG, "opendir %s failed: %s", assets, strerror(errno));
        return;
    }

    ESP_LOGI(TAG, "Listing entries in %s", assets);
    struct dirent *e;
    bool found_exact = false;
    bool found_index_variant = false;
    const char *target_name = "index-CrpXu6zg.js.gz";
    char pathbuf[256];

    while ((e = readdir(d)) != NULL) {
        ESP_LOGI(TAG, "entry: %s", e->d_name);

        /* Check exact filename */
        if (strcmp(e->d_name, target_name) == 0) {
            found_exact = true;
        }

        /* Check for any index-*.js.gz variant */
        if (strncmp(e->d_name, "index-", 6) == 0) {
            size_t len = strlen(e->d_name);
            if (len > 7 && strcmp(e->d_name + len - strlen(".js.gz"), ".js.gz") == 0) {
                ESP_LOGI(TAG, "Found index variant: %s", e->d_name);
                found_index_variant = true;
            }
        }
    }
    closedir(d);

    /* Try to open the exact file path */
    snprintf(pathbuf, sizeof(pathbuf), "%s/%s", assets, target_name);
    FILE *f = fopen(pathbuf, "rb");
    if (!f) {
        ESP_LOGE(TAG, "fopen %s failed: %s", pathbuf, strerror(errno));
        if (!found_exact) {
            ESP_LOGI(TAG, "Exact file not present. Searching for alternatives in %s", assets);
            /* Re-open to list contents again and show alternatives */
            DIR *d2 = opendir(assets);
            if (!d2) {
                ESP_LOGE(TAG, "opendir %s failed: %s", assets, strerror(errno));
                return;
            }
            while ((e = readdir(d2)) != NULL) {
                /* Skip . and .. */
                if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
                ESP_LOGI(TAG, "assets contains: %s", e->d_name);
            }
            closedir(d2);

            if (!found_index_variant) {
                ESP_LOGW(TAG, "No index-*.js.gz variant found in assets. You may need to extract files or upload per-file gzipped JS.");
            } else {
                ESP_LOGI(TAG, "One or more index-*.js.gz files exist; consider mapping requests to the .js.gz files with Content-Encoding: gzip.");
            }
        }
    } else {
        /* File exists, report size */
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fclose(f);
        ESP_LOGI(TAG, "%s exists, size=%ld bytes", pathbuf, size);
    }
}