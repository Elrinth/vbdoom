"""
Generate VB-optimized wall texture tiles for the raycaster.

Instead of converting photo-realistic Doom textures (which look noisy at 3 colours),
this generates clean high-contrast geometric patterns designed specifically for
the VB's 3-colour red palette.

4 wall types, 8 columns x 8 rows x 2 lighting = 512 wall tiles + 56 transition.

Output: wall_textures.c and wall_textures.h
"""

import os
import random
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images")
PREVIEW_DIR = os.path.join(SCRIPT_DIR, "wall_texture_previews")

# Will be set to final value during VRAM layout step
WALL_TEX_CHAR_START = 1211  # after pickups: 1175 + 36 = 1211

TILE_SIZE = 8
COLUMNS_PER_TEX = 8
ROWS_PER_TEX = 8
TILES_PER_LIGHTING = COLUMNS_PER_TEX * ROWS_PER_TEX     # 64
TILES_PER_TYPE = TILES_PER_LIGHTING * 2                   # 128

TEXTURES = [
    "wall_startan",   # Brick
    "wall_stone",     # Stone blocks
    "wall_tech",      # Tech panels
    "wall_rough",     # Rough organic
]

VB_PAL = {0: (0, 0, 0), 1: (80, 0, 0), 2: (170, 0, 0), 3: (255, 0, 0)}


# ---------------------------------------------------------------------------
# Pattern generators -- each returns a 64x64 canvas of VB indices (1-3)
# ---------------------------------------------------------------------------

def gen_brick():
    """Brick wall: horizontal mortar every 8 rows, staggered vertical joints."""
    W, H = 64, 64
    c = [[2] * W for _ in range(H)]

    for y in range(H):
        for x in range(W):
            # Horizontal mortar lines at rows 0 and 4 of each tile group
            row_in_brick = y % 16
            is_h_mortar = row_in_brick == 0 or row_in_brick == 8

            # Vertical mortar: stagger by 16px every other row-group
            stagger = 0 if (y // 16) % 2 == 0 else 16
            is_v_mortar = ((x + stagger) % 32) == 0

            if is_h_mortar or is_v_mortar:
                c[y][x] = 1   # dark mortar
            else:
                # Brick body: slight variation based on position
                if row_in_brick == 1 or row_in_brick == 9:
                    # Row just below mortar: bright edge (top highlight)
                    c[y][x] = 3
                elif row_in_brick == 7 or row_in_brick == 15:
                    # Row just above mortar: slightly dark (shadow)
                    c[y][x] = 1
                else:
                    # Vertical edge highlights
                    col_in_brick = (x + stagger) % 32
                    if col_in_brick == 1:
                        c[y][x] = 3   # left edge highlight
                    elif col_in_brick == 31:
                        c[y][x] = 1   # right edge shadow
                    else:
                        # Subtle pattern within brick
                        if (x + y * 3) % 7 == 0:
                            c[y][x] = 3
                        elif (x * 5 + y * 2) % 11 == 0:
                            c[y][x] = 1
                        else:
                            c[y][x] = 2
    return c


def gen_stone():
    """Stone blocks: irregular block outlines with varied fill."""
    W, H = 64, 64
    c = [[2] * W for _ in range(H)]
    random.seed(42)  # deterministic

    # Define stone block layout (irregular rectangles)
    blocks = [
        (0,  0, 20, 12), (20, 0, 44, 10), (44, 0, 64, 14),
        (0, 12, 18, 28), (18, 10, 46, 26), (46, 14, 64, 30),
        (0, 28, 24, 44), (24, 26, 48, 42), (48, 30, 64, 46),
        (0, 44, 22, 58), (22, 42, 50, 56), (50, 46, 64, 60),
        (0, 58, 28, 64), (28, 56, 52, 64), (52, 60, 64, 64),
    ]

    for bx1, by1, bx2, by2 in blocks:
        fill = random.choice([2, 2, 2, 3])  # mostly medium
        for y in range(by1, min(by2, H)):
            for x in range(bx1, min(bx2, W)):
                # Outline = dark
                if y == by1 or y == by2 - 1 or x == bx1 or x == bx2 - 1:
                    c[y][x] = 1
                # Inner highlight near top-left
                elif y == by1 + 1 and x > bx1 and x < bx2 - 1:
                    c[y][x] = 3
                elif x == bx1 + 1 and y > by1 and y < by2 - 1:
                    c[y][x] = 3
                # Inner shadow near bottom-right
                elif y == by2 - 2 and x > bx1 and x < bx2 - 1:
                    c[y][x] = 1
                elif x == bx2 - 2 and y > by1 and y < by2 - 1:
                    c[y][x] = 1
                else:
                    # Stone fill with minor grain
                    if (x * 3 + y * 7) % 13 == 0:
                        c[y][x] = 1 if fill == 2 else 2
                    else:
                        c[y][x] = fill
    return c


def gen_tech():
    """Tech panels: clean grid with bright edges and dark recesses."""
    W, H = 64, 64
    c = [[2] * W for _ in range(H)]

    for y in range(H):
        for x in range(W):
            # Main panel grid: 16x16 panels
            px = x % 16
            py = y % 16

            if px == 0 or py == 0:
                c[y][x] = 1   # dark grid lines
            elif px == 1 or py == 1:
                c[y][x] = 3   # bright inner edge
            elif px == 15 or py == 15:
                c[y][x] = 1   # dark shadow edge
            elif px == 14 or py == 14:
                c[y][x] = 2   # medium near shadow
            else:
                # Panel interior: add small tech detail
                panel_id = (x // 16) + (y // 16) * 4
                if panel_id % 3 == 0:
                    # Horizontal stripe pattern
                    if py >= 6 and py <= 9:
                        c[y][x] = 3 if (px + py) % 2 == 0 else 1
                    else:
                        c[y][x] = 2
                elif panel_id % 3 == 1:
                    # Center dot/light
                    cx, cy = 8, 8
                    dist = abs(px - cx) + abs(py - cy)
                    if dist <= 2:
                        c[y][x] = 3
                    elif dist <= 4:
                        c[y][x] = 1
                    else:
                        c[y][x] = 2
                else:
                    # Cross pattern
                    if px == 8 or py == 8:
                        c[y][x] = 1
                    else:
                        c[y][x] = 2
    return c


def gen_rough():
    """Rough organic texture: cave-like with dark pitting."""
    W, H = 64, 64
    c = [[2] * W for _ in range(H)]
    random.seed(137)  # deterministic

    # Generate noise field
    noise = [[random.random() for _ in range(W)] for _ in range(H)]

    # Simple box-blur the noise for smoother patterns
    blurred = [[0.0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            total = 0.0
            count = 0
            for dy in range(-2, 3):
                for dx in range(-2, 3):
                    ny = (y + dy) % H
                    nx = (x + dx) % W
                    total += noise[ny][nx]
                    count += 1
            blurred[y][x] = total / count

    # Add some structured crack lines
    for y in range(H):
        for x in range(W):
            # Diagonal cracks
            crack1 = abs((x * 3 + y * 2) % 24 - 12)
            crack2 = abs((x * 2 - y * 3 + 48) % 20 - 10)
            is_crack = crack1 < 1 or crack2 < 1

            b = blurred[y][x]
            if is_crack:
                c[y][x] = 1   # dark crack
            elif b < 0.30:
                c[y][x] = 1   # dark pits
            elif b > 0.68:
                c[y][x] = 3   # bright spots
            else:
                c[y][x] = 2   # medium base
    return c


# ---------------------------------------------------------------------------
# Tile operations
# ---------------------------------------------------------------------------

def slice_canvas_to_tiles(canvas):
    """Slice 64x64 canvas into 8x8 grid of 8x8 tiles. Returns [row][col] = tile."""
    tiles = [[None] * COLUMNS_PER_TEX for _ in range(ROWS_PER_TEX)]
    for tr in range(ROWS_PER_TEX):
        for tc in range(COLUMNS_PER_TEX):
            tile = []
            for y in range(TILE_SIZE):
                row = []
                for x in range(TILE_SIZE):
                    row.append(canvas[tr * TILE_SIZE + y][tc * TILE_SIZE + x])
                tile.append(row)
            tiles[tr][tc] = tile
    return tiles


def darken_tile(tile):
    """Create dark variant: shift each index down by 1 (min 1)."""
    return [[max(1, v - 1) for v in row] for row in tile]


def tile_to_vb_2bpp(tile):
    """Convert 8x8 tile to VB 2bpp sequential format (16 bytes).

    VB stores each row as a u16 (LE) with pixel 0 at bits[1:0] (LSB).
    """
    data = []
    for y in range(TILE_SIZE):
        u16_val = 0
        for x in range(TILE_SIZE):
            u16_val |= (tile[y][x] << (x * 2))
        data.append(u16_val & 0xFF)
        data.append((u16_val >> 8) & 0xFF)
    return data


def bytes_to_u32(data):
    words = []
    for i in range(0, len(data), 4):
        w = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24)
        words.append(w)
    return words


# ---------------------------------------------------------------------------
# Transition tiles
# ---------------------------------------------------------------------------

def generate_transitions(rep_tiles):
    """Generate partially-masked transition tiles.
    rep_tiles: dict of (type_idx, lighting) -> 8x8 tile
    """
    tiles = []
    for ti in range(len(TEXTURES)):
        for tv in range(2):
            src = rep_tiles.get((ti, tv), [[1]*8]*8)
            for fill in range(1, 8):
                masked = []
                blank_rows = 8 - fill
                for y in range(8):
                    if y < blank_rows:
                        masked.append([0] * 8)
                    else:
                        masked.append(list(src[y]))
                tiles.append(bytes_to_u32(tile_to_vb_2bpp(masked)))
    return tiles


# ---------------------------------------------------------------------------
# Preview
# ---------------------------------------------------------------------------

def save_preview(canvas, name, scale=4):
    """Save a 64x64 canvas as a scaled preview PNG."""
    os.makedirs(PREVIEW_DIR, exist_ok=True)
    pw, ph = 64 * scale, 64 * scale
    img = Image.new('RGB', (pw, ph))
    for y in range(64):
        for x in range(64):
            color = VB_PAL[canvas[y][x]]
            for sy in range(scale):
                for sx in range(scale):
                    img.putpixel((x * scale + sx, y * scale + sy), color)
    img.save(os.path.join(PREVIEW_DIR, f"{name}.png"))


def save_combined_preview(canvases, names, scale=3):
    """Save all textures side-by-side: lit on top, dark on bottom."""
    os.makedirs(PREVIEW_DIR, exist_ok=True)
    gap = 4
    n = len(canvases)
    w = (64 * scale + gap) * n + gap
    h = (64 * scale + gap) * 2 + gap
    img = Image.new('RGB', (w, h), (20, 20, 20))

    for i, (canvas, name) in enumerate(zip(canvases, names)):
        ox = gap + i * (64 * scale + gap)
        # Lit
        for y in range(64):
            for x in range(64):
                color = VB_PAL[canvas[y][x]]
                for sy in range(scale):
                    for sx in range(scale):
                        img.putpixel((ox + x*scale+sx, gap + y*scale+sy), color)
        # Dark variant
        oy = gap + 64 * scale + gap
        for y in range(64):
            for x in range(64):
                v = max(1, canvas[y][x] - 1)
                color = VB_PAL[v]
                for sy in range(scale):
                    for sx in range(scale):
                        img.putpixel((ox + x*scale+sx, oy + y*scale+sy), color)

    img.save(os.path.join(PREVIEW_DIR, "all_wall_textures.png"))


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    generators = [gen_brick, gen_stone, gen_tech, gen_rough]
    all_canvases = []
    all_tiles = []
    rep_tiles = {}

    for ti, (gen, name) in enumerate(zip(generators, TEXTURES)):
        canvas = gen()
        all_canvases.append(canvas)
        save_preview(canvas, name)
        print(f"Generated {name}: 64x64 canvas")

        tiles = slice_canvas_to_tiles(canvas)

        # Lit tiles
        for r in range(ROWS_PER_TEX):
            for c in range(COLUMNS_PER_TEX):
                t = tiles[r][c]
                if r == 0 and c == 0:
                    rep_tiles[(ti, 0)] = t
                all_tiles.append(bytes_to_u32(tile_to_vb_2bpp(t)))

        # Dark tiles
        for r in range(ROWS_PER_TEX):
            for c in range(COLUMNS_PER_TEX):
                t = darken_tile(tiles[r][c])
                if r == 0 and c == 0:
                    rep_tiles[(ti, 1)] = t
                all_tiles.append(bytes_to_u32(tile_to_vb_2bpp(t)))

    save_combined_preview(all_canvases, TEXTURES)
    print(f"\nSaved previews to {PREVIEW_DIR}/")

    # Transitions
    trans = generate_transitions(rep_tiles)
    print(f"Generated {len(trans)} transition tiles")

    combined = all_tiles + trans
    total_words = len(combined) * 4

    # --- Write C ---
    c_path = os.path.join(OUTPUT_DIR, "wall_textures.c")
    with open(c_path, 'w') as f:
        f.write("/*\n")
        f.write(" * Wall texture tiles for VB Doom raycaster.\n")
        f.write(" * Auto-generated by prepare_wall_textures.py\n")
        f.write(f" * {len(all_tiles)} wall tiles + {len(trans)} transition = {len(combined)} total\n")
        f.write(" * VB-optimized geometric patterns (brick, stone, tech, rough)\n")
        f.write(" */\n\n")
        f.write(f"const unsigned int wallTextureTiles[{total_words}]"
                f" __attribute__((aligned(4))) =\n{{\n")
        for i, words in enumerate(combined):
            s = ", ".join(f"0x{w:08X}" for w in words)
            comma = "," if i < len(combined) - 1 else ""
            f.write(f"\t{s}{comma}\n")
        f.write("};\n")
    print(f"\nWrote {c_path}")

    # --- Write H ---
    trans_start = WALL_TEX_CHAR_START + len(all_tiles)
    tc_shift = 5   # tc >> 5 for 8 columns
    h_path = os.path.join(OUTPUT_DIR, "wall_textures.h")
    with open(h_path, 'w') as f:
        f.write("#ifndef __WALL_TEXTURES_H__\n#define __WALL_TEXTURES_H__\n\n")
        f.write("/*\n * Wall texture tiles for VB Doom raycaster.\n")
        f.write(" * Auto-generated by prepare_wall_textures.py\n")
        f.write(f" * {len(all_tiles)} wall + {len(trans)} transition = {len(combined)} total\n *\n")
        f.write(f" * Layout per texture ({TILES_PER_TYPE} tiles):\n")
        f.write(f" *   0-{TILES_PER_LIGHTING-1}: lit  ({ROWS_PER_TEX} rows x {COLUMNS_PER_TEX} cols)\n")
        f.write(f" *   {TILES_PER_LIGHTING}-{TILES_PER_TYPE-1}: dark ({ROWS_PER_TEX} rows x {COLUMNS_PER_TEX} cols)\n *\n")
        f.write(f" * Tile index formula:\n")
        f.write(f" *   WALL_TEX_CHAR_START + (wallType-1)*{TILES_PER_TYPE}"
                f" + tv*{TILES_PER_LIGHTING} + row*{COLUMNS_PER_TEX} + (tc >> {tc_shift})\n")
        f.write(f" *   tv=0 lit, tv=1 dark; row = (texY >> 13) & 7\n */\n\n")
        f.write(f"#define WALL_TEX_COUNT     {len(TEXTURES)}\n")
        f.write(f"#define WALL_TEX_COLS      {COLUMNS_PER_TEX}\n")
        f.write(f"#define WALL_TEX_ROWS      {ROWS_PER_TEX}\n")
        f.write(f"#define WALL_TEX_PER_TYPE  {TILES_PER_TYPE}\n")
        f.write(f"#define WALL_TEX_LIT_BLOCK {TILES_PER_LIGHTING}\n")
        f.write(f"#define WALL_TEX_TOTAL     {len(all_tiles)}\n")
        f.write(f"#define WALL_TEX_BYTES     {len(combined) * 16}\n\n")
        f.write(f"#define WALL_TEX_CHAR_START  {WALL_TEX_CHAR_START}\n\n")
        f.write(f"#define TRANS_TEX_CHAR_START  {trans_start}\n")
        f.write(f"#define TRANS_TEX_COUNT       {len(trans)}\n\n")
        f.write(f"extern const unsigned int wallTextureTiles[{total_words}];\n\n")
        for i, name in enumerate(TEXTURES):
            f.write(f"#define WALL_TYPE_{name.upper().replace('WALL_', '')}  {i + 1}\n")
        f.write("\n#endif\n")
    print(f"Wrote {h_path}")

    print(f"\nTotal: {len(combined)} tiles ({len(all_tiles)} wall + {len(trans)} transition)")
    print(f"Total bytes: {len(combined) * 16}")
    print(f"WALL_TEX_CHAR_START: {WALL_TEX_CHAR_START}")
    print(f"TRANS_TEX_CHAR_START: {trans_start}")
    after = trans_start + len(trans)
    print(f"After all: char {after} (free: {2048 - after})")


if __name__ == '__main__':
    main()
