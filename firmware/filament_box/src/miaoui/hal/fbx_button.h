#pragma once
// fbx_button.h — FilamentBox 三键对接 MiaoUI（上=32 / 下=25 / 确认=33，输入上拉，接地按下）
// 纯 C 声明（extern "C"），供 miaoui/indev/indevDriver.c 调用；实现见 fbx_button.cpp。
// 返回值与 UI_ACTION 对齐：0=NONE, 1=UP, 2=DOWN, 3=ENTER。
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void fbxButtonInit(void);
uint8_t fbxKeyScan(void);

#ifdef __cplusplus
}
#endif
