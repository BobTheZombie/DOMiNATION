#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 128
RESOURCE_OUT = Path("content/textures/icons/resources")
UNIT_OUT = Path("content/textures/icons/units")


def _new() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img, "RGBA")


def resource_icons() -> dict[str, Image.Image]:
    icons: dict[str, Image.Image] = {}

    # Food: wheat bundle.
    img, d = _new()
    d.rectangle((22, 86, 106, 96), fill=(58, 46, 24, 255))
    for x in (40, 56, 72, 88):
        d.line((x, 86, x, 36), fill=(238, 197, 82, 255), width=5)
        d.polygon([(x, 36), (x - 9, 50), (x + 9, 50)], fill=(250, 214, 112, 255))
    icons["food"] = img

    # Wood: log + grain rings.
    img, d = _new()
    d.rounded_rectangle((18, 46, 110, 88), radius=20, fill=(116, 76, 38, 255))
    d.ellipse((18, 46, 56, 88), fill=(178, 128, 78, 255))
    d.ellipse((26, 54, 48, 80), outline=(110, 72, 38, 255), width=3)
    d.ellipse((31, 59, 43, 75), outline=(96, 62, 32, 255), width=2)
    icons["wood"] = img

    # Metal: ingot stack.
    img, d = _new()
    d.polygon([(28, 78), (48, 58), (92, 58), (72, 78)], fill=(192, 200, 210, 255))
    d.polygon([(32, 54), (52, 34), (96, 34), (76, 54)], fill=(224, 230, 236, 255))
    d.polygon([(24, 102), (44, 82), (88, 82), (68, 102)], fill=(150, 162, 176, 255))
    icons["metal"] = img

    # Wealth: coin.
    img, d = _new()
    d.ellipse((24, 24, 104, 104), fill=(242, 189, 62, 255), outline=(130, 88, 20, 255), width=4)
    d.line((64, 42, 64, 86), fill=(142, 92, 24, 255), width=8)
    d.arc((42, 38, 86, 60), start=30, end=160, fill=(142, 92, 24, 255), width=6)
    d.arc((42, 68, 86, 90), start=210, end=340, fill=(142, 92, 24, 255), width=6)
    icons["wealth"] = img

    # Knowledge: open book.
    img, d = _new()
    d.rounded_rectangle((18, 34, 62, 98), radius=8, fill=(228, 236, 250, 255), outline=(74, 88, 120, 255), width=3)
    d.rounded_rectangle((66, 34, 110, 98), radius=8, fill=(228, 236, 250, 255), outline=(74, 88, 120, 255), width=3)
    d.line((64, 34, 64, 98), fill=(74, 88, 120, 255), width=4)
    d.line((30, 50, 52, 50), fill=(122, 136, 168, 255), width=3)
    d.line((76, 50, 98, 50), fill=(122, 136, 168, 255), width=3)
    icons["knowledge"] = img

    # Oil: barrel drop symbol.
    img, d = _new()
    d.rounded_rectangle((34, 24, 94, 100), radius=14, fill=(44, 48, 58, 255), outline=(166, 176, 196, 255), width=3)
    d.line((34, 44, 94, 44), fill=(166, 176, 196, 255), width=3)
    d.line((34, 76, 94, 76), fill=(166, 176, 196, 255), width=3)
    d.polygon([(64, 52), (52, 72), (64, 88), (76, 72)], fill=(32, 180, 220, 255))
    icons["oil"] = img

    return icons


def unit_icons() -> dict[str, Image.Image]:
    fg = (245, 245, 245, 255)
    icons: dict[str, Image.Image] = {}

    # Monochrome military silhouettes.
    img, d = _new()
    d.ellipse((54, 20, 74, 40), fill=fg)
    d.rectangle((50, 40, 78, 84), fill=fg)
    d.rectangle((36, 48, 92, 60), fill=fg)
    d.rectangle((50, 84, 60, 108), fill=fg)
    d.rectangle((68, 84, 78, 108), fill=fg)
    icons["infantry"] = img

    img, d = _new()
    d.polygon([(18, 84), (46, 58), (98, 58), (110, 84), (86, 84), (72, 100), (46, 100), (34, 84)], fill=fg)
    d.ellipse((82, 46, 102, 66), fill=fg)
    d.rectangle((16, 82, 32, 96), fill=fg)
    icons["cavalry"] = img

    img, d = _new()
    d.ellipse((54, 22, 74, 42), fill=fg)
    d.rectangle((52, 42, 76, 84), fill=fg)
    d.line((64, 50, 96, 30), fill=fg, width=7)
    d.arc((80, 20, 114, 56), start=220, end=40, fill=fg, width=6)
    icons["archer"] = img

    img, d = _new()
    d.rectangle((24, 80, 104, 92), fill=fg)
    d.rectangle((32, 66, 62, 80), fill=fg)
    d.rectangle((62, 62, 110, 76), fill=fg)
    d.rectangle((90, 58, 118, 66), fill=fg)
    icons["siege"] = img

    img, d = _new()
    d.rounded_rectangle((24, 66, 104, 96), radius=8, fill=fg)
    d.rectangle((48, 48, 88, 68), fill=fg)
    d.rectangle((60, 32, 68, 52), fill=fg)
    d.rectangle((68, 34, 108, 42), fill=fg)
    icons["tank"] = img

    img, d = _new()
    d.ellipse((52, 52, 76, 76), fill=fg)
    d.polygon([(64, 18), (72, 44), (56, 44)], fill=fg)
    d.polygon([(64, 110), (72, 84), (56, 84)], fill=fg)
    d.polygon([(18, 64), (44, 72), (44, 56)], fill=fg)
    d.polygon([(110, 64), (84, 72), (84, 56)], fill=fg)
    icons["drone"] = img

    img, d = _new()
    d.polygon([(18, 72), (64, 52), (110, 72), (84, 76), (64, 88), (44, 76)], fill=fg)
    d.rectangle((58, 38, 70, 58), fill=fg)
    icons["bomber"] = img

    img, d = _new()
    d.polygon([(20, 82), (36, 64), (92, 64), (108, 82), (96, 92), (32, 92)], fill=fg)
    d.polygon([(42, 64), (64, 46), (84, 64)], fill=fg)
    icons["naval_ship"] = img

    img, d = _new()
    d.polygon([(64, 20), (84, 88), (64, 76), (44, 88)], fill=fg)
    d.rectangle((58, 88, 70, 108), fill=fg)
    icons["missile"] = img

    return icons


def main() -> None:
    RESOURCE_OUT.mkdir(parents=True, exist_ok=True)
    UNIT_OUT.mkdir(parents=True, exist_ok=True)

    for name, img in resource_icons().items():
        out = RESOURCE_OUT / f"{name}_resource_icon_128.png"
        img.save(out, "PNG", optimize=True)
        print(f"Wrote {out}")

    for name, img in unit_icons().items():
        out = UNIT_OUT / f"{name}_unit_icon_monochrome_128.png"
        img.save(out, "PNG", optimize=True)
        print(f"Wrote {out}")


if __name__ == "__main__":
    main()
