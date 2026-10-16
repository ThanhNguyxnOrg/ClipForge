#!/usr/bin/env python3
"""Rasterize the ClipForge logo SVG geometry to a multi-size Windows .ico.

The logo is pure geometry (one polygon, rects, one linear gradient), so we
rasterize it directly with supersampled scanline fills instead of pulling in
an SVG engine. stdlib only (struct, zlib, math).

Usage:  python3 tools/make_icon.py  ->  assets/clipforge.ico
"""

import math
import struct
import zlib

NAVY = (0x14, 0x18, 0x26)
ORANGE0 = (0xFF, 0x55, 0x00)
ORANGE1 = (0xFF, 0x88, 0x00)
CYAN = (0x00, 0xE5, 0xFF)

# Outer C shape, viewBox 120x120 coordinates.
POLY = [
    (24, 0), (104, 0), (120, 16), (120, 32), (36, 32), (32, 36),
    (32, 84), (36, 88), (120, 88), (120, 104), (104, 120),
    (24, 120), (0, 96), (0, 24),
]


def lerp(a, b, t):
    return tuple(int(round(x + (y - x) * t)) for x, y in zip(a, b))


def point_in_poly(px, py, poly):
    inside = False
    n = len(poly)
    j = n - 1
    for i in range(n):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > py) != (yj > py):
            xinters = (xj - xi) * (py - yi) / (yj - yi) + xi
            if px < xinters:
                inside = not inside
        j = i
    return inside


def rounded_rect_dist(px, py, x, y, w, h, r):
    qx = abs(px - (x + w / 2.0)) - (w / 2.0 - r)
    qy = abs(py - (y + h / 2.0)) - (h / 2.0 - r)
    ax, ay = max(qx, 0.0), max(qy, 0.0)
    return math.hypot(ax, ay) + min(max(qx, qy), 0.0) - r


def render(size, shape_rgb):
    scale = size / 120.0
    ss = 4
    big = size * ss
    cov = [0.0] * (big * big)
    col = [None] * (big * big)
    poly = [(x * scale * ss, y * scale * ss) for x, y in POLY]

    def put(ix, iy, rgb, a):
        if 0 <= ix < big and 0 <= iy < big:
            idx = iy * big + ix
            if a > cov[idx]:
                cov[idx] = a
                col[idx] = rgb

    # Shape fill (even-odd scanline).
    minx = min(p[0] for p in poly)
    maxx = max(p[0] for p in poly)
    miny = min(p[1] for p in poly)
    maxy = max(p[1] for p in poly)
    for iy in range(max(0, int(miny)), min(big, int(maxy) + 1)):
        inside = False
        for ix in range(max(0, int(minx)), min(big, int(maxx) + 1)):
            if point_in_poly(ix + 0.5, iy + 0.5, poly):
                put(ix, iy, shape_rgb, 1.0)
    # Orange bar with horizontal gradient.
    bx, by, bw, bh, br = 46 * scale * ss, 44 * scale * ss, 74 * scale * ss, 14 * scale * ss, 2 * scale * ss
    for iy in range(int(by), int(by + bh) + 1):
        for ix in range(int(bx), int(bx + bw) + 1):
            d = rounded_rect_dist(ix + 0.5, iy + 0.5, bx, by, bw, bh, br)
            if d < 0.5:
                t = (ix - bx) / max(bw, 1.0)
                a = min(1.0, max(0.0, 0.5 - d))
                put(ix, iy, lerp(ORANGE0, ORANGE1, t), a)
    # Cyan blocks.
    for cx, cw, ch in ((46, 14, 18), (68, 14, 18), (90, 14, 10)):
        x, y, w, h = cx * scale * ss, 64 * scale * ss, cw * scale * ss, ch * scale * ss
        r = 1.5 * scale * ss
        for iy in range(int(y), int(y + h) + 1):
            for ix in range(int(x), int(x + w) + 1):
                d = rounded_rect_dist(ix + 0.5, iy + 0.5, x, y, w, h, r)
                if d < 0.5:
                    put(ix, iy, CYAN, min(1.0, max(0.0, 0.5 - d)))

    # Downsample to RGBA.
    px = bytearray(size * size * 4)
    for oy in range(size):
        for ox in range(size):
            r = g = b = a = 0.0
            for dy in range(ss):
                for dx in range(ss):
                    idx = (oy * ss + dy) * big + (ox * ss + dx)
                    if col[idx] is not None:
                        cr, cg, cb = col[idx]
                        r += cr * cov[idx]
                        g += cg * cov[idx]
                        b += cb * cov[idx]
                        a += cov[idx]
            n = ss * ss
            o = (oy * size + ox) * 4
            px[o] = int(round(r / n))
            px[o + 1] = int(round(g / n))
            px[o + 2] = int(round(b / n))
            px[o + 3] = int(round(255.0 * a / n))
    return bytes(px)


def png_bytes(size, shape_rgb):
    raw = render(size, shape_rgb)
    stride = size * 4
    scan = bytearray()
    for y in range(size):
        scan.append(0)
        scan += raw[y * stride:(y + 1) * stride]

    def chunk(typ, data):
        out = struct.pack(">I", len(data)) + typ + data
        return out + struct.pack(">I", zlib.crc32(typ + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(bytes(scan), 9)) + chunk(b"IEND", b""))


def write_ico(path, shape_rgb):
    sizes = (16, 32, 48, 256)
    blobs = [png_bytes(s, shape_rgb) for s in sizes]
    out = bytearray(struct.pack("<HHH", 0, 1, len(sizes)))
    offset = 6 + 16 * len(sizes)
    for s, blob in zip(sizes, blobs):
        w = s if s < 256 else 0
        out += struct.pack("<BBBBHHII", w, w, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)
    for blob in blobs:
        out += blob
    with open(path, "wb") as f:
        f.write(bytes(out))
    print(f"wrote {path} ({len(out)} bytes)")


if __name__ == "__main__":
    write_ico("assets/clipforge.ico", NAVY)
