#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""make_boot_logo.py — FilamentBox 开机线盘图标（48x48 XBM）生成 + 校样.

用法: python tools/make_boot_logo.py
输出: <TEMP>/boot_spool.png（校样，必看）
      stdout 打印 C 数组（确认好看后手工贴入 images/image.c）
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


def circle(cx, cy, r):
    x, y, d = r, 0, 1 - r
    while x >= y:
        for sx, sy in ((x, y), (y, x), (-x, y), (-y, x), (x, -y), (y, -x), (-x, -y), (-y, -x)):
            dot(cx + sx, cy + sy)
        y += 1
        if d < 0:
            d += 2 * y + 1
        else:
            x -= 1
            d += 2 * (y - x) + 1


def disc(cx, cy, r):
    for y in range(cy - r, cy + r + 1):
        for x in range(cx - r, cx + r + 1):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                dot(x, y)


def hline(x0, x1, y):
    for x in range(min(x0, x1), max(x0, x1) + 1):
        dot(x, y)


def curve(p0, p1, p2, steps=60):
    for i in range(steps + 1):
        t = i / steps
        x = round((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0])
        y = round((1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1])
        dot(x, y)
        dot(x + 1, y)


CX, CY = 22, 22
# 外缘法兰
circle(CX, CY, 20)
circle(CX, CY, 19)
# 绕线：同心环
for r in (15, 12, 9):
    circle(CX, CY, r)
# 轴孔
circle(CX, CY, 4)
disc(CX, CY, 1)
# 出料丝：从右下切线甩到右下角
curve((CX + 14, CY + 14), (CX + 22, CY + 16), (47, 46))
# 丝头小点
disc(45, 44, 1)

# 打包 XBM（LSB 左）
rows = []
for y in range(H):
    rb = bytearray(6)
    for x in range(W):
        if px[y][x]:
            rb[x >> 3] |= 1 << (x & 7)
    rows.append(rb)

print("const unsigned char boot_spool_48[] = {")
for i, rb in enumerate(rows):
    print("    " + ", ".join(f"0x{b:02X}" for b in rb) + ("," if i < H - 1 else ""))
print("};")
print(f"// {W * H // 8} bytes", file=sys.stderr)

# 校样 BMP（x4 放大）
S = 4
Wpx, Hpx = W * S, H * S
stride = ((Wpx * 3 + 3) // 4) * 4
img = bytearray(b"\xff" * (stride * Hpx))
for y in range(H):
    for x in range(W):
        if px[y][x]:
            for sy in range(S):
                base = ((y * S + sy) * stride + x * S * 3)
                for sx in range(S):
                    img[base + sx * 3 : base + sx * 3 + 3] = b"\x00\x00\x00"
hdr = struct.pack("<2sIHHI", b"BM", 54 + len(img), 0, 0, 54)
dib = struct.pack("<IIIHHIIiiII", 40, Wpx, Hpx, 1, 24, 0, len(img), 0, 0, 0, 0)
raw = hdr + dib + b"".join(img[i * stride : (i + 1) * stride] for i in range(Hpx - 1, -1, -1))
out = Path(r"C:\Users\lienc\AppData\Local\Temp\opencode\boot_spool.bmp")
out.write_bytes(raw)
print(f"spec -> {out}", file=sys.stderr)
