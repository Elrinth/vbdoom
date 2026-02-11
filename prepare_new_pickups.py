"""
Convert new pickup sprites to VB format (32x24 px, 4x3 tiles, 12 chars each).
- Helmet: Powerups_016-019 (4 frames for ping-pong animation)
- Armor: Powerups_032-034 (3 frames for ping-pong animation)
- Shells box: Powerups_020 (1 static frame)
Output: src/vbdoom/assets/images/sprites/pickups/pickup_<name>.c + .h
"""
import os
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
POWERUPS_DIR = os.path.join(SCRIPT_DIR, "make_into_4_colors_2", "converted", "frames", "Powerups")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images", "sprites", "pickups")
PREVIEW_DIR = os.path.join(SCRIPT_DIR, "pickup_previews")

TILE_SIZE = 8
PX_W, PX_H = 32, 24
TILES_X, TILES_Y = PX_W // TILE_SIZE, PX_H // TILE_SIZE
U32_PER_FRAME = TILES_X * TILES_Y * 4  # 4x3 tiles x 4 u32/tile = 48

VB_PAL = {0: (0, 0, 0, 0), 1: (80, 0, 0, 255), 2: (170, 0, 0, 255), 3: (255, 0, 0, 255)}

PICKUPS = {
    'helmet': {
        'files': ['Powerups_016.png', 'Powerups_017.png', 'Powerups_018.png', 'Powerups_019.png'],
        'desc': 'Helmet pickup (+1 armor), 4 ping-pong animation frames',
    },
    'armor': {
        'files': ['Powerups_032.png', 'Powerups_033.png', 'Powerups_034.png'],
        'desc': 'Armor pickup (100 armor), 3 ping-pong animation frames',
    },
    'shells': {
        'files': ['Powerups_020.png'],
        'desc': 'Shotgun shells box (+20 shells), 1 static frame',
    },
}


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


def convert_pickup_frame(img_path):
    """Load PNG, scale to 32x24, convert to VB palette, return u32 array."""
    img = Image.open(img_path).convert('RGBA')
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

    if max_x < min_x:
        return [0] * U32_PER_FRAME

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

    # Slice into tiles
    all_u32 = []
    for tr in range(TILES_Y):
        for tc in range(TILES_X):
            tile = []
            for y in range(TILE_SIZE):
                row = []
                for x in range(TILE_SIZE):
                    row.append(canvas[tr * TILE_SIZE + y][tc * TILE_SIZE + x])
                tile.append(row)
            all_u32.extend(bytes_to_u32(tile_to_vb_2bpp(tile)))

    return all_u32, canvas


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    os.makedirs(PREVIEW_DIR, exist_ok=True)

    for pickup_name, info in PICKUPS.items():
        frames_data = []
        for fname in info['files']:
            path = os.path.join(POWERUPS_DIR, fname)
            if not os.path.exists(path):
                print(f"WARNING: {path} not found, skipping")
                continue
            u32_data, canvas = convert_pickup_frame(path)
            frames_data.append((fname, u32_data))

            # Save preview
            prev = Image.new('RGBA', (PX_W * 4, PX_H * 4), (0, 0, 0, 255))
            pp = prev.load()
            for y in range(PX_H):
                for x in range(PX_W):
                    c = VB_PAL[canvas[y][x]]
                    for sy in range(4):
                        for sx in range(4):
                            pp[x*4+sx, y*4+sy] = c
            preview_name = f"pickup_{pickup_name}_{fname.replace('.png','')}.png"
            prev.save(os.path.join(PREVIEW_DIR, preview_name))

        if not frames_data:
            continue

        frame_count = len(frames_data)
        print(f"{pickup_name}: {frame_count} frames, {info['desc']}")

        # Write C file
        c_path = os.path.join(OUTPUT_DIR, f"pickup_{pickup_name}.c")
        with open(c_path, 'w') as f:
            f.write(f"/* {pickup_name.capitalize()} pickup sprite -- auto-generated */\n\n")
            f.write(f'#include "pickup_{pickup_name}.h"\n\n')
            for i, (fname, u32_data) in enumerate(frames_data):
                arr_name = f"pickup_{pickup_name}_{i:03d}Tiles"
                f.write(f"const unsigned int {arr_name}[{len(u32_data)}]"
                        f" __attribute__((aligned(4))) =\n{{\n")
                for j in range(0, len(u32_data), 8):
                    chunk = u32_data[j:j+8]
                    s = ",".join(f"0x{w:08X}" for w in chunk)
                    comma = "," if j + 8 < len(u32_data) else ""
                    f.write(f"\t{s}{comma}\n")
                f.write("};\n\n")
        print(f"  Wrote {c_path}")

        # Write header
        h_path = os.path.join(OUTPUT_DIR, f"pickup_{pickup_name}.h")
        guard = f"__PICKUP_{pickup_name.upper()}_H__"
        with open(h_path, 'w') as f:
            f.write(f"#ifndef {guard}\n#define {guard}\n\n")
            f.write(f"/* {info['desc']} */\n")
            f.write(f"/* {PX_W}x{PX_H} px, {TILES_X}x{TILES_Y} tiles, {U32_PER_FRAME} u32 words per frame */\n\n")
            f.write(f"#define PICKUP_{pickup_name.upper()}_FRAMES {frame_count}\n")
            f.write(f"#define PICKUP_{pickup_name.upper()}_BYTES {U32_PER_FRAME * 4}\n\n")

            for i in range(frame_count):
                arr_name = f"pickup_{pickup_name}_{i:03d}Tiles"
                f.write(f"extern const unsigned int {arr_name}[{U32_PER_FRAME}];\n")

            f.write(f"\nstatic const unsigned int* const PICKUP_{pickup_name.upper()}_FRAME_TABLE[{frame_count}] = {{\n")
            for i in range(frame_count):
                arr_name = f"pickup_{pickup_name}_{i:03d}Tiles"
                comma = "," if i < frame_count - 1 else ""
                f.write(f"    {arr_name}{comma}\n")
            f.write("};\n\n")

            f.write(f"#endif\n")
        print(f"  Wrote {h_path}")

    print("\nDone!")


if __name__ == "__main__":
    main()
