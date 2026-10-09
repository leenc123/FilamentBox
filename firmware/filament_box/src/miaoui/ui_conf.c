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
#include "ui_conf.h"
#include "core/ui.h"
#include "display/dispDriver.h"
#include "images/image.h"
#include "widget/custom.h"
#include "version.h"
#include "hal/fbx_actions.h"

/* FilamentBox 菜单 A（只看不改）:
 * Home[ICON]：槽位入口(LOOP_FUNCTION 定制屏，头图标点确认直进) + 写卡入口 + 系统入口
 * （底部标签中文：槽位/写卡/系统；核心画名处已做中文分流，见 ui.c）
 * 开机封面仍是 Draw_Home（MiaoUi_Setup 直接画一次，与头图标解绑）
 * Slots 定制屏：12px 反白标题条（平时"槽位 n/4"，推送/异常显示成功/失败/写卡）+ 4 行
 * （序号+短名+右侧中文色值，选中行反白，空槽显示"空"）
 * Write[TEXT]：返回 + 槽位/材料/品牌/颜色中文行（点进去是选项浏览器：
 * 材料/品牌英文列表，颜色中文名+HEX；上/下循环选，确认回页）
 * + 预设/颜色实时预览 + 确认写入（结果弹窗"结果:" + 成功/失败/写卡状态）
 *  （材料×品牌见 filament_map.h 的 SCREEN_MATS，颜色见 SCREEN_COLORS；
 *   网页 /write 仍用全表；中文字库见 fonts/font_cn12，加字跑 tools/make_cn12_font.ps1）
 * System[TEXT]：返回 + 对比度(DATA，弹窗英文) + 本机IP(WORD只读) + 重启 + 重置网络
 * 状态/IP/预览行名直接指向下面的缓冲（MiaoUI 只存指针），
 * 槽位走 FbxSlots 结构体；miaoui_menu.cpp 每轮刷新，图标页底部栏实时同步。
 */
/*Page*/
ui_page_t Home_Page, Write_Page, System_Page;
/*item */
ui_item_t HomeHead_Item, Write_Item;
ui_item_t WriteHead_Item, WSlot_Item, WMat_Item, WBrand_Item, WPresetName_Item;
ui_item_t WColor_Item, WColorHex_Item, WGo_Item;
ui_item_t SystemHead_Item, System_Item;
ui_item_t Contrast_Item, Ip_Item, Reboot_Item, WifiReset_Item;

/* 槽位快照 + 动态文本缓冲（全局，喂数函数直接写；MiaoUI 只存指针，不拷贝） */
fbx_slot_t FbxSlots[4];
char FbxStatusText[24];
char FbxIpText[32];

/* 屏上写卡状态（miaoui_menu.cpp 的预览/执行函数读写） */
int  FbxWSlot = 1, FbxWMat = 0, FbxWBrand = 0, FbxWColor = 0;
char FbxWPresetName[13];
char FbxWColorHex[16];
/* Write 列表行名缓冲（传给 AddItem，MiaoUI 只存指针） */
char WMatName[16] = "-材料";
char WBrandName[16] = "-品牌";
char WColorName[20] = "-颜色";

/**
 * 在此建立所需显示或更改的数据
 * 无参数
 * 无返回值
 */
void Create_Parameter(ui_t *ui)
{
    (void)ui;
    static int       Contrast = 255;
    static ui_data_t Contrast_data;
    Contrast_data.name         = "对比度";
    Contrast_data.ptr          = &Contrast;
    Contrast_data.function     = Disp_SetContrast;
    Contrast_data.functionType = UI_DATA_FUNCTION_STEP_EXECUTE;
    Contrast_data.dataType     = UI_DATA_INT;
    Contrast_data.actionType   = UI_DATA_ACTION_RW;
    Contrast_data.max          = 255;
    Contrast_data.min          = 0;
    Contrast_data.step         = 5;
    static ui_element_t Contrast_element;
    Contrast_element.data = &Contrast_data;
    Create_element(&Contrast_Item, &Contrast_element);

    // 屏上写卡：槽位/材料/品牌/颜色四个数字选择器（步进即刷预览），确认项调 Fbx_WriteCard
    static ui_data_t wslot_data;
    wslot_data.name         = "槽位";
    wslot_data.ptr          = &FbxWSlot;
    wslot_data.function     = Fbx_WritePreview;
    wslot_data.functionType = UI_DATA_FUNCTION_STEP_EXECUTE;
    wslot_data.dataType     = UI_DATA_INT;
    wslot_data.actionType   = UI_DATA_ACTION_RW;
    wslot_data.max          = 4;
    wslot_data.min          = 1;
    wslot_data.step         = 1;
    static ui_element_t wslot_element;
    wslot_element.data = &wslot_data;
    Create_element(&WSlot_Item, &wslot_element);

    static ui_data_t wmat_data;
    wmat_data.name         = "材料";
    wmat_data.ptr          = &FbxWMat;
    wmat_data.function     = Fbx_WritePreview;
    wmat_data.functionType = UI_DATA_FUNCTION_STEP_EXECUTE;
    wmat_data.dataType     = UI_DATA_INT;
    wmat_data.actionType   = UI_DATA_ACTION_RW;
    wmat_data.max          = fbxMatCount() - 1;
    wmat_data.min          = 0;
    wmat_data.step         = 1;
    static ui_element_t wmat_element;
    wmat_element.data = &wmat_data;
    Create_element(&WMat_Item, &wmat_element);

    static ui_data_t wbrand_data;
    wbrand_data.name         = "品牌";
    wbrand_data.ptr          = &FbxWBrand;
    wbrand_data.function     = Fbx_WritePreview;
    wbrand_data.functionType = UI_DATA_FUNCTION_STEP_EXECUTE;
    wbrand_data.dataType     = UI_DATA_INT;
    wbrand_data.actionType   = UI_DATA_ACTION_RW;
    wbrand_data.max          = 1;
    wbrand_data.min          = 0;
    wbrand_data.step         = 1;
    static ui_element_t wbrand_element;
    wbrand_element.data = &wbrand_data;
    Create_element(&WBrand_Item, &wbrand_element);

    static ui_data_t wcolor_data;
    wcolor_data.name         = "颜色";
    wcolor_data.ptr          = &FbxWColor;
    wcolor_data.function     = Fbx_WritePreview;
    wcolor_data.functionType = UI_DATA_FUNCTION_STEP_EXECUTE;
    wcolor_data.dataType     = UI_DATA_INT;
    wcolor_data.actionType   = UI_DATA_ACTION_RW;
    wcolor_data.max          = fbxColorCount() - 1;
    wcolor_data.min          = 0;
    wcolor_data.step         = 1;
    static ui_element_t wcolor_element;
    wcolor_element.data = &wcolor_data;
    Create_element(&WColor_Item, &wcolor_element);
}

/**
 * 在此建立所需显示或更改的文本
 * 无参数
 * 无返回值
 */
void Create_Text(ui_t *ui)
{
    (void)ui;
    static ui_text_t ip_text;
    ip_text.font      = UI_FONT;
    ip_text.fontHight = UI_FONT_HIGHT;
    ip_text.fontWidth = UI_FONT_WIDTH;
    ip_text.ptr       = FbxIpText;
    static ui_element_t ip_element;
    ip_element.text = &ip_text;
    ip_element.data = NULL;
    Create_element(&Ip_Item, &ip_element);

    static ui_text_t wpresetname_text;
    wpresetname_text.font      = UI_FONT;
    wpresetname_text.fontHight = UI_FONT_HIGHT;
    wpresetname_text.fontWidth = UI_FONT_WIDTH;
    wpresetname_text.ptr       = FbxWPresetName;
    static ui_element_t wpresetname_element;
    wpresetname_element.text = &wpresetname_text;
    wpresetname_element.data = NULL;
    Create_element(&WPresetName_Item, &wpresetname_element);

    static ui_text_t wcolorhex_text;
    wcolorhex_text.font      = UI_FONT;
    wcolorhex_text.fontHight = UI_FONT_HIGHT;
    wcolorhex_text.fontWidth = UI_FONT_WIDTH;
    wcolorhex_text.ptr       = FbxWColorHex;
    static ui_element_t wcolorhex_element;
    wcolorhex_element.text = &wcolorhex_text;
    wcolorhex_element.data = NULL;
    Create_element(&WColorHex_Item, &wcolorhex_element);
}

/*
 * 菜单构建函数
 * 该函数不接受参数，也不返回任何值。
 * 功能：静态地构建一个菜单系统。
 */
void Create_MenuTree(ui_t *ui)
{
    /* ui树，此处禁用格式化 */
    // clang-format off
    AddPage("[HomePage]", &Home_Page, UI_PAGE_ICON);
        AddItem("槽位", UI_ITEM_LOOP_FUNCTION, logo_allArray[0], &HomeHead_Item, &Home_Page, NULL, Draw_Slots);
        AddItem("写卡", UI_ITEM_PARENTS, logo_allArray[4], &Write_Item, &Home_Page, &Write_Page, NULL);
            AddPage("[Write]", &Write_Page, UI_PAGE_TEXT);
                AddItem("返回", UI_ITEM_RETURN, NULL, &WriteHead_Item, &Write_Page, &Home_Page, NULL);
                AddItem("-槽位", UI_ITEM_DATA, NULL, &WSlot_Item, &Write_Page, NULL, NULL);
                AddItem(WMatName, UI_ITEM_DATA, NULL, &WMat_Item, &Write_Page, NULL, Draw_WMat);
                AddItem(WBrandName, UI_ITEM_DATA, NULL, &WBrand_Item, &Write_Page, NULL, Draw_WBrand);
                AddItem(FbxWPresetName, UI_ITEM_WORD, NULL, &WPresetName_Item, &Write_Page, NULL, NULL);
                AddItem(WColorName, UI_ITEM_DATA, NULL, &WColor_Item, &Write_Page, NULL, Draw_WColor);
                AddItem(FbxWColorHex, UI_ITEM_WORD, NULL, &WColorHex_Item, &Write_Page, NULL, NULL);
                AddItem(" -Write!", UI_ITEM_ONCE_FUNCTION, NULL, &WGo_Item, &Write_Page, NULL, Fbx_WriteCard);
        AddItem("系统", UI_ITEM_PARENTS, logo_allArray[1], &System_Item, &Home_Page, &System_Page, NULL);
            AddPage("[System]", &System_Page, UI_PAGE_TEXT);
                AddItem("返回", UI_ITEM_RETURN, NULL, &SystemHead_Item, &System_Page, &Home_Page, NULL);
                AddItem("-对比度", UI_ITEM_DATA, NULL, &Contrast_Item, &System_Page, NULL, NULL);
                AddItem(FbxIpText, UI_ITEM_WORD, NULL, &Ip_Item, &System_Page, NULL, NULL);
                AddItem("-重启", UI_ITEM_ONCE_FUNCTION, NULL, &Reboot_Item, &System_Page, NULL, Fbx_Reboot);
                AddItem("-重置网络", UI_ITEM_ONCE_FUNCTION, NULL, &WifiReset_Item, &System_Page, NULL, Fbx_ResetWifi);
    // clang-format on
}

void MiaoUi_Setup(ui_t *ui)
{
    Create_UI(ui, &HomeHead_Item); // 创建UI, 必须给定一个头项目
    Draw_Home(ui);
}
