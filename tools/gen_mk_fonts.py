#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Extra UI faces for the MAKU preview styles (firmware/src/ui_maku_style.c, built with -DMK_STYLES; the web
emulator only, run by web/emu/build.sh).  The shipped firmware does not use them.

  gen_mk_fonts.py OUT.h
    RS / RM / RL   Fredoka (SIL OFL 1.1, assets/fonts/preview/) at 500 / 600 / 700, 12 / 15 / 28 px: the rounded look
    DM / DL        DotGothic16 (SIL OFL 1.1, web/emu/fonts/, converted from the emulator's ASCII woff) at its native
                   16 px and at 2x: a pixel font stays crisp only on whole pixels
    IL / IB        Inter Tight (assets/fonts/) at 200 / 44 px and at 700 / 52 px (digits only): the big numerals of HUE and GRID
    PS / PM / PL   IBM Plex Mono (SIL OFL 1.1, assets/fonts/preview/) at 12 / 15 / 30 px: SCOPE's readouts
"""
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
L_CHARS = " #+-./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"   # (Z too: the verbs' values)
DIGITS = "+-./0123456789"                                 # the giant face: numbers only (a word is set in the next size down)


def main():
    out = sys.argv[1]
    from fontTools.ttLib import TTFont
    with tempfile.TemporaryDirectory() as tmp:
        dot = Path(tmp) / "DotGothic16.ttf"
        f = TTFont(str(ROOT / "web/emu/fonts/DotGothic16-subset.woff"))
        f.flavor = None
        f.save(str(dot))
        fre = ROOT / "assets/fonts/preview/Fredoka.ttf"
        px = ROOT / "assets/fonts/preview"
        it = ROOT / "assets/fonts/InterTight[wght].ttf"
        extras = ",".join(hex(ord(c)) for c in L_CHARS[1:])
        dig = ",".join(hex(ord(c)) for c in DIGITS)
        subprocess.run([sys.executable, str(ROOT / "tools/gen_aa_font.py"), out,
                        "--face", f"RS={fre}@500:12:32-126", "--face", f"RM={fre}@600:15:32-126",
                        "--face", f"RL={fre}@700:28:32-32+{extras}",
                        "--face", f"DM={dot}:16:32-126", "--face", f"DL={dot}:32:32-126",
                        "--face", f"IL={it}@200:44:32-32+{extras}", "--face", f"IB={it}@700:52:32-32+{dig}",
                        "--face", f"PS={px}/IBMPlexMono-Regular.ttf:12:32-126", "--face", f"PM={px}/IBMPlexMono-Medium.ttf:15:32-126",
                        "--face", f"PL={px}/IBMPlexMono-Medium.ttf:30:32-32+{extras}"], check=True)


if __name__ == "__main__":
    main()
