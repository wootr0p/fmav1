#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Spectra of a host render without numpy (a radix-2 FFT in Python): the fundamental, the strongest partials, how much
of the energy lies off the harmonic series (aliasing, noise), the level in octave bands.

  tools/ana_spec.py WAV START_S [N=8192]            the strongest peaks and the inharmonic share at START_S
  tools/ana_spec.py WAV START_S N bands             the levels in third-octave bands (a filter's response to noise)
"""
import array
import cmath
import math
import sys
import wave


def fft(x):
    n = len(x)
    if n == 1:
        return [complex(x[0])]
    ev, od = fft(x[0::2]), fft(x[1::2])
    t = [cmath.exp(-2j * math.pi * k / n) * od[k] for k in range(n // 2)]
    return [ev[k] + t[k] for k in range(n // 2)] + [ev[k] - t[k] for k in range(n // 2)]


def load(path, start, n):
    w = wave.open(path)
    fr = w.getframerate()
    w.setpos(int(start * fr))
    a = array.array("h")
    a.frombytes(w.readframes(n))
    mono = [(a[2 * i] + a[2 * i + 1]) / 65536.0 for i in range(len(a) // 2)]
    win = [0.5 - 0.5 * math.cos(2 * math.pi * i / (n - 1)) for i in range(n)]   # Hann
    return [m * h for m, h in zip(mono, win)], fr


def mag_db(x):
    X = fft(x)
    n = len(x)
    return [20 * math.log10(abs(X[k]) / (n / 4) + 1e-12) for k in range(n // 2)]


def peaks(db, fr, n, count=12):
    out = []
    for k in range(2, len(db) - 2):
        if db[k] > db[k - 1] and db[k] >= db[k + 1] and db[k] > -90:
            a, b, c = db[k - 1], db[k], db[k + 1]
            d = 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) != 0 else 0
            out.append((b, (k + d) * fr / n))
    return sorted(out, reverse=True)[:count]


def main():
    path, start = sys.argv[1], float(sys.argv[2])
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 8192
    x, fr = load(path, start, n)
    db = mag_db(x)
    if len(sys.argv) > 4 and sys.argv[4] == "bands":
        f = 31.5
        while f < fr / 2:
            lo, hi = f / 2 ** (1 / 6), f * 2 ** (1 / 6)
            ks = [k for k in range(len(db)) if lo <= k * fr / n < hi]
            if ks:
                e = sum(10 ** (db[k] / 10) for k in ks)
                print(f"{f:8.0f} Hz {10 * math.log10(e + 1e-12):7.1f} dB")
            f *= 2 ** (1 / 3)
        return
    pk = peaks(db, fr, n)
    f0 = min((f for d, f in pk if d > pk[0][0] - 30), default=0)
    tot = sum(10 ** (d / 10) for d in db[1:])
    harm = 0.0
    if f0 > 0:
        for k in range(1, len(db)):
            f = k * fr / n
            h = round(f / f0)
            if h >= 1 and abs(f - h * f0) <= 3 * fr / n:
                harm += 10 ** (db[k] / 10)
    print(f"f0 ~ {f0:.2f} Hz; off-series energy {10 * math.log10(max(tot - harm, 1e-15) / tot):.1f} dB")
    for d, f in pk:
        print(f"  {f:9.2f} Hz {d:6.1f} dB" + (f"  (x{f / f0:.3f})" if f0 else ""))


if __name__ == "__main__":
    main()
