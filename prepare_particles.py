"""
prepare_particles.py

Convert PUFF A-D particle sprites to VB format and create shotgun group variants.

Single puffs: variable size per frame:
  PuffA0: 8x8 (1x1 tile)
  PuffB0: 8x8 (1x1 tile)
  PuffC0: 16x16 (2x2 tiles)
  PuffD0: 16x16 (2x2 tiles)

Shotgun groups: 32x32 pixels (4x4 tiles), 4 variants x 4 frames

All tile data is deduplicated.

Output: particle_sprites.c and particle_sprites.h
"""

import os
import random
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARTICLE_DIR = os.path.join(SCRIPT_DIR, "particles")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images")
PREVIEW_DIR = os.path.join(SCRIPT_DIR, "particle_previews")

PARTICLE_CHAR_START = 764

TILE_SIZE = 8

# Per-frame puff sizes
PUFF_FRAMES = ["PUFFA0.png", "PUFFB0.png", "PUFFC0.png", "PUFFD0.png"]
PUFF_SIZES = [8, 8, 16, 16]  # target pixel size per frame

GROUP_SIZE = 32
GROUP_TILES_W = 4
GROUP_TILES_H = 4
GROUP_TILE_COUNT = GROUP_TILES_W * GROUP_TILES_H  # 16
NUM_GROUP_VARIANTS = 4

MAGENTA_THRESHOLD = 30

VB_PAL = {0: (0, 0, 0, 0), 1: (80, 0, 0, 255), 2: (170, 0, 0, 255), 3: (255, 0, 0, 255)}


def luminance(r, g, b):
    return int(0.299 * r + 0.587 * g + 0.114 * b)


def is_bg(r, g, b, a):
    if a < 128:
        return True
    if g > 200 and b > 200 and r < 40:
        return True
    if r > 220 and g < MAGENTA_THRESHOLD and b > 220:
        return True
    return False


def load_and_convert_puff(filepath, target_size):
    """Load a puff PNG, detect bbox, resize to target_size, convert to VB indices."""
    img = Image.open(filepath).convert('RGBA')
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
        return [[0] * target_size for _ in range(target_size)]

    crop = img.crop((min_x, min_y, max_x + 1, max_y + 1))
    cw, ch = crop.size

    scale = min(target_size / cw, target_size / ch)
    new_w = max(1, int(cw * scale))
    new_h = max(1, int(ch * scale))
    resized = crop.resize((new_w, new_h), Image.LANCZOS)

    canvas = [[0] * target_size for _ in range(target_size)]
    off_x = (target_size - new_w) // 2
    off_y = (target_size - new_h) // 2

    rpix = resized.load()
    for y in range(new_h):
        for x in range(new_w):
            r, g, b, a = rpix[x, y]
            dx = off_x + x
            dy = off_y + y
            if dx < target_size and dy < target_size:
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

    return canvas


def composite_group(puff_canvases, variant_idx, frame_idx):
    """Create a 32x32 shotgun group by compositing multiple puff copies.
    Uses the appropriately-sized puff for this frame."""
    random.seed(42 + variant_idx * 17 + frame_idx)

    group = [[0] * GROUP_SIZE for _ in range(GROUP_SIZE)]
    puff = puff_canvases[frame_idx]
    puff_h = len(puff)
    puff_w = len(puff[0]) if puff_h > 0 else 0

    num_puffs = 4 + (variant_idx % 2)
    for _ in range(num_puffs):
        ox = random.randint(-2, GROUP_SIZE - puff_w + 2)
        oy = random.randint(-2, GROUP_SIZE - puff_h + 2)

        for y in range(puff_h):
            for x in range(puff_w):
                dx = ox + x
                dy = oy + y
                if 0 <= dx < GROUP_SIZE and 0 <= dy < GROUP_SIZE:
                    val = puff[y][x]
                    if val > 0:
                        group[dy][dx] = max(group[dy][dx], val)

    return group


def slice_tiles(canvas, tw, th):
    tiles = []
    for tr in range(th):
        for tc in range(tw):
            tile = []
            for y in range(TILE_SIZE):
                row = []
                for x in range(TILE_SIZE):
                    py = tr * TILE_SIZE + y
                    px = tc * TILE_SIZE + x
                    if py < len(canvas) and px < len(canvas[0]):
                        row.append(canvas[py][px])
                    else:
                        row.append(0)
                tile.append(row)
            tiles.append(tile)
    return tiles


def tile_to_tuple(tile):
    return tuple(tuple(row) for row in tile)


def hflip_tile(tile):
    return [row[::-1] for row in tile]


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


def save_preview(canvas, path, scale=4):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    h = len(canvas)
    w = len(canvas[0])
    img = Image.new('RGBA', (w * scale, h * scale), (0, 0, 0, 255))
    pix = img.load()
    for y in range(h):
        for x in range(w):
            color = VB_PAL[canvas[y][x]]
            for sy in range(scale):
                for sx in range(scale):
                    pix[x * scale + sx, y * scale + sy] = color
    img.save(path)


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    os.makedirs(PREVIEW_DIR, exist_ok=True)

    # --- Load single puffs with per-frame sizes ---
    puff_canvases = []
    for fi, fname in enumerate(PUFF_FRAMES):
        path = os.path.join(PARTICLE_DIR, fname)
        target = PUFF_SIZES[fi]
        canvas = load_and_convert_puff(path, target)
        puff_canvases.append(canvas)
        save_preview(canvas, os.path.join(PREVIEW_DIR, f"puff_{fname[:-4]}.png"))
        print(f"Loaded {fname}: {target}x{target}")

    # --- Create shotgun group variants ---
    group_canvases = []
    for vi in range(NUM_GROUP_VARIANTS):
        frames = []
        for fi in range(4):
            group = composite_group(puff_canvases, vi, fi)
            frames.append(group)
            save_preview(group, os.path.join(PREVIEW_DIR,
                                             f"group_{vi}_frame{fi}.png"))
        group_canvases.append(frames)
        print(f"Generated shotgun group variant {vi}: 4 frames")

    # --- Deduplicate all tiles ---
    unique_tiles = {}
    unique_tile_data = []

    def add_tile(tile):
        key = tile_to_tuple(tile)
        flags = 0
        if key in unique_tiles:
            return unique_tiles[key], flags
        hkey = tile_to_tuple(hflip_tile(tile))
        if hkey in unique_tiles:
            return unique_tiles[hkey], 0x2000
        idx = len(unique_tile_data)
        unique_tiles[key] = idx
        unique_tile_data.append(bytes_to_u32(tile_to_vb_2bpp(tile)))
        return idx, flags

    # Single puff maps: variable tiles per frame
    # Frame 0,1: 1x1 = 1 tile; Frame 2,3: 2x2 = 4 tiles
    puff_maps = []
    puff_tile_counts = []
    for fi, canvas in enumerate(puff_canvases):
        sz = PUFF_SIZES[fi]
        tw = sz // TILE_SIZE
        th = sz // TILE_SIZE
        tile_count = tw * th
        puff_tile_counts.append(tile_count)
        tiles = slice_tiles(canvas, tw, th)
        frame_map = []
        for t in tiles:
            idx, flags = add_tile(t)
            char_idx = PARTICLE_CHAR_START + idx
            frame_map.append(char_idx | flags)
        puff_maps.append(frame_map)

    # Group maps: 4 variants x 4 frames x 16 tiles
    group_maps = []
    for vi in range(NUM_GROUP_VARIANTS):
        variant_maps = []
        for fi in range(4):
            canvas = group_canvases[vi][fi]
            tiles = slice_tiles(canvas, GROUP_TILES_W, GROUP_TILES_H)
            frame_map = []
            for t in tiles:
                idx, flags = add_tile(t)
                char_idx = PARTICLE_CHAR_START + idx
                frame_map.append(char_idx | flags)
            variant_maps.append(frame_map)
        group_maps.append(variant_maps)

    num_tiles = len(unique_tile_data)
    print(f"\nTotal unique particle tiles: {num_tiles}")
    print(f"Tile data: {num_tiles * 16} bytes")

    # Compute puff map offsets for variable-size frames
    # puffMap layout: [frame0 tiles...][frame1 tiles...][frame2 tiles...][frame3 tiles...]
    puff_offsets = []
    off = 0
    for tc in puff_tile_counts:
        puff_offsets.append(off)
        off += tc
    total_puff_entries = off

    # --- Write C file ---
    c_path = os.path.join(OUTPUT_DIR, "particle_sprites.c")
    with open(c_path, 'w') as f:
        f.write("/* Particle sprite tiles -- auto-generated by prepare_particles.py */\n\n")

        # Tile data
        all_u32 = []
        for td in unique_tile_data:
            all_u32.extend(td)
        f.write(f"/* {num_tiles} unique particle tiles */\n")
        f.write(f"const unsigned int particleTiles[{len(all_u32)}]"
                f" __attribute__((aligned(4))) =\n{{\n")
        for i in range(0, len(all_u32), 8):
            chunk = all_u32[i:i+8]
            s = ",".join(f"0x{w:08X}" for w in chunk)
            comma = "," if i + 8 < len(all_u32) else ""
            f.write(f"\t{s}{comma}\n")
        f.write("};\n\n")

        # Single puff maps (variable-size frames concatenated)
        all_puff = []
        for fm in puff_maps:
            all_puff.extend(fm)
        f.write(f"/* Single puff maps: {total_puff_entries} entries "
                f"(frames: {','.join(str(tc) for tc in puff_tile_counts)} tiles) */\n")
        f.write(f"const unsigned short puffMap[{len(all_puff)}]"
                f" __attribute__((aligned(4))) =\n{{\n")
        # Write frame by frame with comments
        for fi in range(4):
            start = puff_offsets[fi]
            count = puff_tile_counts[fi]
            chunk = all_puff[start:start + count]
            s = ",".join(f"0x{v:04X}" for v in chunk)
            comma = "," if fi < 3 else ""
            f.write(f"\t{s}{comma}  /* frame {fi}: {PUFF_SIZES[fi]}x{PUFF_SIZES[fi]} */\n")
        f.write("};\n\n")

        # Shotgun group maps (unchanged structure)
        all_group = []
        for vm in group_maps:
            for fm in vm:
                all_group.extend(fm)
        f.write(f"/* Shotgun group maps: {NUM_GROUP_VARIANTS} variants x 4 frames x {GROUP_TILE_COUNT} tiles */\n")
        f.write(f"const unsigned short shotgunGroupMap[{len(all_group)}]"
                f" __attribute__((aligned(4))) =\n{{\n")
        for i in range(0, len(all_group), GROUP_TILE_COUNT):
            chunk = all_group[i:i+GROUP_TILE_COUNT]
            s = ",".join(f"0x{v:04X}" for v in chunk)
            comma = "," if i + GROUP_TILE_COUNT < len(all_group) else ""
            f.write(f"\t{s}{comma}\n")
        f.write("};\n")

    print(f"\nWrote {c_path}")

    # --- Write H file ---
    h_path = os.path.join(OUTPUT_DIR, "particle_sprites.h")
    with open(h_path, 'w') as f:
        f.write("#ifndef __PARTICLE_SPRITES_H__\n#define __PARTICLE_SPRITES_H__\n\n")
        f.write(f"#define PARTICLE_CHAR_START   {PARTICLE_CHAR_START}\n")
        f.write(f"#define PARTICLE_TILE_COUNT   {num_tiles}\n")
        f.write(f"#define PARTICLE_TILE_BYTES   {num_tiles * 16}\n\n")

        f.write("/* Per-frame puff sizes (pixels) and tile counts */\n")
        for fi in range(4):
            sz = PUFF_SIZES[fi]
            tw = sz // TILE_SIZE
            f.write(f"#define PUFF_FRAME{fi}_SIZE    {sz}\n")
            f.write(f"#define PUFF_FRAME{fi}_TILES_W {tw}\n")
            f.write(f"#define PUFF_FRAME{fi}_TILES   {tw * tw}\n")
        f.write(f"\n/* Puff map offsets (index into puffMap[]) */\n")
        for fi in range(4):
            f.write(f"#define PUFF_OFFSET{fi}         {puff_offsets[fi]}\n")
        f.write(f"#define PUFF_FRAME_COUNT      4\n\n")

        f.write(f"/* Shotgun group: {GROUP_SIZE}x{GROUP_SIZE} px = {GROUP_TILES_W}x{GROUP_TILES_H} tiles */\n")
        f.write(f"#define GROUP_TILES_W         {GROUP_TILES_W}\n")
        f.write(f"#define GROUP_TILES_H         {GROUP_TILES_H}\n")
        f.write(f"#define GROUP_FRAME_TILES     {GROUP_TILE_COUNT}\n")
        f.write(f"#define GROUP_VARIANT_COUNT   {NUM_GROUP_VARIANTS}\n")
        f.write(f"#define GROUP_FRAME_COUNT     4\n\n")

        f.write(f"extern const unsigned int particleTiles[{len(all_u32)}];\n")
        f.write(f"extern const unsigned short puffMap[{total_puff_entries}];\n")
        f.write(f"extern const unsigned short shotgunGroupMap[{len(all_group)}];\n\n")
        f.write("#endif\n")

    print(f"Wrote {h_path}")
    print(f"\nVRAM: PARTICLE_CHAR_START={PARTICLE_CHAR_START}, tiles={num_tiles}, "
          f"last char={PARTICLE_CHAR_START + num_tiles - 1}")


if __name__ == "__main__":
    main()
