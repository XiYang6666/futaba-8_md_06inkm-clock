#pragma once

#include <functional>

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
