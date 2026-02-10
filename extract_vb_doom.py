"""
extract_vb_doom.py

Parse vb_doom.c arrays, extract graphics into separate C files:
  - face_sprites.c/h      (3 face expressions, 3x4 tiles each)
  - fist_sprites.c/h       (4 fist frames)
  - pistol_sprites.c/h     (pistol red + black, 2 frames each)
  - vb_doom.c              (HUD-only: UI bar, numbers, transitions)

Also computes the new VRAM layout and prints updated CHAR_START values.
"""

import re, os, sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
VB_DOOM_C = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images", "vb_doom.c")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images")

MAP_COLS = 48
BYTES_PER_ROW = MAP_COLS * 2  # 96


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

def parse_hex_array(filepath, array_name):
    """Parse a C hex array, return list of ints."""
    with open(filepath, 'r') as f:
        text = f.read()
    pattern = rf'{re.escape(array_name)}\[\d+\].*?=\s*\{{(.*?)\}};'
    m = re.search(pattern, text, re.DOTALL)
    if not m:
        raise ValueError(f"Array '{array_name}' not found in {filepath}")
    return [int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]+', m.group(1))]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def char_of(entry):
    """Char index from a BGMap u16 entry (bits 0-10)."""
    return entry & 0x07FF


def remap(entry, new_char):
    """Replace char bits of a BGMap u16 entry, keep flags/palette."""
    return (entry & 0xF800) | (new_char & 0x07FF)


def tile_words(tiles_u32, char_idx):
    """Return the 4 u32 words for one 8x8 tile."""
    o = char_idx * 4
    return tiles_u32[o:o + 4]


def collect_chars(map_u16, regions):
    """Collect unique non-zero char indices from (byte_off, nbytes) regions."""
    chars = set()
    for boff, nbytes in regions:
        for i in range(0, nbytes, 2):
            idx = (boff + i) // 2
            if idx < len(map_u16):
                c = char_of(map_u16[idx])
                if c != 0:
                    chars.add(c)
    return chars


def collect_entries(map_u16, regions):
    """Collect map entries in order from (byte_off, nbytes) regions."""
    entries = []
    for boff, nbytes in regions:
        for i in range(0, nbytes, 2):
            idx = (boff + i) // 2
            entries.append(map_u16[idx] if idx < len(map_u16) else 0)
    return entries


# ---------------------------------------------------------------------------
# C code generation
# ---------------------------------------------------------------------------

def fmt_tiles(name, data_u32, comment=""):
    """Generate C tile array."""
    lines = []
    if comment:
        lines.append(f"/* {comment} */")
    lines.append(f"const unsigned int {name}[{len(data_u32)}]"
                 f" __attribute__((aligned(4))) =")
    lines.append("{")
    for i in range(0, len(data_u32), 8):
        chunk = data_u32[i:i + 8]
        s = ",".join(f"0x{v:08X}" for v in chunk)
        comma = "," if i + 8 < len(data_u32) else ""
        lines.append(f"\t{s}{comma}")
    lines.append("};")
    return "\n".join(lines)


def fmt_map(name, data_u16, comment=""):
    """Generate C map array."""
    lines = []
    if comment:
        lines.append(f"/* {comment} */")
    lines.append(f"const unsigned short {name}[{len(data_u16)}]"
                 f" __attribute__((aligned(4))) =")
    lines.append("{")
    for i in range(0, len(data_u16), 8):
        chunk = data_u16[i:i + 8]
        s = ",".join(f"0x{v:04X}" for v in chunk)
        comma = "," if i + 8 < len(data_u16) else ""
        lines.append(f"\t{s}{comma}")
    lines.append("};")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    tiles_u32 = parse_hex_array(VB_DOOM_C, "vb_doomTiles")
    map_u16 = parse_hex_array(VB_DOOM_C, "vb_doomMap")
    num_tiles = len(tiles_u32) // 4
    print(f"Parsed {num_tiles} tiles, {len(map_u16)} map entries")

    # -----------------------------------------------------------------------
    # 1. Define regions  (byte_offset, num_bytes) into vb_doomMap
    # -----------------------------------------------------------------------

    # -- HUD --
    hud_regions = []
    for r in range(4):                               # UI bar rows 0-3
        hud_regions.append((r * 96, 96))
    hud_regions.append((4 * 96, 2))                  # black tile (row 4 col 0)
    hud_regions.append((16 * 96, 46))                # large nums row 16
    hud_regions.append((17 * 96, 46))                # large nums row 17
    hud_regions.append((18 * 96, 22))                # small nums row 18
    for r in range(20, 28):                          # transition tiles rows 20-27
        hud_regions.append((r * 96 + 31 * 2, 8))    #   cols 31-34
    hud_regions.append((25 * 96 + 39 * 2, 4))       # wall tiles row 25 cols 39-40

    # -- Faces (3 faces × 4 rows × 3 entries) --
    face_regions = []
    for face_id in range(3):
        wf = face_id * 384
        for rb in [400, 496, 592, 688]:
            face_regions.append((rb + wf, 6))

    # -- Fists (4 frames) --
    fist_fdefs = [
        (96 * 22,       30, 6),   # frame 0  idle
        (96 * 22 + 30,  22, 6),   # frame 1  attack 1
        (96 * 13 + 48,  28, 7),   # frame 2  attack 2
        (96 * 28,       34, 8),   # frame 3  attack 3
    ]
    fist_per_frame = []
    for sp, xt, nr in fist_fdefs:
        fist_per_frame.append([(sp + r * 96, xt) for r in range(nr)])

    # -- Pistol red (2 frames) --
    pist_fdefs = [
        (96 * 9,        16, 7),   # idle
        (96 * 5 + 22,   16, 11),  # shooting
    ]
    pist_per_frame = []
    for sp, xt, nr in pist_fdefs:
        pist_per_frame.append([(sp + r * 96, xt) for r in range(nr)])

    # -- Pistol black (2 frames) --
    pistb_fdefs = [
        (96 * 28 + 34,  16, 8),   # idle black
        (96 * 28 + 50,  16, 8),   # shooting black
    ]
    pistb_per_frame = []
    for sp, xt, nr in pistb_fdefs:
        pistb_per_frame.append([(sp + r * 96, xt) for r in range(nr)])

    # -----------------------------------------------------------------------
    # 2. Collect unique chars per group
    # -----------------------------------------------------------------------
    hud_chars = collect_chars(map_u16, hud_regions) | {0}

    face_chars = collect_chars(map_u16, face_regions)

    fist_chars = set()
    for fr in fist_per_frame:
        fist_chars |= collect_chars(map_u16, fr)

    pist_chars = set()
    for fr in pist_per_frame:
        pist_chars |= collect_chars(map_u16, fr)

    pistb_chars = set()
    for fr in pistb_per_frame:
        pistb_chars |= collect_chars(map_u16, fr)

    pistol_all = pist_chars | pistb_chars
    weapon_max = max(len(fist_chars), len(pistol_all))

    H = len(hud_chars)
    F = len(face_chars)
    W = weapon_max

    FACE_CS = H
    WEAP_CS = H + F
    ZOMBIE_CS = WEAP_CS + W
    PICKUP_CS = ZOMBIE_CS + 3 * 64        # 192
    WALL_CS   = PICKUP_CS + 3 * 12        # 36

    print(f"\nChars per group:  HUD={H}  Face={F}  Weapon={W}"
          f"  (fists={len(fist_chars)}, pistol={len(pistol_all)})")
    print(f"\nNew VRAM layout:")
    print(f"  HUD              0 - {H-1}")
    print(f"  Faces          {FACE_CS} - {FACE_CS+F-1}")
    print(f"  Weapon         {WEAP_CS} - {WEAP_CS+W-1}")
    print(f"  Enemies        {ZOMBIE_CS} - {ZOMBIE_CS+191}")
    print(f"  Pickups        {PICKUP_CS} - {PICKUP_CS+35}")
    print(f"  Wall textures  {WALL_CS} - {WALL_CS+511}")
    after_walls = WALL_CS + 512
    print(f"  After walls    {after_walls}  (free: {2048-after_walls})")

    # -----------------------------------------------------------------------
    # 3. Build remappings   old_char -> new_char
    # -----------------------------------------------------------------------
    hud_sorted = sorted(hud_chars)
    hud_rm = {old: new for new, old in enumerate(hud_sorted)}

    face_sorted = sorted(face_chars)
    face_rm = {0: 0}
    for i, c in enumerate(face_sorted):
        face_rm[c] = FACE_CS + i

    fist_sorted = sorted(fist_chars)
    fist_rm = {0: 0}
    for i, c in enumerate(fist_sorted):
        fist_rm[c] = WEAP_CS + i

    pistol_sorted = sorted(pistol_all)
    pistol_rm = {0: 0}
    for i, c in enumerate(pistol_sorted):
        pistol_rm[c] = WEAP_CS + i

    # -----------------------------------------------------------------------
    # 4. Generate HUD-only vb_doom.c
    # -----------------------------------------------------------------------
    hud_tile_data = []
    for c in hud_sorted:
        hud_tile_data.extend(tile_words(tiles_u32, c))

    hud_map_out = [0] * len(map_u16)
    for boff, nbytes in hud_regions:
        for i in range(0, nbytes, 2):
            idx = (boff + i) // 2
            if idx < len(map_u16):
                e = map_u16[idx]
                c = char_of(e)
                if c in hud_rm:
                    hud_map_out[idx] = remap(e, hud_rm[c])

    hud_c_path = os.path.join(OUTPUT_DIR, "vb_doom.c")
    with open(hud_c_path, 'w') as f:
        f.write("/*\n")
        f.write(" * vb_doom -- HUD-only tileset (auto-generated by extract_vb_doom.py)\n")
        f.write(f" * {H} tiles, {H*16} bytes tile data\n")
        f.write(f" * {len(hud_map_out)} map entries (48x46), non-HUD entries zeroed\n")
        f.write(" */\n\n")
        f.write(fmt_tiles("vb_doomTiles", hud_tile_data,
                          f"{H} HUD tiles"))
        f.write("\n\n")
        f.write(fmt_map("vb_doomMap", hud_map_out,
                        "48x46 map -- HUD entries only"))
        f.write("\n")
    print(f"\nWrote {hud_c_path}  ({H} tiles, {H*16} bytes)")

    # -----------------------------------------------------------------------
    # 5. Generate face_sprites.c/h
    # -----------------------------------------------------------------------
    face_tile_data = []
    for c in face_sorted:
        face_tile_data.extend(tile_words(tiles_u32, c))

    # Compact face map: 3 faces × 4 rows × 3 entries = 36 entries
    face_map_out = []
    for face_id in range(3):
        wf = face_id * 384
        for rb in [400, 496, 592, 688]:
            for i in range(0, 6, 2):
                idx = (rb + wf + i) // 2
                e = map_u16[idx]
                c = char_of(e)
                face_map_out.append(remap(e, face_rm.get(c, 0)) if c in face_rm else 0)

    face_c = os.path.join(OUTPUT_DIR, "face_sprites.c")
    with open(face_c, 'w') as f:
        f.write("/* Face sprite tiles -- auto-generated by extract_vb_doom.py */\n\n")
        f.write(fmt_tiles("faceTiles", face_tile_data, f"{F} face tiles"))
        f.write("\n\n")
        f.write(fmt_map("faceMap", face_map_out,
                        "3 faces x 4 rows x 3 cols = 36 entries"))
        f.write("\n")

    face_h = os.path.join(OUTPUT_DIR, "face_sprites.h")
    with open(face_h, 'w') as f:
        f.write("#ifndef __FACE_SPRITES_H__\n#define __FACE_SPRITES_H__\n\n")
        f.write(f"#define FACE_CHAR_START   {FACE_CS}\n")
        f.write(f"#define FACE_TILE_COUNT   {F}\n")
        f.write(f"#define FACE_TILE_BYTES   {F*16}\n")
        f.write(f"#define FACE_MAP_ENTRIES  36  /* 3 faces x 12 tiles */\n\n")
        f.write(f"extern const unsigned int  faceTiles[{len(face_tile_data)}];\n")
        f.write(f"extern const unsigned short faceMap[{len(face_map_out)}];\n\n")
        f.write("#endif\n")
    print(f"Wrote {face_c}  ({F} tiles)")

    # -----------------------------------------------------------------------
    # 6. Generate fist_sprites.c/h
    # -----------------------------------------------------------------------
    fist_tile_data = []
    for c in fist_sorted:
        fist_tile_data.extend(tile_words(tiles_u32, c))

    fist_frame_maps = []
    fist_frame_sizes = []  # (width_entries, rows)
    for fi, (sp, xt, nr) in enumerate(fist_fdefs):
        w_entries = xt // 2
        fmap = []
        for r in range(nr):
            for i in range(0, xt, 2):
                idx = (sp + r * 96 + i) // 2
                e = map_u16[idx]
                c = char_of(e)
                fmap.append(remap(e, fist_rm.get(c, 0)) if c in fist_rm else 0)
        fist_frame_maps.append(fmap)
        fist_frame_sizes.append((w_entries, nr))

    fist_c = os.path.join(OUTPUT_DIR, "fist_sprites.c")
    with open(fist_c, 'w') as f:
        f.write("/* Fist sprite tiles -- auto-generated by extract_vb_doom.py */\n\n")
        f.write(fmt_tiles("fistTiles", fist_tile_data,
                          f"{len(fist_sorted)} fist tiles"))
        for fi, fm in enumerate(fist_frame_maps):
            f.write("\n\n")
            we, nr = fist_frame_sizes[fi]
            f.write(fmt_map(f"fistFrame{fi}Map", fm,
                            f"frame {fi}: {we} cols x {nr} rows"))
        f.write("\n")

    fist_h = os.path.join(OUTPUT_DIR, "fist_sprites.h")
    with open(fist_h, 'w') as f:
        f.write("#ifndef __FIST_SPRITES_H__\n#define __FIST_SPRITES_H__\n\n")
        f.write(f"#define FIST_TILE_COUNT   {len(fist_sorted)}\n")
        f.write(f"#define FIST_TILE_BYTES   {len(fist_sorted)*16}\n\n")
        f.write(f"extern const unsigned int fistTiles[{len(fist_tile_data)}];\n\n")
        f.write(f"#define FIST_FRAME_COUNT  4\n")
        for fi, (we, nr) in enumerate(fist_frame_sizes):
            xt = we * 2
            f.write(f"#define FIST_F{fi}_XTILES  {xt}\n")
            f.write(f"#define FIST_F{fi}_ROWS    {nr}\n")
            f.write(f"extern const unsigned short fistFrame{fi}Map[{len(fist_frame_maps[fi])}];\n")
        f.write("\n#endif\n")
    print(f"Wrote {fist_c}  ({len(fist_sorted)} tiles)")

    # -----------------------------------------------------------------------
    # 7. Generate pistol_sprites.c/h  (red + black combined tile data)
    # -----------------------------------------------------------------------
    pistol_tile_data = []
    for c in pistol_sorted:
        pistol_tile_data.extend(tile_words(tiles_u32, c))

    # Red frames
    pist_frame_maps = []
    pist_frame_sizes = []
    for fi, (sp, xt, nr) in enumerate(pist_fdefs):
        we = xt // 2
        fmap = []
        for r in range(nr):
            for i in range(0, xt, 2):
                idx = (sp + r * 96 + i) // 2
                e = map_u16[idx]
                c = char_of(e)
                fmap.append(remap(e, pistol_rm.get(c, 0)) if c in pistol_rm else 0)
        pist_frame_maps.append(fmap)
        pist_frame_sizes.append((we, nr))

    # Black frames
    pistb_frame_maps = []
    pistb_frame_sizes = []
    for fi, (sp, xt, nr) in enumerate(pistb_fdefs):
        we = xt // 2
        fmap = []
        for r in range(nr):
            for i in range(0, xt, 2):
                idx = (sp + r * 96 + i) // 2
                e = map_u16[idx]
                c = char_of(e)
                fmap.append(remap(e, pistol_rm.get(c, 0)) if c in pistol_rm else 0)
        pistb_frame_maps.append(fmap)
        pistb_frame_sizes.append((we, nr))

    pist_c = os.path.join(OUTPUT_DIR, "pistol_sprites.c")
    with open(pist_c, 'w') as f:
        f.write("/* Pistol sprite tiles (red + black) -- auto-generated */\n\n")
        f.write(fmt_tiles("pistolTiles", pistol_tile_data,
                          f"{len(pistol_sorted)} pistol tiles (red+black)"))
        # Red frame maps
        for fi, fm in enumerate(pist_frame_maps):
            f.write("\n\n")
            we, nr = pist_frame_sizes[fi]
            f.write(fmt_map(f"pistolRedFrame{fi}Map", fm,
                            f"red frame {fi}: {we} cols x {nr} rows"))
        # Black frame maps
        for fi, fm in enumerate(pistb_frame_maps):
            f.write("\n\n")
            we, nr = pistb_frame_sizes[fi]
            f.write(fmt_map(f"pistolBlackFrame{fi}Map", fm,
                            f"black frame {fi}: {we} cols x {nr} rows"))
        f.write("\n")

    pist_h = os.path.join(OUTPUT_DIR, "pistol_sprites.h")
    with open(pist_h, 'w') as f:
        f.write("#ifndef __PISTOL_SPRITES_H__\n#define __PISTOL_SPRITES_H__\n\n")
        f.write(f"#define PISTOL_TILE_COUNT   {len(pistol_sorted)}\n")
        f.write(f"#define PISTOL_TILE_BYTES   {len(pistol_sorted)*16}\n\n")
        f.write(f"extern const unsigned int pistolTiles[{len(pistol_tile_data)}];\n\n")
        f.write("/* Red layer frames */\n")
        for fi, (we, nr) in enumerate(pist_frame_sizes):
            xt = we * 2
            f.write(f"#define PISTOL_RED_F{fi}_XTILES  {xt}\n")
            f.write(f"#define PISTOL_RED_F{fi}_ROWS    {nr}\n")
            f.write(f"extern const unsigned short pistolRedFrame{fi}Map"
                    f"[{len(pist_frame_maps[fi])}];\n")
        f.write("\n/* Black layer frames */\n")
        for fi, (we, nr) in enumerate(pistb_frame_sizes):
            xt = we * 2
            f.write(f"#define PISTOL_BLK_F{fi}_XTILES  {xt}\n")
            f.write(f"#define PISTOL_BLK_F{fi}_ROWS    {nr}\n")
            f.write(f"extern const unsigned short pistolBlackFrame{fi}Map"
                    f"[{len(pistb_frame_maps[fi])}];\n")
        f.write("\n#endif\n")
    print(f"Wrote {pist_c}  ({len(pistol_sorted)} tiles)")

    # -----------------------------------------------------------------------
    # 8. Print summary of defines to update in C headers
    # -----------------------------------------------------------------------
    print("\n" + "=" * 60)
    print("UPDATE THESE DEFINES IN C HEADERS:")
    print("=" * 60)
    print(f"  doomgfx.h:")
    print(f"    #define ZOMBIE_CHAR_START  {ZOMBIE_CS}")
    print(f"    #define WEAPON_CHAR_START  {WEAP_CS}")
    print(f"  pickup.h (or doomgfx.h):")
    print(f"    #define PICKUP_CHAR_START  {PICKUP_CS}")
    print(f"  wall_textures.h:")
    print(f"    #define WALL_TEX_CHAR_START  {WALL_CS}")
    print(f"  doomgfx.c  loadDoomGfxToMem:")
    print(f"    copymem size = {H * 16}  (was 14224)")
    print("=" * 60)


if __name__ == "__main__":
    main()
