#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""0.3: writes tests/mfm_nav.txt (every section, every page) and tests/mfm_ui.txt (the panel's actions). Run from the
repo root: python3 tools/gen_ui_tests.py"""
from pathlib import Path

HDR = ("# buttons: 0 OCT- 1 OCT+ 2 FX 3 SEL 4 ENV 5 LFO 6 EDIT 7 GLO 8 HOME 9 SAVE 10 ARP 11 SEQ 12 PLAY 13 REC\n"
       "# encoders: 0 SELECT 1 ALGORITHM 2-5 KNOB 1-4 6 PRESETS\n")

# ---- every section (its button), every page (SELECT turned), a note held so the live graphs show it
SECTIONS = [(8, 5), (6, 5), (4, 3), (5, 3), (2, 7), (11, 4), (3, 3), (10, 1), (7, 4)]   # (button, its pages: 4 tracks)
L = ["# every section, every page (SELECT turned; SELECT pressed: UTL), a note held so the live graphs show it\n" + HDR
     + "1500 midi 0xC0 0 0\n1600 midi 0x90 48 100"]
t = 1800
for btn, n in SECTIONS:
    L.append(f"{t} btn {btn}"); t += 200
    L.append(f"{t} shot"); t += 100
    for _ in range(n - 1):
        L.append(f"{t} enc 0 1"); t += 200
        L.append(f"{t} shot"); t += 100
L.append(f"{t} midi 0x80 48 0")
L.append(f"{t + 500} end")
Path("tests/mfm_nav.txt").write_text("\n".join(L) + "\n")

# ---- the panel's actions (0.3): no switch on SELECT; knobs for VCA MODE and the effects' ON; REC (every effect off /
# on); SELECT pressed (UTL); a jack's choices (DUMMY one detent after "---"); LFO SYNC and the CLOCK page under a MIDI
# clock (Start, 24 a quarter, Stop); the arp on that clock; undo; SAVE + G3 (a random sound); SAVE twice; PRESETS;
# ALGORITHM; OCT+ twice (its LED blinking) and back; PLAY (HOLD)
A = [
    (1500, "midi 0xC0 15 0"), (1600, "midi 0x90 60 100"),
    (1800, "btn 4"), (1900, "enc 0 2"), (2000, "enc 3 1"), (2200, "shot"), (2300, "enc 3 -1"),   # VCA: MODE ON, EG
    (2400, "btn 6"), (2500, "enc 3 30"), (2700, "shot"),                                         # VCO: WAVE
    (2800, "enc 0 4"), (2900, "enc 2 2"), (3100, "shot"),                                        # VCF INPUTS: KB CV
    (3200, "btn 2"), (3300, "enc 2 1"), (3500, "shot"),                                          # CHORUS ON
    (3600, "enc 0 2"), (3700, "enc 2 1"), (3800, "enc 3 -20"), (4000, "shot"),                   # DELAY ON, SEND
    (4100, "btn 13"), (4300, "shot"), (4400, "btn 13"), (4600, "shot"),                          # REC: off, on
    (4700, "btn 11"), (4900, "shot"), (5000, "enc 0 1"), (5100, "enc 2 3"), (5200, "enc 3 1"),   # SEQ: overview, cables
    (5400, "shot"), (5500, "enc 0 -1"), (5700, "shot"),
    (5800, "btn 3"), (5900, "enc 2 40"), (6100, "shot"), (6200, "btn 3"), (6400, "shot"),         # UTL (SELECT pressed)
    (6500, "btndown 6"), (6550, "btn 0"), (6600, "btnup 6"), (6800, "shot"),                     # EDIT + OCT-: undo
    (6900, "btn 5"), (7000, "enc 5 12"), (7200, "shot"),                                         # LFO SYNC 1/8
    (7300, "enc 0 2"), (7500, "shot"),                                                           # CLOCK
    (7600, "btn 10"),                                                                            # the arp on
    (7700, "midi 0xFA 0 0"),
]
t = 7720
while t < 9700:                                                                                  # a MIDI clock, 125 bpm
    A.append((t, "midi 0xF8 0 0")); t += 20
A += [(8400, "shot"), (9000, "btn 5"), (9100, "shot"), (9750, "midi 0xFC 0 0"), (9800, "midi 0x80 60 0"),
      (9900, "btn 10"), (10100, "shot"),
      (10200, "btndown 9"), (10300, "keys 0x4"), (10400, "keys 0"), (10500, "btnup 9"), (10700, "shot"),   # random
      (10800, "keys 0x80"), (11300, "keys 0"), (11400, "btn 9"), (11500, "enc 6 5"), (11600, "btn 9"),     # save
      (11800, "shot"), (12700, "enc 6 1"), (12900, "shot"), (13000, "enc 1 1"), (13200, "shot"), (13300, "enc 1 -1"),
      (13400, "btn 1"), (13500, "btn 1"), (13700, "shot"), (13800, "btn 0"), (13900, "btn 0"),
      (14000, "btn 12"), (14100, "keys 0x91"), (14400, "keys 0"), (14600, "shot"), (14700, "btn 12"), (15200, "end")]
A.sort(key=lambda x: x[0])
Path("tests/mfm_ui.txt").write_text("# the panel's actions (0.3: tools/gen_ui_tests.py)\n" + HDR
                                    + "\n".join(f"{a} {b}" for a, b in A) + "\n")
print("ok")
