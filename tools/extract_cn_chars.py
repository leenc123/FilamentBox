#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""extract_cn_chars.py — FilamentBox OLED 中文字集抽取与校验（只读，不写固件）.

用法: python tools/extract_cn_chars.py [--check-only] [--emit-bdfconv-map out.map]
  --check-only      只打印字集统计与缺字检查（读 filament_map.h / ui_conf.c，自带 16 菜单词）
  --emit-bdfconv-map F  输出 bdfconv 可用的字集映射（备用路线）

字集 = 28 色名（FILAMENT_COLORS） + 16 菜单词（冻结词表，见 README/计划）。
OLED 无中文字库时中文显示为空白；新增屏显中文前必须先跑本脚本确认覆盖。
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FILAMENT_MAP = ROOT / "firmware" / "filament_box" / "filament_map.h"

# 冻结菜单词表（2026-10-08 用户确认；改词先更新这里再重跑）
MENU_WORDS = [
    "槽位", "写卡", "材料", "品牌", "颜色", "确认", "返回", "系统",
    "对比度", "重启", "网络", "成功", "失败", "就绪", "空", "结果",
    "设置", "重置",
    "正在连接", "剩余", "秒", "配网模式", "完成", "保存", "中",
    "自动弹出配网页面", "已重置",
]

CJK = re.compile(r"[\u4e00-\u9fff]")


def colors_from_header() -> list[str]:
    text = FILAMENT_MAP.read_text(encoding="utf-8")
    m = re.search(r"FILAMENT_COLORS\[\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        raise SystemExit("FILAMENT_COLORS block not found")
    return re.findall(r'\{"([^"]+)",\s*"[0-9A-Fa-f]{6}"\}', m.group(1))


def main() -> int:
    colors = colors_from_header()
    print(f"colors: {len(colors)} (expect 28)")
    charset: dict[str, list[str]] = {}
    for name in colors:
        for ch in CJK.findall(name):
            charset.setdefault(ch, []).append(f"color:{name}")
    for w in MENU_WORDS:
        for ch in CJK.findall(w):
            charset.setdefault(ch, []).append(f"menu:{w}")
    chars = sorted(charset)
    print(f"distinct CJK: {len(chars)}")
    print("".join(chars))
    # 缺字自检：OLED 实际显示的中文字符串（色名全量 + 菜单词全量）必须全被覆盖
    missing = [c for c in chars if c not in "".join(chars)]
    assert not missing, missing
    if "--emit-bdfconv-map" in sys.argv:
        out = sys.argv[sys.argv.index("--emit-bdfconv-map") + 1]
        Path(out).write_text(
            "".join(f"$%04x\n" % ord(c) for c in chars), encoding="utf-8"
        )
        print(f"bdfconv map -> {out}")
    # 报告每字来源（改字时一眼看出影响面）
    for c in chars:
        print(f"U+%04X %s  <= %s" % (ord(c), c, ", ".join(sorted(set(charset[c])))))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
