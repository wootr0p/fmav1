#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A fuzz script for the host build: random (often extreme) values on every parameter by NRPN while notes play.
  tools/fuzz_script.py SEED [CHANNELS] > build/fuzz.txt   (build/params.txt from tools/nrpn.c first)
CHANNELS > 1 (the four-track build): CC 3 = 1 first (MIDI channel 1), then each step on a random one of the channels."""
import random
import sys
from pathlib import Path
random.seed(int(sys.argv[1]))
nch = int(sys.argv[2]) if len(sys.argv) > 2 else 1
n_par = len(Path("build/params.txt").read_text().splitlines())
L, t, notes = [], 1500, []
if nch > 1:
    L.append(f"{t} midi 0xB0 3 1")
    t += 50
for step in range(1500):
    ch = random.randrange(nch)
    p = random.randrange(n_par); v = random.choice([0, 127, random.randrange(128)])
    L += [f"{t} midi {0xB0 | ch:#x} 99 {p // 128}", f"{t} midi {0xB0 | ch:#x} 98 {p % 128}", f"{t} midi {0xB0 | ch:#x} 6 {v}"]
    if step % 7 == 0:
        n = random.randrange(12, 110); L.append(f"{t} midi {0x90 | ch:#x} {n} {random.randrange(1, 128)}"); notes.append((ch, n))
    if step % 11 == 0 and notes:
        c, n = notes.pop(0)
        L.append(f"{t} midi {0x80 | c:#x} {n} 0")
    if step % 50 == 0:
        L.append(f"{t} midi {0xE0 | ch:#x} {random.randrange(128)} {random.randrange(128)}")
    if step % 97 == 0:
        L.append(f"{t} midi {0xC0 | ch:#x} {random.randrange(128)} 0")
    t += 25
print("\n".join(L) + f"\n{t + 2000} end")
