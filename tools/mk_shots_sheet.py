#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""One comparison sheet from tools/mk_shots.mjs's tiles:  mk_shots_sheet.py TILES_DIR OUT.png [STYLES, e.g. 0,5,6,7,8 [SEEDS]]; one style: a strip of its worlds"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

tiles, out = Path(sys.argv[1]), sys.argv[2]
NAMES = {0: "0 NOW", 1: "1 FUWA", 2: "2 MARU", 3: "3 RPG", 4: "4 CAVE", 5: "5 ORBIT", 6: "6 GRID", 7: "7 HUE", 8: "8 SCOPE", 9: "9 CORNERS", 10: "10 FLANK", 11: "11 BLOOM", 12: "12 STAR", 13: "13 ICE", 14: "14 PARTS", 15: "15 KEY", 16: "16 AMBER", 17: "17 PAPER", 18: "18 PASTEL", 19: "19 STRIPS-R", 20: "20 STRIPS-BG", 21: "21 STRIPS-KEY", 22: "22 WATER", 23: "23 FOREST", 24: "24 SEA"}
COLS = [(NAMES[int(n)], int(n)) for n in (sys.argv[3] if len(sys.argv) > 3 else "0,1,2,3,4").split(",")]
SEEDS = [int(x) for x in (sys.argv[4] if len(sys.argv) > 4 else "7,99").split(",")]
T, G, HEAD = 360, 14, 44                     # a screen drawn 360 px (1.5x of the 720 px capture), the gap, the header
font = ImageFont.truetype(str(Path(__file__).resolve().parents[1] / "assets/fonts/InterTight[wght].ttf"), 26)
if len(COLS) == 1:                           # a strip: one look, its worlds side by side
    name, s_ = COLS[0]
    sheet = Image.new("RGB", (G + len(SEEDS) * (T + G), HEAD + T + G), (30, 30, 34))
    d = ImageDraw.Draw(sheet)
    d.text((G, 8), f"{name}: worlds {', '.join(map(str, SEEDS))}", fill=(235, 235, 240), font=font)
    for i, sd in enumerate(SEEDS):
        sheet.paste(Image.open(tiles / f"s{s_}_w{sd}.png").convert("RGB").resize((T, T), Image.LANCZOS), (G + i * (T + G), HEAD))
else:
    ROWS = [f"w{sd}" for sd in SEEDS] + ["sink"] + [r for r in ("turn", "vol") if all((tiles / f"s{c[1]}_{r}.png").exists() for c in COLS)]
    sheet = Image.new("RGB", (G + len(COLS) * (T + G), HEAD + len(ROWS) * (T + G)), (30, 30, 34))
    d = ImageDraw.Draw(sheet)
    for c, (name, s_) in enumerate(COLS):
        x = G + c * (T + G)
        d.text((x, 8), name, fill=(235, 235, 240), font=font)
        for r, row in enumerate(ROWS):
            im = Image.open(tiles / f"s{s_}_{row}.png").convert("RGB").resize((T, T), Image.LANCZOS)
            sheet.paste(im, (x, HEAD + r * (T + G)))
Path(out).parent.mkdir(parents=True, exist_ok=True)
sheet.save(out)
print("sheet:", out, sheet.size)
