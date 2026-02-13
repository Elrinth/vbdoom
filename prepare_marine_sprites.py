"""
prepare_marine_sprites.py

Convert Marine (Doom player) frames to VB tile format for player 2 rendering.
Source: make_into_4_colors/converted/frames/Marine/ (51 PNGs)

These frames were extracted sequentially from a Doom player sprite sheet.
Likely layout (5 unique directions per pose, dirs 6-8 mirror 4-2):
  0-4:   Walk A, directions 1-5 (front, front-left, left, back-left, back)
  5-9:   Walk B, directions 1-5
  10-14: Walk C, directions 1-5
  15-19: Walk D, directions 1-5
  20-24: Attack E, directions 1-5
  25-29: Attack F, directions 1-5
  30-34: Pain G, directions 1-5
  35-50: Death H-X (16 frames, direction-independent)

Pipeline:
  1. Convert each frame PNG to VB 4-color palette
  2. Pad to 64x64 (8x8 tiles = 64 chars per frame)
  3. Run grit.exe on each frame
  4. Generate marine_sprites.h with frame pointer table

Output: src/vbdoom/assets/images/sprites/marine/
"""

import os
import subprocess
import shutil
import re
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SOURCE_DIR = os.path.join(SCRIPT_DIR, "make_into_4_colors", "converted", "frames", "Marine")
PROJECT_ROOT = SCRIPT_DIR
GRIT_EXE = os.path.join(PROJECT_ROOT, 'grit_conversions', 'grit.exe')
GRIT_DIR = os.path.join(PROJECT_ROOT, 'grit_conversions')
SPRITES_BASE = os.path.join(PROJECT_ROOT, 'src', 'vbdoom', 'assets', 'images', 'sprites')
OUTPUT_DIR = os.path.join(SPRITES_BASE, 'marine')

GRIT_FLAGS = ['-fh!', '-ftc', '-gB2', '-p!', '-m!']

# VB Palette
VB_PALETTE = [
    (0,   0, 0),    # 0: Black (transparent)
    (85,  0, 0),    # 1: Dark red
    (164, 0, 0),    # 2: Medium red
    (239, 0, 0),    # 3: Bright red
]

TILE_SIZE = 8
TARGET_SIZE = 64  # 8x8 tiles

NUM_FRAMES = 51


def luminance(r, g, b):
    return int(0.299 * r + 0.587 * g + 0.114 * b)


def is_magenta(r, g, b):
    return (r > 220 and g < 30 and b > 220)


def map_to_vb_color(gray):
    if gray < 85:
        return 1
    elif gray < 170:
        return 2
    else:
        return 3


def enhance_contrast(img):
    """Adaptive contrast enhancement for sprite pixels."""
    pixels = []
    for y in range(img.height):
        for x in range(img.width):
            r, g, b = img.getpixel((x, y))[:3]
            if not is_magenta(r, g, b):
                gray = luminance(r, g, b)
                if gray > 5:
                    pixels.append(gray)
    if not pixels:
        return img
    pixels.sort()
    lo = pixels[int(len(pixels) * 0.02)]
    hi = pixels[int(len(pixels) * 0.98)]
    if hi <= lo:
        return img
    out = img.copy()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b = img.getpixel((x, y))[:3]
            if not is_magenta(r, g, b):
                gray = luminance(r, g, b)
                if gray <= 5:
                    stretched = gray
                else:
                    stretched = int(255.0 * (gray - lo) / (hi - lo))
                    stretched = max(0, min(255, stretched))
                out.putpixel((x, y), (stretched, stretched, stretched))
    return out


def convert_frame_to_vb(input_path, output_path):
    """Convert a single frame PNG to VB 4-color red palette, padded to 64x64."""
    raw = Image.open(input_path)
    img_rgba = raw.convert('RGBA')
    img = img_rgba.convert('RGB')
    alpha_data = img_rgba.split()[3]
    enhanced = enhance_contrast(img)

    w, h = img.size

    # Find bounding box of non-background content
    min_x, min_y, max_x, max_y = w, h, 0, 0
    has_content = False
    for y in range(h):
        for x in range(w):
            a = alpha_data.getpixel((x, y))
            if a == 0:
                continue
            r, g, b = img.getpixel((x, y))
            if is_magenta(r, g, b) or (r < 5 and g < 5 and b < 5):
                continue
            min_x = min(min_x, x)
            min_y = min(min_y, y)
            max_x = max(max_x, x)
            max_y = max(max_y, y)
            has_content = True

    if not has_content:
        out = Image.new('RGB', (TARGET_SIZE, TARGET_SIZE), VB_PALETTE[0])
        out.save(output_path)
        return

    cw = max_x - min_x + 1
    ch = max_y - min_y + 1
    scale = min(TARGET_SIZE / cw, TARGET_SIZE / ch)
    if scale > 1.0:
        scale = 1.0
    nw = max(1, int(cw * scale))
    nh = max(1, int(ch * scale))

    cropped = img.crop((min_x, min_y, max_x + 1, max_y + 1))
    enhanced_crop = enhanced.crop((min_x, min_y, max_x + 1, max_y + 1))
    alpha_crop = alpha_data.crop((min_x, min_y, max_x + 1, max_y + 1))
    if scale < 1.0:
        cropped = cropped.resize((nw, nh), Image.LANCZOS)
        enhanced_crop = enhanced_crop.resize((nw, nh), Image.LANCZOS)
        alpha_crop = alpha_crop.resize((nw, nh), Image.LANCZOS)

    out = Image.new('RGB', (TARGET_SIZE, TARGET_SIZE), VB_PALETTE[0])
    off_x = (TARGET_SIZE - nw) // 2
    off_y = TARGET_SIZE - nh  # bottom-aligned

    for y in range(nh):
        for x in range(nw):
            a = alpha_crop.getpixel((x, y))
            if a < 128:
                continue
            r, g, b = cropped.getpixel((x, y))
            if is_magenta(r, g, b):
                continue
            er = enhanced_crop.getpixel((x, y))[0]
            idx = map_to_vb_color(er)
            out.putpixel((off_x + x, off_y + y), VB_PALETTE[idx])

    out.save(output_path)


def run_grit(png_path):
    """Run grit on a single PNG file."""
    cmd = [GRIT_EXE, png_path] + GRIT_FLAGS
    result = subprocess.run(cmd, cwd=GRIT_DIR, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"    ERROR running grit: {result.stderr}")
        return None
    basename = os.path.splitext(os.path.basename(png_path))[0]
    c_file = os.path.join(GRIT_DIR, f'{basename}.c')
    if not os.path.exists(c_file):
        print(f"    ERROR: Expected output not found: {c_file}")
        return None
    return c_file


def get_tiles_array_name(c_file_path):
    """Extract the Tiles array name and size from a grit .c file."""
    with open(c_file_path, 'r') as f:
        content = f.read()
    match = re.search(r'const unsigned int (\w+Tiles)\[(\d+)\]', content)
    if match:
        return match.group(1), int(match.group(2))
    return None, None


def main():
    print("=== Marine Sprite Converter ===\n")

    if not os.path.exists(SOURCE_DIR):
        print(f"ERROR: Source dir not found: {SOURCE_DIR}")
        return
    if not os.path.exists(GRIT_EXE):
        print(f"ERROR: grit.exe not found: {GRIT_EXE}")
        return

    frame_files = [f"Marine_{i:03d}.png" for i in range(NUM_FRAMES)]
    missing = [f for f in frame_files if not os.path.exists(os.path.join(SOURCE_DIR, f))]
    if missing:
        print(f"WARNING: Missing {len(missing)} frames: {missing[:5]}...")
    frame_files = [f for f in frame_files if os.path.exists(os.path.join(SOURCE_DIR, f))]
    print(f"Found {len(frame_files)} frames to process")

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    temp_dir = os.path.join(GRIT_DIR, "_marine_temp")
    os.makedirs(temp_dir, exist_ok=True)

    tile_arrays = []
    tiles_per_frame = None

    for i, frame_file in enumerate(frame_files):
        src_path = os.path.join(SOURCE_DIR, frame_file)
        vb_path = os.path.join(temp_dir, f"marine_{i:03d}.png")
        convert_frame_to_vb(src_path, vb_path)

        c_file = run_grit(vb_path)
        if c_file is None:
            continue

        array_name, array_size = get_tiles_array_name(c_file)
        if array_name is None:
            print(f"    WARNING: Could not parse array from {c_file}")
            continue

        if tiles_per_frame is None:
            tiles_per_frame = array_size

        tile_arrays.append((array_name, array_size, i))
        dest_c = os.path.join(OUTPUT_DIR, f"Marine_{i:03d}.c")
        shutil.move(c_file, dest_c)

        # Clean up any .h file grit may have generated
        h_file = c_file.replace('.c', '.h')
        if os.path.exists(h_file):
            os.remove(h_file)

        if (i + 1) % 10 == 0 or i == len(frame_files) - 1:
            print(f"  Converted {i + 1}/{len(frame_files)} frames...")

    # Copy source PNGs for reference
    png_dir = os.path.join(OUTPUT_DIR, 'png')
    os.makedirs(png_dir, exist_ok=True)
    for f in frame_files:
        shutil.copy2(os.path.join(SOURCE_DIR, f), os.path.join(png_dir, f))

    shutil.rmtree(temp_dir, ignore_errors=True)
    generate_header(tile_arrays, tiles_per_frame)
    print(f"\nDone! {len(tile_arrays)} frames converted.")


def generate_header(tile_arrays, tiles_per_frame):
    """Generate marine_sprites.h with frame pointer table and direction LUTs."""
    header_path = os.path.join(OUTPUT_DIR, "marine_sprites.h")
    guard = "__MARINE_SPRITES_H__"
    bytes_per_frame = tiles_per_frame * 4 if tiles_per_frame else 0

    lines = []
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append(f"")
    lines.append(f"/*")
    lines.append(f" * Marine (player 2) sprite frames")
    lines.append(f" * Auto-generated by prepare_marine_sprites.py")
    lines.append(f" *")
    lines.append(f" * Total frames: {len(tile_arrays)}")
    lines.append(f" * Frame size: 64x64 px (8x8 tiles)")
    lines.append(f" * Tiles per frame: {tiles_per_frame} u32 words = {bytes_per_frame} bytes")
    lines.append(f" *")
    lines.append(f" * Frame layout (5 unique directions, mirrored for 8):")
    lines.append(f" *   0-4:   Walk A, dirs 1-5 (front, front-left, left, back-left, back)")
    lines.append(f" *   5-9:   Walk B, dirs 1-5")
    lines.append(f" *   10-14: Walk C, dirs 1-5")
    lines.append(f" *   15-19: Walk D, dirs 1-5")
    lines.append(f" *   20-24: Attack E, dirs 1-5")
    lines.append(f" *   25-29: Attack F, dirs 1-5")
    lines.append(f" *   30-34: Pain G, dirs 1-5")
    lines.append(f" *   35-50: Death (direction-independent)")
    lines.append(f" */")
    lines.append(f"")

    for array_name, array_size, idx in tile_arrays:
        lines.append(f"extern const unsigned int {array_name}[{array_size}];")

    lines.append(f"")
    lines.append(f"#define MARINE_FRAME_COUNT {len(tile_arrays)}")
    lines.append(f"#define MARINE_FRAME_BYTES {bytes_per_frame}")
    lines.append(f"#define MARINE_WALK_ANIM_FRAMES  4")
    lines.append(f"#define MARINE_ATTACK_ANIM_FRAMES 2")
    lines.append(f"#define MARINE_DEATH_FRAMES 16")
    lines.append(f"")

    lines.append(f"static const unsigned int* const MARINE_FRAMES[{len(tile_arrays)}] = {{")
    for i, (array_name, array_size, idx) in enumerate(tile_arrays):
        comma = "," if i < len(tile_arrays) - 1 else ""
        lines.append(f"    {array_name}{comma}")
    lines.append(f"}};")
    lines.append(f"")

    # Walk frame lookup: [direction][pose] -> frame index
    # 5 unique dirs: 0=front, 1=front-left, 2=left, 3=back-left, 4=back
    # Dirs 5-7 mirror: 5=back-right(mirror 3), 6=right(mirror 2), 7=front-right(mirror 1)
    lines.append(f"/* Walk frames: [direction][pose] -> frame index */")
    lines.append(f"static const u8 MARINE_WALK_FRAMES[8][{4}] = {{")
    lines.append(f"    {{ 0,  5, 10, 15}},   /* dir 0: front */")
    lines.append(f"    {{ 1,  6, 11, 16}},   /* dir 1: front-left */")
    lines.append(f"    {{ 2,  7, 12, 17}},   /* dir 2: left */")
    lines.append(f"    {{ 3,  8, 13, 18}},   /* dir 3: back-left */")
    lines.append(f"    {{ 4,  9, 14, 19}},   /* dir 4: back */")
    lines.append(f"    {{ 3,  8, 13, 18}},   /* dir 5: back-right (mirror 3) */")
    lines.append(f"    {{ 2,  7, 12, 17}},   /* dir 6: right (mirror 2) */")
    lines.append(f"    {{ 1,  6, 11, 16}},   /* dir 7: front-right (mirror 1) */")
    lines.append(f"}};")
    lines.append(f"")

    lines.append(f"/* Attack frames: [direction][pose] -> frame index */")
    lines.append(f"static const u8 MARINE_ATTACK_FRAMES[8][{2}] = {{")
    lines.append(f"    {{20, 25}},   /* dir 0: front */")
    lines.append(f"    {{21, 26}},   /* dir 1: front-left */")
    lines.append(f"    {{22, 27}},   /* dir 2: left */")
    lines.append(f"    {{23, 28}},   /* dir 3: back-left */")
    lines.append(f"    {{24, 29}},   /* dir 4: back */")
    lines.append(f"    {{23, 28}},   /* dir 5: back-right (mirror 3) */")
    lines.append(f"    {{22, 27}},   /* dir 6: right (mirror 2) */")
    lines.append(f"    {{21, 26}},   /* dir 7: front-right (mirror 1) */")
    lines.append(f"}};")
    lines.append(f"")

    lines.append(f"/* Pain frames: [direction] -> frame index */")
    lines.append(f"static const u8 MARINE_PAIN_FRAMES[8] = {{30, 31, 32, 33, 34, 33, 32, 31}};")
    lines.append(f"")

    lines.append(f"/* Death frames: sequential starting from 35 */")
    lines.append(f"static const u8 MARINE_DEATH_FRAMES_LUT[{min(16, len(tile_arrays)-35)}] = {{")
    death_indices = list(range(35, min(51, len(tile_arrays))))
    lines.append(f"    " + ", ".join(str(i) for i in death_indices))
    lines.append(f"}};")
    lines.append(f"")

    lines.append(f"#endif /* {guard} */")
    lines.append(f"")

    with open(header_path, 'w') as f:
        f.write('\n'.join(lines))
    print(f"  Header generated: marine_sprites.h")


if __name__ == '__main__':
    main()
