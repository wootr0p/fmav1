#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Levels of a host render, in equal segments: peak and RMS (dBFS), DC, the biggest jump between two samples.

  tools/ana_wav.py build/host/programs.wav START_S SEGMENT_S NAME,NAME,...
"""
import array
import math
import sys
import wave


def main():
    w = wave.open(sys.argv[1])
    n, fr = w.getnframes(), w.getframerate()
    a = array.array("h")
    a.frombytes(w.readframes(n))
    seg0, seglen = float(sys.argv[2]), float(sys.argv[3])
    for i, nm in enumerate(sys.argv[4].split(",")):
        s, e = int((seg0 + i * seglen) * fr), int((seg0 + (i + 1) * seglen) * fr)
        blk = a[2 * s:2 * e]
        if not blk:
            break
        sub = blk[::7]
        pk = max(-min(blk), max(blk))
        rms = math.sqrt(sum(x * x for x in sub) / len(sub))
        dc = sum(sub) / len(sub)
        jump = max(abs(blk[j] - blk[j - 2]) for j in range(2, len(blk), 2))
        db = lambda v: 20 * math.log10(max(v, 1) / 32767)
        print(f"{i + 1:2d} {nm:13s} peak {db(pk):6.1f} dBFS  rms {db(rms):6.1f}  dc {dc:7.1f}  maxjump {jump}")


if __name__ == "__main__":
    main()
