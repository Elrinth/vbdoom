"""
prepare_rocket_projectile.py

Convert rocket projectile directional sprite strip to VB tile format.
Source: new_weapons/rocket[towards_us,away_from_us,...].png (5 frames, 29x20 each)

Produces 5 frame tile arrays. At runtime, H-flip provides the other 3 directions.
Direction mapping:
  0: front (towards_us)      = frame 0
  1: front-right             = frame 4 (towards_us_but_alittle_right)
  2: right                   = frame 3 (right)
  3: back-right              = frame 2 (away_from_us_but_alittle_to_right)
  4: back (away_from_us)     = frame 1
  5: back-left               = frame 2 H-flipped
  6: left                    = frame 3 H-flipped
  7: front-left              = frame 4 H-flipped

Output: rocket_projectile_sprites.c/.h in src/vbdoom/assets/images/
"""

import os
import re
import subprocess
import shutil
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
STRIP_PATH = os.path.join(SCRIPT_DIR, "new_weapons",
    "rocket[towards_us,away_from_us,away_from_us_but_alittle_to_right,right,towards_us_but_alittle_right].png")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images")
GRIT_EXE = os.path.join(SCRIPT_DIR, "grit_conversions", "grit.exe")
GRIT_DIR = os.path.join(SCRIPT_DIR, "grit_conversions")

TILE_SIZE = 8
GRIT_FLAGS = ['-fh!', '-ftc', '-gB2', '-p!', '-m!']
NUM_FRAMES = 5
FRAME_NAMES = ["towards_us", "away_from_us", "away_alittle_right", "right", "towards_alittle_right"]

MAGENTA_THRESHOLD = 30
BAYER_2x2 = [[0.0, 0.5], [0.75, 0.25]]
DITHER_STRENGTH = 22
VB_PAL = {0: (0,0,0,0), 1: (80,0,0,255), 2: (170,0,0,255), 3: (255,0,0,255)}


def luminance(r, g, b):
    return int(0.299 * r + 0.587 * g + 0.114 * b)


def is_bg(r, g, b, a, bg_color=(128, 0, 128, 255)):
    """Check if pixel is background (transparent). Uses sampled bg_color with tolerance."""
    if a < 128:
        return True
    br, bg_g, bb = bg_color[0], bg_color[1], bg_color[2]
    if abs(r - br) < 15 and abs(g - bg_g) < 15 and abs(b - bb) < 15:
        return True
    if r < 10 and g < 10 and b < 10:
        return True
    return False


def crop_to_content(img, target_w, target_h, bg_color):
    """Crop to bounding box of non-background content, pad to target size (centered)."""
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
        return Image.new('RGBA', (target_w, target_h), (0, 0, 0, 0))

    cw = max_x - min_x + 1
    ch = max_y - min_y + 1
    cropped = img.crop((min_x, min_y, max_x + 1, max_y + 1))
    result = Image.new('RGBA', (target_w, target_h), (0, 0, 0, 0))
    ox = (target_w - cw) // 2
    oy = (target_h - ch) // 2
    result.paste(cropped, (ox, oy))
    return result


def quantize_vb(img, bg_color=(128, 0, 128, 255)):
    pix = img.load()
    w, h = img.size
    out = Image.new('RGBA', (w, h), (0,0,0,0))
    opix = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if is_bg(r, g, b, a, bg_color):
                opix[x, y] = (0,0,0,0)
                continue
            lum = luminance(r, g, b)
            bayer = BAYER_2x2[y%2][x%2]
            lum_d = max(0, min(255, lum + (bayer - 0.5) * DITHER_STRENGTH))
            if lum_d < 42: vb = 1
            elif lum_d < 128: vb = 2
            else: vb = 3
            opix[x, y] = VB_PAL[vb]
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
    return [int(x.strip(), 0) for x in raw.split(',') if x.strip()]


def main():
    print("=== Rocket Projectile Sprite Converter ===\n")

    if not os.path.exists(STRIP_PATH):
        print(f"ERROR: Strip not found: {STRIP_PATH}")
        return

    img = Image.open(STRIP_PATH).convert('RGBA')
    print(f"Strip: {img.size[0]}x{img.size[1]}")

    # Sample background from top-left corner
    bg_color = img.getpixel((0, 0))
    print(f"Background color (top-left): RGBA{bg_color}")

    fw = img.size[0] // NUM_FRAMES
    fh = img.size[1]
    # Pad each frame to 16x16 (2x2 tiles) - fits 15x14 first frame
    target_w = 16
    target_h = 16

    temp_dir = os.path.join(GRIT_DIR, "_rocket_proj_temp")
    os.makedirs(temp_dir, exist_ok=True)

    all_data = []
    for i in range(NUM_FRAMES):
        frame = img.crop((i * fw, 0, (i + 1) * fw, fh))
        # Crop to content, remove bg, pad to 16x16 centered
        padded = crop_to_content(frame, target_w, target_h, bg_color)
        quantized = quantize_vb(padded, bg_color)

        path = os.path.join(temp_dir, f"rocket_proj_{i}.png")
        quantized.save(path)
        data = run_grit(path)
        all_data.append(data)
        tiles = len(data) // 4
        print(f"Frame {i} ({FRAME_NAMES[i]}): {tiles} tiles, {len(data)*4} bytes")

    shutil.rmtree(temp_dir, ignore_errors=True)

    tiles_per_frame = len(all_data[0]) // 4  # 4 (2x2 tiles)
    tw = target_w // TILE_SIZE  # 2
    th = target_h // TILE_SIZE  # 2

    # Write C source
    c_path = os.path.join(OUTPUT_DIR, "rocket_projectile_sprites.c")
    h_path = os.path.join(OUTPUT_DIR, "rocket_projectile_sprites.h")

    with open(c_path, 'w') as f:
        f.write("/* Rocket projectile directional sprites -- auto-generated */\n\n")
        for i, data in enumerate(all_data):
            f.write(f"const unsigned int rocketProjFrame{i}Tiles[{len(data)}] __attribute__((aligned(4))) = {{\n")
            for j in range(0, len(data), 4):
                chunk = data[j:j+4]
                vals = ', '.join(f'0x{v:08X}' for v in chunk)
                comma = ',' if j + 4 < len(data) else ''
                f.write(f'    {vals}{comma}\n')
            f.write("};\n\n")

    with open(h_path, 'w') as f:
        f.write("#ifndef __ROCKET_PROJECTILE_SPRITES_H__\n")
        f.write("#define __ROCKET_PROJECTILE_SPRITES_H__\n\n")
        f.write(f"#define ROCKET_PROJ_TILE_W     {tw}\n")
        f.write(f"#define ROCKET_PROJ_TILE_H     {th}\n")
        f.write(f"#define ROCKET_PROJ_TILES      {tiles_per_frame}\n")
        f.write(f"#define ROCKET_PROJ_FRAME_BYTES {tiles_per_frame * 16}\n")
        f.write(f"#define ROCKET_PROJ_FRAME_COUNT {NUM_FRAMES}\n\n")

        for i in range(NUM_FRAMES):
            f.write(f"extern const unsigned int rocketProjFrame{i}Tiles[{len(all_data[i])}];\n")

        f.write(f"\n/* Frame pointer table (5 unique views) */\n")
        f.write(f"static const unsigned int* const ROCKET_PROJ_FRAMES[{NUM_FRAMES}] = {{\n")
        for i in range(NUM_FRAMES):
            comma = ',' if i < NUM_FRAMES - 1 else ''
            f.write(f"    rocketProjFrame{i}Tiles{comma}\n")
        f.write("};\n\n")

        f.write("/*\n")
        f.write(" * Direction mapping (8 directions, 0=front/towards player):\n")
        f.write(" *   0: front        = frame 0\n")
        f.write(" *   1: front-right  = frame 4\n")
        f.write(" *   2: right        = frame 3\n")
        f.write(" *   3: back-right   = frame 2\n")
        f.write(" *   4: back         = frame 1\n")
        f.write(" *   5: back-left    = frame 2 (H-flip)\n")
        f.write(" *   6: left         = frame 3 (H-flip)\n")
        f.write(" *   7: front-left   = frame 4 (H-flip)\n")
        f.write(" */\n")
        f.write("static const u8 ROCKET_PROJ_DIR_FRAME[8] = {0, 4, 3, 2, 1, 2, 3, 4};\n")
        f.write("static const u8 ROCKET_PROJ_DIR_HFLIP[8] = {0, 0, 0, 0, 0, 1, 1, 1};\n\n")

        f.write("#endif\n")

    print(f"\nWritten: {c_path}")
    print(f"Written: {h_path}")


if __name__ == '__main__':
    main()
