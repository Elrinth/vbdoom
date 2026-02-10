"""
extract_doom_weapons.py

Extract all weapon sprites from doom_weapons.png:
  1. Detect individual sprites on magenta background
  2. Save each as a separate PNG file
  3. Convert each to VB 4-color (2bpp) tile data and save as C arrays

Output:
  doom_graphics_unvirtualboyed/weapon_sprites/
    sprite_00.png .. sprite_NN.png    (original color PNGs)
    sprite_00_vb.png .. sprite_NN_vb.png  (VB palette preview PNGs)
    sprite_manifest.txt               (list of all sprites with dimensions)

  src/vbdoom/assets/images/weapons/
    weapon_sprite_00.c/.h  (VB-ready C arrays per sprite)
    ...

The sprites are NOT grouped by weapon automatically since the layout
varies between source images. The manifest file helps the user identify
and organize them.
"""

import os
import sys
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
INPUT_IMG = os.path.join(SCRIPT_DIR, "more_doom_gfx", "doom_weapons.png")

PNG_OUT_DIR = os.path.join(SCRIPT_DIR, "doom_graphics_unvirtualboyed", "weapon_sprites")
C_OUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images", "weapons")

TILE_SIZE = 8
MAGENTA_THRESHOLD = 30

VB_PALETTE = [
    (0,   0,   0),   # 0: Black (transparent)
    (85,  0,   0),   # 1: Dark red
    (164, 0,   0),   # 2: Medium red
    (239, 0,   0),   # 3: Bright red
]


def is_magenta(r, g, b):
    return r > 220 and g < MAGENTA_THRESHOLD and b > 220


def luminance(r, g, b):
    return int(0.299 * r + 0.587 * g + 0.114 * b)


def map_to_vb_index(gray):
    if gray < 85:
        return 1
    elif gray < 170:
        return 2
    else:
        return 3


# ---------------------------------------------------------------------------
# Sprite detection
# ---------------------------------------------------------------------------

def find_sprites(img):
    """Find all sprite bounding boxes in the image."""
    w, h = img.size
    pixels = img.load()

    mask = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            pix = pixels[x, y]
            r, g, b = pix[0], pix[1], pix[2]
            a = pix[3] if len(pix) > 3 else 255
            if not is_magenta(r, g, b) and a > 128:
                mask[y][x] = True

    row_has = [any(mask[y]) for y in range(h)]
    bands = []
    in_band = False
    for y in range(h):
        if row_has[y] and not in_band:
            start = y
            in_band = True
        elif not row_has[y] and in_band:
            bands.append((start, y - 1))
            in_band = False
    if in_band:
        bands.append((start, h - 1))

    sprites = []
    for band_top, band_bottom in bands:
        col_has = [False] * w
        for x in range(w):
            for y in range(band_top, band_bottom + 1):
                if mask[y][x]:
                    col_has[x] = True
                    break
        in_s = False
        for x in range(w):
            if col_has[x] and not in_s:
                sl = x
                in_s = True
            elif not col_has[x] and in_s:
                sprites.append((sl, band_top, x - 1, band_bottom))
                in_s = False
        if in_s:
            sprites.append((sl, band_top, w - 1, band_bottom))

    # Tighten bounding boxes vertically
    tightened = []
    for (l, t, r, b) in sprites:
        top_tight = b
        bot_tight = t
        for y in range(t, b + 1):
            for x in range(l, r + 1):
                if mask[y][x]:
                    if y < top_tight:
                        top_tight = y
                    if y > bot_tight:
                        bot_tight = y
                    break
        tightened.append((l, top_tight, r, bot_tight))

    return tightened


# ---------------------------------------------------------------------------
# VB tile encoding (same as prepare_wall_textures.py)
# ---------------------------------------------------------------------------

def tile_to_vb_2bpp(tile):
    data = []
    for y in range(TILE_SIZE):
        lo_byte = 0
        hi_byte = 0
        for x in range(TILE_SIZE):
            idx = tile[y][x]
            bit0 = (idx >> 0) & 1
            bit1 = (idx >> 1) & 1
            lo_byte |= (bit0 << (7 - x))
            hi_byte |= (bit1 << (7 - x))
        data.append(lo_byte)
        data.append(hi_byte)
    return data


def bytes_to_u32_array(data):
    words = []
    for i in range(0, len(data), 4):
        w = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24)
        words.append(w)
    return words


# ---------------------------------------------------------------------------
# Conversion and output
# ---------------------------------------------------------------------------

def extract_sprite_canvas(img, bbox):
    """Extract sprite pixels as VB index array, pad to tile-aligned size."""
    l, t, r, b = bbox
    sw = r - l + 1
    sh = b - t + 1

    # Pad to tile-aligned dimensions
    tw = ((sw + TILE_SIZE - 1) // TILE_SIZE) * TILE_SIZE
    th = ((sh + TILE_SIZE - 1) // TILE_SIZE) * TILE_SIZE

    canvas = [[0] * tw for _ in range(th)]
    pixels = img.load()

    for sy in range(sh):
        for sx in range(sw):
            pix = pixels[l + sx, t + sy]
            r2, g2, b2 = pix[0], pix[1], pix[2]
            a = pix[3] if len(pix) > 3 else 255
            if is_magenta(r2, g2, b2) or a < 128:
                canvas[sy][sx] = 0
            else:
                gray = luminance(r2, g2, b2)
                canvas[sy][sx] = map_to_vb_index(gray)

    return canvas, tw, th


def canvas_to_tiles(canvas, tw, th):
    """Slice canvas into 8x8 tiles, return tiles and map with deduplication."""
    cols = tw // TILE_SIZE
    rows = th // TILE_SIZE

    unique_tiles = {}
    unique_tile_data = []
    tile_map = []

    for ty in range(rows):
        for tx in range(cols):
            tile = []
            for y in range(TILE_SIZE):
                row = []
                for x in range(TILE_SIZE):
                    row.append(canvas[ty * TILE_SIZE + y][tx * TILE_SIZE + x])
                tile.append(row)

            key = tuple(tuple(r) for r in tile)
            if key in unique_tiles:
                tile_idx = unique_tiles[key]
            else:
                tile_idx = len(unique_tile_data)
                unique_tiles[key] = tile_idx
                vb_bytes = tile_to_vb_2bpp(tile)
                unique_tile_data.append(bytes_to_u32_array(vb_bytes))

            tile_map.append(tile_idx)

    return unique_tile_data, tile_map, cols, rows


def save_vb_preview(canvas, tw, th, path):
    """Save a VB-palette preview PNG."""
    preview = Image.new('RGB', (tw, th))
    for y in range(th):
        for x in range(tw):
            preview.putpixel((x, y), VB_PALETTE[canvas[y][x]])
    preview.save(path)


def write_sprite_c(name, tile_data, tile_map, map_cols, map_rows, c_path, h_path):
    """Write VB-ready C arrays for one sprite."""
    num_tiles = len(tile_data)
    all_u32 = []
    for td in tile_data:
        all_u32.extend(td)

    with open(c_path, 'w') as f:
        f.write(f"/* {name} -- auto-generated by extract_doom_weapons.py */\n\n")
        f.write(f"const unsigned int {name}Tiles[{len(all_u32)}]"
                f" __attribute__((aligned(4))) =\n")
        f.write("{\n")
        for i in range(0, len(all_u32), 8):
            chunk = all_u32[i:i + 8]
            s = ",".join(f"0x{v:08X}" for v in chunk)
            comma = "," if i + 8 < len(all_u32) else ""
            f.write(f"\t{s}{comma}\n")
        f.write("};\n\n")

        f.write(f"const unsigned short {name}Map[{len(tile_map)}]"
                f" __attribute__((aligned(4))) =\n")
        f.write("{\n")
        for i in range(0, len(tile_map), map_cols):
            chunk = tile_map[i:i + map_cols]
            s = ",".join(f"0x{v:04X}" for v in chunk)
            comma = "," if i + map_cols < len(tile_map) else ""
            f.write(f"\t{s}{comma}\n")
        f.write("};\n")

    guard = name.upper()
    with open(h_path, 'w') as f:
        f.write(f"#ifndef __{guard}_H__\n")
        f.write(f"#define __{guard}_H__\n\n")
        f.write(f"#define {guard}_TILE_COUNT   {num_tiles}\n")
        f.write(f"#define {guard}_TILE_BYTES   {num_tiles * 16}\n")
        f.write(f"#define {guard}_MAP_COLS     {map_cols}\n")
        f.write(f"#define {guard}_MAP_ROWS     {map_rows}\n\n")
        f.write(f"extern const unsigned int {name}Tiles[{len(all_u32)}];\n")
        f.write(f"extern const unsigned short {name}Map[{len(tile_map)}];\n\n")
        f.write(f"#endif\n")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    img = Image.open(INPUT_IMG).convert('RGBA')
    w, h = img.size
    print(f"Loaded {INPUT_IMG}: {w}x{h}")

    os.makedirs(PNG_OUT_DIR, exist_ok=True)
    os.makedirs(C_OUT_DIR, exist_ok=True)

    bboxes = find_sprites(img)
    print(f"Found {len(bboxes)} sprites")

    manifest_lines = []
    manifest_lines.append(f"# Doom weapon sprite manifest ({len(bboxes)} sprites)")
    manifest_lines.append(f"# Generated from doom_weapons.png ({w}x{h})")
    manifest_lines.append("#")
    manifest_lines.append("# Format: index  bbox(l,t,r,b)  size  tiles_w x tiles_h  unique_tiles")
    manifest_lines.append("#")

    for si, bbox in enumerate(bboxes):
        l, t, r, b = bbox
        sw = r - l + 1
        sh = b - t + 1
        name = f"weaponSprite{si:02d}"

        # Crop the sprite from the original image
        crop = img.crop((l, t, r + 1, b + 1))
        png_path = os.path.join(PNG_OUT_DIR, f"sprite_{si:02d}.png")
        crop.save(png_path)

        # Convert to VB and save preview
        canvas, tw, th = extract_sprite_canvas(img, bbox)
        vb_path = os.path.join(PNG_OUT_DIR, f"sprite_{si:02d}_vb.png")
        save_vb_preview(canvas, tw, th, vb_path)

        # Generate tiles and C arrays
        tile_data, tile_map, map_cols, map_rows = canvas_to_tiles(canvas, tw, th)

        c_path = os.path.join(C_OUT_DIR, f"weapon_sprite_{si:02d}.c")
        h_path = os.path.join(C_OUT_DIR, f"weapon_sprite_{si:02d}.h")
        write_sprite_c(name, tile_data, tile_map, map_cols, map_rows, c_path, h_path)

        manifest_lines.append(
            f"  {si:2d}  ({l:4d},{t:3d})-({r:4d},{b:3d})  {sw:3d}x{sh:3d}  "
            f"{map_cols:2d}x{map_rows:2d} tiles  {len(tile_data):3d} unique"
        )
        print(f"  [{si:2d}] {sw:3d}x{sh:3d}  -> {map_cols}x{map_rows} tiles, "
              f"{len(tile_data)} unique  ({name})")

    # Write manifest
    manifest_path = os.path.join(PNG_OUT_DIR, "sprite_manifest.txt")
    with open(manifest_path, 'w') as f:
        f.write("\n".join(manifest_lines))
        f.write("\n")

    print(f"\nWrote {len(bboxes)} sprite PNGs to {PNG_OUT_DIR}")
    print(f"Wrote {len(bboxes)} VB C arrays to {C_OUT_DIR}")
    print(f"Manifest: {manifest_path}")


if __name__ == "__main__":
    main()
