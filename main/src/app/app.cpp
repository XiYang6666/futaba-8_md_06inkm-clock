#include "app.h"

#include <cstring>
#include <ctime>
#include <chrono>
#include <format>
#include <string>
#include <vector>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "services/wifi/wifi.hpp"
#include "utils/page_manager.hpp"
#include "utils/scroll_screen.hpp"

constexpr auto max_fps = 100;

// 页面上下文
struct AppContext {
    ScrollScreen<8> &screen;
    bool updated;
};

using AppPage = PageManager<AppContext>::Page;

// ---- 全局单例状态 ----
static vfd_t *vfd = nullptr;
static button_context_t *button = nullptr;
static ScrollScreen<8> screen({
    .get_startup_time = [] { return esp_timer_get_time() / 1000; },
    .set_screen = [](const char *text) { vfd_show_string(vfd, text); },
});
static PageManager<AppContext> page_mgr;
static std::vector<AppPage> pages;
static AppPage setup_page;
static size_t current = 0;
static bool updated = true;
static time_t last_time = 0;
static bool last_colon = false;


static void change_page(const PageManager<AppContext>::Page &page) {
    updated = true;
    page_mgr.set_page(page);
}

static void change_page(const size_t index) {
    updated = true;
    current = index;
    page_mgr.set_page(pages[current]);
}

static void next_page() {
    updated = true;
    current = (current + 1) % pages.size();
    change_page(pages[current]);
}

// 配网提示页: 未连接 WiFi 时显示 SSID/IP
static void page_setup(const AppContext &ctx, PageManager<AppContext> *mgr) {
    if (wifi_is_connected()) {
        updated = true;
        mgr->set_page(pages[0]);
        return;
    }
    if (!ctx.updated) return;

    ctx.screen.update(ScrollScreen<8>::Status{
        .text = std::format("SSID:{}  IP:{}", wifi_ap_ssid(), wifi_ap_ip()),
        .scroll = true,
    });
}

// 时钟页: HH:MM:SS, 冒号每 500ms 闪烁
static void page_clock(const AppContext &ctx, PageManager<AppContext> *mgr) {
    const auto button_stats = button_read(button);
    if (button_stats.up) next_page();
    if (!wifi_is_connected()) change_page(setup_page);

    const time_t now = time(nullptr);
    const bool colon = esp_timer_get_time() / (500 * 1000) % 2 == 0;
    if (!ctx.updated && now == last_time && colon == last_colon) return;
    last_time = now;
    last_colon = colon;

    tm ti = {};
    localtime_r(&now, &ti);
    const char c = colon ? ':' : ' ';
    ctx.screen.update(ScrollScreen<8>::Status{
        .text = std::format("{:02d}{}{:02d}{}{:02d}", ti.tm_hour, c, ti.tm_min, c, ti.tm_sec),
        .scroll = false,
    });
}

// 日期页: YY-MM-DD
static void page_date(const AppContext &ctx, PageManager<AppContext> *mgr) {
    const auto button_stats = button_read(button);
    if (button_stats.up) next_page();
    if (button_stats.duration > 3000 * 1000) change_page(0);
    if (!wifi_is_connected()) change_page(setup_page);
    if (!ctx.updated) return;

    const time_t now = time(nullptr);
    tm ti = {};
    localtime_r(&now, &ti);

    ctx.screen.update(ScrollScreen<8>::Status{
        .text = std::format("{:02d}-{:02d}-{:02d}", (ti.tm_year + 1900) % 100, ti.tm_mon + 1, ti.tm_mday),
        .scroll = false,
    });
}

// 星期与周数页: Mon W38 (ISO week)
static void page_week(const AppContext &ctx, PageManager<AppContext> *mgr) {
    const auto button_stats = button_read(button);
    if (button_stats.up) next_page();
    if (button_stats.duration > 3000 * 1000) change_page(0);
    if (!wifi_is_connected()) change_page(setup_page);
    if (!ctx.updated) return;

    const time_t now = time(nullptr);
    tm ti = {};
    localtime_r(&now, &ti);

    using namespace std::chrono;
    // 用本地 tm 字段构造日历日期，避免再引入时区换算
    const sys_days local_date = year{ti.tm_year + 1900} /
                                month{static_cast<unsigned>(ti.tm_mon + 1)} /
                                day{static_cast<unsigned>(ti.tm_mday)};

    ctx.screen.update(ScrollScreen<8>::Status{
        .text = std::format("{:%a  W%V}", local_date),
        .scroll = false,
    });
}

// 组装各页面到状态机
static void build_pages() {
    setup_page = page_setup;
    pages.emplace_back(page_clock);
    pages.emplace_back(page_date);
    pages.emplace_back(page_week);
    page_mgr.set_page(pages[0]);
}

extern "C" void app_init(vfd_t *vfd_dev, button_context_t *button_dev) {
    vfd = vfd_dev;
    button = button_dev;
    build_pages();
}

extern "C" void app_run() {
    constexpr auto duration_per_frame = 1e6 / max_fps;
    while (true) {
        const auto t0 = esp_timer_get_time();
        const bool just_updated = updated;
        updated = false;
        page_mgr.run(AppContext{.screen = screen, .updated = just_updated});
        screen.draw();
        const auto duration = esp_timer_get_time() - t0;
        if (duration >= duration_per_frame) continue;
        vTaskDelay(pdMS_TO_TICKS((duration- duration_per_frame)/1000));
    }
}
