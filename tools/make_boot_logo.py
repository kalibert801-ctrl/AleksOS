#!/usr/bin/env python3
"""
make_boot_logo.py  —  AleksOS boot image converter
======================================================
Converts any image → /boot.raw for the ESP32 boot screen.

Format: 320×240, RGB565, pre-inverted for ST7789 INVON, little-endian.
Size:   153 600 bytes (320 × 240 × 2).

Usage:
    python make_boot_logo.py <source_image>   → writes boot.raw
    python make_boot_logo.py                  → looks for boot_logo_source.jpg

Then copy boot.raw to the root of your SD card.
"""

from PIL import Image
import struct, sys, os

WIDTH, HEIGHT = 320, 240


def center_crop_resize(img: Image.Image) -> Image.Image:
    """Crop to 4:3 (centred) then resize to 320×240."""
    w, h = img.size
    target_ratio = WIDTH / HEIGHT
    if w / h > target_ratio:
        new_w = int(h * target_ratio)
        left  = (w - new_w) // 2
        img   = img.crop((left, 0, left + new_w, h))
    else:
        new_h = int(w / target_ratio)
        top   = (h - new_h) // 2
        img   = img.crop((0, top, w, top + new_h))
    return img.resize((WIDTH, HEIGHT), Image.LANCZOS)


def to_boot_raw(img: Image.Image) -> bytearray:
    """Convert PIL image (RGB) → raw bytes for boot.raw."""
    data = bytearray()
    pixels = img.load()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            r, g, b = pixels[x, y]
            rgb565   = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            inverted = (~rgb565) & 0xFFFF   # pre-invert for ST7789 INVON hardware
            data    += struct.pack('<H', inverted)   # little-endian
    return data


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "boot_logo_source.jpg"
    if not os.path.exists(src):
        print(f"[ERROR] File not found: '{src}'")
        print("Usage: python make_boot_logo.py <image_file>")
        sys.exit(1)

    print(f"[INFO] Reading: {src}")
    img = Image.open(src).convert("RGB")
    print(f"[INFO] Original size: {img.size[0]}×{img.size[1]}")

    img = center_crop_resize(img)
    print(f"[INFO] Cropped and resized to {WIDTH}×{HEIGHT}")

    data = to_boot_raw(img)
    assert len(data) == WIDTH * HEIGHT * 2, "Internal error: wrong data size"

    out = "boot.raw"
    with open(out, "wb") as f:
        f.write(data)

    print(f"[OK]   Written {len(data):,} bytes → {out}")
    print(f"[OK]   Copy '{out}' to the root of your SD card.")


if __name__ == "__main__":
    main()
