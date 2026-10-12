#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""How much of the app slot a build uses, and where it goes (after ./build.sh).
  tools/size_report.py [BUILD_DIR] [--top N]     default build/, N = 25
Reads build/felucca.bin (the image: .text + the .ram_text / .data load copies) and, through the JieLi objdump
($JIELI_TOOLCHAIN/common/bin/objdump, default ~/.jieli/toolchain), build/felucca.elf: the sections and the biggest
symbols. The slot size is APP_SLOT of tools/fm1pkg_make.py. The objdump -t column layout is assumed to be
'addr flags section size name'; if a line does not fit it is skipped (the first lines are echoed with --debug)."""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
args = [a for a in sys.argv[1:] if not a.startswith("--")]
top = int(sys.argv[sys.argv.index("--top") + 1]) if "--top" in sys.argv else 25
build = Path(args[0]) if args and not args[0].isdigit() else ROOT / "build"
slot = int(re.search(r"^APP_SLOT\s*=\s*(0x[0-9A-Fa-f]+)", (ROOT / "tools" / "fm1pkg_make.py").read_text(), re.M).group(1), 16)
img = (build / "felucca.bin").stat().st_size
print(f"image   {img:>8} B   slot {slot} B   free {slot - img} B ({100 * img / slot:.1f} % used)")
tc = Path(os.environ.get("JIELI_TOOLCHAIN", Path.home() / ".jieli" / "toolchain"))
objdump = tc / "common" / "bin" / "objdump"
elf = build / "felucca.elf"
if not objdump.exists() or not elf.exists():
    sys.exit("(no objdump / felucca.elf: the image size above is the number that matters)")
run = lambda *a: subprocess.run([str(objdump), *a, str(elf)], capture_output=True, text=True, check=True).stdout
print("\nsections (size in B):")
for ln in run("-h").splitlines():
    f = ln.split()
    if len(f) >= 4 and f[0].isdigit() and f[1].startswith("."):
        print(f"  {f[1]:<12} {int(f[2], 16):>8}  at 0x{f[3]}")
syms = []
for ln in run("-t").splitlines():
    f = ln.split()
    if len(f) >= 5 and re.fullmatch(r"[0-9a-fA-F]+", f[-2]) and f[-3].startswith(".") and int(f[-2], 16) > 0:
        syms.append((int(f[-2], 16), f[-3], f[-1]))
syms.sort(reverse=True)
print(f"\nthe {top} biggest symbols:")
for size, sec, name in syms[:top]:
    print(f"  {size:>7}  {sec:<10} {name}")
by = {}
for size, sec, name in syms:
    by[sec] = by.get(sec, 0) + size
print("\nsymbol bytes per section: " + ", ".join(f"{k} {v}" for k, v in sorted(by.items())))
