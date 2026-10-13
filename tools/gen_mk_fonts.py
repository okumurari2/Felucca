#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Extra UI faces for the MAKU preview styles (firmware/src/ui_maku_style.c, built with -DMK_STYLES; the web
emulator only, run by web/emu/build.sh).  The shipped firmware does not use them.

  gen_mk_fonts.py OUT.h
    RS / RM / RL   Fredoka (SIL OFL 1.1, assets/fonts/preview/) at 500 / 600 / 700, 12 / 15 / 28 px: the rounded look
    DM / DL        DotGothic16 (SIL OFL 1.1, web/emu/fonts/, converted from the emulator's ASCII woff) at its native
                   16 px and at 2x: a pixel font stays crisp only on whole pixels
"""
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
L_CHARS = " #+-./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"   # (Z too: the verbs' values)


def main():
    out = sys.argv[1]
    from fontTools.ttLib import TTFont
    with tempfile.TemporaryDirectory() as tmp:
        dot = Path(tmp) / "DotGothic16.ttf"
        f = TTFont(str(ROOT / "web/emu/fonts/DotGothic16-subset.woff"))
        f.flavor = None
        f.save(str(dot))
        fre = ROOT / "assets/fonts/preview/Fredoka.ttf"
        extras = ",".join(hex(ord(c)) for c in L_CHARS[1:])
        subprocess.run([sys.executable, str(ROOT / "tools/gen_aa_font.py"), out,
                        "--face", f"RS={fre}@500:12:32-126", "--face", f"RM={fre}@600:15:32-126",
                        "--face", f"RL={fre}@700:28:32-32+{extras}",
                        "--face", f"DM={dot}:16:32-126", "--face", f"DL={dot}:32:32-126"], check=True)


if __name__ == "__main__":
    main()
