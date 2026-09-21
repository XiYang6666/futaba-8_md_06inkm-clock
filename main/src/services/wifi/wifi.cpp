#include "wifi.hpp"

#include <cassert>
#include <cstring>
#include <format>
#include <string>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs.h"

#define WIFI_NVS_NAMESPACE "wifi"
#define WIFI_NVS_KEY_SSID "ssid"
#define WIFI_NVS_KEY_PASS "password"

static const char *TAG = "wifi";

static esp_netif_t *sta_netif = nullptr;
static esp_netif_t *ap_netif = nullptr;
static bool sta_configured = false;
static bool wifi_connected = false;

static std::string ap_ssid;
static std::string ap_ip = "192.168.4.1";
static std::function<void()> on_connected;

static bool load_credentials(std::string &ssid, std::string &password) {
    nvs_handle_t handle;
    if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return false;

    char buf[65];
    size_t len = sizeof(buf);
    if (nvs_get_str(handle, WIFI_NVS_KEY_SSID, buf, &len) != ESP_OK || len == 0) {
        nvs_close(handle);
        return false;
    }

    ssid.assign(buf, len - 1); // nvs_get_str 的长度包含结尾的 '\0'
    len = sizeof(buf);
    if (nvs_get_str(handle, WIFI_NVS_KEY_PASS, buf, &len) == ESP_OK && len > 0) {
        password.assign(buf, len - 1);
    }
    nvs_close(handle);
    return true;
}

static void update_ap_ip() {
    if (ap_netif == nullptr) return;
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(ap_netif, &ip_info) != ESP_OK) return;

    const auto &ip = ip_info.ip;
    ap_ip = std::format("{}.{}.{}.{}",
                        esp_ip4_addr1_16(&ip), esp_ip4_addr2_16(&ip),
                        esp_ip4_addr3_16(&ip), esp_ip4_addr4_16(&ip));
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (sta_configured) esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        if (sta_configured) esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        if (on_connected) on_connected();
    }
}

void wifi_init(const WifiConfig &cfg) {
    ap_ssid = cfg.ap_ssid;
    on_connected = cfg.on_connected;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    sta_netif = esp_netif_create_default_wifi_sta();
    ap_netif = esp_netif_create_default_wifi_ap();
    assert(sta_netif != nullptr && ap_netif != nullptr);

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    // AP: 未设置 wifi 时提供配网入口
    wifi_config_t ap_cfg = {};
    strncpy(reinterpret_cast<char *>(ap_cfg.ap.ssid), ap_ssid.c_str(), sizeof(ap_cfg.ap.ssid) - 1);
    ap_cfg.ap.ssid_len = static_cast<uint8_t>(ap_ssid.size());
    ap_cfg.ap.channel = cfg.ap_channel;
    ap_cfg.ap.max_connection = cfg.ap_max_conn;
    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));

    // STA: 从 NVS 读取已保存的 wifi 凭据
    std::string ssid;
    std::string password;
    if (load_credentials(ssid, password) && !ssid.empty()) {
        wifi_config_t sta_cfg = {};
        strncpy(reinterpret_cast<char *>(sta_cfg.sta.ssid), ssid.c_str(), sizeof(sta_cfg.sta.ssid) - 1);
        strncpy(reinterpret_cast<char *>(sta_cfg.sta.password), password.c_str(), sizeof(sta_cfg.sta.password) - 1);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
        sta_configured = true;
        ESP_LOGI(TAG, "Using saved WiFi: %s", ssid.c_str());
    } else {
        ESP_LOGW(TAG, "No WiFi credentials saved, running as AP only");
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    // Super Mini 板载稳压器扛不住默认峰值功率, 降到 8.5dBm 避免断连/重启
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(34));
    ESP_ERROR_CHECK(esp_netif_set_hostname(sta_netif, cfg.hostname));

    update_ap_ip();
    ESP_LOGI(TAG, "AP SSID: %s, IP: %s", ap_ssid.c_str(), ap_ip.c_str());
}

bool wifi_is_connected() {
    return wifi_connected;
}

const std::string &wifi_ap_ssid() {
    return ap_ssid;
}

const std::string &wifi_ap_ip() {
    return ap_ip;
}

void wifi_save_credentials(const std::string &ssid, const std::string &password) {
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle));
    ESP_ERROR_CHECK(nvs_set_str(handle, WIFI_NVS_KEY_SSID, ssid.c_str()));
    ESP_ERROR_CHECK(nvs_set_str(handle, WIFI_NVS_KEY_PASS, password.c_str()));
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
    ESP_LOGI(TAG, "WiFi credentials saved");
}
