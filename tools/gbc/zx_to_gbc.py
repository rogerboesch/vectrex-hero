#!/usr/bin/env python3
"""
zx_to_gbc.py — Extract 8x8 tiles from a ZX Spectrum screenshot
and output a GBC Workbench project.json.

Usage: python3 zx_to_gbc.py <screenshot.png> [output.json]

The Spectrum screen is 256x192 pixels (32x24 cells of 8x8).
Each cell has exactly 2 colors (ink/paper). These are mapped to
GBC 2bpp tiles (4 colors per palette, we use indices 0 and 3
for paper and ink).
"""

import sys
import json
from collections import Counter
from PIL import Image
import numpy as np

# ZX Spectrum palette: normal (brightness=0) and bright (brightness=1)
ZX_PALETTE = {
    (0, 0, 0):       (0, "black"),
    (0, 0, 205):     (1, "blue"),
    (205, 0, 0):     (2, "red"),
    (205, 0, 205):   (3, "magenta"),
    (0, 205, 0):     (4, "green"),
    (0, 205, 205):   (5, "cyan"),
    (205, 205, 0):   (6, "yellow"),
    (205, 205, 205): (7, "white"),
    (0, 0, 255):     (9, "bright blue"),
    (255, 0, 0):     (10, "bright red"),
    (255, 0, 255):   (11, "bright magenta"),
    (0, 255, 0):     (12, "bright green"),
    (0, 255, 255):   (13, "bright cyan"),
    (255, 255, 0):   (14, "bright yellow"),
    (255, 255, 255): (15, "bright white"),
}


def rgb8_to_rgb5(r, g, b):
    """Convert 8-bit RGB to GBC 5-bit RGB."""
    return [r >> 3, g >> 3, b >> 3]


def find_content_bounds(pixels):
    """Find the 256x192 Spectrum display area within the image."""
    h, w = pixels.shape[:2]
    border = tuple(pixels[0, 0])

    # Scan for content
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

    # Snap to 256x192 if close
    if abs(cw - 256) <= 2:
        left = left + (cw - 256) // 2
        cw = 256
    if abs(ch - 192) <= 2:
        top = top + (ch - 192) // 2
        ch = 192

    if cw < 256 or ch < 192:
        print(f"Warning: content area {cw}x{ch} is smaller than 256x192", file=sys.stderr)

    return left, top, min(cw, 256), min(ch, 192)


def extract_tiles(content):
    """Extract unique tiles and build tilemap from 256x192 content."""
    rows = content.shape[0] // 8
    cols = content.shape[1] // 8

    tile_data = []       # list of (bytes_16, paper_rgb, ink_rgb)
    tile_index = {}      # (bytes_16) -> index
    tilemap = []         # [row][col] -> tile index
    cell_attrs = []      # [row][col] -> (paper_rgb, ink_rgb)

    for row in range(rows):
        tile_row = []
        attr_row = []
        for col in range(cols):
            cell = content[row * 8:(row + 1) * 8, col * 8:(col + 1) * 8]
            flat = cell.reshape(-1, 3)

            # Find paper (most common) and ink
            color_counts = Counter(map(tuple, flat))
            colors = color_counts.most_common()
            paper = colors[0][0]
            ink = colors[1][0] if len(colors) > 1 else paper

            # Build GBC 2bpp tile: paper=0, ink=3
            # GBC tiles are 16 bytes: 2 bytes per row (lo, hi)
            tile_bytes = []
            for py in range(8):
                lo = 0
                hi = 0
                for px in range(8):
                    p = tuple(cell[py, px])
                    if p != paper:
                        # Color index 3 = both bits set
                        lo |= (1 << (7 - px))
                        hi |= (1 << (7 - px))
                tile_bytes.append(lo)
                tile_bytes.append(hi)

            key = tuple(tile_bytes)
            if key not in tile_index:
                tile_index[key] = len(tile_data)
                tile_data.append((list(tile_bytes), paper, ink))

            tile_row.append(tile_index[key])
            attr_row.append((paper, ink))

        tilemap.append(tile_row)
        cell_attrs.append(attr_row)

    return tile_data, tilemap, cell_attrs


def color_distance(c1, c2):
    """Simple Euclidean distance between two RGB tuples."""
    return sum((int(a) - int(b)) ** 2 for a, b in zip(c1, c2))


def merge_palettes_to_limit(pair_set, max_pals=8):
    """Merge the least-used color pairs into the closest available palette.

    Returns a mapping from original pair -> merged palette index,
    and the final list of (paper, ink) pairs for each palette slot.
    """
    # Sort by usage (most used first to keep those as-is)
    pairs_by_usage = sorted(pair_set.items(), key=lambda x: -x[1])

    # Keep top max_pals as-is
    kept = [p for p, _ in pairs_by_usage[:max_pals]]
    merged_map = {p: i for i, p in enumerate(kept)}

    # Map remaining to nearest kept palette
    for pair, _ in pairs_by_usage[max_pals:]:
        paper, ink = pair
        best_idx = 0
        best_dist = float('inf')
        for i, (kp, ki) in enumerate(kept):
            d = color_distance(paper, kp) + color_distance(ink, ki)
            if d < best_dist:
                best_dist = d
                best_idx = i
        merged_map[pair] = best_idx

    return merged_map, kept


def build_palettes(tile_data, tilemap, cell_attrs):
    """Build GBC palettes from the color pairs used.

    Returns palette list and per-tile palette assignment.
    Each GBC BG palette has 4 colors. We use:
      [0] = paper, [3] = ink, [1] and [2] as interpolated midtones.
    """
    # Collect unique color pairs with usage counts
    pair_counts = {}
    for td in tile_data:
        pair = (td[1], td[2])
        pair_counts[pair] = pair_counts.get(pair, 0) + 1

    # Merge if over 8
    if len(pair_counts) > 8:
        pair_to_pal, kept_pairs = merge_palettes_to_limit(pair_counts, 8)
    else:
        kept_pairs = list(pair_counts.keys())
        pair_to_pal = {p: i for i, p in enumerate(kept_pairs)}

    # Build palette colors
    palettes = []
    for paper, ink in kept_pairs:
        p = rgb8_to_rgb5(*paper)
        i = rgb8_to_rgb5(*ink)
        m1 = [(p[c] * 2 + i[c]) // 3 for c in range(3)]
        m2 = [(p[c] + i[c] * 2) // 3 for c in range(3)]
        palettes.append([p, m1, m2, i])

    # Assign palette to each tile
    tile_palettes = []
    for td in tile_data:
        pair = (td[1], td[2])
        tile_palettes.append(pair_to_pal[pair])

    return palettes, tile_palettes, pair_to_pal


def build_project(tile_data, tilemap, palettes, tile_palettes, name="ZX Import"):
    """Build a GBC Workbench project.json dict."""
    # Tileset: just the raw 16-byte tile data
    tileset = [td[0] for td in tile_data]

    # Limit to 8 BG palettes (GBC limit)
    bg_palettes = palettes[:8]
    if len(palettes) > 8:
        print(f"Warning: {len(palettes)} color pairs, truncating to 8 BG palettes",
              file=sys.stderr)

    # Pad to 6 palettes minimum (workbench expects it)
    while len(bg_palettes) < 6:
        bg_palettes.append([[0, 0, 0], [8, 8, 8], [16, 16, 16], [31, 31, 31]])

    # Default sprite palettes
    spr_palettes = [
        [[0, 0, 0], [0, 12, 0], [0, 24, 4], [8, 31, 8]],
        [[0, 0, 0], [20, 0, 0], [31, 8, 4], [31, 20, 16]],
        [[0, 0, 0], [16, 0, 16], [26, 6, 26], [31, 18, 31]],
        [[0, 0, 0], [0, 10, 16], [0, 22, 28], [12, 31, 31]],
        [[0, 0, 0], [16, 16, 0], [28, 28, 4], [31, 31, 20]],
    ]

    rows = len(tilemap)
    cols = len(tilemap[0]) if rows > 0 else 0

    level = {
        "name": name,
        "width": cols,
        "height": rows,
        "tiles": tilemap,
        "entities": [],
    }

    project = {
        "tileset": tileset,
        "bg_palettes": bg_palettes,
        "spr_palettes": spr_palettes,
        "levels": [level],
    }

    return project


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <screenshot.png> [output.json]", file=sys.stderr)
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else input_path.rsplit('.', 1)[0] + '_gbc.json'

    img = Image.open(input_path).convert('RGB')
    pixels = np.array(img)

    print(f"Input: {input_path} ({img.size[0]}x{img.size[1]})")

    # Find display area
    x0, y0, cw, ch = find_content_bounds(pixels)
    content = pixels[y0:y0 + ch, x0:x0 + cw]
    cols = cw // 8
    rows = ch // 8
    print(f"Content: {cw}x{ch} at ({x0},{y0}) = {cols}x{rows} tiles")

    # Extract tiles
    tile_data, tilemap, cell_attrs = extract_tiles(content)
    print(f"Unique tiles: {len(tile_data)}")

    # Build palettes
    palettes, tile_palettes, pair_to_pal = build_palettes(tile_data, tilemap, cell_attrs)
    print(f"Color pairs (palettes): {len(palettes)}")

    if len(palettes) > 8:
        print(f"WARNING: GBC supports max 8 BG palettes, found {len(palettes)}")
        print("         Some color pairs will share palettes (lossy)")

    # Build project
    name = input_path.rsplit('/', 1)[-1].rsplit('.', 1)[0]
    project = build_project(tile_data, tilemap, palettes, tile_palettes, name)

    # Write output (convert numpy types to native Python)
    def to_native(obj):
        if isinstance(obj, np.integer):
            return int(obj)
        if isinstance(obj, np.floating):
            return float(obj)
        if isinstance(obj, np.ndarray):
            return obj.tolist()
        return obj

    class NumpyEncoder(json.JSONEncoder):
        def default(self, obj):
            v = to_native(obj)
            if v is not obj:
                return v
            return super().default(obj)

    with open(output_path, 'w') as f:
        json.dump(project, f, separators=(',', ':'), cls=NumpyEncoder)

    print(f"Output: {output_path}")
    print(f"  Tileset: {len(project['tileset'])} tiles")
    print(f"  Palettes: {len(project['bg_palettes'])} BG")
    print(f"  Level: {project['levels'][0]['width']}x{project['levels'][0]['height']}")


if __name__ == '__main__':
    main()
