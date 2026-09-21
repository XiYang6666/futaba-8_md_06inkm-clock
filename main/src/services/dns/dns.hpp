#pragma once

#include "esp_log.h"
#include "esp_netif.h"
#include "dns_server.h"

// 启动 captive portal DNS: 把所有 A 查询解析到 AP 自己的 IP
inline void dns_captive_start() {
    // wildcard "*": 未匹配的域名也返回 AP IP, 实现 DNS 劫持
    dns_server_config_t dns_cfg = {
        .num_of_entries = 1,
        .item = {
            {
                .name = "*",
                .if_key = "WIFI_AP_DEF",
                .ip = {},
            }
        },
    };
    start_dns_server(&dns_cfg);
    ESP_LOGI("dns", "DNS server started");
}
