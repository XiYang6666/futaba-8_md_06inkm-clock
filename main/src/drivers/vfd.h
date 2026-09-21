#pragma once

#include "esp_err.h"
#include "driver/spi_master.h"
#include "soc/gpio_num.h"
#include "ftb-8-md.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t da;
    gpio_num_t clk;
    gpio_num_t cs;
    gpio_num_t rst;
    uint8_t dimming;
} vfd_config_t;

typedef struct {
    spi_device_handle_t handle;
} vfd_t;

// 初始化 SPI 总线并注册 VFD 设备
inline esp_err_t vfd_init(vfd_t *self, const vfd_config_t *cfg) {
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = cfg->da;
    bus_cfg.miso_io_num = -1;
    bus_cfg.sclk_io_num = cfg->clk;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 32;

    esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return err;

    self->handle = ftb8md_device_register(SPI2_HOST, cfg->cs, cfg->rst);
    if (self->handle == NULL) return ESP_FAIL;

    if ((err = ftb8md_set_dimming(self->handle, cfg->dimming)) != ESP_OK) return err;
    return ftb8md_clear_display(self->handle);
}

// 从第 0 位开始显示字符串(最多 8 字符)
inline void vfd_show_string(const vfd_t *self, const char *text) {
    ftb8md_show_string(self->handle, 0, text);
}

#ifdef __cplusplus
}
#endif
