#pragma once

#include <cstdint>
#include <functional>
#include <string>

struct WifiConfig {
    const char *ap_ssid;
    uint8_t ap_channel;
    uint8_t ap_max_conn;
    const char *hostname;
    std::function<void()> on_connected; // STA 连上后回调(启动时间同步等)
};

void wifi_init(const WifiConfig &cfg);

bool wifi_is_connected();

const std::string &wifi_ap_ssid();

const std::string &wifi_ap_ip();

// 把配网页面提交的凭据写入 NVS
void wifi_save_credentials(const std::string &ssid, const std::string &password);
