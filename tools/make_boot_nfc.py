#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""make_boot_nfc.py — 开机NFC卡片图标（48x48 XBM，卡片本体；波纹由代码画以做动画）.

用法: python tools/make_boot_nfc.py
输出: <TEMP>/boot_nfc.bmp（校样） + stdout C 数组（确认后贴入 images/image.c）
行字节序同 u8g2_DrawXBMP（每行 6 字节，LSB=左）。
"""
import math
import struct
import sys
from pathlib import Path

W = H = 48
px = [[False] * W for _ in range(H)]


def dot(x, y):
    if 0 <= x < W and 0 <= y < H:
        px[y][x] = True


def hline(x0, x1, y):
    for x in range(min(x0, x1), max(x0, x1) + 1):
        dot(x, y)


def vline(x, y0, y1):
    for y in range(min(y0, y1), max(y0, y1) + 1):
        dot(x, y)


def rrect(x0, y0, x1, y1, r):
    hline(x0 + r, x1 - r, y0)
    hline(x0 + r, x1 - r, y1)
    vline(x0, y0 + r, y1 - r)
    vline(x1, y0 + r, y1 - r)
    for cx, cy, sx, sy in ((x0 + r, y0 + r, -1, -1), (x1 - r, y0 + r, 1, -1),
                           (x0 + r, y1 - r, -1, 1), (x1 - r, y1 - r, 1, 1)):
        rx, ry, d = r, 0, 1 - r
        while rx >= ry:
            for ox, oy in ((rx, ry), (ry, rx)):
                dot(cx + sx * ox, cy + sy * oy)
            ry += 1
            if d < 0:
                d += 2 * ry + 1
            else:
                rx -= 1
                d += 2 * (ry - rx) + 1


# 卡片本体：圆角矩形 + 芯片 + 芯片纹路 + 两行横线
rrect(4, 14, 26, 34, 3)
rrect(8, 20, 14, 26, 1)
vline(11, 20, 26)
hline(8, 14, 23)
hline(17, 24, 21)
hline(17, 24, 25)

rows = []
for y in range(H):
    rb = bytearray(6)
    for x in range(W):
        if px[y][x]:
            rb[x >> 3] |= 1 << (x & 7)
    rows.append(rb)

print("const unsigned char boot_nfc_48[] = {")
for i, rb in enumerate(rows):
    print("    " + ", ".join(f"0x{b:02X}" for b in rb) + ("," if i < H - 1 else ""))
print("};")
print(f"// {W * H // 8} bytes", file=sys.stderr)

S = 4
Wpx, Hpx = W * S, H * S
stride = ((Wpx * 3 + 3) // 4) * 4
img = bytearray(b"\xff" * (stride * Hpx))
for y in range(H):
    for x in range(W):
        if px[y][x]:
            for sy in range(S):
                base = (y * S + sy) * stride + x * S * 3
                for sx in range(S):
                    img[base + sx * 3 : base + sx * 3 + 3] = b"\x00\x00\x00"
hdr = struct.pack("<2sIHHI", b"BM", 54 + len(img), 0, 0, 54)
dib = struct.pack("<IIIHHIIiiII", 40, Wpx, Hpx, 1, 24, 0, len(img), 0, 0, 0, 0)
raw = hdr + dib + b"".join(img[i * stride : (i + 1) * stride] for i in range(Hpx - 1, -1, -1))
out = Path(r"C:\Users\lienc\AppData\Local\Temp\opencode\boot_nfc.bmp")
out.write_bytes(raw)
print(f"spec -> {out}", file=sys.stderr)
