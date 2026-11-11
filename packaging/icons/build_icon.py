#!/usr/bin/env python3
"""Rebuild lanlink.png and the multi-size Windows .ico from the source PNG.

The source artwork is an original LanLink mark (chat + small LAN), not a
Telegram logo. Run this script from any directory; it writes next to itself.
"""
from __future__ import annotations

import io
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw

SIZES = (16, 32, 48, 256)
TEAL_TOP = (16, 96, 78)
TEAL_BOTTOM = (8, 56, 48)
MINT = (232, 245, 240, 255)
AMBER = (246, 196, 69, 255)
GOLD = (255, 229, 160, 255)
OUTLINE = (8, 56, 48, 255)


def render(size: int) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    margin = max(1, size // 32)
    radius = max(3, size * 22 // 100)
    tile = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw_tile = ImageDraw.Draw(tile)
    for y in range(size):
        t = y / max(1, size - 1)
        color = tuple(int(TEAL_TOP[i] * (1 - t) + TEAL_BOTTOM[i] * t) for i in range(3)) + (255,)
        draw_tile.line([(0, y), (size, y)], fill=color)
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [margin, margin, size - 1 - margin, size - 1 - margin], radius=radius, fill=255
    )
    image.paste(tile, (0, 0), mask)
    draw = ImageDraw.Draw(image)

    def bubble(x0, y0, x1, y1, fill, tail):
        r = max(2, (y1 - y0) // 3)
        draw.rounded_rectangle([x0, y0, x1, y1], radius=r, fill=fill)
        if tail == "left":
            draw.polygon(
                [(x0 + r // 2, y1 - 1), (x0, y1 + (y1 - y0) // 4), (x0 + r, y1 - 1)],
                fill=fill,
            )
        else:
            draw.polygon(
                [(x1 - r, y1 - 1), (x1, y1 + (y1 - y0) // 4), (x1 - r // 2, y1 - 1)],
                fill=fill,
            )

    bubble(size * 16 // 100, size * 20 // 100, size * 58 // 100, size * 47 // 100, MINT, "left")
    bubble(size * 42 // 100, size * 36 // 100, size * 84 // 100, size * 63 // 100, AMBER, "right")
    nodes = [
        (size * 28 // 100, size * 78 // 100),
        (size * 50 // 100, size * 70 // 100),
        (size * 72 // 100, size * 78 // 100),
    ]
    width = max(1, size // 28)
    draw.line([nodes[0], nodes[1]], fill=GOLD, width=width)
    draw.line([nodes[1], nodes[2]], fill=GOLD, width=width)
    r = max(2, size * 5 // 100)
    outline_w = max(1, size // 64)
    for x, y in nodes:
        draw.ellipse([x - r, y - r, x + r, y + r], fill=GOLD, outline=OUTLINE, width=outline_w)
    return image


def write_ico(path: Path, images: list[Image.Image]) -> None:
    entries, blobs = [], []
    offset = 6 + 16 * len(images)
    for image in images:
        buf = io.BytesIO()
        image.save(buf, format="PNG")
        data = buf.getvalue()
        width, height = image.size
        entries.append(
            struct.pack(
                "<BBBBHHII",
                0 if width >= 256 else width,
                0 if height >= 256 else height,
                0,
                0,
                1,
                32,
                len(data),
                offset,
            )
        )
        blobs.append(data)
        offset += len(data)
    path.write_bytes(struct.pack("<HHH", 0, 1, len(images)) + b"".join(entries) + b"".join(blobs))


def main() -> int:
    folder = Path(__file__).resolve().parent
    source_path = folder / "lanlink-source.png"
    if source_path.exists():
        source = Image.open(source_path).convert("RGBA")
    else:
        source = render(1024)
        source.save(source_path, "PNG")
    source.resize((256, 256), Image.Resampling.LANCZOS).save(folder / "lanlink.png", "PNG")
    # Render each ICO size from the vector-like source script for crisp small glyphs,
    # falling back to a resize of the committed source PNG only when requested.
    write_ico(folder / "lanlink.ico", [render(size) for size in SIZES])
    print(f"wrote {folder / 'lanlink.png'} and {folder / 'lanlink.ico'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
