#pragma once

#include "drivers/button.h"
#include "drivers/vfd.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化时钟应用(须在 app_main 中调用一次)
void app_init(vfd_t *vfd_dev, button_context_t *button_dev);

// 主循环: 每 100ms 刷新, 阻塞运行
void app_run(void);

#ifdef __cplusplus
}
#endif
