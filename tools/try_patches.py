#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Try sounds by NRPN on the host (a scratch tool for designing the factory programs): each "patch" a dict of
parameter -> value over INIT, a note held; prints peak / RMS and the spectral flatness (1 = noise, 0 = a pure tone)
and the strongest partials.
  python3 tools/try_patches.py   (after cc ... -o build/host/fmav1)"""
import array
import cmath
import math
import subprocess
import sys
import wave
from mfm_params import P, PJ, nrpn

FS = 44100


def script(patches, note=48, hold=900):
    L, t = [], 1500
    for name, pv in patches:
        L.append(f"{t} midi 0xC0 98 0")         # INIT (program 99)
        t += 20
        for p, v in pv.items():
            L += nrpn(t, P[p] if isinstance(p, str) else p, v)
            t += 2
        t += 30
        L += [f"{t} midi 0x90 {pv.get('note', note) if False else note} 110", f"{t + hold} midi 0x80 {note} 0"]
        t += hold + 1200
    L.append(f"{t} end")
    return L, t


def analyse(path, starts, dur=0.8):
    w = wave.open(path)
    n = w.getnframes()
    a = array.array("h")
    a.frombytes(w.readframes(n))
    out = []
    for s in starts:
        i0 = int(s * FS)
        seg = [(a[2 * i] + a[2 * i + 1]) / 65536 for i in range(i0, min(n, i0 + int(dur * FS)))]
        pk = max(abs(x) for x in seg) if seg else 0
        rms = math.sqrt(sum(x * x for x in seg) / max(1, len(seg)))
        N = 4096
        x = seg[2000:2000 + N]
        if len(x) < N:
            out.append((pk, rms, 0, []))
            continue
        X = fft([v * (0.5 - 0.5 * math.cos(2 * math.pi * k / (N - 1))) for k, v in enumerate(x)])
        mag = [abs(X[k]) + 1e-9 for k in range(1, N // 2)]
        flat = math.exp(sum(math.log(m) for m in mag) / len(mag)) / (sum(mag) / len(mag))
        top = sorted(range(len(mag)), key=lambda k: -mag[k])[:5]
        out.append((pk, rms, flat, [round((k + 1) * FS / N) for k in top]))
    return out


def fft(x):
    n = len(x)
    if n == 1:
        return [complex(x[0])]
    ev, od = fft(x[0::2]), fft(x[1::2])
    t = [cmath.exp(-2j * math.pi * k / n) * od[k] for k in range(n // 2)]
    return [ev[k] + t[k] for k in range(n // 2)] + [ev[k] - t[k] for k in range(n // 2)]


def run(patches, note=48, hold=900, name="try"):
    L, _ = script(patches, note, hold)
    open(f"build/{name}.txt", "w").write("\n".join(L) + "\n")
    subprocess.run(["build/host/fmav1", f"build/host/{name}", f"build/{name}.txt"], check=True, capture_output=True)
    starts, t = [], 1500
    for _, pv in patches:
        t += 20 + 2 * len(pv) + 30
        starts.append(t / 1000 + 0.05)
        t += hold + 1200
    for (nm, _), (pk, rms, fl, top) in zip(patches, analyse(f"build/host/{name}.wav", starts)):
        db = lambda v: 20 * math.log10(max(v, 1e-6))
        print(f"{nm:14s} peak {db(pk):6.1f}  rms {db(rms):6.1f}  flat {fl:.3f}  top {top}")


if __name__ == "__main__":
    J = PJ
    run([
        ("A xmod", {J("I_LFORATE"): P["J_VCO"], "P_LFO_RATE": 120, J("I_1VOCT"): P["J_LFO"], "P_LFO_WAVE": 127, "P_PITCH": 84}),
        ("B sh", {J("I_SHGATE"): P["J_VCO"], J("I_SHIN"): P["J_LFO"], "P_LFO_RATE": 127, J("I_FOLD"): P["J_SH"], "P_PITCH": 96}),
        ("C selffm", {J("I_1VOCT"): P["J_VCO"], "P_PITCH": 84}),
        ("D xmod2", {J("I_LFORATE"): P["J_VCO"], "P_LFO_RATE": 110, J("I_1VOCT"): P["J_LFO"], "P_PITCH": 90, J("I_PWM"): P["J_LFO"], "P_WAVE": 127}),
        ("E chaos", {J("I_LFORATE"): P["J_VCO"], "P_LFO_RATE": 127, J("I_FOLD"): P["J_ONETWO"], J("I_ONE"): P["J_LFO"], J("I_TWO"): P["J_VCO"], "P_ONE_LVL": 127, "P_FOLD": 127, J("I_1VOCT"): P["J_SH"], "P_PITCH": 96, J("I_SHGATE"): P["J_VCO"], J("I_SHIN"): P["J_LFO"]}),
    ])
