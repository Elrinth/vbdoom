"""
Run grit on all extracted sprite frames and organize the output
into the project's asset directory structure.

Creates:
  src/vbdoom/assets/images/sprites/marine/
  src/vbdoom/assets/images/sprites/zombie/
  src/vbdoom/assets/images/sprites/zombie_sergeant/

Each folder gets:
  - Individual .c tile files (one per frame)
  - A combined header file with extern declarations and a frame lookup table
  - The source .png frames (for reference)

Usage: python grit_all_frames.py
"""

import os
import subprocess
import shutil
import re

# Paths
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR.replace('\\', '/')))
# Fix: project root should be the vbdoom project root
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, '..'))

GRIT_EXE = os.path.join(PROJECT_ROOT, 'grit_conversions', 'grit.exe')
GRIT_DIR = os.path.join(PROJECT_ROOT, 'grit_conversions')
FRAMES_BASE = os.path.join(SCRIPT_DIR, 'converted', 'frames')
SPRITES_BASE = os.path.join(PROJECT_ROOT, 'src', 'vbdoom', 'assets', 'images', 'sprites')

# Grit flags for raw tiles (no map) - same as convert_without_map.bat
GRIT_FLAGS = ['-fh!', '-ftc', '-gB2', '-p!', '-m!']

# Enemy definitions: folder name -> (display name, c prefix)
ENEMIES = {
    'Marine':            ('Marine',           'marine'),
    'Zombie':            ('Zombie',           'zombie'),
    'Zombie_Sergeant':   ('Zombie Sergeant',  'zombie_sgt'),
}


def run_grit(png_path, output_name):
    """
    Run grit on a single PNG file.
    Grit outputs to CWD, so we run from grit_conversions/ dir
    and then grab the .c file.
    """
    cmd = [GRIT_EXE, png_path] + GRIT_FLAGS
    result = subprocess.run(cmd, cwd=GRIT_DIR, capture_output=True, text=True)

    if result.returncode != 0:
        print(f"    ERROR running grit: {result.stderr}")
        return None

    # Grit creates the .c file in CWD with the input filename
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

    # Look for: const unsigned int SomeName_000Tiles[196]
    match = re.search(r'const unsigned int (\w+Tiles)\[(\d+)\]', content)
    if match:
        return match.group(1), int(match.group(2))
    return None, None


def process_enemy(enemy_folder, display_name, c_prefix):
    """Process all frames for one enemy type."""
    frames_dir = os.path.join(FRAMES_BASE, enemy_folder)
    if not os.path.exists(frames_dir):
        print(f"  Skipping {display_name}: frames dir not found at {frames_dir}")
        return

    # Get all frame PNGs (not preview images)
    frame_files = sorted([f for f in os.listdir(frames_dir)
                          if f.endswith('.png') and not f.startswith('_')])

    if not frame_files:
        print(f"  Skipping {display_name}: no frame PNGs found")
        return

    # Create output directory
    output_dir = os.path.join(SPRITES_BASE, c_prefix)
    os.makedirs(output_dir, exist_ok=True)

    print(f"\n{'='*50}")
    print(f"Processing {display_name}: {len(frame_files)} frames")
    print(f"Output: {os.path.relpath(output_dir, PROJECT_ROOT)}")
    print(f"{'='*50}")

    tile_arrays = []  # (array_name, array_size, frame_index)
    tiles_per_frame = None

    for i, frame_file in enumerate(frame_files):
        frame_path = os.path.join(frames_dir, frame_file)

        # Run grit
        c_file = run_grit(frame_path, f"{c_prefix}_{i:03d}")
        if c_file is None:
            continue

        # Get array info
        array_name, array_size = get_tiles_array_name(c_file)
        if array_name is None:
            print(f"    WARNING: Could not parse array from {c_file}")
            continue

        if tiles_per_frame is None:
            tiles_per_frame = array_size

        tile_arrays.append((array_name, array_size, i))

        # Move .c file to output directory
        dest_c = os.path.join(output_dir, os.path.basename(c_file))
        shutil.move(c_file, dest_c)

        # Progress indicator
        if (i + 1) % 10 == 0 or i == len(frame_files) - 1:
            print(f"  Converted {i + 1}/{len(frame_files)} frames...")

    # Copy PNG frames to output dir for reference (in a png subfolder)
    png_dir = os.path.join(output_dir, 'png')
    os.makedirs(png_dir, exist_ok=True)
    for frame_file in frame_files:
        src = os.path.join(frames_dir, frame_file)
        dst = os.path.join(png_dir, frame_file)
        shutil.copy2(src, dst)
    # Copy preview too
    for f in os.listdir(frames_dir):
        if f.startswith('_preview_'):
            shutil.copy2(os.path.join(frames_dir, f), os.path.join(png_dir, f))

    # Generate header file with all extern declarations and frame table
    generate_header(output_dir, c_prefix, display_name, tile_arrays, tiles_per_frame)

    print(f"  Done! {len(tile_arrays)} .c files + header generated")
    return len(tile_arrays)


def generate_header(output_dir, c_prefix, display_name, tile_arrays, tiles_per_frame):
    """
    Generate a header file that:
    - Extern-declares all tile arrays
    - Provides a frame pointer table for easy animation
    - Documents frame count and tile size info
    """
    header_name = f"{c_prefix}_sprites.h"
    header_path = os.path.join(output_dir, header_name)
    guard = f"__{c_prefix.upper()}_SPRITES_H__"

    bytes_per_frame = tiles_per_frame * 4 if tiles_per_frame else 0

    lines = []
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append(f"")
    lines.append(f"/*")
    lines.append(f" * {display_name} sprite frames")
    lines.append(f" * Auto-generated by grit_all_frames.py")
    lines.append(f" *")
    lines.append(f" * Total frames: {len(tile_arrays)}")
    lines.append(f" * Frame size: 56x56 px (7x7 tiles)")
    lines.append(f" * Tiles per frame: {tiles_per_frame} u32 words = {bytes_per_frame} bytes")
    lines.append(f" *")
    lines.append(f" * Usage:")
    lines.append(f" *   // To swap animation frame, copy tile data into char memory:")
    lines.append(f" *   copymem((void*)CharMem(n), (void*){c_prefix.upper()}_FRAMES[frameIndex], {bytes_per_frame});")
    lines.append(f" */")
    lines.append(f"")

    # Extern declarations for each tile array
    lines.append(f"/* Individual frame tile data (each is {tiles_per_frame} u32 words = {bytes_per_frame} bytes) */")
    for array_name, array_size, idx in tile_arrays:
        lines.append(f"extern const unsigned int {array_name}[{array_size}];")

    lines.append(f"")
    lines.append(f"/* Number of frames */")
    lines.append(f"#define {c_prefix.upper()}_FRAME_COUNT {len(tile_arrays)}")
    lines.append(f"")
    lines.append(f"/* Bytes per frame (for copymem) */")
    lines.append(f"#define {c_prefix.upper()}_FRAME_BYTES {bytes_per_frame}")
    lines.append(f"")

    # Frame pointer lookup table
    lines.append(f"/* Frame pointer table for animation */")
    lines.append(f"static const unsigned int* const {c_prefix.upper()}_FRAMES[{len(tile_arrays)}] = {{")
    for i, (array_name, array_size, idx) in enumerate(tile_arrays):
        comma = "," if i < len(tile_arrays) - 1 else ""
        lines.append(f"    {array_name}{comma}")
    lines.append(f"}};")
    lines.append(f"")
    lines.append(f"#endif /* {guard} */")
    lines.append(f"")

    with open(header_path, 'w') as f:
        f.write('\n'.join(lines))

    print(f"  Header generated: {header_name}")


def main():
    print(f"Project root: {PROJECT_ROOT}")
    print(f"Grit: {GRIT_EXE}")
    print(f"Frames source: {FRAMES_BASE}")
    print(f"Output base: {SPRITES_BASE}")

    if not os.path.exists(GRIT_EXE):
        print(f"\nERROR: grit.exe not found at {GRIT_EXE}")
        return

    os.makedirs(SPRITES_BASE, exist_ok=True)

    total = 0
    for folder, (display_name, c_prefix) in ENEMIES.items():
        count = process_enemy(folder, display_name, c_prefix)
        if count:
            total += count

    print(f"\n{'='*60}")
    print(f"ALL DONE! Converted {total} total frames.")
    print(f"{'='*60}")
    print(f"\nOrganized in: src/vbdoom/assets/images/sprites/")
    print(f"  marine/          - {c_prefix}_sprites.h + .c files")
    print(f"  zombie/          - {c_prefix}_sprites.h + .c files")
    print(f"  zombie_sergeant/ - {c_prefix}_sprites.h + .c files")
    print(f"\nEach folder has a header with:")
    print(f"  - Extern declarations for all tile arrays")
    print(f"  - ENEMY_FRAMES[] pointer table for animation")
    print(f"  - ENEMY_FRAME_COUNT and ENEMY_FRAME_BYTES defines")


if __name__ == '__main__':
    main()
