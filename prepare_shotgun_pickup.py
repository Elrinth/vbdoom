"""
Convert Powerups_005.png (shotgun pickup) to VB format for use as a pickup sprite.
Output: pickup_shotgun.c / pickup_shotgun.h (4x3 tiles = 32x24 pixels)
"""
import os
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
INPUT = os.path.join(SCRIPT_DIR, "make_into_4_colors_2", "converted", "frames",
                     "Powerups", "Powerups_005.png")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images",
                          "sprites", "pickups")
PREVIEW_DIR = os.path.join(SCRIPT_DIR, "pickup_previews")

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


def main():
    img = Image.open(INPUT).convert('RGBA')
    w, h = img.size
    print(f"Loaded {INPUT}: {w}x{h}")

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

    crop = img.crop((min_x, min_y, max_x + 1, max_y + 1))
    cw, ch = crop.size
    print(f"Bounding box: {cw}x{ch}")

    # Scale to fit 32x24
    scale = min(PX_W / cw, PX_H / ch)
    nw = max(1, int(cw * scale))
    nh = max(1, int(ch * scale))
    resized = crop.resize((nw, nh), Image.LANCZOS)

    # Center on 32x24 canvas
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
                    gray = luminance(r, g, b)
                    if gray < 85:
                        canvas[dy][dx] = 1
                    elif gray < 170:
                        canvas[dy][dx] = 2
                    else:
                        canvas[dy][dx] = 3

    # Save preview
    os.makedirs(PREVIEW_DIR, exist_ok=True)
    prev = Image.new('RGBA', (PX_W * 4, PX_H * 4), (0, 0, 0, 255))
    pp = prev.load()
    for y in range(PX_H):
        for x in range(PX_W):
            c = VB_PAL[canvas[y][x]]
            for sy in range(4):
                for sx in range(4):
                    pp[x*4+sx, y*4+sy] = c
    prev.save(os.path.join(PREVIEW_DIR, "pickup_shotgun.png"))

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
    c_path = os.path.join(OUTPUT_DIR, "pickup_shotgun.c")
    with open(c_path, 'w') as f:
        f.write("/* Shotgun pickup sprite -- auto-generated */\n\n")
        f.write(f"const unsigned int pickup_shotgunTiles[{len(all_u32)}]"
                f" __attribute__((aligned(4))) =\n{{\n")
        for i in range(0, len(all_u32), 8):
            chunk = all_u32[i:i+8]
            s = ",".join(f"0x{w:08X}" for w in chunk)
            comma = "," if i + 8 < len(all_u32) else ""
            f.write(f"\t{s}{comma}\n")
        f.write("};\n")
    print(f"Wrote {c_path}")

    h_path = os.path.join(OUTPUT_DIR, "pickup_shotgun.h")
    with open(h_path, 'w') as f:
        f.write("#ifndef __PICKUP_SHOTGUN_H__\n#define __PICKUP_SHOTGUN_H__\n\n")
        f.write(f"extern const unsigned int pickup_shotgunTiles[{len(all_u32)}];\n\n")
        f.write("#endif\n")
    print(f"Wrote {h_path}")


if __name__ == "__main__":
    main()
