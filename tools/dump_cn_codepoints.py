#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""dump_cn_codepoints.py — 输出 OLED 中文字集码点表（ASCII，每行 U+XXXX，供点阵生成用）."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FILAMENT_MAP = ROOT / "firmware" / "filament_box" / "filament_map.h"
MENU_WORDS = [
    "槽位", "写卡", "材料", "品牌", "颜色", "确认", "返回", "系统",
    "对比度", "重启", "网络", "成功", "失败", "就绪", "空", "结果",
    "设置", "重置",
    "正在连接", "剩余", "秒", "配网模式", "完成", "保存", "中",
    "自动弹出配网页面", "已重置",
]
# 网页字符串里顺带覆盖的字（屏上暂不用，补上让覆盖检查全绿、以后直接可用）：
# 来自 filament_box.ino 两条 dashWriteResult 网页文案（write failed / 卡已写入但推送失败）
EXTRA_COVER = [
    0x4E0A, 0x4F1A, 0x4F46, 0x5165, 0x52A8, 0x540E, 0x5426, 0x5668,
    0x5728, 0x5907, 0x5DF2, 0x63A8, 0x653E, 0x662F, 0x7A0D, 0x81EA,
    0x8BD5, 0x8BE5, 0x8BFB, 0x9001,
]

text = FILAMENT_MAP.read_text(encoding="utf-8")
m = re.search(r"FILAMENT_COLORS\[\]\s*=\s*\{(.*?)\};", text, re.S)
if not m:
    raise SystemExit("FILAMENT_COLORS block not found")
colors = re.findall(r'"([^"]+)",\s*"[0-9A-Fa-f]{6}"', m.group(1))
assert len(colors) == 28, len(colors)
chars = sorted(set("".join(colors) + "".join(MENU_WORDS)) | set(chr(c) for c in EXTRA_COVER))
out = Path(sys.argv[1]) if len(sys.argv) > 1 else None
body = "\n".join("U+%04X" % ord(c) for c in chars) + "\n"
if out:
    out.write_text(body, encoding="ascii")
print(f"{len(chars)} chars -> {out or 'stdout'}")
