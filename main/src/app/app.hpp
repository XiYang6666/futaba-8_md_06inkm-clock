#pragma once

#include <ctime>
#include <format>
#include <string>
#include <vector>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "drivers/button.h"
#include "drivers/vfd.h"
#include "services/wifi/wifi.hpp"
#include "utils/page_manager.hpp"
#include "utils/scroll_screen.hpp"

// 时钟 UI: 时钟/日期/配网提示三个页面 + 主循环
class ClockApp {
public:
    ClockApp(vfd_t *vfd, button_context_t *button)
        : vfd_(vfd),
          screen_({
              .get_startup_time = [] { return esp_timer_get_time() / 1000; },
              .set_screen = [this](const char *text) { vfd_show_string(vfd_, text); },
          }),
          button_(button) {
        build_pages_();
    }

    void run() {
        while (true) {
            const bool just_updated = updated_;
            updated_ = false;
            page_mgr_.run(Context{.screen = screen_, .updated = just_updated});
            screen_.draw();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

private:
    struct Context {
        ScrollScreen<8> &screen;
        bool updated;
    };

    using Page = PageManager<Context>::Page;

    vfd_t *vfd_;
    ScrollScreen<8> screen_;
    button_context_t *button_;

    PageManager<Context> page_mgr_;
    std::vector<Page> pages_;
    Page setup_page_;
    size_t current_ = 0;
    bool updated_ = true;
    time_t last_time_ = 0;
    bool last_colon_ = false;

    void build_pages_() {
        // 未连接 WiFi 时提示 SSID/IP 用于配网
        setup_page_ = [this](const Context &ctx, PageManager<Context> *mgr) {
            if (wifi_is_connected()) {
                updated_ = true;
                mgr->set_page(pages_[0]);
                return;
            }
            if (!ctx.updated) return;

            ctx.screen.update(ScrollScreen<8>::Status{
                .text = std::format("SSID:{}  IP:{}", wifi_ap_ssid(), wifi_ap_ip()),
                .scroll = true,
            });
        };


        // 时钟
        pages_.emplace_back([this](const Context &ctx, PageManager<Context> *mgr) {
            const auto button_stats = button_read(button_);
            if (button_stats.up) next_page_();
            if (!wifi_is_connected()) {
                updated_ = true;
                mgr->set_page(setup_page_);
                return;
            }

            const time_t now = time(nullptr);
            const bool colon = esp_timer_get_time() / (500 * 1000) % 2 == 0; // 冒号每 500ms 闪烁
            if (!ctx.updated && now == last_time_ && colon == last_colon_) return;
            last_time_ = now;
            last_colon_ = colon;

            tm ti = {};
            localtime_r(&now, &ti);
            const char c = colon ? ':' : ' '; // 冒号熄灭时用空格占位
            ctx.screen.update(ScrollScreen<8>::Status{
                .text = std::format("{:02d}{}{:02d}{}{:02d}", ti.tm_hour, c, ti.tm_min, c, ti.tm_sec),
                .scroll = false,
            });
        });

        // 日期
        pages_.emplace_back([this](const Context &ctx, PageManager<Context> *mgr) {
            const auto button_stats = button_read(button_);
            if (button_stats.up) next_page_();
            if (!wifi_is_connected()) {
                updated_ = true;
                mgr->set_page(setup_page_);
                return;
            }
            if (!ctx.updated) return;

            const time_t now = time(nullptr);
            tm ti = {};
            localtime_r(&now, &ti);
            ctx.screen.update(ScrollScreen<8>::Status{
                .text = std::format("{:02d}-{:02d}-{:02d}", (ti.tm_year + 1900) % 100, ti.tm_mon + 1, ti.tm_mday),
                .scroll = false,
            });
        });

        page_mgr_.set_page(pages_[0]);
    }

    void next_page_() {
        updated_ = true;
        current_ = (current_ + 1) % pages_.size();
        page_mgr_.set_page(pages_[current_]);
    }
};
