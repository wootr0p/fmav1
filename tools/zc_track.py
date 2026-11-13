#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The pitch over time of a host render by zero crossings (a quick check of a pitch sweep, a glide, a stepped S+H):
  tools/zc_track.py WAV START_S DUR_S [WIN_MS=10]"""
import array
import sys
import wave

w = wave.open(sys.argv[1])
fr = w.getframerate()
start, dur, win = float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4]) if len(sys.argv) > 4 else 10.0
w.setpos(int(start * fr))
a = array.array("h")
a.frombytes(w.readframes(int(dur * fr)))
m = [(a[2 * i] + a[2 * i + 1]) // 2 for i in range(len(a) // 2)]
n = int(win / 1000 * fr)
out = []
for k in range(0, len(m) - n, n):
    seg = m[k:k + n]
    z = sum(1 for i in range(1, n) if seg[i - 1] < 0 <= seg[i])
    pk = max(abs(x) for x in seg)
    out.append(f"{k / fr * 1000:5.0f}ms {z * fr / n:6.0f}Hz pk{pk:6d}")
print("\n".join(out))
