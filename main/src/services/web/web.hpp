#pragma once

#include <cstdint>

struct WebConfig {
    uint16_t port;
    const char *spiffs_base_path;  // 静态文件根路径(需与 storage 挂载路径一致)
};

// 启动 HTTP 配置服务器
void web_start(const WebConfig &cfg);
