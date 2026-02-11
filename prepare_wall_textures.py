"""
Generate VB-optimized wall texture tiles for the raycaster.

3 algorithmic patterns (brick, stone, tech) + 2 image-based (door, switch).
5 wall types, 8 columns x 8 rows x 2 lighting = 640 wall tiles + 70 transition.

Switch "on" state tiles stored separately for runtime VRAM swap.

Output: wall_textures.c and wall_textures.h
"""

import os
import random
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images")
PREVIEW_DIR = os.path.join(SCRIPT_DIR, "wall_texture_previews")

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
    "wall_door",      # Door (from door_texture.png)
    "wall_switch",    # Switch off state (from gaint-switch1-doom-palette.png)
]

VB_PAL = {0: (0, 0, 0), 1: (80, 0, 0), 2: (170, 0, 0), 3: (255, 0, 0)}


def luminance(r, g, b):
    return int(0.299 * r + 0.587 * g + 0.114 * b)


# ---------------------------------------------------------------------------
# Pattern generators -- each returns a 64x64 canvas of VB indices (1-3)
# ---------------------------------------------------------------------------

def gen_brick():
    """Brick wall: horizontal mortar every 8 rows, staggered vertical joints."""
    W, H = 64, 64
    c = [[2] * W for _ in range(H)]

    for y in range(H):
        for x in range(W):
            row_in_brick = y % 16
            is_h_mortar = row_in_brick == 0 or row_in_brick == 8

            stagger = 0 if (y // 16) % 2 == 0 else 16
            is_v_mortar = ((x + stagger) % 32) == 0

            if is_h_mortar or is_v_mortar:
                c[y][x] = 1
            else:
                if row_in_brick == 1 or row_in_brick == 9:
                    c[y][x] = 3
                elif row_in_brick == 7 or row_in_brick == 15:
                    c[y][x] = 1
                else:
                    col_in_brick = (x + stagger) % 32
                    if col_in_brick == 1:
                        c[y][x] = 3
                    elif col_in_brick == 31:
                        c[y][x] = 1
                    else:
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
    random.seed(42)

    blocks = [
        (0,  0, 20, 12), (20, 0, 44, 10), (44, 0, 64, 14),
        (0, 12, 18, 28), (18, 10, 46, 26), (46, 14, 64, 30),
        (0, 28, 24, 44), (24, 26, 48, 42), (48, 30, 64, 46),
        (0, 44, 22, 58), (22, 42, 50, 56), (50, 46, 64, 60),
        (0, 58, 28, 64), (28, 56, 52, 64), (52, 60, 64, 64),
    ]

    for bx1, by1, bx2, by2 in blocks:
        fill = random.choice([2, 2, 2, 3])
        for y in range(by1, min(by2, H)):
            for x in range(bx1, min(bx2, W)):
                if y == by1 or y == by2 - 1 or x == bx1 or x == bx2 - 1:
                    c[y][x] = 1
                elif y == by1 + 1 and x > bx1 and x < bx2 - 1:
                    c[y][x] = 3
                elif x == bx1 + 1 and y > by1 and y < by2 - 1:
                    c[y][x] = 3
                elif y == by2 - 2 and x > bx1 and x < bx2 - 1:
                    c[y][x] = 1
                elif x == bx2 - 2 and y > by1 and y < by2 - 1:
                    c[y][x] = 1
                else:
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
            px = x % 16
            py = y % 16

            if px == 0 or py == 0:
                c[y][x] = 1
            elif px == 1 or py == 1:
                c[y][x] = 3
            elif px == 15 or py == 15:
                c[y][x] = 1
            elif px == 14 or py == 14:
                c[y][x] = 2
            else:
                panel_id = (x // 16) + (y // 16) * 4
                if panel_id % 3 == 0:
                    if py >= 6 and py <= 9:
                        c[y][x] = 3 if (px + py) % 2 == 0 else 1
                    else:
                        c[y][x] = 2
                elif panel_id % 3 == 1:
                    cx, cy = 8, 8
                    dist = abs(px - cx) + abs(py - cy)
                    if dist <= 2:
                        c[y][x] = 3
                    elif dist <= 4:
                        c[y][x] = 1
                    else:
                        c[y][x] = 2
                else:
                    if px == 8 or py == 8:
                        c[y][x] = 1
                    else:
                        c[y][x] = 2
    return c


def gen_from_image(image_path):
    """Load PNG, scale to 64x64, convert to VB palette (1-3 for walls)."""
    img = Image.open(image_path).convert('RGBA')
    resized = img.resize((64, 64), Image.LANCZOS)
    canvas = [[2] * 64 for _ in range(64)]
    pix = resized.load()
    for y in range(64):
        for x in range(64):
            r, g, b, a = pix[x, y]
            if a < 128:
                canvas[y][x] = 1  # transparent -> dark for walls
            else:
                gray = luminance(r, g, b)
                if gray < 85:
                    canvas[y][x] = 1
                elif gray < 170:
                    canvas[y][x] = 2
                else:
                    canvas[y][x] = 3
    return canvas


def gen_door():
    """Door texture from door_texture.png."""
    path = os.path.join(SCRIPT_DIR, "door_texture.png")
    return gen_from_image(path)


def gen_switch_off():
    """Switch off state from gaint-switch1-doom-palette.png."""
    path = os.path.join(SCRIPT_DIR, "gaint-switch1-doom-palette.png")
    return gen_from_image(path)


def gen_switch_on():
    """Switch on state from gaint-switch2-doom-palette.png."""
    path = os.path.join(SCRIPT_DIR, "gaint-switch2-doom-palette.png")
    return gen_from_image(path)


# ---------------------------------------------------------------------------
# Tile operations
# ---------------------------------------------------------------------------

def slice_canvas_to_tiles(canvas):
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
    return [[max(1, v - 1) for v in row] for row in tile]


def tile_to_vb_2bpp(tile):
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
    os.makedirs(PREVIEW_DIR, exist_ok=True)
    gap = 4
    n = len(canvases)
    w = (64 * scale + gap) * n + gap
    h = (64 * scale + gap) * 2 + gap
    img = Image.new('RGB', (w, h), (20, 20, 20))

    for i, (canvas, name) in enumerate(zip(canvases, names)):
        ox = gap + i * (64 * scale + gap)
        for y in range(64):
            for x in range(64):
                color = VB_PAL[canvas[y][x]]
                for sy in range(scale):
                    for sx in range(scale):
                        img.putpixel((ox + x*scale+sx, gap + y*scale+sy), color)
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
# Switch ON tiles (separate ROM array for runtime swap)
# ---------------------------------------------------------------------------

def generate_switch_on_tiles():
    """Generate tile data for switch 'on' state, stored separately."""
    canvas = gen_switch_on()
    tiles = slice_canvas_to_tiles(canvas)
    all_u32 = []
    # Lit tiles
    for r in range(ROWS_PER_TEX):
        for c in range(COLUMNS_PER_TEX):
            all_u32.extend(bytes_to_u32(tile_to_vb_2bpp(tiles[r][c])))
    # Dark tiles
    for r in range(ROWS_PER_TEX):
        for c in range(COLUMNS_PER_TEX):
            all_u32.extend(bytes_to_u32(tile_to_vb_2bpp(darken_tile(tiles[r][c]))))
    return all_u32


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    generators = [gen_brick, gen_stone, gen_tech, gen_door, gen_switch_off]
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

    # Switch ON tiles (separate)
    switch_on_u32 = generate_switch_on_tiles()
    print(f"Generated switch ON tiles: {len(switch_on_u32)} u32 words")

    combined = all_tiles + trans
    total_words = len(combined) * 4

    # --- Write C ---
    c_path = os.path.join(OUTPUT_DIR, "wall_textures.c")
    with open(c_path, 'w') as f:
        f.write("/*\n")
        f.write(" * Wall texture tiles for VB Doom raycaster.\n")
        f.write(" * Auto-generated by prepare_wall_textures.py\n")
        f.write(f" * {len(all_tiles)} wall tiles + {len(trans)} transition = {len(combined)} total\n")
        f.write(" * Patterns: brick, stone, tech, door, switch\n")
        f.write(" */\n\n")
        f.write('#include "wall_textures.h"\n\n')
        f.write(f"const unsigned int wallTextureTiles[{total_words}]"
                f" __attribute__((aligned(4))) =\n{{\n")
        for i, words in enumerate(combined):
            s = ", ".join(f"0x{w:08X}" for w in words)
            comma = "," if i < len(combined) - 1 else ""
            f.write(f"\t{s}{comma}\n")
        f.write("};\n\n")

        # Switch ON data (ROM, for runtime swap into VRAM)
        f.write("/* Switch ON state tiles -- copymem over switch VRAM chars on activation */\n")
        f.write(f"const unsigned int switchOnTiles[{len(switch_on_u32)}]"
                f" __attribute__((aligned(4))) =\n{{\n")
        for i in range(0, len(switch_on_u32), 8):
            chunk = switch_on_u32[i:i+8]
            s = ", ".join(f"0x{w:08X}" for w in chunk)
            comma = "," if i + 8 < len(switch_on_u32) else ""
            f.write(f"\t{s}{comma}\n")
        f.write("};\n")
    print(f"\nWrote {c_path}")

    # --- Write H ---
    trans_start = WALL_TEX_CHAR_START + len(all_tiles)
    tc_shift = 5
    # Switch type is the last one (index 4, wall type 5)
    switch_type_idx = len(TEXTURES) - 1
    switch_vram_offset = switch_type_idx * TILES_PER_TYPE

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

        # Switch ON data extern
        f.write(f"/* Switch ON tiles: {TILES_PER_TYPE} tiles in ROM for runtime swap */\n")
        f.write(f"extern const unsigned int switchOnTiles[{len(switch_on_u32)}];\n")
        f.write(f"#define SWITCH_ON_TILES_BYTES  {len(switch_on_u32) * 4}\n")
        f.write(f"#define SWITCH_VRAM_OFFSET     {switch_vram_offset}\n\n")

        for i, name in enumerate(TEXTURES):
            short = name.upper().replace('WALL_', '')
            f.write(f"#define WALL_TYPE_{short}  {i + 1}\n")
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
