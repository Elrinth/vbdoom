"""
prepare_keycard_sprites.py

Convert Powerups_052-057 (keycard animation, 6 frames) to VB format.
All 3 key colors share the same sprite data (VB can't show color differences).

Output: pickup_keycard.c / pickup_keycard.h (4x3 tiles = 32x24 pixels per frame)
"""
import os
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
INPUT_DIR = os.path.join(SCRIPT_DIR, "make_into_4_colors_2", "converted",
                         "frames", "Powerups")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images",
                          "sprites", "pickups")
PREVIEW_DIR = os.path.join(SCRIPT_DIR, "pickup_previews")

FRAME_FILES = [f"Powerups_{52+i:03d}.png" for i in range(6)]

TILE_W, TILE_H = 4, 3
PX_W, PX_H = 32, 24
TILE_SIZE = 8

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


def convert_frame(input_path):
    """Convert a single keycard frame to VB tile data (48 u32 words)."""
    img = Image.open(input_path).convert('RGBA')
    w, h = img.size

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

    if min_x > max_x:
        # Empty frame
        return [0] * (TILE_W * TILE_H * 4), [[0]*PX_W for _ in range(PX_H)]

    crop = img.crop((min_x, min_y, max_x + 1, max_y + 1))
    cw, ch = crop.size

    scale = min(PX_W / cw, PX_H / ch)
    nw = max(1, int(cw * scale))
    nh = max(1, int(ch * scale))
    resized = crop.resize((nw, nh), Image.LANCZOS)

    canvas = [[0] * PX_W for _ in range(PX_H)]
    off_x = (PX_W - nw) // 2
    off_y = (PX_H - nh) // 2

    rpix = resized.load()
    for y in range(nh):
        for x in range(nw):
            r, g, b, a = rpix[x, y]
            dx, dy = off_x + x, off_y + y
            if dx < PX_W and dy < PX_H:
                if is_bg(r, g, b, a):
                    canvas[dy][dx] = 0
                else:
                    if r < 43:
                        canvas[dy][dx] = 1
                    elif r < 127:
                        canvas[dy][dx] = 2
                    else:
                        canvas[dy][dx] = 3

    all_u32 = []
    for tr in range(TILE_H):
        for tc in range(TILE_W):
            tile = []
            for y in range(TILE_SIZE):
                row = []
                for x in range(TILE_SIZE):
                    row.append(canvas[tr * TILE_SIZE + y][tc * TILE_SIZE + x])
                tile.append(row)
            all_u32.extend(bytes_to_u32(tile_to_vb_2bpp(tile)))

    return all_u32, canvas


def main():
    print("=== Keycard Sprite Converter ===\n")

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    os.makedirs(PREVIEW_DIR, exist_ok=True)

    all_frames = []
    for i, fname in enumerate(FRAME_FILES):
        fpath = os.path.join(INPUT_DIR, fname)
        if not os.path.exists(fpath):
            print(f"WARNING: Missing {fpath}")
            all_frames.append([0] * (TILE_W * TILE_H * 4))
            continue
        u32_data, canvas = convert_frame(fpath)
        all_frames.append(u32_data)
        print(f"  Frame {i}: {fname} -> {len(u32_data)} u32 words")

        # Save preview
        prev = Image.new('RGBA', (PX_W * 4, PX_H * 4), (0, 0, 0, 255))
        pp = prev.load()
        for y in range(PX_H):
            for x in range(PX_W):
                c = VB_PAL[canvas[y][x]]
                for sy in range(4):
                    for sx in range(4):
                        pp[x*4+sx, y*4+sy] = c
        prev.save(os.path.join(PREVIEW_DIR, f"pickup_keycard_{i}.png"))

    # Write C file with all 6 frames
    c_path = os.path.join(OUTPUT_DIR, "pickup_keycard.c")
    with open(c_path, 'w') as f:
        f.write("/* Keycard pickup sprites (6 animation frames) -- auto-generated */\n\n")
        for i, u32_data in enumerate(all_frames):
            f.write(f"const unsigned int pickup_keycard_{i:03d}Tiles[{len(u32_data)}]"
                    f" __attribute__((aligned(4))) =\n{{\n")
            for j in range(0, len(u32_data), 8):
                chunk = u32_data[j:j+8]
                s = ",".join(f"0x{w:08X}" for w in chunk)
                comma = "," if j + 8 < len(u32_data) else ""
                f.write(f"\t{s}{comma}\n")
            f.write("};\n\n")
    print(f"\nWrote {c_path}")

    # Write header
    h_path = os.path.join(OUTPUT_DIR, "pickup_keycard.h")
    with open(h_path, 'w') as f:
        f.write("#ifndef __PICKUP_KEYCARD_H__\n#define __PICKUP_KEYCARD_H__\n\n")
        f.write("#define KEYCARD_ANIM_FRAMES 6\n\n")
        for i in range(len(all_frames)):
            f.write(f"extern const unsigned int pickup_keycard_{i:03d}Tiles[48];\n")
        f.write("\n/* Frame table for keycard animation */\n")
        f.write(f"static const unsigned int* const PICKUP_KEYCARD_FRAME_TABLE[{len(all_frames)}] = {{\n")
        for i in range(len(all_frames)):
            comma = "," if i < len(all_frames) - 1 else ""
            f.write(f"    pickup_keycard_{i:03d}Tiles{comma}\n")
        f.write("};\n\n")
        f.write("#endif\n")
    print(f"Wrote {h_path}")


if __name__ == "__main__":
    main()
