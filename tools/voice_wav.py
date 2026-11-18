#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(0.6) Where a host WAV speaks (tests/run_tests.sh, the FM-1's own voice): each window a-b (seconds) quiet or
speech, by its loudness (RMS over 50 ms steps: speech, one step at least over SPEECH; quiet, none over QUIET).

    voice_wav.py build/host/voice.wav 2.3-5.1:quiet 5.3-6.5:speech ...
"""
import math
import struct
import sys
import wave

SPEECH, QUIET = 800, 60


def main(path, *wins):
    w = wave.open(path)
    fs, ch, n = w.getframerate(), w.getnchannels(), w.getnframes()
    s = struct.unpack("<%dh" % (n * ch), w.readframes(n))
    bad = 0
    for win in wins:
        span, want = win.split(":")
        a, b = (float(x) for x in span.split("-"))
        step = fs // 20
        rms = []
        for i in range(int(a * fs), int(b * fs) - step, step):
            seg = s[i * ch:(i + step) * ch:ch]
            rms.append(math.sqrt(sum(x * x for x in seg) / len(seg)))
        top = max(rms) if rms else 0
        ok = top > SPEECH if want == "speech" else top < QUIET
        print(f"{a:5.1f}-{b:5.1f} s {want:6}: loudest {top:7.0f}  {'ok' if ok else 'WRONG'}")
        bad |= not ok
    return bad


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:]))
