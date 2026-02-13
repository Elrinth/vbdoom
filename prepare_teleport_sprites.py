"""
prepare_teleport_sprites.py

Convert Doom teleport fog animation frames (TFOGA0-TFOGJ0) to VB tile format.
Source: make_into_4_colors_3/TeleportAnimation/ (10 PNGs, 64x64 each)

Transparency handling identical to ZombieCommando:
  - Magenta (R>220, G<30, B>220) -> transparent (index 0)
  - Alpha < 128 -> transparent (index 0)
  - Near-black (R<5, G<5, B<5) -> transparent (index 0)
  - Other pixels: luminance -> VB palette 1-3

Pipeline:
  1. Convert each PNG to VB 4-color palette (luminance-based)
  2. Pad/align to 64x64 (8x8 tiles = 64 chars per frame)
  3. Run grit.exe on each frame
  4. Generate teleport_sprites.h with frame pointer table

Output: src/vbdoom/assets/images/sprites/teleport/
"""

import os
import subprocess
import shutil
import re
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SOURCE_DIR = os.path.join(SCRIPT_DIR, "make_into_4_colors_3", "TeleportAnimation")
PROJECT_ROOT = SCRIPT_DIR
GRIT_EXE = os.path.join(PROJECT_ROOT, 'grit_conversions', 'grit.exe')
GRIT_DIR = os.path.join(PROJECT_ROOT, 'grit_conversions')
SPRITES_BASE = os.path.join(PROJECT_ROOT, 'src', 'vbdoom', 'assets', 'images', 'sprites')
OUTPUT_DIR = os.path.join(SPRITES_BASE, 'teleport')

GRIT_FLAGS = ['-fh!', '-ftc', '-gB2', '-p!', '-m!']

# VB Palette (indices 1-3 for sprite pixels, 0 for transparent)
VB_PALETTE = [
    (0,   0, 0),    # 0: Black (transparent)
    (85,  0, 0),    # 1: Dark red
    (164, 0, 0),    # 2: Medium red
    (239, 0, 0),    # 3: Bright red
]

TILE_SIZE = 8
TARGET_SIZE = 64  # 8x8 tiles

# Teleport fog frames in order (A through J)
FRAME_ORDER = [
    "TFOGA0.png",
    "TFOGB0.png",
    "TFOGC0.png",
    "TFOGD0.png",
    "TFOGE0.png",
    "TFOGF0.png",
    "TFOGG0.png",
    "TFOGH0.png",
    "TFOGI0.png",
    "TFOGJ0.png",
]


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
    # Convert to RGBA first to capture palette transparency / alpha
    img_rgba = raw.convert('RGBA')
    img = img_rgba.convert('RGB')

    # Build alpha mask from RGBA (alpha == 0 means transparent)
    alpha_data = img_rgba.split()[3]  # A channel

    # Enhance contrast (works on RGB)
    enhanced = enhance_contrast(img)

    w, h = img.size

    # Find bounding box of non-background content
    min_x, min_y, max_x, max_y = w, h, 0, 0
    has_content = False
    for y in range(h):
        for x in range(w):
            a = alpha_data.getpixel((x, y))
            if a == 0:
                continue  # transparent pixel
            r, g, b = img.getpixel((x, y))
            if is_magenta(r, g, b) or (r < 5 and g < 5 and b < 5):
                continue  # background color
            min_x = min(min_x, x)
            min_y = min(min_y, y)
            max_x = max(max_x, x)
            max_y = max(max_y, y)
            has_content = True

    if not has_content:
        # Empty frame - just make a transparent 64x64
        out = Image.new('RGB', (TARGET_SIZE, TARGET_SIZE), VB_PALETTE[0])
        out.save(output_path)
        return

    # Crop to content
    cw = max_x - min_x + 1
    ch = max_y - min_y + 1

    # Scale to fit TARGET_SIZE while preserving aspect ratio
    scale = min(TARGET_SIZE / cw, TARGET_SIZE / ch)
    if scale > 1.0:
        scale = 1.0  # Don't upscale
    nw = max(1, int(cw * scale))
    nh = max(1, int(ch * scale))

    # Crop and resize (both RGB and alpha)
    cropped = img.crop((min_x, min_y, max_x + 1, max_y + 1))
    enhanced_crop = enhanced.crop((min_x, min_y, max_x + 1, max_y + 1))
    alpha_crop = alpha_data.crop((min_x, min_y, max_x + 1, max_y + 1))
    if scale < 1.0:
        cropped = cropped.resize((nw, nh), Image.LANCZOS)
        enhanced_crop = enhanced_crop.resize((nw, nh), Image.LANCZOS)
        alpha_crop = alpha_crop.resize((nw, nh), Image.LANCZOS)

    # Create target canvas, center-aligned (teleport fog is centered, not bottom-aligned)
    out = Image.new('RGB', (TARGET_SIZE, TARGET_SIZE), VB_PALETTE[0])
    off_x = (TARGET_SIZE - nw) // 2
    off_y = (TARGET_SIZE - nh) // 2  # center-aligned for fog effect

    for y in range(nh):
        for x in range(nw):
            a = alpha_crop.getpixel((x, y))
            if a < 128:
                continue  # transparent (threshold for resized alpha)
            r, g, b = cropped.getpixel((x, y))
            if is_magenta(r, g, b):
                continue  # magenta background
            er = enhanced_crop.getpixel((x, y))[0]
            idx = map_to_vb_color(er)
            out.putpixel((off_x + x, off_y + y), VB_PALETTE[idx])

    out.save(output_path)


def run_grit(png_path, output_name):
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
    print("=== Teleport Animation Sprite Converter ===\n")

    if not os.path.exists(SOURCE_DIR):
        print(f"ERROR: Source dir not found: {SOURCE_DIR}")
        return
    if not os.path.exists(GRIT_EXE):
        print(f"ERROR: grit.exe not found: {GRIT_EXE}")
        return

    # Verify all expected frames exist
    missing = [f for f in FRAME_ORDER if not os.path.exists(os.path.join(SOURCE_DIR, f))]
    if missing:
        print(f"WARNING: Missing {len(missing)} frames: {missing}")

    # Filter to only existing frames
    frame_files = [f for f in FRAME_ORDER if os.path.exists(os.path.join(SOURCE_DIR, f))]
    print(f"Found {len(frame_files)} frames to process")

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    temp_dir = os.path.join(GRIT_DIR, "_teleport_temp")
    os.makedirs(temp_dir, exist_ok=True)

    tile_arrays = []
    tiles_per_frame = None

    for i, frame_file in enumerate(frame_files):
        src_path = os.path.join(SOURCE_DIR, frame_file)

        # Convert to VB palette and pad to 64x64
        vb_path = os.path.join(temp_dir, f"teleport_{i:02d}.png")
        convert_frame_to_vb(src_path, vb_path)

        # Run grit
        c_file = run_grit(vb_path, f"teleport_{i:02d}")
        if c_file is None:
            continue

        array_name, array_size = get_tiles_array_name(c_file)
        if array_name is None:
            print(f"    WARNING: Could not parse array from {c_file}")
            continue

        if tiles_per_frame is None:
            tiles_per_frame = array_size

        tile_arrays.append((array_name, array_size, i))

        # Move .c file to output directory
        dest_c = os.path.join(OUTPUT_DIR, f"Teleport_{i:02d}.c")
        shutil.move(c_file, dest_c)

        print(f"  Converted frame {i}: {frame_file} -> Teleport_{i:02d}.c ({array_size} words)")

    # Clean up temp
    shutil.rmtree(temp_dir, ignore_errors=True)

    # Generate header
    generate_header(tile_arrays, tiles_per_frame)
    print(f"\nDone! {len(tile_arrays)} frames converted.")


def generate_header(tile_arrays, tiles_per_frame):
    """Generate teleport_sprites.h with frame pointer table."""
    header_path = os.path.join(OUTPUT_DIR, "teleport_sprites.h")
    guard = "__TELEPORT_SPRITES_H__"
    bytes_per_frame = tiles_per_frame * 4 if tiles_per_frame else 0

    lines = []
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append(f"")
    lines.append(f"/*")
    lines.append(f" * Teleport fog animation sprite frames")
    lines.append(f" * Auto-generated by prepare_teleport_sprites.py")
    lines.append(f" *")
    lines.append(f" * Total frames: {len(tile_arrays)}")
    lines.append(f" * Frame size: 64x64 px (8x8 tiles)")
    lines.append(f" * Tiles per frame: {tiles_per_frame} u32 words = {bytes_per_frame} bytes")
    lines.append(f" *")
    lines.append(f" * Frame layout: TFOGA0 through TFOGJ0 (10 sequential frames)")
    lines.append(f" */")
    lines.append(f"")

    for array_name, array_size, idx in tile_arrays:
        lines.append(f"extern const unsigned int {array_name}[{array_size}];")

    lines.append(f"")
    lines.append(f"#define TELEPORT_SPRITE_FRAME_COUNT {len(tile_arrays)}")
    lines.append(f"#define TELEPORT_SPRITE_FRAME_BYTES {bytes_per_frame}")
    lines.append(f"")

    lines.append(f"static const unsigned int* const TELEPORT_FRAMES[{len(tile_arrays)}] = {{")
    for i, (array_name, array_size, idx) in enumerate(tile_arrays):
        comma = "," if i < len(tile_arrays) - 1 else ""
        lines.append(f"    {array_name}{comma}")
    lines.append(f"}};")
    lines.append(f"")
    lines.append(f"#endif /* {guard} */")
    lines.append(f"")

    with open(header_path, 'w') as f:
        f.write('\n'.join(lines))
    print(f"  Header generated: teleport_sprites.h")


if __name__ == '__main__':
    main()
