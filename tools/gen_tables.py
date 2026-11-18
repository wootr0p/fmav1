#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""FMav-1's fixed-point tables (the chip has no FPU): tools/gen_tables.py -> firmware/gen/mfm_tables.h
(committed, so a build needs no regeneration). FS = 44100, control rate every CTL = 16 frames.

The ranges are the reference instrument's user's manual's: VCO 8 Hz .. 16 kHz, CUTOFF 30 Hz .. 20 kHz, LFO 0.1 .. 550 Hz,
ATTACK 0.8 ms .. 5.5 s, DECAY and RELEASE 3 ms .. 18 s, GLIDE up to ~9 s.
From OberVave: pitch, tanh, pan, cos, sine, and the LADDER's tables after FMini-1's
ladder (Ruppert, GPL-3.0, github.com/ruppert/fmini-1, tools/fmini_filter.py: four differential-pair stages and the
emphasis network's lag pole, which makes it sing at 1.578 times its -3 dB corner, at a loop gain of 2.92; the emphasis
law and the passband loss from the CA-72 project's measurements of a hardware unit). Since 0.5 the CUTOFF and
RESONANCE laws (KNOB), LAD_RATIO, LADDER_DRIVE_DB and RES_LEVEL_DB are fitted to our own recordings of the reference
instrument (wootrop & gpix)."""
import math
import sys
from pathlib import Path

FS = 44100
CTL = 16


def arr(name, ctype, vals, per=10):
    out = [f"static const {ctype} {name}[{len(vals)}] = {{"]
    for i in range(0, len(vals), per):
        out.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
    out.append("};")
    return out


def k_block(tau_s):
    """one-pole coefficient per control block, Q24: level += (target - level) * k >> 24"""
    return max(1, min((1 << 24) - 1, int(round((1 << 24) * (1 - math.exp(-CTL / (tau_s * FS)))))))


def lin(points, x):
    if x <= points[0][0]:
        return points[0][1]
    for (xa, ya), (xb, yb) in zip(points, points[1:]):
        if x <= xb:
            return ya + (yb - ya) * (x - xa) / (xb - xa)
    return points[-1][1]


def expo(lo, hi, v, n=127):
    return lo * (hi / lo) ** (v / n)


# ================================================================================== THE KNOBS' CALIBRATION
# (0.5) Each knob of the reference panel: its value fully counterclockwise (7 o'clock), in between (None: the law's own
# curve; a number: at noon; {hour: value, ...}: at those hours, 8 .. 4 o'clock) and fully clockwise (5 o'clock), and its
# law: "exp" equal ratios over the turn (Hz, times), "lin" equal steps, between each two neighbouring points (the
# reference's pots do not all follow one curve: measure them at a few hours).
# After an edit:  python3 tools/gen_tables.py   then   ./build.sh   (on the computer: sh tests/run_host.sh)
# The factory programs keep their knob values: a new range changes how they sound.
VCF_AMT_LAW = 1.5   # VCF MOD AMT's curve, |x| ^ this (0.5: the reference's recording, 3.3 octaves at 2:48; was 2)
KNOB = {
    #                7 o'clock   noon   5 o'clock  law
    "PITCH":       (8.1758,     None,  8372.02,  "exp"),   # Hz, the VCO on the keyboard's lowest C (the manual: 8 Hz
                                                           # .. 8 kHz); 121 steps: a semitone each at these ends
    "CUTOFF":      (30.0,       {9: 50.4, 10.47: 95.0, 12: 612.9, 3: 10249.5}, 20000.0, "exp"),   # (0.5: the reference's recordings,
                                                           # C2 saw, fitted on the simulator: build/cal/fit2.py)   # Hz, the ladder's singing pitch (the manual: 30 Hz .. 20 kHz)
    "RESONANCE":   (0.0,        {12: 4.0, 3.5: 8.0}, 9.0, "lin"),   # the hardware's dial 0 .. 10 it reaches (it sings from 8: the
                                                           # user, on the reference: from halfway between 3 and 4 o'clock)
    "LFO_RATE":    (0.165,      {12: 4.7}, 550.0, "exp"),   # Hz (the manual: 0.1 .. 550 Hz; the user's scope on the real
                                                           # unit, RATE at its minimum: a cycle every ~6.06 s; at noon
                                                           # the user's recording, the LFO's PWM gating a pulse: a burst
                                                           # every ~213 ms, 4.7 Hz (it was ~9.5 Hz, the ends' mean))
    "ATTACK":      (0.8,        {9: 44.0, 10: 153.0, 3: 1150.0}, 2500.0, "exp"),   # ms (the manual: 0.8 ms .. 5.5 s; the user's ear: 2.5 s;
                                                           # 0.5.2, the user by ear on the reference: 153 ms at 10
                                                           # o'clock, it was at noon)
    "DECAY":       (3.0,        {9: 437.0, 12: 1200.0}, 16000.0, "exp"),   # (0.5.2, the user: 1.2 s at noon, was 668 ms)
      # ms, the reference's seconds (TAUS below; the manual: 3 ms
                                                           # .. 18 s; the user, by the real unit: 16 s)
    "RELEASE":     (3.0,        {9: 458.0, 12: 627.0}, 18000.0, "exp"),   # (0.5.2, the user: 458 ms at 9)
      # ms to 5 % of the way (the manual: 3 ms .. 18 s;
                                                           # 9 o'clock: the recording below, was 39 ms; near DECAY's)
    "SUSTAIN":     (0.0,        {9: 1.34, 12: 4.8}, 10.0, "lin"),   # the dial, 10 = 8 V (the manual: 0 .. 8 V); linear
                                                           # (the user) between: 4.8 at noon (the user), 1.34 at 9
                                                           # (the recording below; it was 2.0)
    # (0.5: the user set this firmware's EG by the reference's, every EG knob at noon: ATTACK 153 ms, DECAY 668 ms,
    # (CUTOFF: the reference's recordings, after the user's 429, 527 Hz and 5 kHz by ear; by ear against the reference, RESONANCE 0); SUSTAIN 4.8, RELEASE 627 ms; at 9 o'clock: ATTACK 44 ms, DECAY 437 ms, RELEASE 39 ms; ATTACK at 3 o'clock, the
    # user's scope: 2.1, 5.6, 7.6, 8.0 V at 150, 470, 890, 1100 ms, flat at ~8.2 V: the RC toward 1.11 x, the top at 1.15 s)
    # (0.5, the user's recording of the reference set as a photo shows, CLASSIC BASS's sound, a C2 held 1.85 s: CUTOFF
    # 10:28, RESONANCE 7, VCF MOD AMT 2:48, ATTACK 7, DECAY 12:35, SUSTAIN 9, RELEASE 9; the simulator fitted to it,
    # the level and h2 / h4 over time (build/cal/cb_fit.py): CUTOFF 95 Hz at 10:28, SUSTAIN 1.34 at 9, RELEASE 330 ms
    # at 9, VCF MOD AMT 3.3 octaves at 2:48 (VCF_AMT_LAW); DECAY within the photo's reading, kept)
    "GLIDE":       (3.0,        None,  9000.0,   "exp"),   # ms to 95 % of the way, from its first step (0: off; the
                                                           # manual: up to ~9 s)
    "PULSE_WIDTH": (5.0,        None,  50.0,     "lin"),   # % (the manual: narrow .. square); noon not used
    "KB_SCALE":    (1.0,        None,  5.0,      "lin"),   # octaves the keyboard spans (the manual: 1 .. 5); noon not used
}
# An EG time is this many time constants (an RC's): ATTACK aims at 1.3 and stops at 1.0 (1.466); DECAY 3.53 (the user,
# 0.5: the reference's 16 s sounded as 13.6 s to 5 % of the way here, 3 x 16 / 13.6); RELEASE 3 (to 5 % of the way)
TAUS = {"DECAY": 3.0 * 16.0 / 13.6, "RELEASE": 3.0}
# The attack's shape (its time to the top stays the knob's): an RC charging toward ATTACK_AIM times the top, stopped at
# the top (the user's scope on the reference, 0.5: 0, 1.7, 4.5, 6.4, 7.5, 8.1 V at 0, 142, 475, 855, 1235, 1600 ms:
# toward ~9 V, 1.11 times its 8.1 V top). Higher: a straighter rise; near 1: rounder into the top.
ATTACK_AIM = 1.11
# The level the RESONANCE knob takes off the filter's output (and its whistle), in dB at hours, linear in dB between,
# on top of the ladder's own loss (0.5: with LAD_RATIO 2.02 the ladder alone loses as the reference's recordings,
# -7.7 / -14.9 dB at noon / the top against -7.9 / -15.2: none added)
RES_LEVEL_DB = {7: 0.0, 12: 0.0, 5: 0.0}
# The VCO's drive into the ladder, made up after it (0.5, the reference's recordings: its whistle at the top is ~4 dB
# louder against the note than here at 0; the whistle's level is the stages' tanh's, the note's the drive's)
LADDER_DRIVE_DB = -0.5


def res_level_db(v, n=127):
    pts = sorted((hour_at(h, n) if h != 7 and h != 5 else (0.0 if h == 7 else 1.0), db) for h, db in RES_LEVEL_DB.items())
    f = v / n
    for (fa, a), (fb, b) in zip(pts, pts[1:]):
        if f <= fb:
            return a + (b - a) * (f - fa) / (fb - fa)
    return pts[-1][1]


def att_shape(x):
    r = ATTACK_AIM
    return min(1.0, r * (1 - math.exp(-x * math.log(r / (r - 1)))))
# The LFO's skew: the share of its cycle the triangle rises (and the square is high); it falls over the rest (the user's
# scope on the reference instrument, 0.5: 58 %; 50 = symmetric)
LFO_RISE_PCT = 58.0


def hour_at(h, n):
    """an hour on the knob (7 o'clock .. 5 o'clock, 30 degrees each) as a share of the turn, on a whole step of n
    (noon: 64 of 127, the step drawn there)"""
    f = ((h - 7) % 12) / 10
    return math.ceil(n / 2) / n if f == 0.5 else round(f * n) / n


def knob(name, v, n=127):
    """the knob's value at v of n (0: 7 o'clock, n: 5 o'clock)"""
    lo, mid, hi, law = KNOB[name]
    pts = {0.0: lo, 1.0: hi}
    if isinstance(mid, dict):
        pts.update({hour_at(h, n): x for h, x in mid.items()})
    elif mid is not None:
        pts[hour_at(12, n)] = mid
    pts = sorted(pts.items())
    f = v / n
    for (fa, a), (fb, b) in zip(pts, pts[1:]):
        if f <= fb:
            g = (f - fa) / (fb - fa)
            return a * (b / a) ** g if law == "exp" else a + (b - a) * g
    return hi


assert KNOB["PULSE_WIDTH"][1] is None and KNOB["KB_SCALE"][1] is None, "PULSE WIDTH and KB SCALE: linear, no noon"


ATTACK_051 = (0.8, {9: 44.0, 12: 153.0, 3: 1150.0}, 2500.0, "exp")   # 0.5 / 0.5.1's ATTACK (ATT_FROM_051)
DECAY_051 = (3.0, {9: 437.0, 12: 668.0}, 16000.0, "exp")   # 0.5 / 0.5.1's DECAY (DEC_FROM_051)
RELEASE_051 = (3.0, {9: 330.0, 12: 627.0}, 18000.0, "exp")   # 0.5 / 0.5.1's RELEASE (REL_FROM_051)


def rel_from_051(v):
    """a RELEASE set by 0.5 / 0.5.1: the knob's value nearest to its time now"""
    saved = KNOB["RELEASE"]
    KNOB["RELEASE"] = RELEASE_051
    t = knob("RELEASE", v)
    KNOB["RELEASE"] = saved
    return min(range(128), key=lambda k: abs(math.log(knob("RELEASE", k) / t)))


def dec_from_051(v):
    """a DECAY set by 0.5 / 0.5.1: the knob's value nearest to its time now"""
    saved = KNOB["DECAY"]
    KNOB["DECAY"] = DECAY_051
    t = knob("DECAY", v)
    KNOB["DECAY"] = saved
    return min(range(128), key=lambda k: abs(math.log(knob("DECAY", k) / t)))


def att_from_051(v):
    """an ATTACK set by 0.5 / 0.5.1: the knob's value nearest to its time now"""
    saved = KNOB["ATTACK"]
    KNOB["ATTACK"] = ATTACK_051
    t = knob("ATTACK", v)
    KNOB["ATTACK"] = saved
    return min(range(128), key=lambda k: abs(math.log(knob("ATTACK", k) / t)))


def att_from_04(v):
    """an ATTACK saved before 0.5 (0.8 ms .. 5.5 s): the knob's value nearest to its time now"""
    t = expo(0.8, 5500.0, v)
    return min(range(128), key=lambda k: abs(math.log(knob("ATTACK", k) / t)))


def midi_of(f):
    return 69 + 12 * math.log2(f / 440.0)


# ---- the LADDER's laws (FMini-1's, see the docstring)
LAD_C4 = math.sqrt(2 ** 0.25 - 1)                 # corner / stage pole of four equal poles
LAD_RATIO = 2.12                                   # the singing pitch / the -3 dB corner (FMini-1: 1.578; 0.5, the
                                                   # reference's recordings: whistles at 583 Hz over a 288 Hz corner)
LAD_W0 = LAD_RATIO * LAD_C4                        # the singing pitch in stage-pole units: 0.6864
LAD_WX = LAD_W0 / math.tan(math.pi - 4 * math.atan(LAD_W0))   # the lag pole: 0.7587
LAD_KC = 1.0 / abs((1 + 1j * LAD_W0) ** -4 * (1 / (1 + 1j * LAD_W0 / LAD_WX)))   # the singing loop gain: 2.9186
# RES (dial 0..10) -> the loop gain as a share of the singing one; the passband loss the hardware shows (dB)
LAD_R = [(0, 0.0), (1, 0.007), (2, 0.033), (2.5, 0.10), (3, 0.20), (4, 0.48), (5, 0.58), (6, 0.68), (7, 0.80),
         (7.5, 0.88), (8, 1.00), (8.5, 1.06), (9, 1.12), (10, 1.25)]
LAD_LOSS = [(0, 0.0), (2, -0.8), (2.5, -2.25), (3, -4.0), (4, -8.0), (5, -9.5), (6, -10.43), (7, -11.92), (7.5, -12.5),
            (8.5, -14.67), (10, -15.5)]
# The RESONANCE knob (the reference manual): "at maximum Resonance settings, the VCF can become self-oscillating": its travel is the
# dial 0 .. 9.2, so the filter starts to sing (dial 8) at ~87 % of the knob and sings firmly at the end


def res_dial(v):
    return knob("RESONANCE", v)            # (KNOB: the dial the knob reaches; RES_DIAL was its top)


def main(path):
    L = ["/* generated by tools/gen_tables.py: do not edit */", "#pragma once", "#include <stdint.h>",
         f"#define CTL {CTL}u", ""]
    # pitch: one octave of phase steps for MIDI 120..131 (C9..B9), lower octaves shift right
    L += arr("INC_C9", "uint32_t", [int(440.0 * 2 ** ((n - 69) / 12) / FS * 2 ** 32) for n in range(120, 132)], 6)
    # a semitone in 256 steps: 2^(i / 3072), Q16
    L += arr("FINE_Q16", "uint32_t", [int(round(65536 * 2 ** (i / 3072))) for i in range(256)])
    # the frequency of each semitone 0..136 (the screen's numbers), capped at 0.45 fs
    fc = [min(440.0 * 2 ** ((n - 69) / 12), 0.45 * FS) for n in range(137)]
    L += arr("CUT_HZ", "uint16_t", [int(round(f)) for f in fc])
    # ---- the knobs' ranges (KNOB above)
    # CUTOFF: the ladder's singing pitch over the knob, Q8 semitones (MIDI numbers); 129: the last twice (the slewed
    # knob reads between two)
    cut = [int(round(midi_of(knob("CUTOFF", v)) * 256)) for v in range(128)]
    L += arr("CUT_P8", "int32_t", cut + [cut[-1]])   # (int32: up to MIDI 135 x 256)
    # PITCH: the VCO on the lowest C over the knob (0 .. 120), Q8 semitones from MIDI 60
    L += arr("PITCH_P8", "int16_t", [int(round((midi_of(knob("PITCH", v, 120)) - 60) * 256)) for v in range(121)])
    # PULSE WIDTH (Q16 of the turn), KB SCALE (Q12 octaves; tenths for the screen): linear
    pw_lo, _, pw_hi, _ = KNOB["PULSE_WIDTH"]
    kb_lo, _, kb_hi, _ = KNOB["KB_SCALE"]
    L += [f"#define DUTY_MIN16 {int(round(65536 * pw_lo / 100))}   /* PULSE WIDTH at 0: {pw_lo:g} % */",
          f"#define DUTY_SPAN16 {int(round(65536 * (pw_hi - pw_lo) / 100))}   /* .. 127: {pw_hi:g} % */",
          f"#define PW_LO_PCT {int(round(pw_lo))}", f"#define PW_HI_PCT {int(round(pw_hi))}",
          f"#define KBS_LO12 {int(round(4096 * kb_lo))}   /* KB SCALE at 0: {kb_lo:g} octaves */",
          f"#define KBS_SPAN12 {int(round(4096 * (kb_hi - kb_lo)))}   /* .. 127: {kb_hi:g} */",
          f"#define KBS_LO10 {int(round(10 * kb_lo))}", f"#define KBS_SPAN10 {int(round(10 * (kb_hi - kb_lo)))}"]
    # tanh(x / 32768) * 32767 for x = i * 256, x up to 4.0 (the saturations: the ladder, the output)
    L += arr("TANH_Q15", "int16_t", [int(32767 * math.tanh(i / 128)) for i in range(513)])
    # the EG (the reference manual): ATTACK 0.8 ms .. 5.5 s (0.5: .. 2.5 s), DECAY and RELEASE 3 ms .. 18 s. An analog contour: the attack aims at
    # 1.3 and stops at 1.0 (t = 1.466 tau), the decay and the release reach 5 % of the way in their time (t = 3 tau)
    # 0.5 (the user, with the reference instrument): 0.8 ms .. 2.5 s (the manual's 5.5 s top was too slow)
    att = [knob("ATTACK", v) for v in range(128)]
    dec = [knob("DECAY", v) for v in range(128)]
    rel = [knob("RELEASE", v) for v in range(128)]
    L += arr("ATT_MS_X10", "uint32_t", [int(round(m * 10)) for m in att])
    L += arr("DEC_MS_X10", "uint32_t", [int(round(m * 10)) for m in dec])
    L += arr("REL_MS_X10", "uint32_t", [int(round(m * 10)) for m in rel])
    # the attack: its phase's step a block (Q24 of the rise, ATTACK's time to the top), its shape (Q15, 257 points)
    L += arr("ATT_DPH", "uint32_t", [max(1, min(1 << 24, int(round((1 << 24) * CTL / (m / 1000 * FS))))) for m in att], 8)
    L += arr("ATT_SHAPE", "uint16_t", [int(round(32767 * att_shape(i / 256))) for i in range(257)], 12)
    # an ATTACK saved before 0.5 (0.8 ms .. 5.5 s) -> the same time on the knob now (its nearest)
    L += arr("ATT_FROM_04", "uint8_t", [att_from_04(v) for v in range(128)], 16)
    # (0.5.2) an ATTACK set by 0.5 / 0.5.1 (153 ms at noon) -> the same time on the knob now (153 ms at 10)
    L += arr("ATT_FROM_051", "uint8_t", [att_from_051(v) for v in range(128)], 16)
    # (0.5.2) a DECAY set by 0.5 / 0.5.1 (668 ms at noon) -> the same time on the knob now (1.2 s at noon)
    L += arr("DEC_FROM_051", "uint8_t", [dec_from_051(v) for v in range(128)], 16)
    # (0.5.2) a RELEASE set by 0.5 / 0.5.1 (330 ms at 9) -> the same time on the knob now (458 ms at 9)
    L += arr("REL_FROM_051", "uint8_t", [rel_from_051(v) for v in range(128)], 16)
    # SUSTAIN: the level over the knob, Q15 of the EG's top (8 V)
    L += arr("SUS_Q15", "uint16_t", [int(round(32767 * knob("SUSTAIN", v) / 10.0)) for v in range(128)])
    # (0.5) VCF MOD AMT: 64 = 0, +-32767 at the ends (EG at its top: +-8 octaves), |x| ^ VCF_AMT_LAW between
    L += arr("VCF_AMT_Q15", "int16_t", [int(round(math.copysign(32767 * (abs(v - 64) / (63 if v > 64 else 64))
                                                               ** VCF_AMT_LAW, v - 64))) for v in range(128)])
    L += arr("ENV_K_DEC", "uint32_t", [k_block(m / 1000 / TAUS["DECAY"]) for m in dec], 8)
    L += arr("ENV_K_REL", "uint32_t", [k_block(m / 1000 / TAUS["RELEASE"]) for m in rel], 8)
    # GLIDE: 0 = none, else its range (KNOB) to 95 % of the way (an RC lag on the keyboard CV), Q16 a block
    gms = [knob("GLIDE", v - 1, 126) for v in range(1, 128)]
    L += arr("GLIDE_MS", "uint16_t", [0] + [int(round(m)) for m in gms])
    L += arr("GLIDE_K", "uint16_t", [65535] + [max(1, min(65535, int(round(65536 * (1 - math.exp(-CTL / (m / 3000 * FS)))))))
                                             for m in gms])
    # the LFO (the reference manual): 0.1 .. 550 Hz over RATE, as a pitch (Q8 semitones from MIDI 0, negative below 8.18 Hz) so its
    # step comes from the oscillators' exponential converter (the LFO RATE input adds 1 V an octave)
    lhz = [knob("LFO_RATE", v) for v in range(128)]
    L += arr("LFO_HZ_X100", "uint32_t", [int(round(h * 100)) for h in lhz])
    # its skew (LFO_RISE_PCT): the square's fall (Q32 of the turn) and the triangle's slopes for a 15-bit phase (Q15)
    rise = int(round(LFO_RISE_PCT / 100 * 2 ** 32)) & ~((1 << 17) - 1)
    r15, f15 = rise >> 17, (2 ** 32 - rise) >> 17
    L += [f"#define LFO_RISE {rise}u   /* {LFO_RISE_PCT:g} % of the turn: the triangle's top, the square's fall */",
          f"#define LFO_KU {int(65535 * 32768 // r15)}u   /* the rise: 65535 over {r15} steps, Q15 */",
          f"#define LFO_KD {int(65535 * 32768 // f15)}u   /* the fall: over {f15} */"]
    L += arr("LFO_P8", "int16_t", [int(round((69 + 12 * math.log2(h / 440.0)) * 256)) for h in lhz])
    # 0.2, the effects: the chorus's sweep 0.02 .. 50 Hz (a phase step a sample; TYPE I / II / I+II at 52, 61,
    # 100); the delay's LOW CUT (20 .. 2000 Hz) and HIGH CUT (500 .. 20000 Hz) one-poles (k Q15)
    chz = [0.02 * (2500 ** (v / 127)) for v in range(128)]
    L += arr("CH_HZ_X100", "uint32_t", [int(round(h * 100)) for h in chz])
    L += arr("CH_INC", "uint32_t", [int(h / FS * 2 ** 32) for h in chz], 8)
    lc = [20 * 100 ** (v / 127) for v in range(128)]
    hc = [500 * 40 ** (v / 127) for v in range(128)]
    L += arr("DLC_HZ", "uint16_t", [int(round(f)) for f in lc])
    L += arr("DHC_HZ", "uint16_t", [int(round(f)) for f in hc])
    L += arr("DLC_K", "uint16_t", [int(round(32768 * (1 - math.exp(-2 * math.pi * f / FS)))) for f in lc])
    L += arr("DHC_K", "uint16_t", [min(32767, int(round(32768 * (1 - math.exp(-2 * math.pi * f / FS))))) for f in hc])
    # equal-power pan: gain Q12 for position 0..128 (sin), the other side reads it mirrored
    L += arr("PAN_Q12", "uint16_t", [int(round(4096 * math.sin(math.pi / 2 * i / 128))) for i in range(129)])
    # the knobs' arcs: cos in 5 degree steps, Q8
    L += arr("COS_Q8", "int16_t", [int(round(256 * math.cos(math.radians(5 * i)))) for i in range(72)], 12)
    # a sine period in 1024 steps (+1 to interpolate), Q15
    L += arr("SINE_Q15", "int16_t", [int(round(32767 * math.sin(2 * math.pi * i / 1024))) for i in range(1025)], 12)
    # ---- the LADDER: the loop gain (Q12) and the passband trim (Q12) over the RESONANCE knob (its dial: KNOB)
    dial = [res_dial(v) for v in range(128)]
    lk = [LAD_KC * lin(LAD_R, d) for d in dial]
    L += [f"#define LAD_KC_Q12 {int(round(LAD_KC * 4096))}", "#define LAD_GM0 16   /* the first note of LAD_G / LAD_GX */"]
    L += arr("LAD_K", "uint16_t", [int(round(k * 4096)) for k in lk])
    trim = [max(-1.5, min(0.0, lin(LAD_LOSS, d) - (-20 * math.log10(1 + k)))) + res_level_db(v)
            for v, (d, k) in enumerate(zip(dial, lk))]   # (0.5: + RES_LEVEL_DB, the reference's level)
    L += arr("LAD_TRIM", "uint16_t", [int(round(4096 * 10 ** ((t - LADDER_DRIVE_DB) / 20))) for t in trim])   # (+ the drive's makeup)
    L += [f"#define LAD_IN_Q8 {int(round(224 * 10 ** (LADDER_DRIVE_DB / 20)))}   /* the VCO into the ladder: 7/8 x the drive (LADDER_DRIVE_DB {LADDER_DRIVE_DB:g} dB) */"]
    G, GX = [], []
    for m in range(16, 138):                      # the singing pitch, MIDI 16..137
        f = min(440.0 * 2 ** ((m - 69) / 12.0), FS * 0.45)
        g = math.tan(math.pi * f / FS) / LAD_W0
        gx = g * LAD_WX
        G.append(int(round(g / (1 + g) * (1 << 24))))
        GX.append(int(round(gx / (1 + gx) * (1 << 24))))
    L += arr("LAD_G", "uint32_t", G, 6)
    L += arr("LAD_GX", "uint32_t", GX, 6)
    # the screen's EG shapes over a segment (33 points, Q15): the attack (att_shape: it starts slow), an RC decay to 5 %
    L += arr("ENVG_ATT", "uint16_t", [int(round(32767 * att_shape(i / 32))) for i in range(33)], 11)
    L += arr("ENVG_DEC", "uint16_t", [int(round(32767 * math.exp(-3.0 * i / 32))) for i in range(33)], 11)
    L += filter_graphs()
    Path(path).write_text("\n".join(L) + "\n")
    print(f"tables: {path}  (LADDER: KC {LAD_KC:.4f}, W0 {LAD_W0:.4f}, WX {LAD_WX:.4f}; CUTOFF MIDI {midi_of(knob('CUTOFF', 0)):.2f} .. {midi_of(knob('CUTOFF', 127)):.2f})")


# ---- the screen's filter curve: the ladder's response around its cutoff (-10 .. +10 octaves, 6 points an octave), at
# 9 resonances over the knob, in half-dB (int8, -60 .. +30 dB). The screen interpolates it and shifts it to the cutoff
# (a shift on a log axis), the live one included.
FR_N, FR_PER_OCT, FR_ROWS = 121, 6, (0, 16, 32, 48, 64, 80, 96, 112, 127)


def filter_graphs():
    def k_lad(v):
        return min(LAD_KC * lin(LAD_R, res_dial(v)), LAD_KC * 0.985)

    def trim_lad(v):
        d = res_dial(v)
        k = LAD_KC * lin(LAD_R, d)
        return 10 ** (max(-1.5, min(0.0, lin(LAD_LOSS, d) - (-20 * math.log10(1 + k)))) / 20)

    def lad(r, v):
        w = r * LAD_W0
        h = 1 / (1 + 1j * w)
        hx = 1 / (1 + 1j * w / LAD_WX)
        return trim_lad(v) * h ** 4 / (1 + k_lad(v) * hx * h ** 4)

    L = [f"#define FR_N {FR_N}", f"#define FR_PER_OCT {FR_PER_OCT}", f"#define FR_NROWS {len(FR_ROWS)}"]
    vals = []
    for v in FR_ROWS:
        for i in range(FR_N):
            r = 2 ** ((i - (FR_N - 1) // 2) / FR_PER_OCT)
            db = 20 * math.log10(abs(lad(r, v)) + 1e-9)
            vals.append(max(-120, min(60, int(round(db * 2)))))
    L += arr("FR_DB", "int8_t", vals, 30)
    return L


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "firmware/gen/mfm_tables.h")
