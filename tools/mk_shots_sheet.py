#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""One comparison sheet from tools/mk_shots.mjs's tiles:  mk_shots_sheet.py TILES_DIR OUT.png"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

tiles, out = Path(sys.argv[1]), sys.argv[2]
COLS = [("0 NOW", 0), ("1 FUWA", 1), ("2 MARU", 2), ("3 RPG", 3), ("4 CAVE", 4)]
ROWS = ["w7", "w99", "sink"]
T, G, HEAD = 360, 14, 44                     # a screen drawn 360 px (1.5x of the 720 px capture), the gap, the header
font = ImageFont.truetype(str(Path(__file__).resolve().parents[1] / "assets/fonts/InterTight[wght].ttf"), 26)
sheet = Image.new("RGB", (G + len(COLS) * (T + G), HEAD + len(ROWS) * (T + G)), (30, 30, 34))
d = ImageDraw.Draw(sheet)
for c, (name, s) in enumerate(COLS):
    x = G + c * (T + G)
    d.text((x, 8), name, fill=(235, 235, 240), font=font)
    for r, row in enumerate(ROWS):
        im = Image.open(tiles / f"s{s}_{row}.png").convert("RGB").resize((T, T), Image.LANCZOS)
        sheet.paste(im, (x, HEAD + r * (T + G)))
Path(out).parent.mkdir(parents=True, exist_ok=True)
sheet.save(out)
print("sheet:", out, sheet.size)
