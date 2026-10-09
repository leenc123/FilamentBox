#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""sim_cn_draw.py — 按 custom.c Cn_DrawStr 逻辑 + 真实 font_cn12.c 字节，
仿真 OLED 行绘制输出（BMP 校样，1px=1px，便core查解码/查表/XBM链路）。"""
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
C = (ROOT / "firmware/filament_box/src/miaoui/fonts/font_cn12.c").read_text(encoding="utf-8")

codes = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", C.split("CN12_CODES")[1].split("};")[0])]
rows = re.findall(r"\{([0-9xA-Fa-f,]+)\}, // U\+", C)
assert len(codes) == len(rows) == 67, (len(codes), len(rows))
GLYPHS = {}
for cp, r in zip(codes, rows):
    vals = [int(x, 16) for x in r.split(",")]
    assert len(vals) == 24, (hex(cp), len(vals))
    GLYPHS[cp] = vals


def decode_utf8(data: bytes):
    out = []
    i = 0
    while i < len(data):
        b = data[i]
        if b < 0x80:
            out.append(("A", b))
            i += 1
        elif (b & 0xE0) == 0xC0 and i + 1 < len(data) and (data[i + 1] & 0xC0) == 0x80:
            out.append(("C", ((b & 0x1F) << 6) | (data[i + 1] & 0x3F)))
            i += 2
        elif (b & 0xF0) == 0xE0 and i + 2 < len(data) and (data[i + 1] & 0xC0) == 0x80 and (data[i + 2] & 0xC0) == 0x80:
            out.append(("C", ((b & 0x0F) << 12) | ((data[i + 1] & 0x3F) << 6) | (data[i + 2] & 0x3F)))
            i += 3
        else:
            i += 1
    return out


def render_line(s: str, ascii_adv: int = 6):
    """返回 (pixels[y][x] bool 网格, 宽度)。ASCII 用块体占位（只看中文部分）。"""
    toks = decode_utf8(s.encode("utf-8"))
    w = sum(12 if k == "C" else ascii_adv for k, _ in toks)
    grid = [[False] * w for _ in range(12)]
    x = 0
    missing = []
    for k, v in toks:
        if k == "A":
            x += ascii_adv
        else:
            bits = GLYPHS.get(v)
            if bits is None:
                missing.append(v)
            else:
                for y in range(12):
                    for px in range(12):
                        if bits[y * 2 + (px >> 3)] & (1 << (px % 8)):
                            grid[y][x + px] = True
            x += 12
    return grid, missing


def to_bmp(grids, path: Path, scale=4):
    gh = len(grids[0])
    gw = max(len(g[0]) for g in grids)
    rows_n = len(grids)
    H = rows_n * (gh + 6) * scale
    W = gw * scale
    row_bytes = ((W * 3 + 3) // 4) * 4
    img = bytearray(row_bytes * H)
    for r, g in enumerate(grids):
        oy = r * (gh + 6) * scale
        for y in range(gh):
            for xx in range(len(g[0])):
                if g[y][xx]:
                    for sy in range(scale):
                        for sx in range(scale):
                            o = (oy + (y * scale + sy)) * row_bytes + xx * scale + sx
                            img[o : o + 3] = b"\x00\x00\x00"
    for i in range(len(img)):
        if img[i] == 0 and False:
            pass
    # 背景刷白：先全白再画黑
    img = bytearray(b"\xff" * (row_bytes * H))
    for r, g in enumerate(grids):
        oy = r * (gh + 6) * scale
        for y in range(gh):
            for xx in range(len(g[0])):
                if g[y][xx]:
                    for sy in range(scale):
                        base = (oy + (y * scale + sy)) * row_bytes + xx * scale * 3
                        for sx in range(scale):
                            o = base + sx * 3
                            img[o : o + 3] = b"\x00\x00\x00"
    hdr = struct.pack("<2sIHHI", b"BM", 54 + len(img), 0, 0, 54)
    dib = struct.pack("<IIIHHIIiiII", 40, W, H, 1, 24, 0, len(img), 0, 0, 0, 0)
    # BMP 自下而上：翻转
    flip = bytearray()
    for y in range(H - 1, -1, -1):
        flip += img[y * row_bytes : (y + 1) * row_bytes]
    path.write_bytes(hdr + dib + bytes(flip))
    print("spec ->", path)


tests = ["空", "结果:", "红色", "巧克力", "槽位", "Write结果"]
grids = []
for t in tests:
    g, missing = render_line(t)
    print(repr(t), "missing:", [hex(m) for m in missing] or "none")
    grids.append(g)
to_bmp(grids, Path(r"C:\Users\lienc\AppData\Local\Temp\opencode\cn_draw_sim.bmp"))
