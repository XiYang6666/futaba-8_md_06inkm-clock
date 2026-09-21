#pragma once

#include <vector>

#include "esp_log.h"
#include "esp_sntp.h"

struct SntpConfig {
    std::vector<const char *> servers;
    const char *tz;
};

// 启动 SNTP 时间同步(幂等)
inline void sntp_start(const SntpConfig &cfg) {
    static bool started = false; // inline 函数的函数局部 static 在所有 TU 间共享
    if (started) return;
    started = true;

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    for (size_t i = 0; i < cfg.servers.size() && i < 3; ++i) {
        esp_sntp_setservername(static_cast<uint8_t>(i), cfg.servers[i]);
    }

    setenv("TZ", cfg.tz, 1);
    tzset();

    esp_sntp_init();
    ESP_LOGI("sntp", "SNTP started");
}
