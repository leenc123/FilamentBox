#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_cn_coverage.py — 校验固件里所有屏显中文字符串都被点阵字库覆盖.

扫 firmware/ 下 .c/.h/.cpp/.ino，去掉注释后取字符串字面量里的 CJK，
与 tools/cn12_codepoints.txt 对比。缺字 => 屏上显示空白，须先补字再编译。
用法: python tools/check_cn_coverage.py (exit 1 = 有缺字)
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
codes = set(
    l.strip()
    for l in (ROOT / "tools" / "cn12_codepoints.txt").read_text(encoding="ascii").splitlines()
    if l.strip()
)
covered = set(chr(int(x[2:], 16)) for x in codes)

exts = (".c", ".h", ".cpp", ".ino")
shown: dict[str, set[str]] = {}
for p in ROOT.joinpath("firmware").rglob("*"):
    if not (p.is_file() and p.suffix in exts):
        continue
    if "libraries" in p.parts:  # 第三方库副本不扫（非 UTF-8、无屏显中文）
        continue
    t = p.read_text(encoding="utf-8")
    t = re.sub(r"//.*", "", t)
    t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
    for s in re.findall(r'"((?:[^"\\]|\\.)*)"', t):
        for c in re.findall(r"[\u4e00-\u9fff]", s):
            shown.setdefault(c, set()).add(str(p.relative_to(ROOT)))

missing = sorted(shown)
bad = [(c, shown[c]) for c in missing if c not in covered]
print(f"displayed CJK: {len(missing)}, missing: {len(bad)}")
for c, files in bad:
    print(f"  U+{ord(c):04X} {c} <= {sorted(files)}")
if "--list-missing" in sys.argv:
    print("MISSING_CHARS=" + "".join(c for c, _ in bad))
    print("MISSING_CODES=" + " ".join(f"U+{ord(c):04X}" for c, _ in bad))
sys.exit(1 if bad else 0)
