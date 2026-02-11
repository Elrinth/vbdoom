"""
Convert Powerups_003.png (rocket launcher pickup) to VB format for use as a pickup sprite.
Output: pickup_rocket.c / pickup_rocket.h (4x3 tiles = 32x24 pixels)
"""
import os
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
INPUT = os.path.join(SCRIPT_DIR, "make_into_4_colors_2", "converted", "frames",
                     "Powerups", "Powerups_003.png")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images",
                          "sprites", "pickups")

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
    """VB 2bpp sequential: each row u16 (LE), pixel 0 at bits[1:0]. Matches wall/fireball/particle pipeline."""
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


def main():
    img = Image.open(INPUT).convert('RGBA')
    w, h = img.size
    print(f"Loaded {INPUT}: {w}x{h}")

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

    crop = img.crop((min_x, min_y, max_x + 1, max_y + 1))
    cw, ch = crop.size
    print(f"Bounding box: {cw}x{ch}")

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

    # Slice into tiles
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

    # Write C file
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    c_path = os.path.join(OUTPUT_DIR, "pickup_rocket.c")
    with open(c_path, 'w') as f:
        f.write("/* Rocket launcher pickup sprite -- auto-generated */\n\n")
        f.write(f"const unsigned int pickup_rocketTiles[{len(all_u32)}]"
                f" __attribute__((aligned(4))) =\n{{\n")
        for i in range(0, len(all_u32), 8):
            chunk = all_u32[i:i+8]
            s = ",".join(f"0x{w:08X}" for w in chunk)
            comma = "," if i + 8 < len(all_u32) else ""
            f.write(f"\t{s}{comma}\n")
        f.write("};\n")
    print(f"Wrote {c_path}")

    h_path = os.path.join(OUTPUT_DIR, "pickup_rocket.h")
    with open(h_path, 'w') as f:
        f.write("#ifndef __PICKUP_ROCKET_H__\n#define __PICKUP_ROCKET_H__\n\n")
        f.write(f"extern const unsigned int pickup_rocketTiles[{len(all_u32)}];\n\n")
        f.write("#endif\n")
    print(f"Wrote {h_path}")


if __name__ == "__main__":
    main()
