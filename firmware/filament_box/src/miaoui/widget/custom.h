/*
 * This file is part of the MiaoUI Library.
 *
 * Copyright (c) 2025, JFeng-Z, <2834294740@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Created on: 2025-02-08
 */
#ifndef _CUSTOM_H
#define _CUSTOM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../core/ui.h"
#include "../ui_conf.h"

void Draw_Home(ui_t *ui);
void Draw_Slots(ui_t *ui);
// Write 页英文选项浏览器（挂在 DATA 项的 itemFunction 上，替代数字弹窗；
// 上/下键循环步进，确认键由框架接管退出；实现见 custom.c）
void Draw_WMat(ui_t *ui);
void Draw_WBrand(ui_t *ui);
void Draw_WColor(ui_t *ui);
// 开机动画单帧（卡片+波纹闪烁+字母滑入；循环与按键跳过由调用方做）
void Draw_BootAnim(uint8_t f);
// 配网/联网动画件（小卡片+0-3道波；yOff 适配顶部死区；只画图形不写字）
void Portal_Art(uint8_t yOff, uint8_t stage);
// 结果符号（圆圈+对勾/叉；和 Portal_Art 同一套原点逻辑）
void Portal_Result(uint8_t yOff, uint8_t ok);
// 中英文混排绘制：中文走 12px 子集点阵（font_cn12），ASCII 走 UI_FONT；
// 缺字中文跳过（留空位），绝不画乱码；返回绘制终点 x
uint16_t Cn_DrawStr(uint16_t x, uint16_t baseline, const char *str);
// 混排串像素宽度（纯 ASCII 时与 strlen*UI_FONT_WIDTH 一致）
uint16_t Cn_Measure(const char *str);
void Show_Version(ui_t *ui);
void Show_Logo(ui_t *ui);
void TaskLvel_Setting(ui_t *ui);

#ifdef __cplusplus
}
#endif

#endif
