#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The render time on the FM-1 itself: plays scenarios over USB-MIDI and reads FMav-1's CPU report (CC 3, answered
on channel 16: the slowest audio half of the last 2 s in us, of 5805; the late halves; the tracks). Needs mido with
python-rtmidi; run it on the computer the FM-1 is plugged into (Windows: py fm1_cpu.py).

It sets the MIDI channel to 1 for the test (track t on channel t + 1) and puts back the one it found; it loads programs
onto the tracks (the sounds on the FM-1 change; nothing is saved)."""
import sys
import time

import mido

# the worst sound (tools/gen_cpu_test.py): NRPN number, value as sent (0..127): the folder fed by the mixer (the LFO at
# 550 Hz and the VCO), the LFO on the pitch, its rate from the keyboard, the S+H on the cutoff, the EG through the
# attenuator on 1V/OCT, the LFO on the VCA and (by the mult) on PWM, a resonant ladder
WORST = [(30, 60), (33, 28), (34, 20), (21, 90), (20, 110), (14, 120), (12, 40), (26, 44), (28, 12), (24, 68), (35, 36),
         (1, 115), (2, 100), (13, 80), (8, 64), (27, 28), (25, 28), (18, 40), (16, 90),
         (45, 64), (50, 64), (59, 64), (69, 64)]   # (0.2: the chorus, delay and reverb on; 0.3: the sources renumbered, no
                                         # mult; 0.4: the noise)
INIT = 98                                       # program 99


def port_names():
    ins = [n for n in mido.get_input_names() if "felucca" in n.lower() or "fm-1" in n.lower()]
    outs = [n for n in mido.get_output_names() if "felucca" in n.lower() or "fm-1" in n.lower()]
    if not ins or not outs:
        raise SystemExit(f"FM-1 not found (inputs: {mido.get_input_names()})")
    return ins[0], outs[0]


class Fm1:
    def __init__(self):
        i, o = port_names()
        self.inp, self.out = mido.open_input(i), mido.open_output(o)
        time.sleep(0.3)

    def send(self, *b):
        self.out.send(mido.Message.from_bytes(list(b)))

    def report(self, value=0, ch=0):
        while self.inp.poll():
            pass
        self.send(0xB0 | ch, 3, value)
        got, end = {}, time.time() + 1.0
        while time.time() < end and len(got) < 7:
            m = self.inp.poll()
            if m is None:
                time.sleep(0.005)
                continue
            if m.type == "control_change" and m.channel == 15 and 20 <= m.control <= 26:
                got[m.control] = m.value
        if len(got) < 7:
            raise SystemExit(f"no CPU report (another firmware?): {got}")
        return {"peak": got[20] << 7 | got[21], "now": got[22] << 7 | got[23], "late": got[24], "tracks": got[25], "ch": got[26]}

    def nrpn(self, ch, p, v):
        self.send(0xB0 | ch, 99, p // 128); self.send(0xB0 | ch, 98, p % 128); self.send(0xB0 | ch, 6, v)
        time.sleep(0.002)


def worst(f, ch):
    f.send(0xC0 | ch, INIT)
    time.sleep(0.05)
    for p, v in WORST:
        f.nrpn(ch, p, v)


def scenario(f, name, progs, notes, setup=None):
    for ch, prog in progs.items():
        f.send(0xC0 | ch, prog)
    if setup:
        setup(f)
    time.sleep(0.3)
    for ch, n in notes:
        f.send(0x90 | ch, n, 110)
    time.sleep(2.6)
    r = f.report(0, notes[0][0] if notes else 0)
    for ch, n in notes:
        f.send(0x80 | ch, n, 0)
    print(f"{name:44s} slowest {r['peak']:5d} us of 5805 ({r['peak'] * 100 // 5805:3d} %)  last {r['now']:5d}  late {r['late']:3d}")
    time.sleep(2.5)
    return r


def main():
    f = Fm1()
    r0 = f.report(0)
    old, nt = r0["ch"], r0["tracks"]
    print(f"FM-1 found ({nt} tracks); MIDI channel {old or 'OMNI'}; late halves so far {r0['late']}")
    f.report(1)                                  # channel 1: track t on channel t + 1
    time.sleep(0.2)
    try:
        scenario(f, "idle", {}, [])
        scenario(f, "INIT (one saw, ladder) on track 1", {0: INIT}, [(0, 60)])
        scenario(f, "CLASSIC BASS", {0: 0}, [(0, 48)])
        scenario(f, "DUO LEAD (the LFO a second oscillator)", {0: 15}, [(0, 60)])
        scenario(f, "NOISE HIT (everything patched)", {0: 33}, [(0, 60)])
        scenario(f, "worst sound on track 1", {}, [(0, 60)], lambda f: worst(f, 0))
        if nt > 1:
            scenario(f, f"worst sound on all {nt} tracks", {}, [(c, 48 + 5 * c) for c in range(nt)],
                     lambda f: [worst(f, c) for c in range(nt)])
    finally:
        for ch in range(max(1, nt)):
            f.send(0xB0 | ch, 123, 0)
        f.report(old if old else 17, 0)          # the channel as it was
        print("MIDI channel put back:", old or "OMNI")


if __name__ == "__main__":
    sys.exit(main())
