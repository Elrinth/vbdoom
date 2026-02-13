"""
prepare_chaingun_sprites.py

Convert chaingun first-person sprite strip to VB tile format using grit.exe.
Source: new_weapons/chaingun[idle,shootframe1,shootframe2].png

Generates DUAL-LAYER sprites (red + black), matching the shotgun/rocket approach:
  - Red layer: the weapon body (normal palette GPLT0)
  - Black layer: 1-pixel outline around the weapon (GPLT2 palette)

Output: src/vbdoom/assets/images/sprites/chaingun/chaingun_sprites.c/.h
"""

import os
import re
import math
import shutil
import subprocess
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
STRIP_PATH = os.path.join(SCRIPT_DIR, "new_weapons",
    "chaingun[idle,shootframe1,shootframe2].png")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images",
                          "sprites", "chaingun")
GRIT_EXE = os.path.join(SCRIPT_DIR, "grit_conversions", "grit.exe")
GRIT_DIR = os.path.join(SCRIPT_DIR, "grit_conversions")

TILE_SIZE = 8
CHAINGUN_CHAR_START = 120  # Same shared region as shotgun/rocket (only one loaded at a time)

MAGENTA_THRESHOLD = 30
BAYER_2x2 = [[0.0, 0.5], [0.75, 0.25]]
DITHER_STRENGTH = 22

VB_PAL = {0: (0, 0, 0, 0), 1: (80, 0, 0, 255), 2: (170, 0, 0, 255),
           3: (255, 0, 0, 255)}

FRAME_NAMES = ["idle", "shootframe1", "shootframe2"]
GRIT_FLAGS = ['-fh!', '-ftc', '-gB2', '-p!', '-m!']

NUM_FRAMES = 3


def luminance(r, g, b):
    return int(0.299 * r + 0.587 * g + 0.114 * b)


def is_bg(r, g, b, a, bg_color=(128, 0, 128, 255)):
    if a < 128:
        return True
    br, bg_g, bb = bg_color[0], bg_color[1], bg_color[2]
    if abs(r - br) < 15 and abs(g - bg_g) < 15 and abs(b - bb) < 15:
        return True
    return False


def find_strip_bboxes(img, num_frames, bg_color):
    """Find num_frames sub-sprite bounding boxes in a horizontal strip."""
    w, h = img.size
    pix = img.load()

    mask = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if not is_bg(r, g, b, a, bg_color):
                mask[y][x] = True

    col_has_content = [any(mask[y][x] for y in range(h)) for x in range(w)]

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

    if len(cols) == num_frames:
        bboxes = []
        for col_left, col_right in cols[:num_frames]:
            min_y = h
            max_y = 0
            for y in range(h):
                for x in range(col_left, col_right + 1):
                    if mask[y][x]:
                        min_y = min(min_y, y)
                        max_y = max(max_y, y)
                        break
            bboxes.append((col_left, min_y, col_right, max_y))
        return bboxes

    # Fall back to tile-aligned split
    fw = (w // num_frames // TILE_SIZE) * TILE_SIZE
    if fw < TILE_SIZE:
        fw = TILE_SIZE
    return [(i * fw, 0, (i + 1) * fw - 1, h - 1) for i in range(num_frames)]


def split_strip(img, num_frames, bg_color=None):
    w, h = img.size
    if bg_color is None:
        bg_color = img.getpixel((0, 0))
    bboxes = find_strip_bboxes(img, num_frames, bg_color)
    frames = []
    for left, top, right, bottom in bboxes:
        frame = img.crop((left, top, right + 1, bottom + 1))
        frames.append(frame)
    return frames


def crop_to_content(img, max_w_tiles=8, max_h_tiles=8, bg_color=(128, 0, 128, 255)):
    pix = img.load()
    w, h = img.size
    min_x, min_y, max_x, max_y = w, h, 0, 0
    has_content = False
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if not is_bg(r, g, b, a, bg_color):
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)
                has_content = True
    if not has_content:
        return Image.new('RGBA', (TILE_SIZE, TILE_SIZE), (0, 0, 0, 0))
    cw = max_x - min_x + 1
    ch = max_y - min_y + 1
    tw = ((cw + TILE_SIZE - 1) // TILE_SIZE) * TILE_SIZE
    th = ((ch + TILE_SIZE - 1) // TILE_SIZE) * TILE_SIZE
    tw = min(tw, max_w_tiles * TILE_SIZE)
    th = min(th, max_h_tiles * TILE_SIZE)
    ox = (tw - cw) // 2
    oy = (th - ch) // 2
    result = Image.new('RGBA', (tw, th), (0, 0, 0, 0))
    cropped = img.crop((min_x, min_y, max_x + 1, max_y + 1))
    result.paste(cropped, (ox, oy))
    return result


def quantize_vb(img, bg_color=(128, 0, 128, 255)):
    pix = img.load()
    w, h = img.size
    out = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    opix = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if is_bg(r, g, b, a, bg_color):
                opix[x, y] = (0, 0, 0, 0)
                continue
            lum = luminance(r, g, b)
            bayer = BAYER_2x2[y % 2][x % 2]
            dither = (bayer - 0.5) * DITHER_STRENGTH
            lum_d = max(0, min(255, lum + dither))
            if lum_d < 42:
                vb = 1
            elif lum_d < 128:
                vb = 2
            else:
                vb = 3
            opix[x, y] = VB_PAL[vb]
    return out


def make_black_layer(img):
    pix = img.load()
    w, h = img.size
    mask = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if a > 0 and not (r == 0 and g == 0 and b == 0 and a == 0):
                mask[y][x] = True
    out = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    opix = out.load()
    for y in range(h):
        for x in range(w):
            if mask[y][x]:
                continue
            for dy, dx in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                ny, nx = y + dy, x + dx
                if 0 <= ny < h and 0 <= nx < w and mask[ny][nx]:
                    opix[x, y] = VB_PAL[1]
                    break
    return out


def run_grit(png_path):
    cmd = [GRIT_EXE, png_path] + GRIT_FLAGS
    result = subprocess.run(cmd, cwd=GRIT_DIR, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  GRIT ERROR: {result.stderr}")
        return []
    basename = os.path.splitext(os.path.basename(png_path))[0]
    c_file = os.path.join(GRIT_DIR, f'{basename}.c')
    if not os.path.exists(c_file):
        return []
    with open(c_file) as f:
        text = f.read()
    os.remove(c_file)
    m = re.search(r'const unsigned int \w+\[\d+\][^=]*=\s*\{([^}]+)\}', text, re.DOTALL)
    if not m:
        return []
    raw = m.group(1).replace('\n', ' ').replace('\t', ' ')
    vals = [int(x.strip(), 0) for x in raw.split(',') if x.strip()]
    return vals


def tiles_to_tuples(data, tile_count):
    tiles = []
    for i in range(tile_count):
        t = tuple(data[i*4:(i+1)*4])
        tiles.append(t)
    return tiles


def deduplicate_tiles(all_frame_tiles):
    EMPTY = (0, 0, 0, 0)
    unique = [EMPTY]
    tile_map = {EMPTY: 0}
    frame_maps = []
    for frame_tiles, tw, th in all_frame_tiles:
        fmap = []
        for tile in frame_tiles:
            if tile not in tile_map:
                tile_map[tile] = len(unique)
                unique.append(tile)
            fmap.append(tile_map[tile])
        frame_maps.append((fmap, tw, th))
    return unique, frame_maps


def write_output(unique_tiles, red_maps, blk_maps, frame_sizes):
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    c_path = os.path.join(OUTPUT_DIR, "chaingun_sprites.c")
    h_path = os.path.join(OUTPUT_DIR, "chaingun_sprites.h")

    tile_count = len(unique_tiles)

    with open(c_path, 'w') as f:
        f.write("/* Chaingun weapon sprites -- auto-generated */\n")
        f.write(f"const unsigned int chaingunTiles[{tile_count * 4}] = {{\n")
        for i, tile in enumerate(unique_tiles):
            vals = ', '.join(f'0x{v:08X}' for v in tile)
            comma = ',' if i < tile_count - 1 else ''
            f.write(f'    {vals}{comma}\n')
        f.write("};\n\n")

        for layer, maps, prefix in [("Red", red_maps, "chaingun"),
                                     ("Black", blk_maps, "chaingunBlk")]:
            for fi, (fmap, tw, th) in enumerate(maps):
                arr_name = f"{prefix}Frame{fi}Map"
                f.write(f"const unsigned short {arr_name}[{len(fmap)}] = {{\n")
                for row in range(th):
                    entries = []
                    for col in range(tw):
                        idx = fmap[row * tw + col]
                        char_idx = CHAINGUN_CHAR_START + idx
                        entries.append(f'0x{char_idx:04X}')
                    comma = ',' if row < th - 1 else ''
                    f.write(f'    {", ".join(entries)}{comma}\n')
                f.write("};\n\n")

    with open(h_path, 'w') as f:
        f.write("#ifndef __CHAINGUN_SPRITES_H__\n")
        f.write("#define __CHAINGUN_SPRITES_H__\n\n")
        f.write(f"#define CHAINGUN_CHAR_START   {CHAINGUN_CHAR_START}\n")
        f.write(f"#define CHAINGUN_TILE_COUNT   {tile_count}\n")
        f.write(f"#define CHAINGUN_TILE_BYTES   {tile_count * 16}\n\n")
        f.write(f"extern const unsigned int chaingunTiles[{tile_count * 4}];\n\n")

        f.write("/* Red layer frames */\n")
        for fi, (fmap, tw, th) in enumerate(red_maps):
            f.write(f"#define CHAINGUN_F{fi}_XTILES  {tw * 2}\n")
            f.write(f"#define CHAINGUN_F{fi}_ROWS    {th}\n")
            f.write(f"extern const unsigned short chaingunFrame{fi}Map[{len(fmap)}];\n\n")

        f.write("/* Black layer frames */\n")
        for fi, (fmap, tw, th) in enumerate(blk_maps):
            f.write(f"#define CHAINGUN_BLK_F{fi}_XTILES  {tw * 2}\n")
            f.write(f"#define CHAINGUN_BLK_F{fi}_ROWS    {th}\n")
            f.write(f"extern const unsigned short chaingunBlkFrame{fi}Map[{len(fmap)}];\n\n")

        f.write("#endif\n")

    print(f"Written: {c_path}")
    print(f"Written: {h_path}")
    print(f"Total unique tiles: {tile_count}")


def main():
    print("=== Chaingun Sprite Converter ===\n")

    if not os.path.exists(STRIP_PATH):
        print(f"ERROR: Strip not found: {STRIP_PATH}")
        return
    if not os.path.exists(GRIT_EXE):
        print(f"ERROR: grit.exe not found: {GRIT_EXE}")
        return

    img = Image.open(STRIP_PATH).convert('RGBA')
    print(f"Strip: {img.size[0]}x{img.size[1]}")

    bg_color = img.getpixel((0, 0))
    print(f"Background color (top-left pixel): RGBA{bg_color}")

    frames = split_strip(img, NUM_FRAMES, bg_color)
    print(f"Split into {len(frames)} frames")

    all_red_tiles = []
    all_blk_tiles = []
    frame_sizes = []
    temp_dir = os.path.join(GRIT_DIR, "_chaingun_temp")
    os.makedirs(temp_dir, exist_ok=True)

    for fi, frame in enumerate(frames):
        print(f"\nFrame {fi} ({FRAME_NAMES[fi]}):")
        cropped = crop_to_content(frame, bg_color=bg_color)
        tw = cropped.size[0] // TILE_SIZE
        th = cropped.size[1] // TILE_SIZE
        print(f"  Content: {cropped.size[0]}x{cropped.size[1]} ({tw}x{th} tiles)")
        frame_sizes.append((tw, th))

        quantized = quantize_vb(cropped, bg_color=bg_color)
        black = make_black_layer(quantized)

        red_path = os.path.join(temp_dir, f"chaingun_red_{fi}.png")
        blk_path = os.path.join(temp_dir, f"chaingun_blk_{fi}.png")
        quantized.save(red_path)
        black.save(blk_path)

        red_data = run_grit(red_path)
        blk_data = run_grit(blk_path)

        red_tiles = tiles_to_tuples(red_data, tw * th)
        blk_tiles = tiles_to_tuples(blk_data, tw * th)

        all_red_tiles.append((red_tiles, tw, th))
        all_blk_tiles.append((blk_tiles, tw, th))

    shutil.rmtree(temp_dir, ignore_errors=True)

    combined = all_red_tiles + all_blk_tiles
    unique_tiles, all_maps = deduplicate_tiles(combined)

    red_maps = all_maps[:NUM_FRAMES]
    blk_maps = all_maps[NUM_FRAMES:]

    print(f"\nDeduplication: {sum(tw*th for _, tw, th in combined)} total -> {len(unique_tiles)} unique tiles")

    write_output(unique_tiles, red_maps, blk_maps, frame_sizes)


if __name__ == '__main__':
    main()
