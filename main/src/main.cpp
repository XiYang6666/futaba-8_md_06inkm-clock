#include "config.h"

#include "app/app.h"
#include "drivers/button.h"
#include "drivers/vfd.h"
#include "services/dns/dns.hpp"
#include "services/sntp/sntp.hpp"
#include "services/storage/storage.hpp"
#include "services/web/web.hpp"
#include "services/wifi/wifi.hpp"

extern "C" void app_main() {
    storage_init({
        .spiffs_base_path = SPIFFS_BASE_PATH,
        .spiffs_label = SPIFFS_LABEL,
    });

    vfd_t vfd;
    constexpr vfd_config_t vfd_cfg = {
        .da = VFD_PIN_DA,
        .clk = VFD_PIN_CLK,
        .cs = VFD_PIN_CS,
        .rst = VFD_PIN_RST,
        .dimming = VFD_DIMMING,
    };
    ESP_ERROR_CHECK(vfd_init(&vfd, &vfd_cfg));

    button_context_t button;
    ESP_ERROR_CHECK(button_init(&button, BUTTON_PIN));

    wifi_init({
        .ap_ssid = AP_SSID,
        .ap_channel = AP_CHANNEL,
        .ap_max_conn = AP_MAX_CONN,
        .hostname = HOSTNAME,
        .on_connected = [] {
            sntp_start({
                .servers = {"ntp.aliyun.com", "cn.pool.ntp.org"},
                .tz = TZ_SETTING,
            });
        },
    });

    web_start({
        .port = HTTP_PORT,
        .spiffs_base_path = SPIFFS_BASE_PATH,
    });

    dns_captive_start();

    app_init(&vfd, &button);
    app_run();
}
