#include "web.hpp"

#include <cstdio>
#include <format>
#include <string>
#include <vector>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_system.h"

#include "services/wifi/wifi.hpp"

static auto TAG = "web";

static httpd_handle_t http_server = nullptr;
static auto spiffs_base_path = "/spiffs";

static esp_err_t serve_file(httpd_req_t *req, const char *path, const char *type) {
    FILE *f = fopen(path, "rb");
    if (f == nullptr) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string body(static_cast<size_t>(size), '\0');
    const size_t read = fread(body.data(), 1, body.size(), f);
    fclose(f);

    httpd_resp_set_type(req, type);
    return httpd_resp_send(req, body.data(), static_cast<ssize_t>(read));
}

static esp_err_t root_handler(httpd_req_t *req) {
    return serve_file(req, std::format("{}/index.html", spiffs_base_path).c_str(), "text/html; charset=utf-8");
}

static std::string escape_json(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char c: s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

static esp_err_t scan_handler(httpd_req_t *req) {
    // 被动扫描: AP 有客户端连接时, 主动扫描会极慢甚至卡住, 被动扫描只监听 beacon
    wifi_scan_config_t scan_cfg = {};
    scan_cfg.scan_type = WIFI_SCAN_TYPE_PASSIVE;
    scan_cfg.scan_time.passive = 120;
    scan_cfg.show_hidden = false;

    if (esp_wifi_scan_start(&scan_cfg, true) != ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "[]", 2);
    }

    uint16_t count = 0;
    if (esp_wifi_scan_get_ap_num(&count) != ESP_OK) count = 0;
    if (count > 32) count = 32;

    // wifi_ap_record_t 很大, 不能放在 httpd 的小栈上
    std::vector<wifi_ap_record_t> records(count);
    if (count > 0) esp_wifi_scan_get_ap_records(&count, records.data());
    ESP_LOGI(TAG, "Scan found %u APs", count);

    std::string json = "[";
    for (uint16_t i = 0; i < count; ++i) {
        if (i > 0) json += ',';
        json += std::format(
            R"({{"ssid":"{}","rssi":{},"auth":{}}})",
            escape_json(reinterpret_cast<const char *>(records[i].ssid)),
            static_cast<int>(records[i].rssi),
            static_cast<int>(records[i].authmode)
        );
    }
    json += ']';

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

static esp_err_t captive_redirect_handler(httpd_req_t *req, httpd_err_code_t err) {
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS 需要响应体才能识别 captive portal, 仅 302 不够
    httpd_resp_send(req, "Redirect to captive portal", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static int hex_value(const char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static std::string url_decode(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            out.push_back(static_cast<char>(hex_value(s[i + 1]) * 16 + hex_value(s[i + 2])));
            i += 2;
        } else if (s[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

static bool parse_form(const char *body, std::string &ssid, std::string &password) {
    const char *ssid_at = strstr(body, "ssid=");
    const char *pass_at = strstr(body, "password=");
    if (ssid_at == nullptr || pass_at == nullptr) return false;

    ssid_at += 5;
    pass_at += 9;

    const char *ssid_end = strchr(ssid_at, '&');
    const char *pass_end = strchr(pass_at, '&');

    ssid = url_decode(std::string(ssid_at, ssid_end ? ssid_end - ssid_at : strlen(ssid_at)));
    password = url_decode(std::string(pass_at, pass_end ? pass_end - pass_at : strlen(pass_at)));
    return !ssid.empty();
}

static esp_err_t http_post_handler(httpd_req_t *req) {
    char body[512];
    int received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) return httpd_resp_send_500(req);
    body[received] = '\0';

    std::string ssid;
    std::string password;
    httpd_resp_set_type(req, "text/html; charset=utf-8");

    if (!parse_form(body, ssid, password)) {
        return httpd_resp_send(req, "<h3>参数错误</h3>", HTTPD_RESP_USE_STRLEN);
    }

    wifi_save_credentials(ssid, password);
    httpd_resp_send(req, "<h3>已保存, 正在重启...</h3>", HTTPD_RESP_USE_STRLEN);
    esp_restart();
    return ESP_OK;
}

// 路由表: 新增页面/接口只需在这里登记一行
struct Route {
    const char *uri;
    httpd_method_t method;

    esp_err_t (*handler)(httpd_req_t *);
};

static const Route routes[] = {
    {"/", HTTP_GET, root_handler},
    {"/scan", HTTP_GET, scan_handler},
    {"/config", HTTP_POST, http_post_handler},
};

static void start_http_server(const uint16_t port) {
    // captive portal 会产生大量无效请求, 压低 httpd 日志
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    // std::format 栈帧较大, 默认 4096 不够用
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    ESP_ERROR_CHECK(httpd_start(&http_server, &config));

    for (const auto &route: routes) {
        const httpd_uri_t uri = {
            .uri = route.uri,
            .method = route.method,
            .handler = route.handler,
            .user_ctx = nullptr,
        };
        ESP_ERROR_CHECK(httpd_register_uri_handler(http_server, &uri));
    }
    // 未匹配的请求(含 captive portal 探测)一律重定向到配网页
    ESP_ERROR_CHECK(httpd_register_err_handler(http_server, HTTPD_404_NOT_FOUND, captive_redirect_handler));
    ESP_LOGI(TAG, "HTTP server started");
}

void web_start(const WebConfig &cfg) {
    spiffs_base_path = cfg.spiffs_base_path;
    start_http_server(cfg.port);
}
