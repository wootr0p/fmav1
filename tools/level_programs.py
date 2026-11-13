#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The factory programs, played and levelled: tests/mfm_programs.txt plays each one a phrase (3 s
apiece), the host renders it, each program's peak is measured, and its VOLUME (a square law) is moved toward the
target peak. Run from the repo root after the host build:
  python3 tools/level_programs.py script      writes tests/mfm_programs.txt
  python3 tools/level_programs.py measure     renders and prints each program's peak / RMS
  python3 tools/level_programs.py level [-6]  ... and rewrites the VOLUMEs in firmware/src/mfm_patches.h"""
import array
import math
import re
import subprocess
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCHES = ROOT / "firmware" / "src" / "mfm_patches.h"
SCRIPT = ROOT / "tests" / "mfm_programs.txt"
T0, STEP = 1500, 3000
PHRASE = [(50, 48, 280), (350, 55, 280), (650, 60, 580), (1250, 63, 230), (1500, 60, 230), (1750, 48, 500)]


def programs():
    txt = PATCHES.read_text()
    body = txt[txt.index("FACTORY[] = {"):]
    return re.findall(r'\{"([^"]+)",\s*(PF_\w+)\}', body)


def write_script():
    n = len(programs())
    L = ["# every factory program: a program change, a little phrase, 3 s each (tools/level_programs.py script)"]
    for i in range(n):
        t = T0 + i * STEP
        L.append(f"{t} midi 0xC0 {i} 0")
        for dt, note, ln in PHRASE:
            L += [f"{t + dt} midi 0x90 {note} 110", f"{t + dt + ln} midi 0x80 {note} 0"]
    L.append(f"{T0 + n * STEP + 500} end")
    SCRIPT.write_text("\n".join(L) + "\n")


def measure():
    subprocess.run(["build/host/fmav1", "build/host/programs", str(SCRIPT)], check=True, cwd=ROOT, capture_output=True)
    w = wave.open(str(ROOT / "build" / "host" / "programs.wav"))
    fr, n = w.getframerate(), w.getnframes()
    a = array.array("h")
    a.frombytes(w.readframes(n))
    res = []
    for i, (name, ident) in enumerate(programs()):
        s, e = int((T0 + i * STEP + 40) / 1000 * fr), int((T0 + (i + 1) * STEP) / 1000 * fr)
        blk = a[2 * s:2 * e]
        pk = max(1, max(-min(blk), max(blk)))
        rms = math.sqrt(sum(x * x for x in blk[::5]) / max(1, len(blk[::5])))
        res.append((name, ident, 20 * math.log10(pk / 32767), 20 * math.log10(max(rms, 1) / 32767)))
    return res


def level(target):
    txt = PATCHES.read_text()
    for name, ident, pk, rms in measure():
        m = re.search(r"(static const uint8_t " + ident + r"\[\] = \{.*?P_VOLUME, )(\d+)(, X\};)", txt, flags=re.S)
        if not m:
            print(f"{name}: no VOLUME")
            continue
        v = int(m.group(2))
        nv = max(20, min(127, round(v * 10 ** ((target - pk) / 40))))
        print(f"{name:14s} peak {pk:6.1f}  rms {rms:6.1f}  VOLUME {v} -> {nv}")
        txt = txt[:m.start(2)] + str(nv) + txt[m.end(2):]
    PATCHES.write_text(txt)


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "measure"
    if what == "script":
        write_script()
    elif what == "level":
        level(float(sys.argv[2]) if len(sys.argv) > 2 else -6.0)
    else:
        for name, ident, pk, rms in measure():
            print(f"{name:14s} peak {pk:6.1f}  rms {rms:6.1f}")
