#pragma once

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"

struct StorageConfig {
    const char *spiffs_base_path;
    const char *spiffs_label;
};

// 初始化 NVS 并挂载 SPIFFS
inline void storage_init(const StorageConfig &cfg) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_vfs_spiffs_conf_t conf = {
        .base_path = cfg.spiffs_base_path,
        .partition_label = cfg.spiffs_label,
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    if (esp_err_t err = esp_vfs_spiffs_register(&conf); err != ESP_OK) {
        ESP_LOGW("storage", "SPIFFS unavailable: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI("storage", "SPIFFS mounted at %s", conf.base_path);
}
