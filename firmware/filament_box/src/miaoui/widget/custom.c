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
#include "custom.h"
#include "../display/dispDriver.h"
#include "../indev/indevDriver.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../images/image.h"
#include "../version.h"
#include "../fonts/font_cn12.h"
#include "../hal/fbx_actions.h"

// 状态行已不再占首页图标位（Status_Item 已删）；推送/异常反馈改走 Slots 标题条，
// 由 miaoui_menu.cpp 每轮喂数（idle 时 "FBX Ready"，有状态时如 SENT/FAIL/WRITE）。
extern char FbxStatusText[24];
// Write 页选择值（miaoui_menu.cpp 维护，浏览器里步进后调 Fbx_WritePreview 刷新预览）
extern int FbxWMat;
extern int FbxWBrand;
extern int FbxWColor;

// 本文件内部画件前置声明（Portal_Art 在 bootArc/ringFull 之前调用）
static void bootArc(uint16_t cx, uint16_t cy, uint8_t r);
static void ringFull(uint16_t cx, uint16_t cy, uint8_t r);

#if(UI_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#endif

const unsigned char UI_NAME_LOGO[] = {

    0x80, 0xc1, 0x00, 0x00, 0x00, 0x00, 0x08, 0x01, 0xc0, 0xe3, 0x10, 0x00, 0x00, 0x08, 0x8c, 0x01, 0x60, 0xf3, 0x18, 0x00, 0x00, 0x0c, 0x8c, 0x01, 0x30, 0xdb, 0x00, 0x00, 0x00, 0x0c, 0x8c, 0x00, 0x30, 0xc9, 0x00, 0x00, 0x00, 0x0c,
    0x8c, 0x00, 0x18, 0x4d, 0x00, 0x1e, 0x1c, 0x04, 0xc4, 0x00, 0x88, 0x47, 0x0c, 0x1f, 0x3f, 0x04, 0xc6, 0x00, 0x8c, 0x43, 0x8c, 0x09, 0x33, 0x06, 0x46, 0x00, 0x8c, 0x63, 0xc4, 0x88, 0x31, 0x02, 0x63, 0x00, 0x86, 0x61, 0x44, 0xcc,
    0x30, 0x02, 0x63, 0x00, 0xc6, 0x61, 0x66, 0xce, 0x10, 0x82, 0x61, 0x00, 0xc2, 0x20, 0x66, 0xc7, 0x18, 0xc2, 0x20, 0x00, 0xc3, 0x20, 0xc6, 0xc5, 0x0f, 0x7e, 0x20, 0x00, 0x43, 0x00, 0xc2, 0x84, 0x07, 0x3c, 0x20, 0x00

};

const unsigned char UI_URL[] = {

    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xaf, 0xbf, 0xff, 0xfe, 0xdf, 0xff, 0xf7, 0xfb, 0xab, 0xfe, 0xf4, 0xd6, 0xff, 0x69, 0xff, 0xf9, 0xaf,
    0xfb, 0xbd, 0xfe, 0xef, 0xd9, 0xfd, 0xfb, 0xaf, 0xef, 0xaf, 0xfa, 0xef, 0xf0, 0xbe, 0xf7, 0xff, 0xef, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xef, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff

};

//首页
// FilamentBox 移植说明：原版是 while(1) 阻塞式开机动画（等动画播完或按确认才返回），
// 在 setup 里同步调用会饿死 ESP32 看门狗导致复位，且在菜单里选中也会卡住轮询/Web。
// 改为单帧静态绘制并立即返回，菜单后续会重绘画布。
void Draw_Home(ui_t *ui)
{
    uint8_t color = 1;
    (void)ui;
    Disp_ClearBuffer( );
    Disp_SetDrawColor(&color);
    Disp_DrawXBMP(34, 26, 57, 14, UI_NAME_LOGO);
    Disp_SetFont(font_home_h6w4);
    Disp_DrawStr(14, 58, VERSION_PROJECT_LINK);
    Disp_SendBuffer( );
}

// 槽位总览屏（FilamentBox 定制，挂在 Slots 的 LOOP_FUNCTION 上）
// 每 tick 调一次：按 ui->action 翻选并全屏重绘后立即返回（不阻塞）；
// 确认键由框架接管退出回菜单。版式：12px 反白标题条（中文需 12px 高，
// 平时"槽位 n/4"，有推送/异常时显示中文状态）+ 4 行 x13px，
// 每行：序号 + 短名 + 右侧中文色值；选中行整行反白；空槽显示"空"。
void Draw_Slots(ui_t *ui)
{
    static uint8_t sel = 0;
    if(ui->action == UI_ACTION_UP)
        sel = (uint8_t)((sel + 3) & 0x03);
    else if(ui->action == UI_ACTION_DOWN)
        sel = (uint8_t)((sel + 1) & 0x03);

    uint8_t c1 = 1, c0 = 0;
    uint8_t n = 0;
    for(uint8_t i = 0; i < 4; i++) n += (FbxSlots[i].present ? 1 : 0);

    Disp_ClearBuffer( );
    // 标题反白条（12px 高，容 12px 汉字；平时在位计数，有状态显示中文状态）
    Disp_SetDrawColor(&c1);
    Disp_DrawBox(0, 0, UI_HOR_RES, 12);
    Disp_SetDrawColor(&c0);
    {
        char title[24];
        if (FbxStatusText[0] == '\0' || strcmp(FbxStatusText, "FBX Ready") == 0) {
            snprintf(title, sizeof(title), "槽位 %d/4", n);
        } else if (strncmp(FbxStatusText, "SENT ", 5) == 0) {
            snprintf(title, sizeof(title), "成功 %s", FbxStatusText + 5);
        } else if (strncmp(FbxStatusText, "FAIL ", 5) == 0) {
            snprintf(title, sizeof(title), "失败 %s", FbxStatusText + 5);
        } else if (strncmp(FbxStatusText, "WRITE ", 6) == 0) {
            snprintf(title, sizeof(title), "写卡 %s", FbxStatusText + 6);
        } else {
            snprintf(title, sizeof(title), "%.23s", FbxStatusText);
        }
        Cn_DrawStr(2, 11, title);
    }
    // 4 行：y0 = 13/26/39/52，高 12，基线 y0+10（末行收在 64px 内）
    Disp_SetFont(UI_FONT);
    for(uint8_t i = 0; i < 4; i++)
    {
        uint16_t y0   = (uint16_t)(13 + i * 13);
        uint16_t base = (uint16_t)(y0 + 10);
        char     no[4];
        snprintf(no, sizeof(no), "%d", FbxSlots[i].tray);
        if(i == sel)
        {
            Disp_SetDrawColor(&c1);
            Disp_DrawBox(0, y0, UI_HOR_RES, 12);
            Disp_SetDrawColor(&c0);
        }
        else
        {
            Disp_SetDrawColor(&c1);
        }
        Disp_DrawStr(2, base, no);
        if(FbxSlots[i].present)
        {
            Disp_DrawStr(14, base, FbxSlots[i].name);
            // 色值列：命中调色板显示中文名，否则回落 HEX（卡可存任意合法色）
            const char *cn = fbxColorCnByRgb(FbxSlots[i].color);
            if (cn != 0)
                Cn_DrawStr(90, base, cn);
            else
                Disp_DrawStr(92, base, FbxSlots[i].color);
        }
        else
        {
            // 空槽：读卡器缺席加 ! 标记（标题 n 保持有卡数不变）
            Cn_DrawStr(14, base, FbxSlots[i].reader ? "空" : "空 !");
        }
    }
    Disp_SetDrawColor(&c1);
    Disp_SendBuffer( );
}

const unsigned char AUTHOR[] = {

    0x00, 0x00, 0x40, 0x10, 0x00, 0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x90, 0x00, 0x00, 0x04, 0x88, 0x20, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x00, 0x00, 0x08, 0x20, 0x80, 0x28, 0x80, 0x20, 0x00, 0x00, 0x44, 0x00, 0x20,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x02, 0x01, 0x20, 0x00, 0x08, 0x02, 0x00, 0x20, 0x00, 0x08, 0x02, 0x00, 0x08, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x08, 0x01, 0x00, 0x01, 0x10, 0x20, 0x00, 0x08, 0x40, 0x00, 0x00, 0x80, 0x00, 0x00, 0x89, 0x04, 0x01, 0x00, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x20, 0x10, 0x00, 0x10, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x00, 0x00, 0x81, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x04, 0x00, 0x00, 0x08, 0x02, 0x20, 0x20, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x08, 0x10, 0x02, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x80, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x00, 0x00, 0x81, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x7f, 0x40, 0x04, 0x22, 0x10, 0x00, 0x75, 0x10, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd0, 0x30, 0x00, 0x00, 0x40, 0x00, 0xe4, 0x71, 0x01, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x8b, 0x08, 0x04, 0x00, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x07, 0x00,
    0x00, 0x80, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x20, 0xff, 0xff, 0x07, 0x00, 0x00, 0x80, 0xff, 0xff, 0x0f, 0x10, 0x00, 0x08, 0xf9, 0xff, 0x1f, 0x00, 0x5a, 0x01, 0xf0, 0xff, 0x0f, 0x09, 0xff, 0x01, 0xf1, 0xff, 0x01, 0x00, 0xff, 0x32, 0xf8, 0xff, 0xa1, 0x1e, 0x80, 0x4f, 0xf8, 0xff,
    0x01, 0x00, 0x81, 0x37, 0xf0, 0xff, 0x01, 0x00, 0x01, 0x7e, 0xf8, 0xff, 0x08, 0x00, 0x29, 0x3c, 0xf3, 0xff, 0x02, 0x00, 0x85, 0x80, 0xff, 0xff, 0x06, 0x00, 0x00, 0x80, 0xfb, 0x7f, 0x0f, 0x00, 0x00, 0x80, 0xff, 0x7f, 0x47, 0x00

};

void Show_Version(ui_t *ui)
{
    int16_t value = -128;
    uint8_t state = 0;
    while(1)
    {
        if(indevScan( ) == UI_ACTION_ENTER)
            return;

        Disp_ClearBuffer( );

        switch(state)
        {
        case 0:
            value = (int16_t)UI_Animation(0.0, (float)value, &ui->animation.textPage_ani);
            Disp_DrawXBMP(value, 0, 45, 60, AUTHOR);
            if(value == 0)
            {
                state = 1;
                value = 128;
            }
            break;
        case 1:
            value = (int16_t)UI_Animation(50, (float)value, &ui->animation.textPage_ani);
            Disp_DrawXBMP(0, 0, 45, 60, AUTHOR);
            Disp_SetFont(font_home_h6w4);
            Disp_DrawStr(value, 6, "Author:JFeng-Z");
            Disp_DrawStr(value, 18, "UI:MiaoUI");
            Disp_DrawStr(value, 30, "Version:1.2");
            Disp_DrawStr(value, 42, "OS:FreeRTOS v10.3");
            Disp_DrawStr(value, 56, "Hardware:STM32F103");
            break;
        default:
            break;
        }
        Disp_SendBuffer( );
    }
}

// 开机 NFC 波纹：圆心 (cx,cy) 右侧 ±55° 弧，线宽 2px
static void bootArc(uint16_t cx, uint16_t cy, uint8_t r)
{
    int16_t lx0 = -1, ly0 = -1, lx1 = -1, ly1 = -1;
    for (int16_t d = -55; d <= 55; d += 6)
    {
        float a = (float)d * 3.14159265f / 180.0f;
        float c = cosf(a), s = sinf(a);
        int16_t x0 = (int16_t)(cx + r * c);
        int16_t y0 = (int16_t)(cy + r * s);
        int16_t x1 = (int16_t)(cx + (r + 1) * c);
        int16_t y1 = (int16_t)(cy + (r + 1) * s);
        if (lx0 >= 0)
        {
            Disp_DrawLine((uint16_t)lx0, (uint16_t)ly0, (uint16_t)x0, (uint16_t)y0);
            Disp_DrawLine((uint16_t)lx1, (uint16_t)ly1, (uint16_t)x1, (uint16_t)y1);
        }
        lx0 = x0; ly0 = y0; lx1 = x1; ly1 = y1;
    }
}

// 配网/联网动画件：小卡片轮廓 + 按 stage 点亮 0-3 道波；yOff 适配顶部死区。
// 只画图形不写字（文字由调用方按需配 6x10/12px 字体），供 wifi_portal 直刷屏用。
void Portal_Art(uint8_t yOff, uint8_t stage)
{
    uint8_t c = 1;
    Disp_SetDrawColor(&c);
    Disp_DrawRFrame(2, (uint16_t)(yOff + 2), 26, 18, 2);
    Disp_DrawBox(6, (uint16_t)(yOff + 8), 5, 5);
    if (stage > 3) stage = 3;
    const uint8_t rs[3] = {3, 6, 9};
    for (uint8_t k = 0; k < stage; k++) bootArc(34, (uint16_t)(yOff + 11), rs[k]);
}

// 整圆环（圆心/半径，线宽 2px），供结果符号用
static void ringFull(uint16_t cx, uint16_t cy, uint8_t r)
{
    int16_t lx0 = -1, ly0 = -1, lx1 = -1, ly1 = -1;
    for (int16_t d = -180; d <= 180; d += 10)
    {
        float a = (float)d * 3.14159265f / 180.0f;
        float c = cosf(a), s = sinf(a);
        int16_t x0 = (int16_t)(cx + r * c);
        int16_t y0 = (int16_t)(cy + r * s);
        int16_t x1 = (int16_t)(cx + (r + 1) * c);
        int16_t y1 = (int16_t)(cy + (r + 1) * s);
        if (lx0 >= 0)
        {
            Disp_DrawLine((uint16_t)lx0, (uint16_t)ly0, (uint16_t)x0, (uint16_t)y0);
            Disp_DrawLine((uint16_t)lx1, (uint16_t)ly1, (uint16_t)x1, (uint16_t)y1);
        }
        lx0 = x0; ly0 = y0; lx1 = x1; ly1 = y1;
    }
}

// 结果符号：圆圈+对勾(ok=1)/圆圈+叉(ok=0)，圆心 (17,yOff+13) r=11；
// 和 Portal_Art 同一套原点逻辑，供 wifi_portal 成功/失败屏用
void Portal_Result(uint8_t yOff, uint8_t ok)
{
    uint8_t c = 1;
    Disp_SetDrawColor(&c);
    ringFull(17, (uint16_t)(yOff + 13), 11);
    if (ok)
    {
        Disp_DrawLine(11, (uint16_t)(yOff + 13), 16, (uint16_t)(yOff + 18));
        Disp_DrawLine(16, (uint16_t)(yOff + 18), 25, (uint16_t)(yOff + 7));
    }
    else
    {
        Disp_DrawLine(12, (uint16_t)(yOff + 8), 22, (uint16_t)(yOff + 18));
        Disp_DrawLine(22, (uint16_t)(yOff + 8), 12, (uint16_t)(yOff + 18));
    }
}

// 开机动画单帧：卡片 + 波纹闪烁 + FilamentBox 逐字滑入；循环与按键跳过由调用方做
// f: 0-5 波纹闪两下；6 起每 2 帧一字、8 帧减速滑入；34 后定格（调用方播到 38）
void Draw_BootAnim(uint8_t f)
{
    static const char word[] = "FilamentBox";
    uint8_t c = 1;
    Disp_ClearBuffer( );
    Disp_SetDrawColor(&c);
    Disp_DrawXBMP(2, 8, 48, 48, boot_nfc_48);
    if (f >= 6 || (f % 3) != 2)
    {
        const uint8_t rs[3] = {5, 10, 15};
        for (uint8_t k = 0; k < 3; k++) bootArc(33, 32, rs[k]);
    }
    Disp_SetFont(UI_FONT);
    for (uint8_t i = 0; i < 11; i++)
    {
        int16_t s = (int16_t)(6 + 2 * i);
        int16_t k = (int16_t)f - s;
        if (k < 0) continue;
        if (k > 8) k = 8;
        int16_t tx = (int16_t)(58 + 6 * i);
        int16_t off = (int16_t)((132 - tx) * (8 - k) * (8 - k) / 64);
        char ch[2] = {word[i], '\0'};
        Disp_DrawStr((uint16_t)(tx + off), 36, ch);
    }
    Disp_SendBuffer( );
}

void Show_Logo(ui_t *ui)
{
    Disp_ClearBuffer( );
    Disp_DrawXBMP(34, 26, 57, 14, UI_NAME_LOGO);
    Disp_SendBuffer( );
}

// 中英文混排：ASCII 攒串用 UI_FONT 画，中文单字查点阵画（12px 宽/字，基线对齐）；
// 超出字集的中文跳过（调用方已用 extract 脚本保证覆盖，这里是最后防线）。
uint16_t Cn_DrawStr(uint16_t x, uint16_t baseline, const char *str)
{
    Disp_SetFont(UI_FONT);
    uint16_t cur = x;
    const uint8_t *p = (const uint8_t *)str;
    char run[24];
    uint8_t rn = 0;
    while (1)
    {
        uint8_t b = *p;
        int isAscii = (b != 0 && b < 0x80);
        if (!isAscii && rn > 0)
        {
            run[rn] = '\0';
            Disp_DrawStr(cur, baseline, run);
            cur += Disp_GetUTF8Width(run);
            rn = 0;
        }
        if (b == 0) break;
        if (isAscii)
        {
            if (rn >= sizeof(run) - 1)
            {
                run[rn] = '\0';
                Disp_DrawStr(cur, baseline, run);
                cur += Disp_GetUTF8Width(run);
                rn = 0;
            }
            run[rn++] = (char)b;
            p++;
        }
        else
        {
            uint32_t cp = 0;
            uint8_t len = 0;
            if ((b & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80)
            {
                cp = ((uint32_t)(b & 0x1F) << 6) | (p[1] & 0x3F);
                len = 2;
            }
            else if ((b & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80)
            {
                cp = ((uint32_t)(b & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
                len = 3;
            }
            else
            {
                p++;
                continue;
            }
            const uint8_t *bits = cn12_get(cp);
            if (bits != 0)
            {
                uint16_t top = (baseline > 11) ? (uint16_t)(baseline - 11) : 0;
                Disp_DrawXBMP(cur, top, CN12_W, CN12_H, bits);
            }
            cur += CN12_W;
            p += len;
        }
    }
    return cur;
}

// 混排串像素宽度（ASCII 按 6px、中文按 12px；纯 ASCII 时与 strlen*6 一致，
// 可直接替换核心里的光标宽度计算）。
uint16_t Cn_Measure(const char *str)
{
    uint16_t w = 0;
    const uint8_t *p = (const uint8_t *)str;
    while (*p)
    {
        if (*p < 0x80) { w += UI_FONT_WIDTH; p++; }
        else if ((*p & 0xF0) == 0xE0) { w += CN12_W; p += 3; }
        else if ((*p & 0xE0) == 0xC0) { w += CN12_W; p += 2; }
        else p++;
    }
    return w;
}

// 英文选项浏览器通用绘制：上/下循环步进，4 行一屏（多于 4 项自动跟随滚动），
// 确认键由框架接管退出回 Write 页。nameFn 取行名（useCn 时为中文，经过 Cn_DrawStr），
// tailFn 取右侧 ASCII 尾列（如色值 HEX，可为 NULL）。
static void optBrowser(ui_t *ui, const char *title, int total,
                       const char *(*nameFn)(int), const char *(*tailFn)(int),
                       int useCn, int *psel)
{
    int sel = *psel;
    if (ui->action == UI_ACTION_UP)
        sel--;
    else if (ui->action == UI_ACTION_DOWN)
        sel++;
    if (sel < 0) sel = total - 1;
    if (sel >= total) sel = 0;
    if (sel != *psel) {
        *psel = sel;
        Fbx_WritePreview(NULL);  // 同步预设/颜色预览行 + Write 列表行名
    }

    int top = 0;
    if (total > 4) {
        top = sel - 1;
        if (top < 0) top = 0;
        if (top + 4 > total) top = total - 4;
    }

    uint8_t c1 = 1, c0 = 0;
    Disp_ClearBuffer( );
    Disp_SetDrawColor(&c1);
    Disp_DrawBox(0, 0, UI_HOR_RES, 10);
    Disp_SetFont(font_home_h6w4);
    Disp_SetDrawColor(&c0);
    {
        char t[16];
        snprintf(t, sizeof(t), "%s %d/%d", title, sel + 1, total);
        Disp_DrawStr(2, 8, t);
    }
    Disp_SetFont(UI_FONT);
    for (int r = 0; r < 4 && top + r < total; r++)
    {
        uint16_t y0   = (uint16_t)(11 + r * 13);
        uint16_t base = (uint16_t)(y0 + 10);
        if (top + r == sel)
        {
            Disp_SetDrawColor(&c1);
            Disp_DrawBox(0, y0, UI_HOR_RES, 12);
            Disp_SetDrawColor(&c0);
        }
        else
        {
            Disp_SetDrawColor(&c1);
        }
        if (useCn)
            Cn_DrawStr(4, base, nameFn(top + r));
        else
            Disp_DrawStr(4, base, nameFn(top + r));
        if (tailFn != 0)
        {
            Disp_SetFont(UI_FONT);
            Disp_DrawStr(92, base, tailFn(top + r));
        }
    }
    Disp_SetDrawColor(&c1);
    Disp_SendBuffer( );
}

void Draw_WMat(ui_t *ui)
{
    optBrowser(ui, "MAT", fbxMatCount(), fbxMatName, 0, 0, &FbxWMat);
}

void Draw_WBrand(ui_t *ui)
{
    optBrowser(ui, "BRAND", 2, fbxBrandName, 0, 0, &FbxWBrand);
}

void Draw_WColor(ui_t *ui)
{
    optBrowser(ui, "COLOR", fbxColorCount(), fbxColorCnName, fbxColorHex, 1, &FbxWColor);
}

#if(UI_USE_FREERTOS == 1)
void TaskLvel_Setting(ui_t *ui)
{
    vTaskPrioritySet(*ui->nowItem->element->data->dataRootTask, *(UBaseType_t *)ui->nowItem->element->data->ptr);
}
#endif
