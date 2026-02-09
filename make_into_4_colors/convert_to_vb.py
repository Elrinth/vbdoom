"""
Convert Doom sprite sheets to Virtual Boy 4-color red palette.

VB Palette (from vb_doom.png):
  Color 0: (0,   0, 0) - Black (transparent on VB -- ONLY for background!)
  Color 1: (85,  0, 0) - Dark red (darkest VISIBLE shade)
  Color 2: (164, 0, 0) - Medium red
  Color 3: (239, 0, 0) - Bright red

Option C conversion: sprite pixels ONLY use indices 1-3 (always opaque).
Index 0 (transparent) is ONLY used for the magenta background.
This ensures sprites are fully solid with no see-through artifacts,
allowing single-layer rendering on VB (1 world per enemy = more enemies).
"""

import os
import sys
from PIL import Image

# VB palette
VB_PALETTE = [
    (0,   0, 0),    # Color 0: Black (transparent)
    (85,  0, 0),    # Color 1: Dark red
    (164, 0, 0),    # Color 2: Medium red
    (239, 0, 0),    # Color 3: Bright red
]

# Magenta transparency color (with some tolerance)
MAGENTA_THRESHOLD = 30  # How close to pure magenta (255,0,255) a pixel must be


def is_magenta(r, g, b):
    """Check if a pixel is close to magenta (255, 0, 255)."""
    return (r > 220 and g < MAGENTA_THRESHOLD and b > 220)


def luminance(r, g, b):
    """Convert RGB to grayscale luminance (0-255)."""
    return int(0.299 * r + 0.587 * g + 0.114 * b)


def map_to_vb_color(gray):
    """
    Map a grayscale value (0-255) to VB palette indices 1-3 ONLY.

    Index 0 (transparent) is NEVER returned here -- it's reserved for
    the magenta background. All sprite pixels are opaque (indices 1-3).

    Thresholds split the luminance range into 3 bands:
    - Dark pixels (outlines, deep shadows) -> index 1 (dark red)
    - Medium pixels (body, mid-tones) -> index 2 (medium red)
    - Bright pixels (highlights, skin) -> index 3 (bright red)
    """
    if gray < 85:
        return 1   # Dark red (darkest visible shade)
    elif gray < 170:
        return 2   # Medium red
    else:
        return 3   # Bright red


def enhance_contrast(img):
    """
    Apply per-sprite adaptive contrast enhancement.
    Stretches the non-magenta pixel luminance range to use the full 0-255
    span, improving detail separation before quantization.
    """
    pixels = []
    for y in range(img.height):
        for x in range(img.width):
            r, g, b = img.getpixel((x, y))
            if not is_magenta(r, g, b):
                gray = luminance(r, g, b)
                if gray > 5:  # ignore near-black outline pixels
                    pixels.append(gray)

    if not pixels:
        return img

    # Use 2nd and 98th percentile to avoid outlier influence
    pixels.sort()
    lo = pixels[int(len(pixels) * 0.02)]
    hi = pixels[int(len(pixels) * 0.98)]
    if hi <= lo:
        return img

    # Stretch contrast
    out = img.copy()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b = img.getpixel((x, y))
            if not is_magenta(r, g, b):
                gray = luminance(r, g, b)
                if gray <= 5:
                    # Keep very dark pixels as-is
                    stretched = gray
                else:
                    # Linear stretch
                    stretched = int(255.0 * (gray - lo) / (hi - lo))
                    stretched = max(0, min(255, stretched))
                # Store stretched grayscale in R channel for later mapping
                out.putpixel((x, y), (stretched, stretched, stretched))
            else:
                out.putpixel((x, y), (r, g, b))
    return out


def convert_to_vb(input_path, output_path):
    """Convert a full-color sprite sheet to VB 4-color red palette."""
    img = Image.open(input_path).convert('RGB')

    # First pass: enhance contrast for better detail separation
    enhanced = enhance_contrast(img)

    out = Image.new('RGB', (img.width, img.height), (0, 0, 0))

    for y in range(img.height):
        for x in range(img.width):
            r, g, b = img.getpixel((x, y))

            if is_magenta(r, g, b):
                # Transparent -> black
                out.putpixel((x, y), VB_PALETTE[0])
            else:
                # Use the contrast-enhanced grayscale value
                er, _, _ = enhanced.getpixel((x, y))
                idx = map_to_vb_color(er)
                out.putpixel((x, y), VB_PALETTE[idx])

    out.save(output_path)
    print(f"  Converted: {os.path.basename(input_path)} -> {os.path.basename(output_path)}")

    # Print color distribution
    colors = {}
    for y in range(out.height):
        for x in range(out.width):
            c = out.getpixel((x, y))
            colors[c] = colors.get(c, 0) + 1
    total = out.width * out.height
    for i, pal in enumerate(VB_PALETTE):
        count = colors.get(pal, 0)
        pct = 100.0 * count / total
        print(f"    Color {i} {pal}: {count} px ({pct:.1f}%)")


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_dir = os.path.join(script_dir, "converted")
    os.makedirs(output_dir, exist_ok=True)

    # Find all PNG files in the same directory (excluding already converted)
    png_files = []
    for f in os.listdir(script_dir):
        if f.lower().endswith('.png') and not f.startswith('vb_'):
            png_files.append(f)

    if not png_files:
        print("No PNG files found to convert!")
        return

    print(f"Converting {len(png_files)} file(s) to VB 4-color red palette...")
    print()

    for f in sorted(png_files):
        input_path = os.path.join(script_dir, f)
        # Output name: vb_<original_name>
        base = os.path.splitext(f)[0]
        output_name = f"vb_{base}.png"
        output_path = os.path.join(output_dir, output_name)
        convert_to_vb(input_path, output_path)
        print()

    print(f"All conversions complete! Output in: {output_dir}")


if __name__ == '__main__':
    main()
