"""
Extract individual sprite frames from VB-palette sprite sheets,
pad them to 8x8 tile boundaries, and make all frames the same
dimensions per enemy (for easy tile-swap animation on Virtual Boy).

Uses connected-component flood-fill to cleanly separate each sprite,
even when credit text or other elements bridge row gaps.

Usage: python extract_frames.py

Processes all vb_*.png files in the 'converted' folder.
Outputs individual frames to 'converted/frames/<enemy_name>/'.
"""

import os
import sys
from collections import deque
from PIL import Image

# VB palette - color 0 (black) is transparent/background
BG_COLOR = (0, 0, 0)

# Minimum sprite size to consider (filters out noise/small artifacts)
MIN_SPRITE_WIDTH = 14
MIN_SPRITE_HEIGHT = 14

# Maximum aspect ratio - filters out credit text (very wide, short)
MAX_ASPECT_RATIO = 4.0  # width/height > this = probably text, not a sprite


def is_background_px(r, g, b):
    """Check if pixel is background (black)."""
    return r == 0 and g == 0 and b == 0


def find_connected_components(img):
    """
    Find connected components of non-background pixels using BFS flood-fill.
    Returns a list of bounding boxes: [(x1, y1, x2, y2), ...].

    Uses 4-connectivity (up/down/left/right) to avoid merging diagonally
    adjacent sprites.
    """
    w, h = img.size
    pixels = img.load()

    # Create a visited mask
    visited = [[False] * w for _ in range(h)]

    components = []

    for start_y in range(h):
        for start_x in range(w):
            if visited[start_y][start_x]:
                continue

            r, g, b = pixels[start_x, start_y]
            if is_background_px(r, g, b):
                visited[start_y][start_x] = True
                continue

            # BFS flood-fill from this unvisited non-bg pixel
            queue = deque()
            queue.append((start_x, start_y))
            visited[start_y][start_x] = True

            min_x, min_y = start_x, start_y
            max_x, max_y = start_x, start_y

            while queue:
                cx, cy = queue.popleft()

                min_x = min(min_x, cx)
                min_y = min(min_y, cy)
                max_x = max(max_x, cx)
                max_y = max(max_y, cy)

                # 4-connectivity neighbors
                for nx, ny in [(cx - 1, cy), (cx + 1, cy), (cx, cy - 1), (cx, cy + 1)]:
                    if 0 <= nx < w and 0 <= ny < h and not visited[ny][nx]:
                        visited[ny][nx] = True
                        pr, pg, pb = pixels[nx, ny]
                        if not is_background_px(pr, pg, pb):
                            queue.append((nx, ny))

            components.append((min_x, min_y, max_x, max_y))

    return components


def pixel_density(img, x1, y1, x2, y2):
    """
    Calculate the ratio of non-bg pixels to total pixels in a bbox.
    Sprite characters typically have 30-80% density, while text blocks
    and noise have lower or higher.
    """
    pixels = img.load()
    total = (x2 - x1 + 1) * (y2 - y1 + 1)
    count = 0
    for y in range(y1, y2 + 1):
        for x in range(x1, x2 + 1):
            r, g, b = pixels[x, y]
            if not is_background_px(r, g, b):
                count += 1
    return count / max(total, 1)


def filter_components(components, img):
    """
    Filter out components that are:
    - Too small (noise/individual pixels)
    - Too wide (credit text)
    - Have very low pixel density (scattered noise)
    - Look like text blocks (positioned in corners, specific density)
    """
    valid = []
    removed_reasons = {}

    for (x1, y1, x2, y2) in components:
        w = x2 - x1 + 1
        h = y2 - y1 + 1
        reason = None

        if w < MIN_SPRITE_WIDTH or h < MIN_SPRITE_HEIGHT:
            reason = "too_small"
        elif w / max(h, 1) > MAX_ASPECT_RATIO:
            reason = "too_wide"
        else:
            # Check pixel density to filter text blocks
            # Text typically has 20-40% density, sprites have 40-80%
            density = pixel_density(img, x1, y1, x2, y2)
            if density < 0.10:
                reason = "too_sparse"

            # Check if it looks like a text block
            # (positioned in upper-right area, relatively low density, smallish)
            if reason is None and x1 > img.width * 0.6 and y1 < img.height * 0.25:
                if density < 0.45 and w > 80 and h > 40:
                    reason = "credit_text"
                    print(f"    Filtered credit text: {w}x{h} at ({x1},{y1}) density={density:.2f}")

        if reason:
            removed_reasons[reason] = removed_reasons.get(reason, 0) + 1
        else:
            valid.append((x1, y1, x2, y2))

    for reason, count in sorted(removed_reasons.items()):
        print(f"    Filtered {count} elements ({reason})")

    return valid


def pad_to_tile_boundary(width, height, tile_size=8):
    """Round up dimensions to the nearest multiple of tile_size."""
    new_w = ((width + tile_size - 1) // tile_size) * tile_size
    new_h = ((height + tile_size - 1) // tile_size) * tile_size
    return new_w, new_h


def save_frames(img, bboxes, output_dir, enemy_name):
    """
    Save extracted sprites as individual PNGs, all padded to the same
    8x8-aligned dimensions for easy tile-swap animation.
    """
    os.makedirs(output_dir, exist_ok=True)

    if not bboxes:
        print(f"  No sprites found!")
        return []

    # Sort: top to bottom, then left to right (reading order)
    bboxes.sort(key=lambda b: (b[1], b[0]))

    # Show size distribution
    sizes = [(x2 - x1 + 1, y2 - y1 + 1) for x1, y1, x2, y2 in bboxes]
    widths = sorted(set(w for w, h in sizes))
    heights = sorted(set(h for w, h in sizes))
    median_w = sorted(w for w, h in sizes)[len(sizes) // 2]
    median_h = sorted(h for w, h in sizes)[len(sizes) // 2]
    max_w = max(w for w, h in sizes)
    max_h = max(h for w, h in sizes)

    print(f"  Size range: {min(w for w,h in sizes)}-{max_w} x {min(h for w,h in sizes)}-{max_h}")
    print(f"  Median size: {median_w}x{median_h}")

    # Pad to 8x8 tile boundary
    tile_w, tile_h = pad_to_tile_boundary(max_w, max_h)

    print(f"  Total sprites: {len(bboxes)}")
    print(f"  Max sprite size: {max_w}x{max_h} px")
    print(f"  Padded tile size: {tile_w}x{tile_h} px ({tile_w // 8}x{tile_h // 8} tiles)")
    print(f"  Tiles per frame: {(tile_w // 8) * (tile_h // 8)}")

    frames = []
    for i, (x1, y1, x2, y2) in enumerate(bboxes):
        sprite_w = x2 - x1 + 1
        sprite_h = y2 - y1 + 1

        # Create padded frame (black background)
        frame = Image.new('RGB', (tile_w, tile_h), BG_COLOR)

        # Center horizontally, align to bottom (feet on ground)
        paste_x = (tile_w - sprite_w) // 2
        paste_y = tile_h - sprite_h  # Bottom-aligned

        # Copy sprite pixels
        crop = img.crop((x1, y1, x2 + 1, y2 + 1))
        frame.paste(crop, (paste_x, paste_y))

        frame_path = os.path.join(output_dir, f"{enemy_name}_{i:03d}.png")
        frame.save(frame_path)
        frames.append(frame_path)

    return frames


def create_preview_grid(frames_dir, enemy_name, cols=8):
    """Create a preview grid showing all frames."""
    frame_files = sorted([f for f in os.listdir(frames_dir)
                          if f.endswith('.png') and not f.startswith('_')])
    if not frame_files:
        return

    sample = Image.open(os.path.join(frames_dir, frame_files[0]))
    fw, fh = sample.size

    rows = (len(frame_files) + cols - 1) // cols
    gap = 2
    grid_w = fw * min(cols, len(frame_files)) + (min(cols, len(frame_files)) - 1) * gap
    grid_h = fh * rows + (rows - 1) * gap
    grid = Image.new('RGB', (grid_w, grid_h), (20, 0, 0))

    for i, fname in enumerate(frame_files):
        frame = Image.open(os.path.join(frames_dir, fname))
        col = i % cols
        row = i // cols
        x = col * (fw + gap)
        y = row * (fh + gap)
        grid.paste(frame, (x, y))

    preview_path = os.path.join(frames_dir, f"_preview_{enemy_name}.png")
    grid.save(preview_path)
    print(f"  Preview saved: _preview_{enemy_name}.png")


def process_spritesheet(input_path, base_output_dir):
    """Process a single sprite sheet."""
    basename = os.path.splitext(os.path.basename(input_path))[0]
    enemy_name = basename.replace('vb_', '').replace(' ', '_')

    print(f"\n{'='*50}")
    print(f"Processing: {basename}")
    print(f"{'='*50}")

    img = Image.open(input_path).convert('RGB')
    print(f"  Image size: {img.width}x{img.height}")

    # Detect connected components
    print("  Detecting sprites (flood-fill)...")
    sys.setrecursionlimit(500000)  # BFS doesn't use recursion, but just in case
    components = find_connected_components(img)
    print(f"  Found {len(components)} raw components")

    # Filter
    print("  Filtering...")
    valid = filter_components(components, img)

    # Save individual frames
    output_dir = os.path.join(base_output_dir, enemy_name)

    # Clear old frames first
    if os.path.exists(output_dir):
        for old_file in os.listdir(output_dir):
            os.remove(os.path.join(output_dir, old_file))

    frames = save_frames(img, valid, output_dir, enemy_name)

    # Create preview grid
    if frames:
        create_preview_grid(output_dir, enemy_name)

    return enemy_name, frames


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    converted_dir = os.path.join(script_dir, "converted")
    frames_base_dir = os.path.join(converted_dir, "frames")
    os.makedirs(frames_base_dir, exist_ok=True)

    # Find all vb_*.png files
    vb_files = sorted([f for f in os.listdir(converted_dir)
                       if f.lower().startswith('vb_') and f.lower().endswith('.png')])

    if not vb_files:
        print("No vb_*.png files found in converted/ folder!")
        return

    print(f"Found {len(vb_files)} sprite sheet(s) to process.\n")

    for f in vb_files:
        input_path = os.path.join(converted_dir, f)
        process_spritesheet(input_path, frames_base_dir)

    print(f"\n{'='*60}")
    print(f"ALL DONE!")
    print(f"{'='*60}")
    print(f"Frames saved to: {frames_base_dir}")
    print(f"\nEach enemy folder contains:")
    print(f"  - Individual frame PNGs (all same size, 8x8-tile-aligned)")
    print(f"  - Bottom-aligned (feet at bottom of tile grid)")
    print(f"  - A _preview_ grid image")
    print(f"\nTo convert for VB with grit:")
    print(f"  grit <frame>.png -fh! -ftc -gB2 -p! -m!")
    print(f"  (-m! = no map, raw tiles only -- swap tiles at runtime to animate)")


if __name__ == '__main__':
    main()
