#pragma once
// filament_map.h — 耗材枚举对照表
// 对齐 ha-bambulab 的 filaments_detail.json（Bambu 官方预设体系）：
//   https://github.com/greghesp/ha-bambulab/blob/main/custom_components/bambu_lab/filaments_detail.json
// 卡片 Block4 存的是预设 ID（tray_info_idx，如 GFA01）；推送时：
//   tray_info_idx = 预设 ID，tray_type = 家族名（如 PLA），喷嘴温度 = 该预设范围
#include <Arduino.h>
#include <stdint.h>
#include <string.h>

struct FilamentInfo {
  const char* idx;    // 预设 ID = tray_info_idx，也是卡片内存的值
  const char* type;   // 家族名 = 推送用的 tray_type
  const char* name;   // 显示名（如 Bambu PLA Matte）
  uint16_t nmin;
  uint16_t nmax;
};

// 按家族分组排列（写卡页按此顺序一次遍历生成 optgroup）
static const FilamentInfo FILAMENT_TABLE[] = {
  // ---- PLA ----
  {"GFA00", "PLA", "Bambu PLA Basic", 190, 240},
  {"GFA01", "PLA", "Bambu PLA Matte", 190, 240},
  {"GFA02", "PLA", "Bambu PLA Metal", 190, 240},
  {"GFA05", "PLA", "Bambu PLA Silk", 190, 240},
  {"GFA06", "PLA", "Bambu PLA Silk+", 190, 240},
  {"GFA07", "PLA", "Bambu PLA Marble", 190, 240},
  {"GFA08", "PLA", "Bambu PLA Sparkle", 190, 240},
  {"GFA09", "PLA", "Bambu PLA Tough", 190, 240},
  {"GFA10", "PLA", "Bambu PLA Tough+", 220, 250},
  {"GFA12", "PLA", "Bambu PLA Glow", 190, 240},
  {"GFA13", "PLA", "Bambu PLA Dynamic", 190, 240},
  {"GFA15", "PLA", "Bambu PLA Galaxy", 190, 240},
  {"GFA16", "PLA", "Bambu PLA Wood", 190, 240},
  {"GFA17", "PLA", "Bambu PLA Translucent", 190, 240},
  {"GFA18", "PLA", "Bambu PLA Lite", 190, 240},
  {"GFL95", "PLA", "Generic PLA High Speed", 190, 240},
  {"GFL96", "PLA", "Generic PLA Silk", 190, 240},
  {"GFL99", "PLA", "Generic PLA", 190, 240},
  {"GFS00", "PLA", "Bambu Support W", 190, 240},
  {"GFS02", "PLA", "Bambu Support For PLA", 190, 240},
  {"GFS05", "PLA", "Bambu Support For PLA/PETG", 190, 240},
  {"GFSL99", "PLA", "Generic PLA", 190, 240},
  {"GFSL99_01", "PLA", "Generic PLA Silk", 190, 240},
  // ---- PLA-CF ----
  {"GFA50", "PLA-CF", "Bambu PLA-CF", 210, 250},
  {"GFL98", "PLA-CF", "Generic PLA-CF", 190, 240},
  {"GFSL98", "PLA-CF", "Generic PLA-CF", 190, 240},
  // ---- PLA-AERO ----
  {"GFA11", "PLA-AERO", "Bambu PLA Aero", 210, 260},
  // ---- PETG ----
  {"GFG00", "PETG", "Bambu PETG Basic", 230, 270},
  {"GFG01", "PETG", "Bambu PETG Translucent", 230, 270},
  {"GFG02", "PETG", "Bambu PETG HF", 230, 270},
  {"GFG96", "PETG", "Generic PETG HF", 220, 270},
  {"GFG99", "PETG", "Generic PETG", 220, 270},
  {"GFSG99", "PETG", "Generic PETG", 220, 270},
  // ---- PETG-CF ----
  {"GFG50", "PETG-CF", "Bambu PETG-CF", 240, 270},
  {"GFG98", "PETG-CF", "Generic PETG-CF", 240, 270},
  // ---- PCTG ----
  {"GFG97", "PCTG", "Generic PCTG", 240, 270},
  // ---- ABS ----
  {"GFB00", "ABS", "Bambu ABS", 240, 280},
  {"GFB99", "ABS", "Generic ABS", 240, 280},
  {"GFSB99", "ABS", "Generic ABS", 240, 280},
  {"GFS06", "ABS", "Bambu Support for ABS", 240, 270},
  // ---- ABS-GF ----
  {"GFB50", "ABS-GF", "Bambu ABS-GF", 240, 280},
  // ---- ASA ----
  {"GFB01", "ASA", "Bambu ASA", 240, 280},
  {"GFB98", "ASA", "Generic ASA", 240, 280},
  {"GFSB98", "ASA", "Generic ASA", 240, 280},
  // ---- ASA-AERO ----
  {"GFB02", "ASA-AERO", "Bambu ASA-Aero", 240, 280},
  // ---- ASA-CF ----
  {"GFB51", "ASA-CF", "Bambu ASA-CF", 250, 280},
  // ---- PA ----
  {"GFN99", "PA", "Generic PA", 240, 280},
  {"GFSN98", "PA", "Generic PA", 240, 280},
  {"GFS01", "PA", "Bambu Support G", 260, 300},
  {"GFS03", "PA", "Bambu Support For PA/PET", 260, 300},
  // ---- PA-CF ----
  {"GFN03", "PA-CF", "Bambu PA-CF", 260, 300},
  {"GFN04", "PA-CF", "Bambu PAHT-CF", 260, 300},
  {"GFN98", "PA-CF", "Generic PA-CF", 260, 300},
  {"GFSN99", "PA-CF", "Generic PA-CF", 260, 300},
  // ---- PA6-CF ----
  {"GFN05", "PA6-CF", "Bambu PA6-CF", 260, 300},
  // ---- PA-GF ----
  {"GFN08", "PA-GF", "Bambu PA6-GF", 260, 300},
  // ---- PC ----
  {"GFC00", "PC", "Bambu PC", 260, 290},
  {"GFC01", "PC", "Bambu PC FR", 260, 290},
  {"GFC99", "PC", "Generic PC", 260, 290},
  {"GFSC99", "PC", "Generic PC", 260, 290},
  // ---- PET-CF ----
  {"GFT01", "PET-CF", "Bambu PET-CF", 260, 290},
  // ---- PPS ----
  {"GFT97", "PPS", "Generic PPS", 300, 340},
  // ---- PPS-CF ----
  {"GFT02", "PPS-CF", "Bambu PPS-CF", 310, 340},
  {"GFT98", "PPS-CF", "Generic PPS-CF", 310, 340},
  // ---- PPA-CF ----
  {"GFN06", "PPA-CF", "Bambu PPA-CF", 280, 320},
  {"GFN97", "PPA-CF", "Generic PPA-CF", 280, 320},
  // ---- PPA-GF ----
  {"GFN96", "PPA-GF", "Generic PPA-GF", 280, 320},
  // ---- PP ----
  {"GFP97", "PP", "Generic PP", 220, 250},
  // ---- PP-CF ----
  {"GFP96", "PP-CF", "Generic PP-CF", 220, 250},
  // ---- PP-GF ----
  {"GFP95", "PP-GF", "Generic PP-GF", 220, 250},
  // ---- PE ----
  {"GFP99", "PE", "Generic PE", 175, 220},
  // ---- PE-CF ----
  {"GFP98", "PE-CF", "Generic PE-CF", 175, 220},
  // ---- PHA ----
  {"GFR98", "PHA", "Generic PHA", 190, 240},
  // ---- EVA ----
  {"GFR99", "EVA", "Generic EVA", 175, 220},
  // ---- TPU ----
  {"GFU00", "TPU", "Bambu TPU 95A HF", 200, 250},
  {"GFU01", "TPU", "Bambu TPU 95A", 200, 250},
  {"GFU03", "TPU", "Bambu TPU 90A", 200, 240},
  {"GFU04", "TPU", "Bambu TPU 85A", 200, 240},
  {"GFU99", "TPU", "Generic TPU", 200, 250},
  {"GFSR99", "TPU", "Generic TPU", 200, 250},
  // ---- TPU-AMS ----
  {"GFU02", "TPU-AMS", "Bambu TPU for AMS", 220, 240},
  {"GFU98", "TPU-AMS", "Generic TPU for AMS", 200, 250},
  // ---- PVA ----
  {"GFS04", "PVA", "Bambu PVA", 210, 250},
  {"GFS99", "PVA", "Generic PVA", 190, 240},
  {"GFSS99", "PVA", "Generic PVA", 190, 240},
  // ---- BVOH ----
  {"GFS97", "BVOH", "Generic BVOH", 190, 240},
  // ---- HIPS ----
  {"GFS98", "HIPS", "Generic HIPS", 220, 270},
};
static const size_t FILAMENT_COUNT = sizeof(FILAMENT_TABLE) / sizeof(FILAMENT_TABLE[0]);

// 屏上快捷写卡表：常用 12 个预设 ID（屏上 Write 页数字选择器用；
// 网页 /write 仍用上面全表）。改这里即改屏上可选范围，无需动菜单代码。
static const char* FILAMENT_QUICK[] = {
  "GFL99", "GFA01", "GFA00", "GFA05",
  "GFG99", "GFG00", "GFG02",
  "GFB99", "GFB00", "GFB01",
  "GFU99", "GFU00",
};
static const size_t FILAMENT_QUICK_COUNT =
    sizeof(FILAMENT_QUICK) / sizeof(FILAMENT_QUICK[0]);

// 屏上 Write 页材料表：6 个常用家族 × 官方/通用两档（B=0/G=1）。
// 每档只收敛一个代表预设（写卡+推送用），全量预设仍走网页 /write。
// 新增家族只加一行，菜单代码无需改动（上限由 fbxMatCount() 暴露）。
struct ScreenMat {
  const char* family;   // 家族名（推送 tray_type 用）
  const char* bambu;    // 官方代表预设 ID
  const char* generic;  // 通用代表预设 ID
};
static const ScreenMat SCREEN_MATS[] = {
  {"PLA",   "GFA01", "GFL99"},
  {"PETG",  "GFG00", "GFG99"},
  {"ABS",   "GFB00", "GFB99"},
  {"ASA",   "GFB01", "GFB98"},
  {"TPU",   "GFU00", "GFU99"},
  {"PA-CF", "GFN03", "GFN98"},
};
static const size_t SCREEN_MAT_COUNT =
    sizeof(SCREEN_MATS) / sizeof(SCREEN_MATS[0]);

// 屏上 Write 页常用色：12 色，存 FILAMENT_COLORS 下标（0-27）。
// 屏上选择器 0-11 步进，OLED 无中文字库，英文短名由菜单层维护显示。
static const uint8_t SCREEN_COLORS[] = {
  0, 1, 2, 8, 10, 12, 17, 20, 23, 15, 13, 5,
};
static const size_t SCREEN_COLOR_COUNT =
    sizeof(SCREEN_COLORS) / sizeof(SCREEN_COLORS[0]);

// 旧版卡片（Block4 存家族名字符串）的别名 → 新预设，保证已写好的卡继续可用
// 注意旧版 PA-CF 曾指向不存在的 GFN00，现归并到官方 Bambu PA-CF（GFN03）
struct FilamentAlias {
  const char* legacy;
  const char* idx;
};
static const FilamentAlias FILAMENT_ALIASES[] = {
  {"PLA", "GFA00"}, {"PLA-Matte", "GFA01"}, {"PETG", "GFG00"},
  {"ABS", "GFB00"}, {"ASA", "GFB01"}, {"TPU", "GFU00"},
  {"PA-CF", "GFN03"}, {"PC", "GFC00"}, {"Support", "GFS05"},
};
static const size_t FILAMENT_ALIAS_COUNT =
    sizeof(FILAMENT_ALIASES) / sizeof(FILAMENT_ALIASES[0]);

// 查表：key 为预设 ID（新卡）或旧版家族名字符串（旧卡别名）；找不到返回 nullptr
inline const FilamentInfo* lookupFilament(const String& key) {
  for (size_t i = 0; i < FILAMENT_COUNT; i++) {
    if (key == FILAMENT_TABLE[i].idx) return &FILAMENT_TABLE[i];
  }
  for (size_t i = 0; i < FILAMENT_ALIAS_COUNT; i++) {
    if (key == FILAMENT_ALIASES[i].legacy) {
      for (size_t j = 0; j < FILAMENT_COUNT; j++) {
        if (strcmp(FILAMENT_TABLE[j].idx, FILAMENT_ALIASES[i].idx) == 0)
          return &FILAMENT_TABLE[j];
      }
    }
  }
  return nullptr;
}

// 显示短名：去掉厂商前缀（Bambu / Generic）
  inline const char* shortFilamentName(const FilamentInfo* info) {
  if (!info || !info->name) return "?";
  static const char* kVendors[] = {
    "Bambu", "Generic",
  };
  const char* sp = strchr(info->name, ' ');
  if (!sp) return info->name;
  for (size_t i = 0; i < sizeof(kVendors) / sizeof(kVendors[0]); i++) {
    size_t n = strlen(kVendors[i]);
    if (strncmp(info->name, kVendors[i], n) == 0 && info->name[n] == ' ')
      return sp + 1;
  }
  return info->name;
}

// 颜色校验：8 位 HEX RGBA
inline bool checkColor(const String& s) {
  if (s.length() != 8) return false;
  for (uint8_t i = 0; i < 8; i++) {
    char c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')))
      return false;
  }
  return true;
}

// 写卡调色板：写卡只允许从中选择（读卡仍接受任意合法 HEX，旧卡不受影响）
struct FilamentColor {
  const char* name;  // 中文名
  const char* rgb;   // 6 位 RRGGBB
};
static const FilamentColor FILAMENT_COLORS[] = {
  {"白色", "FFFFFF"}, {"黑色", "000000"}, {"灰色", "808080"}, {"银色", "C0C0C0"},
  {"米色", "F5F5DC"}, {"棕色", "A52A2A"}, {"巧克力", "D2691E"}, {"奶油", "FFFDD0"},
  {"红色", "FF0000"}, {"深红", "8B0000"}, {"橙色", "FFA500"}, {"金色", "FFD700"},
  {"黄色", "FFFF00"}, {"粉色", "FFC0CB"}, {"品红", "FF00FF"}, {"紫色", "800080"},
  {"薰衣草", "E6E6FA"}, {"绿色", "008000"}, {"亮绿", "00FF00"}, {"拓竹绿", "00AE42"},
  {"青色", "00FFFF"}, {"深青", "008080"}, {"松石", "40E0D0"}, {"蓝色", "0000FF"},
  {"天蓝", "87CEEB"}, {"藏青", "000080"}, {"橄榄", "808000"}, {"珊瑚", "FF7F50"},
};
static const size_t FILAMENT_COLOR_COUNT =
    sizeof(FILAMENT_COLORS) / sizeof(FILAMENT_COLORS[0]);

// s 为 8 位 RGBA：合法 HEX 且前 6 位命中调色板返回 true
inline bool isPaletteColor(const String& s) {
  if (!checkColor(s)) return false;
  String rgb = s.substring(0, 6);
  rgb.toUpperCase();
  for (size_t i = 0; i < FILAMENT_COLOR_COUNT; i++) {
    if (rgb == FILAMENT_COLORS[i].rgb) return true;
  }
  return false;
}

// 写卡页类型下拉：按家族分组（optgroup），value 为预设 ID（要求表内同家族连续排列）
inline String filamentTypeOptions() {
  String h;
  const char* group = nullptr;
  for (size_t i = 0; i < FILAMENT_COUNT; i++) {
    if (!group || strcmp(group, FILAMENT_TABLE[i].type) != 0) {
      if (group) h += "</optgroup>";
      group = FILAMENT_TABLE[i].type;
      h += "<optgroup label='";
      h += group;
      h += "'>";
    }
    h += "<option value='";
    h += FILAMENT_TABLE[i].idx;
    h += "'>";
    h += FILAMENT_TABLE[i].name;
    h += " (";
    h += FILAMENT_TABLE[i].idx;
    h += ")</option>";
  }
  if (group) h += "</optgroup>";
  return h;
}

// 写卡页颜色色块：调色板点选（radio 网格），value 为 8 位 RGBA
// （alpha 固定 FF，只允许调色板；首块默认选中）
inline String filamentColorSwatches() {
  String h = "<div class='cgrid'>";
  for (size_t i = 0; i < FILAMENT_COLOR_COUNT; i++) {
    h += "<label class='csw'><input type='radio' name='color' value='";
    h += FILAMENT_COLORS[i].rgb;
    h += "FF'";
    if (i == 0) h += " checked";
    h += "><span style='background:#";
    h += FILAMENT_COLORS[i].rgb;
    h += "'></span><i>";
    h += FILAMENT_COLORS[i].name;
    h += "</i></label>";
  }
  h += "</div>";
  return h;
}
