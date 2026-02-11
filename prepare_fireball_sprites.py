"""
Convert fireball flight (2 frames) and explosion (5 frames) PNGs to VB tile arrays.
Fireball flight: 32x32 px (4x4 tiles = 16 chars)
Fireball explosion: 32x32 px (4x4 tiles = 16 chars)
Output: src/vbdoom/assets/images/sprites/fireball/fireball_sprites.c + .h
"""
import os
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
FLIGHT_DIR = os.path.join(SCRIPT_DIR, "make_into_4_colors_2", "converted", "frames", "fireball_2_anim")
EXPLODE_DIR = os.path.join(SCRIPT_DIR, "make_into_4_colors_2", "converted", "frames", "fireball_explode")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images", "sprites", "fireball")
PREVIEW_DIR = os.path.join(SCRIPT_DIR, "fireball_previews")

TILE_SIZE = 8
TARGET_W, TARGET_H = 32, 32
TILES_X, TILES_Y = TARGET_W // TILE_SIZE, TARGET_H // TILE_SIZE
TILES_PER_FRAME = TILES_X * TILES_Y  # 16

VB_PAL = {0: (0, 0, 0, 0), 1: (80, 0, 0, 255), 2: (170, 0, 0, 255), 3: (255, 0, 0, 255)}


def luminance(r, g, b):
    return int(0.299 * r + 0.587 * g + 0.114 * b)


def is_bg(r, g, b, a):
    if a < 128:
        return True
    if r < 15 and g < 15 and b < 15:
        return True
    return False


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


def convert_frame(img_path, target_w, target_h):
    """Load PNG, scale to target, convert to VB palette, return u32 tile array."""
    img = Image.open(img_path).convert('RGBA')
    w, h = img.size

    # Find bounding box
    pix = img.load()
    min_x, min_y, max_x, max_y = w, h, 0, 0
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if not is_bg(r, g, b, a):
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)

    if max_x < min_x:
        # Fully transparent frame
        return [0] * (target_w * target_h // (TILE_SIZE * TILE_SIZE) * 4)

    crop = img.crop((min_x, min_y, max_x + 1, max_y + 1))
    cw, ch = crop.size

    scale = min(target_w / cw, target_h / ch)
    nw = max(1, int(cw * scale))
    nh = max(1, int(ch * scale))
    resized = crop.resize((nw, nh), Image.LANCZOS)

    # Center on canvas
    canvas = [[0] * target_w for _ in range(target_h)]
    off_x = (target_w - nw) // 2
    off_y = (target_h - nh) // 2

    rpix = resized.load()
    for y in range(nh):
        for x in range(nw):
            r, g, b, a = rpix[x, y]
            dx, dy = off_x + x, off_y + y
            if dx < target_w and dy < target_h:
                if is_bg(r, g, b, a):
                    canvas[dy][dx] = 0
                else:
                    gray = luminance(r, g, b)
                    if gray < 85:
                        canvas[dy][dx] = 1
                    elif gray < 170:
                        canvas[dy][dx] = 2
                    else:
                        canvas[dy][dx] = 3

    # Slice into tiles
    tiles_x = target_w // TILE_SIZE
    tiles_y = target_h // TILE_SIZE
    all_u32 = []
    for tr in range(tiles_y):
        for tc in range(tiles_x):
            tile = []
            for y in range(TILE_SIZE):
                row = []
                for x in range(TILE_SIZE):
                    row.append(canvas[tr * TILE_SIZE + y][tc * TILE_SIZE + x])
                tile.append(row)
            all_u32.extend(bytes_to_u32(tile_to_vb_2bpp(tile)))

    return all_u32


def save_preview(canvas, target_w, target_h, name):
    os.makedirs(PREVIEW_DIR, exist_ok=True)
    prev = Image.new('RGBA', (target_w * 4, target_h * 4), (0, 0, 0, 255))
    pp = prev.load()
    for y in range(target_h):
        for x in range(target_w):
            c = VB_PAL.get(canvas[y][x], (0, 0, 0, 255))
            for sy in range(4):
                for sx in range(4):
                    pp[x*4+sx, y*4+sy] = c
    prev.save(os.path.join(PREVIEW_DIR, f"{name}.png"))


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Collect all frames
    flight_frames = sorted([f for f in os.listdir(FLIGHT_DIR)
                           if f.endswith('.png') and not f.startswith('_')])
    explode_frames = sorted([f for f in os.listdir(EXPLODE_DIR)
                            if f.endswith('.png') and not f.startswith('_')])

    print(f"Flight frames: {len(flight_frames)}")
    print(f"Explosion frames: {len(explode_frames)}")

    all_arrays = []  # (name, u32_data, type)

    for i, fname in enumerate(flight_frames):
        path = os.path.join(FLIGHT_DIR, fname)
        u32_data = convert_frame(path, TARGET_W, TARGET_H)
        name = f"fireball_flight_{i:03d}"
        all_arrays.append((name, u32_data, 'flight'))
        print(f"  Converted {fname} -> {name} ({len(u32_data)} u32 words)")

    for i, fname in enumerate(explode_frames):
        path = os.path.join(EXPLODE_DIR, fname)
        u32_data = convert_frame(path, TARGET_W, TARGET_H)
        name = f"fireball_explode_{i:03d}"
        all_arrays.append((name, u32_data, 'explode'))
        print(f"  Converted {fname} -> {name} ({len(u32_data)} u32 words)")

    # Write C file
    c_path = os.path.join(OUTPUT_DIR, "fireball_sprites.c")
    with open(c_path, 'w') as f:
        f.write("/* Fireball sprites -- auto-generated by prepare_fireball_sprites.py */\n\n")
        f.write('#include "fireball_sprites.h"\n\n')
        for name, u32_data, _ in all_arrays:
            f.write(f"const unsigned int {name}Tiles[{len(u32_data)}]"
                    f" __attribute__((aligned(4))) =\n{{\n")
            for j in range(0, len(u32_data), 8):
                chunk = u32_data[j:j+8]
                s = ",".join(f"0x{w:08X}" for w in chunk)
                comma = "," if j + 8 < len(u32_data) else ""
                f.write(f"\t{s}{comma}\n")
            f.write("};\n\n")
    print(f"Wrote {c_path}")

    # Write header
    words_per_frame = TILES_PER_FRAME * 4  # 16 tiles * 4 u32 per tile = 64
    flight_count = len([a for a in all_arrays if a[2] == 'flight'])
    explode_count = len([a for a in all_arrays if a[2] == 'explode'])

    h_path = os.path.join(OUTPUT_DIR, "fireball_sprites.h")
    with open(h_path, 'w') as f:
        f.write("#ifndef __FIREBALL_SPRITES_H__\n")
        f.write("#define __FIREBALL_SPRITES_H__\n\n")
        f.write("/*\n")
        f.write(f" * Fireball sprites ({TARGET_W}x{TARGET_H} px, {TILES_X}x{TILES_Y} tiles)\n")
        f.write(f" * Flight frames: {flight_count}\n")
        f.write(f" * Explosion frames: {explode_count}\n")
        f.write(f" * Tiles per frame: {TILES_PER_FRAME} ({words_per_frame} u32 words = {words_per_frame*4} bytes)\n")
        f.write(" */\n\n")

        for name, u32_data, _ in all_arrays:
            f.write(f"extern const unsigned int {name}Tiles[{len(u32_data)}];\n")

        f.write(f"\n#define FIREBALL_FLIGHT_COUNT {flight_count}\n")
        f.write(f"#define FIREBALL_EXPLODE_COUNT {explode_count}\n")
        f.write(f"#define FIREBALL_FRAME_BYTES {words_per_frame * 4}\n\n")

        f.write("static const unsigned int* const FIREBALL_FLIGHT_FRAMES[] = {\n")
        for name, _, typ in all_arrays:
            if typ == 'flight':
                f.write(f"    {name}Tiles,\n")
        f.write("};\n\n")

        f.write("static const unsigned int* const FIREBALL_EXPLODE_FRAMES[] = {\n")
        for name, _, typ in all_arrays:
            if typ == 'explode':
                f.write(f"    {name}Tiles,\n")
        f.write("};\n\n")

        f.write("#endif\n")
    print(f"Wrote {h_path}")


if __name__ == "__main__":
    main()
