"""
prepare_doom_faces.py

Convert Doom face sprites to VB 2bpp tile format using grit.exe,
matching the proven zombie sprite pipeline exactly.

Face layout:
  - Faces 0-2: hand-crafted originals from vb_doom.c (idle full-health left/center/right)
  - Faces 3-14: DOOM faces from doom_faces.png rows 1-4 (idle, health brackets 1-4)
  - Faces 15-19: DOOM ouch front (one per damage level)
  - Faces 20-24: DOOM severe ouch
  - Faces 25-29: DOOM evil grin
  - Face 30: dead
  - Face 31: god mode

Ouch-left and ouch-right are removed (too large, won't downscale well).

Output (individual .c files + header, matching zombie_sprites.h pattern):
  - src/vbdoom/assets/images/sprites/faces/face_XX.c  (grit-generated tile data)
  - src/vbdoom/assets/images/sprites/faces/face_sprites.h  (extern decls + static ptr table)
  - face_previews/  (VB-palette preview PNGs)
"""

import os
import re
import sys
import math
import shutil
import subprocess
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
INPUT_IMG = os.path.join(SCRIPT_DIR, "more_doom_gfx", "doom_faces.png")
VB_DOOM_C = os.path.join(SCRIPT_DIR, "grit_conversions", "vb_doom.c")
GRIT_EXE = os.path.join(SCRIPT_DIR, "grit_conversions", "grit.exe")
GRIT_DIR = os.path.join(SCRIPT_DIR, "grit_conversions")
FACES_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images",
                         "sprites", "faces")
PREVIEW_DIR = os.path.join(SCRIPT_DIR, "face_previews")

FACE_CHAR_START = 108
TILE_SIZE = 8
FACE_TILE_W = 3   # 3 tiles wide = 24px
FACE_TILE_H = 4   # 4 tiles tall = 32px
FACE_PX_W = FACE_TILE_W * TILE_SIZE   # 24
FACE_PX_H = FACE_TILE_H * TILE_SIZE   # 32
TILES_PER_FACE = FACE_TILE_W * FACE_TILE_H  # 12
WORDS_PER_FACE = TILES_PER_FACE * 4          # 48

MAGENTA_THRESHOLD = 30
DITHER_STRENGTH = 28
EDGE_BOOST = 18
THRESH_DARK = 72
THRESH_MED = 158

BAYER_2x2 = [
    [0.00, 0.50],
    [0.75, 0.25],
]

VB_PALETTE = {0: (0, 0, 0, 0), 1: (80, 0, 0, 255), 2: (170, 0, 0, 255),
              3: (255, 0, 0, 255)}

FACE_COUNT = 32

GRIT_FLAGS = ['-fh!', '-ftc', '-gB2', '-p!', '-m!']


# ---------------------------------------------------------------------------
# Parse vb_doom.c for hand-crafted face tiles
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


def extract_handcrafted_faces():
    """Extract the 3 hand-crafted faces from vb_doom.c tile data.

    The faces live in the BGMap at (col 8, rows 4-15), 3 faces stacked vertically.
    Each face is 3 tiles wide x 4 tiles tall = 12 tiles.
    Returns list of 3 canvases (each 24x32, values 0-3).
    """
    tiles_u32 = parse_hex_array(VB_DOOM_C, "vb_doomTiles")
    map_u16 = parse_hex_array(VB_DOOM_C, "vb_doomMap")

    MAP_COLS = 48
    canvases = []

    for face_id in range(3):
        face_tile_data = []
        for row_off in range(4):
            map_row = 4 + face_id * 4 + row_off
            for col_off in range(3):
                map_col = 8 + col_off
                map_idx = map_row * MAP_COLS + map_col
                entry = map_u16[map_idx]
                char_idx = entry & 0x07FF
                hflip = (entry >> 13) & 1

                t_off = char_idx * 4
                words = list(tiles_u32[t_off:t_off + 4])

                if hflip:
                    words = hflip_tile_words(words)

                face_tile_data.extend(words)

        # Decode tile data to canvas
        canvas = tile_u32_to_canvas(face_tile_data)
        canvases.append(canvas)

    return canvases


def hflip_tile_words(words):
    """H-flip a VB 2bpp tile (sequential format) given as 4 u32 words.

    VB 2bpp tiles use sequential packing: each row is a u16 (LE) where
    bits[15:14]=px0, bits[13:12]=px1, ..., bits[1:0]=px7.
    H-flip reverses pixel order within each row.
    """
    data = []
    for w in words:
        data.append(w & 0xFF)
        data.append((w >> 8) & 0xFF)
        data.append((w >> 16) & 0xFF)
        data.append((w >> 24) & 0xFF)

    flipped = []
    for row in range(8):
        byte0 = data[row * 2]       # low byte of u16 (pixels 4-7)
        byte1 = data[row * 2 + 1]   # high byte of u16 (pixels 0-3)
        u16_val = (byte1 << 8) | byte0

        # Extract 8 pixels (px0 at LSB), reverse them, repack
        pixels = [(u16_val >> (x * 2)) & 3 for x in range(8)]
        pixels.reverse()
        new_u16 = 0
        for x in range(8):
            new_u16 |= (pixels[x] << (x * 2))

        flipped.append(new_u16 & 0xFF)
        flipped.append((new_u16 >> 8) & 0xFF)

    result = []
    for i in range(0, 16, 4):
        w = (flipped[i] | (flipped[i+1] << 8) |
             (flipped[i+2] << 16) | (flipped[i+3] << 24))
        result.append(w)
    return result


def tile_u32_to_canvas(tile_words):
    """Convert 48 u32 words back to a 24x32 VB-index canvas.

    VB 2bpp tiles use sequential packing: each row is a u16 (LE) where
    bits[15:14]=px0, bits[13:12]=px1, ..., bits[1:0]=px7.
    """
    canvas = [[0] * FACE_PX_W for _ in range(FACE_PX_H)]

    for ti in range(TILES_PER_FACE):
        ty = ti // FACE_TILE_W
        tx = ti % FACE_TILE_W
        base = ti * 4

        data = []
        for wi in range(4):
            w = tile_words[base + wi]
            data.append(w & 0xFF)
            data.append((w >> 8) & 0xFF)
            data.append((w >> 16) & 0xFF)
            data.append((w >> 24) & 0xFF)

        for y in range(TILE_SIZE):
            byte0 = data[y * 2]       # low byte of u16
            byte1 = data[y * 2 + 1]   # high byte of u16
            u16_val = (byte1 << 8) | byte0

            for x in range(TILE_SIZE):
                shift = x * 2  # px0 (leftmost) at bits 1:0 (LSB)
                px_val = (u16_val >> shift) & 3
                py = ty * TILE_SIZE + y
                px = tx * TILE_SIZE + x
                canvas[py][px] = px_val

    return canvas


# ---------------------------------------------------------------------------
# DOOM face detection and extraction
# ---------------------------------------------------------------------------

def is_magenta(r, g, b):
    return r > 220 and g < MAGENTA_THRESHOLD and b > 220


def luminance(r, g, b):
    return int(0.299 * r + 0.587 * g + 0.114 * b)


def find_faces(img):
    """Find individual face bounding boxes by scanning for magenta separators."""
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

    row_has_content = [any(mask[y]) for y in range(h)]
    bands = []
    in_band = False
    band_start = 0
    for y in range(h):
        if row_has_content[y] and not in_band:
            band_start = y
            in_band = True
        elif not row_has_content[y] and in_band:
            bands.append((band_start, y - 1))
            in_band = False
    if in_band:
        bands.append((band_start, h - 1))

    faces = []
    for band_top, band_bottom in bands:
        col_has_content = [False] * w
        for x in range(w):
            for y in range(band_top, band_bottom + 1):
                if mask[y][x]:
                    col_has_content[x] = True
                    break

        in_face = False
        face_left = 0
        for x in range(w):
            if col_has_content[x] and not in_face:
                face_left = x
                in_face = True
            elif not col_has_content[x] and in_face:
                faces.append((face_left, band_top, x - 1, band_bottom))
                in_face = False
        if in_face:
            faces.append((face_left, band_top, w - 1, band_bottom))

    return faces


def extract_face_grayscale(img, bbox):
    """Extract face, DOWNSCALE to 24x32, convert to grayscale+alpha."""
    left, top, right, bottom = bbox
    face_img = img.crop((left, top, right + 1, bottom + 1))
    face_img = face_img.resize((FACE_PX_W, FACE_PX_H), Image.LANCZOS)

    pixels = face_img.load()
    gray_canvas = [[0.0] * FACE_PX_W for _ in range(FACE_PX_H)]
    alpha_canvas = [[0] * FACE_PX_W for _ in range(FACE_PX_H)]

    for y in range(FACE_PX_H):
        for x in range(FACE_PX_W):
            pix = pixels[x, y]
            r, g, b = pix[0], pix[1], pix[2]
            a = pix[3] if len(pix) > 3 else 255

            if is_magenta(r, g, b) or a < 128:
                gray_canvas[y][x] = 0.0
                alpha_canvas[y][x] = 0
            else:
                gray_canvas[y][x] = float(luminance(r, g, b))
                alpha_canvas[y][x] = 1

    return gray_canvas, alpha_canvas


def contrast_stretch(gray, alpha, low_pct=2, high_pct=98):
    """Apply per-face contrast stretching (percentile-based)."""
    values = []
    for y in range(FACE_PX_H):
        for x in range(FACE_PX_W):
            if alpha[y][x]:
                values.append(gray[y][x])

    if len(values) < 10:
        return gray

    values.sort()
    lo = values[max(0, len(values) * low_pct // 100)]
    hi = values[min(len(values) - 1, len(values) * high_pct // 100)]
    if hi - lo < 20:
        hi = lo + 20

    out = [[0.0] * FACE_PX_W for _ in range(FACE_PX_H)]
    for y in range(FACE_PX_H):
        for x in range(FACE_PX_W):
            if alpha[y][x]:
                v = (gray[y][x] - lo) / (hi - lo) * 255.0
                out[y][x] = max(0.0, min(255.0, v))
    return out


def sobel_edge_magnitude(gray, alpha):
    """Compute edge magnitude via Sobel filter."""
    edge = [[0.0] * FACE_PX_W for _ in range(FACE_PX_H)]
    for y in range(1, FACE_PX_H - 1):
        for x in range(1, FACE_PX_W - 1):
            if not alpha[y][x]:
                continue
            gx = (-gray[y-1][x-1] + gray[y-1][x+1]
                  - 2*gray[y][x-1]   + 2*gray[y][x+1]
                  - gray[y+1][x-1] + gray[y+1][x+1])
            gy = (-gray[y-1][x-1] - 2*gray[y-1][x]   - gray[y-1][x+1]
                  + gray[y+1][x-1] + 2*gray[y+1][x] + gray[y+1][x+1])
            edge[y][x] = math.sqrt(gx*gx + gy*gy) / 4.0
    return edge


def quantize_face(gray, alpha, edges):
    """Quantize to VB palette with ordered dithering and edge boost."""
    canvas = [[0] * FACE_PX_W for _ in range(FACE_PX_H)]
    for y in range(FACE_PX_H):
        for x in range(FACE_PX_W):
            if not alpha[y][x]:
                continue

            g = gray[y][x]
            edge_mag = min(edges[y][x], 80.0)
            if edge_mag > 20:
                boost = EDGE_BOOST * (edge_mag - 20) / 60.0
                if g < 128:
                    g = max(0.0, g - boost)
                else:
                    g = min(255.0, g + boost)

            dval = BAYER_2x2[y % 2][x % 2] * DITHER_STRENGTH
            g = g + dval - DITHER_STRENGTH * 0.375

            if g < THRESH_DARK:
                canvas[y][x] = 1
            elif g < THRESH_MED:
                canvas[y][x] = 2
            else:
                canvas[y][x] = 3

    return canvas


def extract_doom_face(img, bbox):
    """Full pipeline: extract, downscale, contrast stretch, edge detect, dither, quantize."""
    gray, alpha = extract_face_grayscale(img, bbox)
    gray = contrast_stretch(gray, alpha)
    edges = sobel_edge_magnitude(gray, alpha)
    return quantize_face(gray, alpha, edges)


# ---------------------------------------------------------------------------
# Grit integration
# ---------------------------------------------------------------------------

def save_indexed_png(canvas, path):
    """Save a 24x32 indexed PNG with 4-color grayscale palette for grit."""
    img = Image.new('P', (FACE_PX_W, FACE_PX_H))
    # 4-color palette: index 0=black, 1=dark, 2=medium, 3=white
    palette = [0, 0, 0, 85, 85, 85, 170, 170, 170, 255, 255, 255]
    palette += [0] * (768 - len(palette))
    img.putpalette(palette)
    pixels = img.load()
    for y in range(FACE_PX_H):
        for x in range(FACE_PX_W):
            pixels[x, y] = canvas[y][x]
    img.save(path)


def run_grit(png_path):
    """Run grit on a face PNG, return the generated .c filepath (in GRIT_DIR)."""
    basename = os.path.splitext(os.path.basename(png_path))[0]

    cmd = [GRIT_EXE, png_path] + GRIT_FLAGS
    result = subprocess.run(cmd, cwd=GRIT_DIR, capture_output=True, text=True)

    if result.returncode != 0:
        print(f"  ERROR: grit failed for {basename}: {result.stderr}")
        return None

    c_file = os.path.join(GRIT_DIR, f"{basename}.c")
    if not os.path.exists(c_file):
        print(f"  ERROR: grit output not found: {c_file}")
        return None

    # Clean up any .h grit might create
    h_file = os.path.join(GRIT_DIR, f"{basename}.h")
    if os.path.exists(h_file):
        os.remove(h_file)

    return c_file


def get_grit_array_info(c_path):
    """Extract tile array name and size from a grit .c file."""
    with open(c_path, 'r') as f:
        content = f.read()
    m = re.search(r'const unsigned int (\w+Tiles)\[(\d+)\]', content)
    if m:
        return m.group(1), int(m.group(2))
    return None, None


# ---------------------------------------------------------------------------
# Preview generation
# ---------------------------------------------------------------------------

def save_face_preview(canvas, path):
    """Save a 24x32 PNG at true pixel size with VB red palette."""
    img = Image.new('RGBA', (FACE_PX_W, FACE_PX_H), (0, 0, 0, 255))
    pix = img.load()
    for y in range(FACE_PX_H):
        for x in range(FACE_PX_W):
            pix[x, y] = VB_PALETTE[canvas[y][x]]
    img.save(path)


def save_all_faces_sheet(all_canvases, path):
    """Save sheet of all faces at 1x (true pixel size)."""
    cols = 8
    rows = (len(all_canvases) + cols - 1) // cols
    gap = 1
    sw = (FACE_PX_W + gap) * cols
    sh = (FACE_PX_H + gap) * rows
    sheet = Image.new('RGBA', (sw, sh), (40, 0, 0, 255))
    spix = sheet.load()
    for fi, canvas in enumerate(all_canvases):
        col = fi % cols
        row = fi // cols
        ox = col * (FACE_PX_W + gap)
        oy = row * (FACE_PX_H + gap)
        for y in range(FACE_PX_H):
            for x in range(FACE_PX_W):
                color = VB_PALETTE[canvas[y][x]]
                px = ox + x
                py = oy + y
                if 0 <= px < sw and 0 <= py < sh:
                    spix[px, py] = color
    sheet.save(path)


# ---------------------------------------------------------------------------
# Header generation (matches zombie_sprites.h pattern)
# ---------------------------------------------------------------------------

def generate_header(array_names, array_sizes):
    """Generate face_sprites.h matching zombie_sprites.h pattern."""
    h_path = os.path.join(FACES_DIR, "face_sprites.h")

    with open(h_path, 'w') as f:
        f.write("#ifndef __FACE_SPRITES_H__\n")
        f.write("#define __FACE_SPRITES_H__\n\n")
        f.write("/*\n")
        f.write(" * Face sprite frames\n")
        f.write(" * Auto-generated by prepare_doom_faces.py + grit\n")
        f.write(" *\n")
        f.write(f" * Total faces: {FACE_COUNT}\n")
        f.write(f" * Face size: {FACE_PX_W}x{FACE_PX_H} px "
                f"({FACE_TILE_W}x{FACE_TILE_H} tiles)\n")
        f.write(f" * Tiles per face: {WORDS_PER_FACE} u32 words "
                f"= {TILES_PER_FACE * 16} bytes\n")
        f.write(" *\n")
        f.write(" * Usage:\n")
        f.write(" *   copymem((void*)addr, "
                f"(void*)FACE_TILE_DATA[faceIndex], {TILES_PER_FACE * 16});\n")
        f.write(" */\n\n")

        # Defines
        f.write(f"#define FACE_CHAR_START   {FACE_CHAR_START}\n")
        f.write(f"#define FACE_TILE_COUNT   {TILES_PER_FACE}\n")
        f.write(f"#define FACE_TILE_BYTES   {TILES_PER_FACE * 16}\n")
        f.write(f"#define FACE_COUNT        {FACE_COUNT}\n\n")

        f.write("/* Face index defines */\n")
        f.write("#define FACE_IDLE_BASE     0   "
                "/* 15 faces: 5 damage x 3 dir */\n")
        f.write("#define FACE_OUCH_FRONT   15   "
                "/* 5 faces: one per damage lvl */\n")
        f.write("#define FACE_OUCH_SEVERE  20\n")
        f.write("#define FACE_EVIL_GRIN    25\n")
        f.write("#define FACE_DEAD         30\n")
        f.write("#define FACE_GOD          31\n\n")

        # Extern declarations (like zombie_sprites.h)
        f.write(f"/* Individual face tile data "
                f"(each is {WORDS_PER_FACE} u32 words "
                f"= {TILES_PER_FACE * 16} bytes) */\n")
        for i in range(FACE_COUNT):
            f.write(f"extern const unsigned int "
                    f"{array_names[i]}[{array_sizes[i]}];\n")

        f.write("\n")

        # Static pointer table (like ZOMBIE_FRAMES)
        f.write("/* Frame pointer table for dynamic face loading */\n")
        f.write(f"static const unsigned int* const "
                f"FACE_TILE_DATA[{FACE_COUNT}] = {{\n")
        for i in range(FACE_COUNT):
            comma = "," if i < FACE_COUNT - 1 else ""
            f.write(f"    {array_names[i]}{comma}\n")
        f.write("};\n\n")

        # Static faceMap
        f.write("/* Fixed face BGMap: always references chars 108-119 */\n")
        f.write(f"static const unsigned short faceMap[{TILES_PER_FACE}]"
                f" __attribute__((aligned(4))) = {{\n\t")
        entries = [FACE_CHAR_START + i for i in range(TILES_PER_FACE)]
        f.write(",".join(f"0x{e:04X}" for e in entries))
        f.write("\n};\n\n")

        f.write("#endif /* __FACE_SPRITES_H__ */\n")

    return h_path


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    os.makedirs(PREVIEW_DIR, exist_ok=True)
    os.makedirs(FACES_DIR, exist_ok=True)

    if not os.path.exists(GRIT_EXE):
        print(f"ERROR: grit.exe not found at {GRIT_EXE}")
        sys.exit(1)

    # --- Step 1: Extract 3 hand-crafted faces from vb_doom.c ---
    print("Extracting 3 hand-crafted faces from vb_doom.c...")
    handcrafted_canvases = extract_handcrafted_faces()
    print(f"  Got {len(handcrafted_canvases)} hand-crafted faces")

    # --- Step 2: Load and detect DOOM faces ---
    img = Image.open(INPUT_IMG).convert('RGBA')
    w, h = img.size
    print(f"\nLoaded {INPUT_IMG}: {w}x{h}")

    bboxes = find_faces(img)
    print(f"Found {len(bboxes)} faces in doom_faces.png")

    if len(bboxes) < 42:
        print(f"WARNING: Expected at least 42 faces, found {len(bboxes)}")

    # Group bboxes by row
    doom_bboxes = bboxes[:42] if len(bboxes) >= 42 else bboxes
    row_counts = [3, 3, 3, 3, 3, 5, 5, 5, 5, 5, 2]
    rows = []
    idx = 0
    for cnt in row_counts:
        rows.append(doom_bboxes[idx:idx + cnt])
        idx += cnt

    # --- Step 3: Build all 32 face canvases ---
    all_canvases = []

    # Faces 0-2: hand-crafted
    for fi in range(3):
        all_canvases.append(handcrafted_canvases[fi])
        print(f"  Face {fi:2d}: hand-crafted (from vb_doom.c)")

    # Faces 3-14: idle damage 1-4 (rows 1-4, 3 faces each)
    for row_idx in range(1, 5):
        for col_idx in range(3):
            fi = len(all_canvases)
            bbox = rows[row_idx][col_idx]
            canvas = extract_doom_face(img, bbox)
            all_canvases.append(canvas)
            fw = bbox[2] - bbox[0] + 1
            fh = bbox[3] - bbox[1] + 1
            print(f"  Face {fi:2d}: doom row {row_idx} col {col_idx} "
                  f"({fw}x{fh} -> 24x32)")

    # Faces 15-19: ouch front (row 5, 5 faces)
    for col_idx in range(5):
        fi = len(all_canvases)
        bbox = rows[5][col_idx]
        canvas = extract_doom_face(img, bbox)
        all_canvases.append(canvas)
        fw = bbox[2] - bbox[0] + 1
        fh = bbox[3] - bbox[1] + 1
        print(f"  Face {fi:2d}: ouch front ({fw}x{fh} -> 24x32)")

    # SKIP rows 6 and 7 (ouch left, ouch right)

    # Faces 20-24: severe ouch (row 8, 5 faces)
    for col_idx in range(5):
        fi = len(all_canvases)
        bbox = rows[8][col_idx]
        canvas = extract_doom_face(img, bbox)
        all_canvases.append(canvas)
        fw = bbox[2] - bbox[0] + 1
        fh = bbox[3] - bbox[1] + 1
        print(f"  Face {fi:2d}: severe ouch ({fw}x{fh} -> 24x32)")

    # Faces 25-29: evil grin (row 9, 5 faces)
    for col_idx in range(5):
        fi = len(all_canvases)
        bbox = rows[9][col_idx]
        canvas = extract_doom_face(img, bbox)
        all_canvases.append(canvas)
        fw = bbox[2] - bbox[0] + 1
        fh = bbox[3] - bbox[1] + 1
        print(f"  Face {fi:2d}: evil grin ({fw}x{fh} -> 24x32)")

    # Face 30: dead (row 10 col 0)
    fi = len(all_canvases)
    bbox = rows[10][0]
    canvas = extract_doom_face(img, bbox)
    all_canvases.append(canvas)
    print(f"  Face {fi:2d}: dead")

    # Face 31: god mode (row 10 col 1)
    fi = len(all_canvases)
    bbox = rows[10][1]
    canvas = extract_doom_face(img, bbox)
    all_canvases.append(canvas)
    print(f"  Face {fi:2d}: god mode")

    assert len(all_canvases) == FACE_COUNT, \
        f"Expected {FACE_COUNT} faces, got {len(all_canvases)}"

    # --- Step 4: Save previews (true 24x32 pixel size, VB palette) ---
    for fi, canvas in enumerate(all_canvases):
        save_face_preview(canvas, os.path.join(PREVIEW_DIR,
                                               f"face_{fi:02d}.png"))
    save_all_faces_sheet(all_canvases, os.path.join(PREVIEW_DIR,
                                                     "all_faces.png"))
    print(f"\nSaved {FACE_COUNT} previews to {PREVIEW_DIR}/")

    # --- Step 5: Save indexed PNGs and run grit on each ---
    print(f"\nRunning grit on {FACE_COUNT} faces...")
    grit_input_dir = os.path.join(PREVIEW_DIR, "grit_input")
    os.makedirs(grit_input_dir, exist_ok=True)

    array_names = []
    array_sizes = []

    for fi, canvas in enumerate(all_canvases):
        name = f"face_{fi:02d}"
        png_path = os.path.join(grit_input_dir, f"{name}.png")

        # Save indexed 4-color PNG for grit
        save_indexed_png(canvas, png_path)

        # Run grit
        c_file = run_grit(png_path)
        if c_file is None:
            print(f"  FATAL: grit failed for face {fi}")
            sys.exit(1)

        # Move .c file to faces directory
        dest = os.path.join(FACES_DIR, f"{name}.c")
        shutil.move(c_file, dest)

        # Parse array name/size from grit output
        arr_name, arr_size = get_grit_array_info(dest)
        if arr_name is None:
            print(f"  FATAL: could not parse grit output for face {fi}")
            sys.exit(1)

        array_names.append(arr_name)
        array_sizes.append(arr_size)
        print(f"  Face {fi:2d}: {arr_name}[{arr_size}] -> {name}.c")

    # --- Step 6: Generate face_sprites.h (zombie-style) ---
    h_path = generate_header(array_names, array_sizes)
    print(f"\nWrote {h_path}")

    # --- Step 7: Clean up old files ---
    old_c = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images",
                         "face_sprites.c")
    old_h = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "images",
                         "face_sprites.h")
    for old_file in [old_c, old_h]:
        if os.path.exists(old_file):
            os.remove(old_file)
            print(f"Removed old {old_file}")

    print(f"\nDone! {FACE_COUNT} face .c files + header in {FACES_DIR}/")
    print(f"VRAM: FACE_CHAR_START={FACE_CHAR_START}, "
          f"tiles per face={TILES_PER_FACE}")
    print(f"Total ROM: {FACE_COUNT} faces x {TILES_PER_FACE * 16} bytes = "
          f"{FACE_COUNT * TILES_PER_FACE * 16} bytes")


if __name__ == "__main__":
    main()
