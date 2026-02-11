"""
prepare_title_screen.py

Convert wolf_title_vb.png to VB 2bpp tile + map C arrays for the title screen.

Handles palette remapping (sorts by brightness to match VB hardware palette),
tile deduplication with H/V/HV flip reduction, and outputs title_screenTiles[]
and title_screenMap[] arrays compatible with the existing titleScreen.c code.

Output: src/vbdoom/assets/images/title_screen.c
"""

import os
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
INPUT_PNG = os.path.join(SCRIPT_DIR, "wolf_title_vb.png")
OUTPUT_C = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images", "title_screen.c")

TILE_W = 8
TILE_H = 8
MAP_W = 40   # tiles across (320 / 8)
MAP_H = 26   # tiles down  (208 / 8)

# BGMap entry flip bits (VB hardware)
HFLIP_BIT = 1 << 13
VFLIP_BIT = 1 << 12


def build_palette_remap(img):
    """Build a remap table: PNG palette index -> VB brightness level (0-3).

    Sorts the palette entries by red channel brightness so index 0 = black,
    1 = darkest, 2 = medium, 3 = brightest -- matching VB GPLT0 = 0xE4.
    """
    pal = img.getpalette()
    if pal is None:
        raise ValueError("Image must be indexed (palette mode P)")

    used_indices = sorted(set(img.getdata()))

    idx_brightness = []
    for idx in used_indices:
        r = pal[idx * 3]
        idx_brightness.append((idx, r))

    # Sort by brightness (red channel), assign VB levels 0..N
    idx_brightness.sort(key=lambda x: x[1])
    remap = {}
    for vb_level, (png_idx, _) in enumerate(idx_brightness):
        remap[png_idx] = vb_level

    print(f"  Palette remap: { {k: v for k, v in sorted(remap.items())} }")
    return remap


def extract_tile_pixels(img, tx, ty, remap):
    """Return 64-element tuple of remapped VB pixel values for tile (tx, ty)."""
    pixels = []
    x0, y0 = tx * TILE_W, ty * TILE_H
    for py in range(TILE_H):
        for px in range(TILE_W):
            val = img.getpixel((x0 + px, y0 + py))
            pixels.append(remap.get(val, 0))
    return tuple(pixels)


def flip_h(pixels):
    """Horizontally flip an 8x8 tile (mirror left-right)."""
    out = []
    for row in range(TILE_H):
        for col in range(TILE_W - 1, -1, -1):
            out.append(pixels[row * TILE_W + col])
    return tuple(out)


def flip_v(pixels):
    """Vertically flip an 8x8 tile (mirror top-bottom)."""
    out = []
    for row in range(TILE_H - 1, -1, -1):
        for col in range(TILE_W):
            out.append(pixels[row * TILE_W + col])
    return tuple(out)


def encode_tile_words(pixels):
    """Convert 64-pixel tuple to 4 VB 2bpp u32 words.

    VB sequential 2bpp format: each row is a u16 (little-endian) where
    pixel 0 occupies bits [1:0], pixel 1 bits [3:2], ..., pixel 7 bits [15:14].

    Each u32 word packs two consecutive rows:
      word = u16_row_even | (u16_row_odd << 16)
    """
    words = []
    for pair in range(4):
        row_a = pair * 2
        row_b = pair * 2 + 1

        u16_a = 0
        u16_b = 0
        for x in range(TILE_W):
            u16_a |= (pixels[row_a * TILE_W + x] << (x * 2))
            u16_b |= (pixels[row_b * TILE_W + x] << (x * 2))

        word = u16_a | (u16_b << 16)
        words.append(word)
    return tuple(words)


def convert():
    print(f"Reading {INPUT_PNG} ...")
    img = Image.open(INPUT_PNG)
    w, h = img.size
    assert (w, h) == (MAP_W * TILE_W, MAP_H * TILE_H), \
        f"Expected {MAP_W*TILE_W}x{MAP_H*TILE_H}, got {w}x{h}"

    remap = build_palette_remap(img)

    # ----- build tile bank with flip-reduction -----
    tile_bank = []          # list of encoded word-tuples (4 u32 each)
    tile_lookup = {}        # pixel_tuple -> tile_index
    map_entries = []        # one u16 per map cell

    # Force tile 0 to be all-zeros (blank/transparent).
    # BGMap entries cleared to 0x0000 reference tile 0, so it MUST be blank
    # for the skull overlay worlds to not show garbage in unused cells.
    blank_pixels = tuple([0] * 64)
    tile_bank.append(encode_tile_words(blank_pixels))
    tile_lookup[blank_pixels] = 0

    for ty in range(MAP_H):
        for tx in range(MAP_W):
            pixels = extract_tile_pixels(img, tx, ty, remap)

            found = False
            # Try the tile as-is, then each flipped variant.
            # If flip_h(pixels) is already in the bank, the hardware H-flip
            # will reconstruct the original pixels on screen.
            for variant, flip_bits in [
                (pixels,                        0),
                (flip_h(pixels),                HFLIP_BIT),
                (flip_v(pixels),                VFLIP_BIT),
                (flip_h(flip_v(pixels)),        HFLIP_BIT | VFLIP_BIT),
            ]:
                if variant in tile_lookup:
                    idx = tile_lookup[variant]
                    map_entries.append(idx | flip_bits)
                    found = True
                    break

            if not found:
                idx = len(tile_bank)
                tile_bank.append(encode_tile_words(pixels))
                tile_lookup[pixels] = idx
                map_entries.append(idx)

    num_tiles   = len(tile_bank)
    num_words   = num_tiles * 4
    tiles_bytes = num_tiles * 16
    map_count   = len(map_entries)
    map_bytes   = map_count * 2

    print(f"  Unique tiles : {num_tiles}  ({tiles_bytes} bytes)")
    print(f"  Map entries  : {map_count}  ({map_bytes} bytes)")
    print(f"  Total        : {tiles_bytes + map_bytes} bytes")

    # ----- write C source -----
    with open(OUTPUT_C, "w") as f:
        f.write(f"\n//{{{{BLOCK(title_screen)\n\n")
        f.write(f"//======================================================================\n")
        f.write(f"//\n")
        f.write(f"//\ttitle_screen, {w}x{h}@2, \n")
        f.write(f"//\t+ {num_tiles} tiles (t|f reduced) not compressed\n")
        f.write(f"//\t+ regular map (flat), not compressed, {MAP_W}x{MAP_H} \n")
        f.write(f"//\tTotal size: {tiles_bytes} + {map_bytes} = {tiles_bytes + map_bytes}\n")
        f.write(f"//\n")
        f.write(f"//\tGenerated by prepare_title_screen.py from wolf_title_vb.png\n")
        f.write(f"//\n")
        f.write(f"//======================================================================\n\n")

        f.write(f"const unsigned int title_screenTiles[{num_words}] __attribute__((aligned(4)))=\n{{\n")

        all_words = [w for tile in tile_bank for w in tile]
        for i, word in enumerate(all_words):
            if i % 8 == 0:
                f.write("\t")
            f.write(f"0x{word:08X}")
            if i < num_words - 1:
                f.write(",")
            if i % 8 == 7:
                f.write("\n")
        if num_words % 8 != 0:
            f.write("\n")

        f.write(f"}};\n\n")

        f.write(f"const unsigned short title_screenMap[{map_count}] __attribute__((aligned(4)))=\n{{\n")

        for i, entry in enumerate(map_entries):
            if i % 8 == 0:
                f.write("\t")
            f.write(f"0x{entry:04X}")
            if i < map_count - 1:
                f.write(",")
            if i % 8 == 7:
                f.write("\n")
        if map_count % 8 != 0:
            f.write("\n")

        f.write(f"}};\n\n")
        f.write(f"//}}}}BLOCK(title_screen)\n")

    print(f"\nWritten to {OUTPUT_C}")
    return tiles_bytes


if __name__ == "__main__":
    tiles_bytes = convert()
    print(f"\n>>> Update titleScreen.c copymem size to: {tiles_bytes}")
