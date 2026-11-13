#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The parameter numbers of firmware/src/mfm_params.h for the scripts (tests, the CC chart): its enums read as C reads
them. P["P_CUTOFF"] -> 0; PJ("I_FOLD") the FOLD IN jack; nrpn(t, p, v) -> the script lines of an NRPN."""
import re
from pathlib import Path

SRC = Path(__file__).resolve().parents[1] / "firmware" / "src" / "mfm_params.h"
P = {}
MAX = {}                                         # the parameters' ranges (PDEF), by number


def _load():
    txt = re.sub(r"/\*.*?\*/", "", SRC.read_text(), flags=re.S)
    for name, val in re.findall(r"^#define\s+(\w+)\s+(\d+)u?\s*$", txt, flags=re.M):
        P[name] = int(val)
    for body in re.findall(r"\benum\s*\{(.*?)\}", txt, flags=re.S):
        nxt = 0
        for item in body.split(","):
            item = item.strip()
            if not item:
                continue
            if "=" in item:
                name, expr = (s.strip() for s in item.split("=", 1))
                nxt = eval(expr.replace("u", ""), {}, dict(P))
            else:
                name = item
            P[name] = nxt
            nxt += 1
    for name, mx in re.findall(r"\[(P_\w+)\]\s*=\s*\{\"[^\"]*\",\s*([^,]+),", txt):
        MAX[P[name]] = eval(mx, {}, dict(P))
    for i in range(P["NJIN"]):
        MAX[P["P_JACK"] + i] = P["NJSRC"] - 1


_load()


def PJ(i):
    return P["P_JACK"] + (P[i] if isinstance(i, str) else i)


def cc_value(p, v, mx=None):
    """the 0..127 a CC / NRPN sends for value v of parameter p (the firmware scales by the range: (cc * max + 63) / 127)"""
    mx = MAX.get(p, 127) if mx is None else mx
    if mx == 127:
        return v
    for cc in range(128):
        if (cc * mx + 63) // 127 == v:
            return cc
    raise ValueError(f"parameter {p}: {v} out of 0..{mx}")


def nrpn(t, p, v, ch=0):
    """script lines: parameter p to value v (in its own range) at time t (ms), on MIDI channel ch (0 = 1)"""
    b = 0xB0 | ch
    return [f"{t} midi {b:#x} 99 {p // 128}", f"{t} midi {b:#x} 98 {p % 128}", f"{t} midi {b:#x} 6 {cc_value(p, v)}"]


if __name__ == "__main__":
    for k, v in P.items():
        if k.startswith("P_"):
            print(k, v)
