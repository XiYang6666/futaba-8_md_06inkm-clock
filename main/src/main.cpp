#include <functional>
#include <iostream>
#include <string>
#include <utility>

#include "soc/gpio_num.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "ftb-8-md.h"

#define VFD_EN GPIO_NUM_4
#define VFD_RST GPIO_NUM_3
#define VFD_CS GPIO_NUM_2
#define VFD_CLK GPIO_NUM_1
#define VFD_DA GPIO_NUM_0

#define BUTTON_DA GPIO_NUM_21

class VfdScreen {
public:
    struct Config {
        std::function<int64_t()> get_startup_time;
        std::function<void(const char *)> set_screen;
    } cfg;

    explicit VfdScreen(Config cfg) : cfg(std::move(cfg)) {
    }

    struct Status {
        std::string text;
        bool scroll = true;
        int scroll_blank = 3;
        int char_stay_time = 200;
    };

    void update(const Status &status) {
        current_status = status;
        start_scroll_time = cfg.get_startup_time();
        text_buffer.clear();

        if (status.scroll && status.text.length() > width) {
            text_buffer.reserve(status.text.size() + status.scroll_blank + width - 1);
            text_buffer.append(status.text);
            text_buffer.append(status.scroll_blank, ' ');
            text_buffer.append(status.text, 0, width - 1);
        } else {
            text_buffer.reserve(std::max(status.text.size(), width));
            text_buffer.append(status.text);
            if (status.text.length() < width) text_buffer.append(width - status.text.length(), ' ');
        }
    }

    void draw() {
        if (current_status.scroll && current_status.text.length() > width) {
            const auto content_length = current_status.text.length() + current_status.scroll_blank;
            const auto duration = cfg.get_startup_time() - start_scroll_time;
            const auto current_pos = duration / current_status.char_stay_time % content_length;
            cfg.set_screen(text_buffer.c_str() + current_pos);
        } else {
            cfg.set_screen(text_buffer.c_str());
        }
    }

private:
    constexpr static unsigned int width = 8;

    Status current_status = Status();
    std::string text_buffer;
    int64_t start_scroll_time = 0;
};

template<typename Context>
class PageManager {
public:
    using Page = std::function<void(const Context &, PageManager *)>;

    void run(const Context &ctx) {
        current_page(ctx, this);
    }

    void set_page(const Page &page) {
        this->current_page = page;
    }

private:
    Page current_page = [](const Context &, PageManager *) {};
};

// 初始化 vfd

static spi_device_handle_t vfd = nullptr;

static esp_err_t init_vfd() {
    // Initialize SPI bus
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = VFD_DA;
    bus_cfg.miso_io_num = -1;
    bus_cfg.sclk_io_num = VFD_CLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 32;
    spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

    // Register VFD device
    vfd = ftb8md_device_register(
        SPI2_HOST,
        VFD_CS, // CS pin
        VFD_RST // Reset pin (use -1 if not connected)
    );

    if (vfd == nullptr) return ESP_ERR_INVALID_STATE;
    // Set brightness (0-240)
    ftb8md_set_dimming(vfd, 120);
    return ESP_OK;
}

// 初始化按钮

struct ButtonStatus {
    bool press: 1;
    bool up: 1;
    bool down: 1;
};

static bool button_last_status = false;

static esp_err_t init_button() {
    constexpr uint64_t bitmask = BIT64(BUTTON_DA);
    gpio_config_t io_conf;
    io_conf.pin_bit_mask = bitmask;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&io_conf);
}

static ButtonStatus read_button() {
    const auto current_status = gpio_get_level(BUTTON_DA);
    const auto result = ButtonStatus{
        .press = current_status != 0,
        .up = button_last_status && current_status == 0,
        .down = !button_last_status && current_status != 0,
    };
    button_last_status = current_status;
    return result;
}

// 主函数

struct Context {
    const VfdScreen &screen;
    bool updated;
};

extern "C" void app_main() {
    init_vfd();

    auto screen = VfdScreen({
        .get_startup_time = [] { return esp_timer_get_time() / 1000; },
        .set_screen = [](const char *content) { ftb8md_show_string(vfd, 0, content); },
    });

    auto page_mgr = PageManager<Context>();
    const auto setup_page = [](const Context &ctx, PageManager<Context> *mgr) {
        // 未设置 wifi, 显示 ssid/ip地址
    };

    int current_status = 0;
    bool updated = false;
    auto pages = std::vector<std::function<void(const Context &, PageManager<Context> *)> >();
    auto next_page = [&] {
        updated = true;
        current_status++;
        current_status %= pages.size();
        page_mgr.set_page(pages[current_status]);
    };
    auto check_wifi_status = [&] {
        return true;
    };
    // 时钟
    pages.emplace_back([&](const Context &ctx, PageManager<Context> *mgr) {
        if (read_button().up) next_page();
        if (!check_wifi_status()) mgr->set_page(setup_page);
    });
    // 日期
    pages.emplace_back([&](const Context &ctx, PageManager<Context> *mgr) {
        if (read_button().up) next_page();
        if (!check_wifi_status()) mgr->set_page(setup_page);
    });

    // 主循环
    while (true) {
        page_mgr.run(Context{
            .screen = screen,
            .updated = updated,
        });
        updated = false;
        screen.draw();
    }
}
