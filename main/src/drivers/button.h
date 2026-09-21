#pragma once

#include "esp_bit_defs.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "soc/gpio_num.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool press: 1;
    bool up: 1;
    bool down: 1;
    int64_t duration;
} button_state_t;

typedef struct {
    gpio_num_t pin;
    bool last_state;
    int64_t last_change_time;
} button_context_t;

inline esp_err_t button_init(button_context_t *self, const gpio_num_t pin) {
    self->pin = pin;
    self->last_change_time = esp_timer_get_time();
    self->last_state = false;

    const uint64_t bitmask = BIT64(pin);
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = bitmask;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&io_conf);
}

inline button_state_t button_read(button_context_t *self) {
    const bool current_state = !!gpio_get_level(self->pin);
    const int64_t time = esp_timer_get_time();
    if (current_state != self->last_state) self->last_change_time = time;
    const button_state_t result = {
        .press = current_state,
        .up = self->last_state && !current_state,
        .down = !self->last_state && current_state,
        .duration = time - self->last_change_time,
    };
    self->last_state = current_state;
    return result;
}

#ifdef __cplusplus
}
#endif
