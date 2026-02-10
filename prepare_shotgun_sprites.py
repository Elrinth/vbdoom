"""
prepare_shotgun_sprites.py

Convert shotgun first-person sprite sheet to VB tile format using grit.exe.
Source: shotgun_6_sprites/shottygun01_2.png (3x2 grid)

Tiles are deduplicated across all 6 frames (including H-flip detection).
Uses grit.exe for tile encoding to match the proven zombie sprite pipeline.

Frame layout:
  0: idle (top-left)      -- holding shotgun
  1: fire1 (top-middle)   -- small muzzle flash
  2: fire2 (top-right)    -- large muzzle flash
  3: pump1 (bottom-left)  -- pump start
  4: pump2 (bottom-middle)-- pump mid
  5: pump3 (bottom-right) -- pump end

Output: shotgun_sprites.c, shotgun_sprites.h, shotgun_previews/
"""

import os
import re
import math
import shutil
import subprocess
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SHEET_PATH = os.path.join(SCRIPT_DIR, "shotgun_6_sprites", "shottygun01_2.png")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images")
PREVIEW_DIR = os.path.join(SCRIPT_DIR, "shotgun_previews")
GRIT_EXE = os.path.join(SCRIPT_DIR, "grit_conversions", "grit.exe")
GRIT_DIR = os.path.join(SCRIPT_DIR, "grit_conversions")

TILE_SIZE = 8
MAX_WIDTH_TILES = 8     # max 64px wide
MAX_HEIGHT_TILES = 8    # max 64px tall
WEAPON_CHAR_START = 544

MAGENTA_THRESHOLD = 30
BAYER_2x2 = [[0.0, 0.5], [0.75, 0.25]]
DITHER_STRENGTH = 22

VB_PAL = {0: (0, 0, 0, 0), 1: (80, 0, 0, 255), 2: (170, 0, 0, 255),
           3: (255, 0, 0, 255)}

FRAME_NAMES = ["idle", "fire1", "fire2", "pump1", "pump2", "pump3"]

GRIT_FLAGS = ['-fh!', '-ftc', '-gB2', '-p!', '-m!']


def luminance(r, g, b):
    return int(0.299 * r + 0.587 * g + 0.114 * b)


def is_bg(r, g, b, a):
    if a < 128:
        return True
    if r > 220 and g < MAGENTA_THRESHOLD and b > 220:
        return True
    return False


def find_sprite_bboxes(img):
    """Find 6 sub-sprite bounding boxes in a 3x2 grid layout."""
    w, h = img.size
    pix = img.load()

    mask = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if not is_bg(r, g, b, a):
                mask[y][x] = True

    row_has_content = [any(mask[y]) for y in range(h)]
    bands = []
    in_band = False
    band_start = 0
    for y in range(h):
        if row_has_content[y] and not in_band:
            band_start = y
            in_band = True
        elif not row_has_content[y] and in_band:
            bands.append((band_start, y - 1))
            in_band = False
    if in_band:
        bands.append((band_start, h - 1))

    if len(bands) < 2:
        raise ValueError(f"Expected 2 rows of sprites, found {len(bands)} bands")

    bboxes = []
    for band_top, band_bot in bands[:2]:
        col_has_content = [False] * w
        for x in range(w):
            for y in range(band_top, band_bot + 1):
                if mask[y][x]:
                    col_has_content[x] = True
                    break

        cols = []
        in_col = False
        col_start = 0
        for x in range(w):
            if col_has_content[x] and not in_col:
                col_start = x
                in_col = True
            elif not col_has_content[x] and in_col:
                cols.append((col_start, x - 1))
                in_col = False
        if in_col:
            cols.append((col_start, w - 1))

        if len(cols) < 3:
            raise ValueError(f"Expected 3 cols in row, found {len(cols)}")

        for col_left, col_right in cols[:3]:
            min_y = band_bot + 1
            max_y = band_top - 1
            for y in range(band_top, band_bot + 1):
                for x in range(col_left, col_right + 1):
                    if mask[y][x]:
                        min_y = min(min_y, y)
                        max_y = max(max_y, y)
                        break
            bboxes.append((col_left, min_y, col_right, max_y))

    return bboxes


def convert_frame(img, bbox, max_w, max_h):
    """Crop, scale, dither, quantize a single frame. Returns (canvas, cols, rows)."""
    left, top, right, bottom = bbox
    crop = img.crop((left, top, right + 1, bottom + 1))
    cw, ch = crop.size

    scale = min(max_w / cw, max_h / ch)
    nw = max(1, int(cw * scale))
    nh = max(1, int(ch * scale))
    resized = crop.resize((nw, nh), Image.LANCZOS)

    cols = (nw + TILE_SIZE - 1) // TILE_SIZE
    rows = (nh + TILE_SIZE - 1) // TILE_SIZE
    canvas_w = cols * TILE_SIZE
    canvas_h = rows * TILE_SIZE

    gray = [[0.0] * canvas_w for _ in range(canvas_h)]
    alpha = [[0] * canvas_w for _ in range(canvas_h)]
    off_x = (canvas_w - nw) // 2
    off_y = canvas_h - nh  # bottom-align

    rpix = resized.load()
    for y in range(nh):
        for x in range(nw):
            r, g, b, a = rpix[x, y]
            dx, dy = off_x + x, off_y + y
            if dx < canvas_w and dy < canvas_h:
                if not is_bg(r, g, b, a):
                    gray[dy][dx] = float(luminance(r, g, b))
                    alpha[dy][dx] = 1

    # Contrast stretch
    values = [gray[y][x] for y in range(canvas_h)
              for x in range(canvas_w) if alpha[y][x]]
    if len(values) > 10:
        values.sort()
        lo = values[len(values) * 3 // 100]
        hi = values[len(values) * 97 // 100]
        if hi - lo < 20:
            hi = lo + 20
        for y in range(canvas_h):
            for x in range(canvas_w):
                if alpha[y][x]:
                    v = (gray[y][x] - lo) / (hi - lo) * 255.0
                    gray[y][x] = max(0.0, min(255.0, v))

    # Quantize with Bayer dithering
    canvas = [[0] * canvas_w for _ in range(canvas_h)]
    for y in range(canvas_h):
        for x in range(canvas_w):
            if not alpha[y][x]:
                continue
            g = gray[y][x]
            g += BAYER_2x2[y % 2][x % 2] * DITHER_STRENGTH - DITHER_STRENGTH * 0.375
            if g < 72:
                canvas[y][x] = 1
            elif g < 158:
                canvas[y][x] = 2
            else:
                canvas[y][x] = 3

    return canvas, cols, rows


def save_indexed_png(canvas, path):
    """Save a canvas as indexed 4-color PNG for grit."""
    h = len(canvas)
    w = len(canvas[0]) if h > 0 else 0
    img = Image.new('P', (w, h))
    palette = [0, 0, 0, 85, 85, 85, 170, 170, 170, 255, 255, 255]
    palette += [0] * (768 - len(palette))
    img.putpalette(palette)
    pixels = img.load()
    for y in range(h):
        for x in range(w):
            pixels[x, y] = canvas[y][x]
    img.save(path)


def run_grit(png_path):
    """Run grit on a PNG, return .c filepath (in GRIT_DIR)."""
    basename = os.path.splitext(os.path.basename(png_path))[0]
    cmd = [GRIT_EXE, png_path] + GRIT_FLAGS
    result = subprocess.run(cmd, cwd=GRIT_DIR, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  ERROR: grit failed for {basename}: {result.stderr}")
        return None
    c_file = os.path.join(GRIT_DIR, f"{basename}.c")
    if not os.path.exists(c_file):
        print(f"  ERROR: grit output not found: {c_file}")
        return None
    # Clean up any .h
    h_file = os.path.join(GRIT_DIR, f"{basename}.h")
    if os.path.exists(h_file):
        os.remove(h_file)
    return c_file


def parse_grit_tiles(c_path):
    """Parse grit .c output, return list of tiles (each tile = 4 u32 words)."""
    with open(c_path, 'r') as f:
        content = f.read()
    m = re.search(r'const unsigned int \w+Tiles\[\d+\].*?=\s*\{(.*?)\};',
                  content, re.DOTALL)
    if not m:
        return []
    words = [int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]+', m.group(1))]
    tiles = []
    for i in range(0, len(words), 4):
        tiles.append(tuple(words[i:i+4]))
    return tiles


def hflip_tile_words(words):
    """H-flip a VB 2bpp tile (sequential format) given as 4 u32 words.

    VB 2bpp tiles use sequential packing: each row is a u16 (LE) where
    bits[15:14]=px0, bits[13:12]=px1, ..., bits[1:0]=px7.
    H-flip reverses pixel order within each row.
    Returns tuple of 4 words.
    """
    data = []
    for w in words:
        data.append(w & 0xFF)
        data.append((w >> 8) & 0xFF)
        data.append((w >> 16) & 0xFF)
        data.append((w >> 24) & 0xFF)

    flipped = []
    for row in range(8):
        byte0 = data[row * 2]       # low byte of u16 (pixels 4-7)
        byte1 = data[row * 2 + 1]   # high byte of u16 (pixels 0-3)
        u16_val = (byte1 << 8) | byte0

        # Extract 8 pixels (px0 at LSB), reverse them, repack
        pixels = [(u16_val >> (x * 2)) & 3 for x in range(8)]
        pixels.reverse()
        new_u16 = 0
        for x in range(8):
            new_u16 |= (pixels[x] << (x * 2))

        flipped.append(new_u16 & 0xFF)
        flipped.append((new_u16 >> 8) & 0xFF)

    result = []
    for i in range(0, 16, 4):
        w = (flipped[i] | (flipped[i+1] << 8) |
             (flipped[i+2] << 16) | (flipped[i+3] << 24))
        result.append(w)
    return tuple(result)


def save_preview(canvas, path, scale=3):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    h = len(canvas)
    w = len(canvas[0]) if h > 0 else 0
    img = Image.new('RGBA', (w * scale, h * scale), (0, 0, 0, 255))
    pix = img.load()
    for y in range(h):
        for x in range(w):
            c = VB_PAL[canvas[y][x]]
            for sy in range(scale):
                for sx in range(scale):
                    pix[x * scale + sx, y * scale + sy] = c
    img.save(path)


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    os.makedirs(PREVIEW_DIR, exist_ok=True)

    if not os.path.exists(GRIT_EXE):
        print(f"ERROR: grit.exe not found at {GRIT_EXE}")
        return

    img = Image.open(SHEET_PATH).convert('RGBA')
    w, h = img.size
    print(f"Loaded {SHEET_PATH}: {w}x{h}")

    bboxes = find_sprite_bboxes(img)
    print(f"Found {len(bboxes)} sub-sprites")

    max_w = MAX_WIDTH_TILES * TILE_SIZE
    max_h = MAX_HEIGHT_TILES * TILE_SIZE

    # Temp dir for grit input PNGs
    grit_input_dir = os.path.join(PREVIEW_DIR, "grit_input")
    os.makedirs(grit_input_dir, exist_ok=True)

    # Phase 1: process frames and run grit on each
    frame_grit_tiles = []  # list of lists of tile tuples (4 u32 words each)
    frame_dims = []        # list of (cols, rows)

    for fi in range(6):
        bbox = bboxes[fi]
        bw = bbox[2] - bbox[0] + 1
        bh = bbox[3] - bbox[1] + 1
        canvas, cols, rows = convert_frame(img, bbox, max_w, max_h)
        save_preview(canvas, os.path.join(PREVIEW_DIR,
                                          f"shotgun_{FRAME_NAMES[fi]}.png"))
        print(f"  Frame {fi} ({FRAME_NAMES[fi]}): {bw}x{bh} -> "
              f"{cols}x{rows} tiles ({cols*8}x{rows*8} px)")

        # Save indexed PNG for grit
        png_path = os.path.join(grit_input_dir, f"shotgun_f{fi}.png")
        save_indexed_png(canvas, png_path)

        # Run grit
        c_file = run_grit(png_path)
        if c_file is None:
            print(f"  FATAL: grit failed for frame {fi}")
            return

        # Parse grit tile data
        tiles = parse_grit_tiles(c_file)
        os.remove(c_file)  # clean up temp .c file

        expected_tiles = cols * rows
        if len(tiles) != expected_tiles:
            print(f"  WARNING: expected {expected_tiles} tiles from grit, "
                  f"got {len(tiles)}")

        frame_grit_tiles.append(tiles)
        frame_dims.append((cols, rows))

    # Phase 2: deduplicate tiles across all frames (using grit-encoded words)
    unique_tiles = {}     # tile_words_tuple -> index
    unique_tile_data = [] # list of tile word tuples
    frame_maps = []       # list of (cols, rows, map_entries)

    for fi in range(6):
        cols, rows = frame_dims[fi]
        tiles = frame_grit_tiles[fi]
        frame_map = []

        for tile_words in tiles:
            flags = 0

            if tile_words in unique_tiles:
                idx = unique_tiles[tile_words]
            else:
                hflipped = hflip_tile_words(tile_words)
                if hflipped in unique_tiles:
                    idx = unique_tiles[hflipped]
                    flags = 0x2000  # H-flip
                else:
                    idx = len(unique_tile_data)
                    unique_tiles[tile_words] = idx
                    unique_tile_data.append(tile_words)

            char_idx = WEAPON_CHAR_START + idx
            frame_map.append(char_idx | flags)

        frame_maps.append((cols, rows, frame_map))

    num_tiles = len(unique_tile_data)
    print(f"\nTotal unique shotgun tiles: {num_tiles}")
    print(f"Tile data: {num_tiles * 16} bytes ({num_tiles * 4} u32 words)")

    if num_tiles > 220:
        print(f"WARNING: {num_tiles} tiles exceeds weapon slot limit of 220!")

    # --- Write C file ---
    c_path = os.path.join(OUTPUT_DIR, "shotgun_sprites.c")
    with open(c_path, 'w') as f:
        f.write("/* Shotgun weapon sprite tiles -- auto-generated by "
                "prepare_shotgun_sprites.py + grit */\n\n")

        all_u32 = []
        for td in unique_tile_data:
            all_u32.extend(td)
        f.write(f"/* {num_tiles} unique shotgun tiles */\n")
        f.write(f"const unsigned int shotgunTiles[{len(all_u32)}]"
                f" __attribute__((aligned(4))) =\n{{\n")
        for i in range(0, len(all_u32), 8):
            chunk = all_u32[i:i+8]
            s = ",".join(f"0x{w:08X}" for w in chunk)
            comma = "," if i + 8 < len(all_u32) else ""
            f.write(f"\t{s}{comma}\n")
        f.write("};\n\n")

        for fi in range(6):
            cols, rows, fmap = frame_maps[fi]
            entries = cols * rows
            f.write(f"/* frame {fi} ({FRAME_NAMES[fi]}): "
                    f"{cols} cols x {rows} rows */\n")
            f.write(f"const unsigned short shotgunFrame{fi}Map[{entries}]"
                    f" __attribute__((aligned(4))) =\n{{\n")
            for r in range(rows):
                chunk = fmap[r * cols:(r + 1) * cols]
                s = ",".join(f"0x{v:04X}" for v in chunk)
                comma = "," if r < rows - 1 else ""
                f.write(f"\t{s}{comma}\n")
            f.write("};\n\n")

    print(f"\nWrote {c_path}")

    # --- Write H file ---
    h_path = os.path.join(OUTPUT_DIR, "shotgun_sprites.h")
    with open(h_path, 'w') as f:
        f.write("#ifndef __SHOTGUN_SPRITES_H__\n"
                "#define __SHOTGUN_SPRITES_H__\n\n")
        f.write(f"#define SHOTGUN_TILE_COUNT   {num_tiles}\n")
        f.write(f"#define SHOTGUN_TILE_BYTES   {num_tiles * 16}\n\n")
        f.write(f"extern const unsigned int shotgunTiles[{len(all_u32)}];\n\n")

        for fi in range(6):
            cols, rows, fmap = frame_maps[fi]
            entries = cols * rows
            f.write(f"#define SHOTGUN_F{fi}_XTILES  {cols * 2}  "
                    f"/* {cols} cols x 2 bytes */\n")
            f.write(f"#define SHOTGUN_F{fi}_ROWS    {rows}\n")
            f.write(f"extern const unsigned short "
                    f"shotgunFrame{fi}Map[{entries}];\n\n")

        f.write("#endif\n")
    print(f"Wrote {h_path}")


if __name__ == "__main__":
    main()
