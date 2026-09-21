#pragma once

#include "soc/gpio_num.h"

// ===== 硬件 =====
// VFD (Futaba 8-MD-06INK) SPI
#define VFD_PIN_DA  GPIO_NUM_0
#define VFD_PIN_CLK GPIO_NUM_1
#define VFD_PIN_CS  GPIO_NUM_2
#define VFD_PIN_RST GPIO_NUM_3
#define VFD_DIMMING 120

// 按钮
#define BUTTON_PIN GPIO_NUM_21

// ===== WiFi AP =====
#define AP_SSID     "VFD-CLOCK"
#define AP_CHANNEL  1
#define AP_MAX_CONN 4
#define HOSTNAME    "vfd-clock"

// ===== 时间 =====
#define TZ_SETTING "CST-8"

// ===== 存储 =====
#define SPIFFS_BASE_PATH "/spiffs"
#define SPIFFS_LABEL     "storage"

// ===== Web =====
#define HTTP_PORT 80
