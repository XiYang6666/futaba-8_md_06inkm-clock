#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

template<size_t width>
class ScrollScreen {
public:
    struct Config {
        std::function<int64_t()> get_startup_time;
        std::function<void(const char *)> set_screen;
    } cfg;

    explicit ScrollScreen(Config cfg) : cfg(std::move(cfg)) {
    }

    struct Status {
        std::string text;
        bool scroll = true;
        int scroll_blank = 3;
        int char_stay_time = 200;
    };

    // 完全重设显示内容, 是否调用由状态机结合 updated 判断
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
            text_buffer.reserve(std::max(status.text.size(), static_cast<size_t>(width)));
            text_buffer.append(status.text);
            if (status.text.length() < width) text_buffer.append(width - status.text.length(), ' ');
        }
    }

    void draw() const {
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
    Status current_status = Status();
    std::string text_buffer;
    int64_t start_scroll_time = 0;
};
