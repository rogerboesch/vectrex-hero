#!/usr/bin/env python3
"""
zx_to_gbc_color.py — Extract 8x8 tiles from a ZX Spectrum screenshot
with full-color palettes.

Unlike zx_to_gbc.py (which uses 2-color tiles), this version maps all
ZX Spectrum colors into GBC palettes and uses all 4 color indices per tile.

GBC has 8 BG palettes x 4 colors = 32 color slots. The ZX Spectrum uses
up to 15 colors, so we cluster them into palettes and assign each tile
to the best-matching palette.

Since GBC Workbench renders all tiles with palette 0 in the editor,
we also output a "best 4 colors" single-palette version that shows the
most color variety.

Usage: python3 zx_to_gbc_color.py <screenshot.png> [output.json]
"""

import sys
import json
from collections import Counter
from PIL import Image
import numpy as np

# ZX Spectrum palette (14 colors used + black)
ZX_COLORS_RGB = [
    (0, 0, 0),        # 0  black
    (0, 0, 205),       # 1  blue
    (205, 0, 0),       # 2  red
    (205, 0, 205),     # 3  magenta
    (0, 205, 0),       # 4  green
    (0, 205, 205),     # 5  cyan
    (205, 205, 0),     # 6  yellow
    (205, 205, 205),   # 7  white
    (0, 0, 255),       # 9  bright blue
    (255, 0, 0),       # 10 bright red
    (255, 0, 255),     # 11 bright magenta
    (0, 255, 0),       # 12 bright green
    (0, 255, 255),     # 13 bright cyan
    (255, 255, 0),     # 14 bright yellow
    (255, 255, 255),   # 15 bright white
]

def rgb8_to_rgb5(r, g, b):
    return [r >> 3, g >> 3, b >> 3]

def color_dist(c1, c2):
    return sum((int(a) - int(b)) ** 2 for a, b in zip(c1, c2))

def nearest_color(pixel, palette):
    """Find the index of the nearest color in palette to pixel."""
    best_i = 0
    best_d = float('inf')
    for i, c in enumerate(palette):
        d = color_dist(pixel, c)
        if d < best_d:
            best_d = d
            best_i = i
    return best_i

def find_content_bounds(pixels):
    h, w = pixels.shape[:2]
    border = tuple(pixels[0, 0])
    left = right = top = bottom = 0
    for x in range(w):
        if not np.all(pixels[:, x] == border):
            left = x; break
    for x in range(w - 1, -1, -1):
        if not np.all(pixels[:, x] == border):
            right = x; break
    for y in range(h):
        if not np.all(pixels[y, :] == border):
            top = y; break
    for y in range(h - 1, -1, -1):
        if not np.all(pixels[y, :] == border):
            bottom = y; break
    cw = right - left + 1
    ch = bottom - top + 1
    if abs(cw - 256) <= 2:
        left = left + (cw - 256) // 2; cw = 256
    if abs(ch - 192) <= 2:
        top = top + (ch - 192) // 2; ch = 192
    return left, top, min(cw, 256), min(ch, 192)


def pick_best_4_colors(content):
    """Pick the 4 most representative colors from the screen.

    Strategy: take all unique colors, pick the 4 that cover the most
    pixels when each pixel is mapped to its nearest palette color.
    We use a greedy approach: start with black, then add the color
    that reduces total error the most.
    """
    flat = content.reshape(-1, 3)
    unique_colors = [tuple(c) for c in np.unique(flat, axis=0)]
    color_counts = Counter(map(tuple, flat))

    # Always start with black (most common background)
    palette = [(0, 0, 0)]
    remaining = [c for c in unique_colors if c != (0, 0, 0)]

    # Greedily add colors that reduce total mapping error most
    for _ in range(3):
        best_color = None
        best_error = float('inf')
        for candidate in remaining:
            test_pal = palette + [candidate]
            error = 0
            for color, count in color_counts.items():
                d = min(color_dist(color, pc) for pc in test_pal)
                error += d * count
            if error < best_error:
                best_error = error
                best_color = candidate
        if best_color:
            palette.append(best_color)
            remaining.remove(best_color)

    return palette


def build_multi_palettes(content):
    """Build up to 8 palettes of 4 colors each, covering all ZX colors.

    Strategy: collect all unique color pairs (paper, ink) per cell,
    then cluster into palettes of 4 colors. Black is always color 0
    in every palette.
    """
    rows = content.shape[0] // 8
    cols = content.shape[1] // 8

    # Collect all unique colors used
    flat = content.reshape(-1, 3)
    unique_colors = list(set(map(tuple, flat)))

    # Always have black
    if (0, 0, 0) in unique_colors:
        unique_colors.remove((0, 0, 0))

    # Each palette: [black, color_a, color_b, color_c]
    # Greedily pack colors into palettes of 3 non-black colors each
    palettes_rgb = []
    remaining = list(unique_colors)

    while remaining and len(palettes_rgb) < 8:
        pal = [(0, 0, 0)]
        # Pick up to 3 remaining colors
        for _ in range(3):
            if not remaining:
                break
            # Pick the most frequent remaining color
            best = max(remaining,
                       key=lambda c: np.sum(np.all(flat == c, axis=1)))
            pal.append(best)
            remaining.remove(best)
        # Pad to 4 if needed
        while len(pal) < 4:
            pal.append((0, 0, 0))
        palettes_rgb.append(pal)

    # Pad to at least 6 palettes
    while len(palettes_rgb) < 6:
        palettes_rgb.append([(0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0)])

    return palettes_rgb


def best_palette_for_cell(cell, palettes_rgb):
    """Find which palette best represents the colors in an 8x8 cell."""
    cell_colors = set(map(tuple, cell.reshape(-1, 3)))
    best_pal = 0
    best_err = float('inf')
    for pi, pal in enumerate(palettes_rgb):
        err = 0
        for cc in cell_colors:
            err += min(color_dist(cc, pc) for pc in pal)
        if err < best_err:
            best_err = err
            best_pal = pi
    return best_pal


def extract_tiles_multipal(content, palettes_rgb):
    """Extract tiles with per-tile palette assignment."""
    rows = content.shape[0] // 8
    cols = content.shape[1] // 8

    tile_data = []
    tile_index = {}
    tilemap = []
    palmap = []

    for row in range(rows):
        tile_row = []
        pal_row = []
        for col in range(cols):
            cell = content[row * 8:(row + 1) * 8, col * 8:(col + 1) * 8]

            # Find best palette for this cell
            pi = best_palette_for_cell(cell, palettes_rgb)
            pal = palettes_rgb[pi]

            # Build GBC 2bpp tile using that palette
            tile_bytes = []
            for py in range(8):
                lo = 0
                hi = 0
                for px in range(8):
                    pixel = tuple(cell[py, px])
                    ci = nearest_color(pixel, pal)
                    if ci & 1:
                        lo |= (1 << (7 - px))
                    if ci & 2:
                        hi |= (1 << (7 - px))
                tile_bytes.append(lo)
                tile_bytes.append(hi)

            key = (tuple(tile_bytes), pi)
            if key not in tile_index:
                tile_index[key] = len(tile_data)
                tile_data.append(list(tile_bytes))

            tile_row.append(tile_index[key])
            pal_row.append(pi)

        tilemap.append(tile_row)
        palmap.append(pal_row)

    return tile_data, tilemap, palmap


def build_project(tile_data, tilemap, palmap, palettes_rgb, name="ZX Color Import"):
    """Build GBC Workbench project.json with per-tile palettes."""
    bg_palettes = []
    for pal in palettes_rgb[:8]:
        bg_palettes.append([rgb8_to_rgb5(*c) for c in pal])
    while len(bg_palettes) < 6:
        bg_palettes.append([[0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]])

    spr_palettes = [
        [[0, 0, 0], [0, 12, 0], [0, 24, 4], [8, 31, 8]],
        [[0, 0, 0], [20, 0, 0], [31, 8, 4], [31, 20, 16]],
        [[0, 0, 0], [16, 0, 16], [26, 6, 26], [31, 18, 31]],
        [[0, 0, 0], [0, 10, 16], [0, 22, 28], [12, 31, 31]],
        [[0, 0, 0], [16, 16, 0], [28, 28, 4], [31, 31, 20]],
    ]

    rows = len(tilemap)
    cols = len(tilemap[0]) if rows > 0 else 0

    return {
        "tileset": tile_data,
        "bg_palettes": bg_palettes,
        "spr_palettes": spr_palettes,
        "levels": [{
            "name": name,
            "width": cols,
            "height": rows,
            "tiles": tilemap,
            "palettes": palmap,
            "entities": [],
        }],
    }


class NumpyEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, (np.integer,)):
            return int(obj)
        if isinstance(obj, (np.floating,)):
            return float(obj)
        if isinstance(obj, np.ndarray):
            return obj.tolist()
        return super().default(obj)


ZX_COLOR_MAP = {
    "black": (0, 0, 0), "blue": (0, 0, 205), "red": (205, 0, 0),
    "magenta": (205, 0, 205), "green": (0, 205, 0), "cyan": (0, 205, 205),
    "yellow": (205, 205, 0), "white": (205, 205, 205),
    "bright-blue": (0, 0, 255), "bright-red": (255, 0, 0),
    "bright-magenta": (255, 0, 255), "bright-green": (0, 255, 0),
    "bright-cyan": (0, 255, 255), "bright-yellow": (255, 255, 0),
    "bright-white": (255, 255, 255),
}


def parse_color(s):
    """Parse a color name or 'r,g,b' string."""
    s = s.strip().lower()
    if s in ZX_COLOR_MAP:
        return ZX_COLOR_MAP[s]
    parts = s.split(',')
    if len(parts) == 3:
        return tuple(int(x) for x in parts)
    raise ValueError(f"Unknown color: {s}. Use a ZX name or r,g,b.")


def main():
    import argparse
    parser = argparse.ArgumentParser(description="ZX Spectrum screenshot to GBC tiles (color)")
    parser.add_argument("input", help="Input screenshot PNG")
    parser.add_argument("output", nargs="?", help="Output JSON (default: <input>_color.json)")
    parser.add_argument("--colors", "-c", nargs=4, metavar="COLOR",
                        help="4 palette colors (ZX names or r,g,b). "
                             "Names: black, red, blue, cyan, green, yellow, white, magenta, "
                             "bright-red, bright-blue, bright-cyan, bright-green, bright-yellow, "
                             "bright-white, bright-magenta")
    parser.add_argument("--list-colors", action="store_true",
                        help="Show color usage in the image and exit")
    args = parser.parse_args()

    input_path = args.input
    output_path = args.output or input_path.rsplit('.', 1)[0] + '_color.json'

    img = Image.open(input_path).convert('RGB')
    pixels = np.array(img)
    print(f"Input: {input_path} ({img.size[0]}x{img.size[1]})")

    x0, y0, cw, ch = find_content_bounds(pixels)
    content = pixels[y0:y0 + ch, x0:x0 + cw]
    cols = cw // 8
    rows = ch // 8
    print(f"Content: {cw}x{ch} at ({x0},{y0}) = {cols}x{rows} tiles")

    if args.list_colors:
        flat = content.reshape(-1, 3)
        counts = Counter(map(tuple, flat))
        total = sum(counts.values())
        rev_map = {v: k for k, v in ZX_COLOR_MAP.items()}
        print(f"\nColors in image:")
        for color, count in counts.most_common():
            pct = count / total * 100
            name = rev_map.get(color, f"{color[0]},{color[1]},{color[2]}")
            print(f"  {pct:5.1f}%  {count:6d}px  {name}")
        return

    rev_map = {v: k for k, v in ZX_COLOR_MAP.items()}

    if args.colors:
        # Single palette mode (legacy)
        palette_rgb = [parse_color(c) for c in args.colors]
        print("Using single custom palette:")
        for i, c in enumerate(palette_rgb):
            rgb5 = rgb8_to_rgb5(*c)
            name = rev_map.get(c, "custom")
            print(f"  [{i}] {name:14s} ({c[0]:3d},{c[1]:3d},{c[2]:3d}) -> GBC ({rgb5[0]:2d},{rgb5[1]:2d},{rgb5[2]:2d})")
        tile_data, tilemap = extract_tiles_color(content, palette_rgb)
        palmap = [[0] * (cw // 8) for _ in range(ch // 8)]
        palettes_rgb = [palette_rgb]
    else:
        # Multi-palette mode: auto-build up to 8 palettes
        palettes_rgb = build_multi_palettes(content)
        print(f"Built {len(palettes_rgb)} palettes:")
        for pi, pal in enumerate(palettes_rgb):
            colors_str = ", ".join(rev_map.get(c, f"{c[0]},{c[1]},{c[2]}") for c in pal)
            print(f"  Pal {pi}: [{colors_str}]")
        tile_data, tilemap, palmap = extract_tiles_multipal(content, palettes_rgb)

    print(f"Unique tiles: {len(tile_data)}")

    name = input_path.rsplit('/', 1)[-1].rsplit('.', 1)[0]
    project = build_project(tile_data, tilemap, palmap, palettes_rgb, name)

    with open(output_path, 'w') as f:
        json.dump(project, f, separators=(',', ':'), cls=NumpyEncoder)

    print(f"Output: {output_path}")
    print(f"  Tileset: {len(project['tileset'])} tiles")
    print(f"  Level: {project['levels'][0]['width']}x{project['levels'][0]['height']}")


if __name__ == '__main__':
    main()
