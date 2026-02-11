"""
preview_vb_colors.py

Convert images to the Virtual Boy 4-level monochrome red palette and save
preview PNGs so you can see how they look on the VB display.

Palette:
  0 = (  0,   0,   0) -- black / transparent
  1 = ( 80,   0,   0) -- dark red
  2 = (170,   0,   0) -- medium red
  3 = (255,   0,   0) -- bright red
"""

import os
import sys
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

VB_PALETTE = {
    0: (0, 0, 0),
    1: (80, 0, 0),
    2: (170, 0, 0),
    3: (255, 0, 0),
}

# Bayer 4x4 ordered dither matrix (normalized 0-1)
BAYER_4x4 = [
    [0/16, 8/16, 2/16, 10/16],
    [12/16, 4/16, 14/16, 6/16],
    [3/16, 11/16, 1/16, 9/16],
    [15/16, 7/16, 13/16, 5/16],
]

DITHER_STRENGTH = 40  # tweak for more or less dithering


def luminance(r, g, b):
    return 0.299 * r + 0.587 * g + 0.114 * b


def convert_to_vb(img):
    """Convert an RGBA/RGB image to VB 4-level red palette with dithering."""
    img = img.convert('RGBA')
    w, h = img.size
    pix = img.load()
    out = Image.new('RGB', (w, h), (0, 0, 0))
    opix = out.load()

    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if a < 128:
                opix[x, y] = VB_PALETTE[0]
                continue

            lum = luminance(r, g, b)
            bayer = BAYER_4x4[y % 4][x % 4]
            dithered = lum + (bayer - 0.5) * DITHER_STRENGTH
            dithered = max(0, min(255, dithered))

            if dithered < 42:
                vb = 0
            elif dithered < 110:
                vb = 1
            elif dithered < 180:
                vb = 2
            else:
                vb = 3

            opix[x, y] = VB_PALETTE[vb]

    return out


def main():
    images = [
        "wolf_titlescreen.png",
        "wolfen_doom_logo.png",
    ]

    for name in images:
        path = os.path.join(SCRIPT_DIR, name)
        if not os.path.exists(path):
            print(f"  SKIP: {path} not found")
            continue

        img = Image.open(path)
        print(f"Converting {name} ({img.width}x{img.height})...")

        vb_img = convert_to_vb(img)

        out_name = os.path.splitext(name)[0] + "_vb_preview.png"
        out_path = os.path.join(SCRIPT_DIR, out_name)
        vb_img.save(out_path)
        print(f"  Saved: {out_path}")

    print("\nDone!")


if __name__ == '__main__':
    main()
