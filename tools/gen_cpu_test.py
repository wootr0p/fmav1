#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The worst case for the CPU: every track with everything at audio rate (the folder in the path, the LFO at
550 Hz on the pitch and through the mixer, the S+H on the cutoff, the LFO RATE and the 1V/OCT patched, a resonant
ladder), notes on every track's channel.
  tools/gen_cpu_test.py [NTRK] > tests/mfm_cpu.txt     (CC 3 = 1 sets the MIDI channel to 1: track t on channel t + 1)"""
import sys
from mfm_params import P, PJ, nrpn

n = int(sys.argv[1]) if len(sys.argv) > 1 else 4
L = ["# the worst case: every track with everything at audio rate (tools/gen_cpu_test.py)", "1500 midi 0xB0 3 1"]
t = 1600
for ch in range(n):
    for p, v in ((PJ("I_FOLD"), P["J_ONETWO"]), (PJ("I_ONE"), P["J_LFO"]), (PJ("I_TWO"), P["J_VCO"]), (P["P_ONE_LVL"], 90),
                 (P["P_FOLD"], 110), (P["P_LFO_RATE"], 120), (P["P_PITCH_AMT"], 40), (PJ("I_CUTOFF"), P["J_SH"]),
                 (PJ("I_LFORATE"), P["J_KBCV"]), (PJ("I_1VOCT"), P["J_ATTN"]), (PJ("I_ATTN"), P["J_EG"]), (P["P_RES"], 115),
                 (P["P_VCF_AMT"], 100), (P["P_PWM_AMT"], 80), (P["P_WAVE"], 64), (PJ("I_VCACV"), P["J_LFO"]),
                 (PJ("I_PWM"), P["J_LFO"]), (P["P_GLIDE"], 40), (P["P_VOLUME"], 90),
                 (P["P_CH_ON"], 1), (P["P_DL_ON"], 1), (P["P_RV_ON"], 1), (P["P_NOISE"], 64)):   # (0.2: the chorus, and the delay and reverb sends)
        L += nrpn(t, p, v, ch)
        t += 2
for k in range(12):
    for ch in range(n):
        note = [36, 43, 48, 55][ch % 4] + (k % 3) * 2
        L += [f"{2400 + 400 * k} midi {0x90 | ch:#x} {note} 100", f"{2400 + 400 * k + 350} midi {0x80 | ch:#x} {note} 0"]
L.append(f"{2400 + 400 * 12 + 1500} end")
print("\n".join(L))
