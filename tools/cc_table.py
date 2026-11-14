#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The MIDI chart for the docs: every parameter's NRPN number (MSB 0, LSB = number) and its CC, and the values a
patch cable's CC takes.
  cc -Ifirmware/src -Ifirmware/gen -o build/nrpn tools/nrpn.c && build/nrpn > build/params.txt
  tools/cc_table.py > CC-NRPN.md"""
from pathlib import Path
from mfm_params import P, MAX, cc_value

NAME = {
    "P_CUTOFF": "VCF CUTOFF", "P_RES": "VCF RESONANCE", "P_VCF_AMT": "VCF MOD AMT (64 = 0)", "P_VCF_MIX": "VCF MOD MIX (0 EG .. 127 LFO)",
    "P_ATTACK": "EG ATTACK", "P_DECAY": "EG DECAY", "P_SUSTAIN": "EG SUSTAIN", "P_RELEASE": "EG RELEASE",
    "P_WAVE": "VCO WAVE (0 saw .. 127 pulse)", "P_PW": "VCO PULSE WIDTH (0 = 5 %, 127 = 50 %)",
    "P_PITCH": "VCO PITCH (64 = 0, un semitono ogni ~1,06)", "P_VCO_MIX": "VCO MOD MIX (0 EG .. 127 LFO)",
    "P_PITCH_AMT": "VCO PITCH MOD AMT", "P_PWM_AMT": "VCO PWM AMT", "P_LFO_RATE": "LFO RATE (0,1 .. 550 Hz)",
    "P_LFO_WAVE": "LFO WAVE (0 tri .. 127 square)", "P_VOLUME": "VCA VOLUME", "P_VCA_MODE": "VCA MODE (0-63 EG, 64-127 ON)",
    "P_GLIDE": "KB GLIDE", "P_KB_SCALE": "KB SCALE", "P_FOLD": "UTL FOLD", "P_ONE_LVL": "UTL ONE LVL",
    "P_ATTN": "UTL ATTENUATOR", "P_FINE": "VCO FINE (64 = 0) *", "P_PRIO": "KEYBOARD PRIORITY (LOW / LAST / HIGH) *",
    "P_RETRIG": "KEYBOARD RETRIG (LEGATO / ALWAYS) *", "P_BEND": "PITCH BEND RANGE *", "P_DRIFT": "ANALOG (la deriva) *",
    "P_ARP_MODE": "ARP MODE *", "P_ARP_OCT": "ARP OCTAVES *", "P_ARP_RATE": "ARP RATE *", "P_ARP_GATE": "ARP GATE *",
    "P_CH_ON": "CHORUS ON *", "P_CH_TYPE": "CHORUS TYPE (I, II, I+II, FREE) *", "P_CH_RATE": "CHORUS RATE *",
    "P_CH_DEPTH": "CHORUS DEPTH *", "P_CH_MIX": "CHORUS SEND *", "P_DL_ON": "DELAY ON *", "P_DL_SEND": "DELAY SEND *",
    "P_DL_TIMEL": "DELAY TIME L (valori di nota) *", "P_DL_TIMER": "DELAY TIME R *", "P_DL_FB": "DELAY FEEDBACK *",
    "P_DL_LOW": "DELAY LOW CUT *", "P_DL_HIGH": "DELAY HIGH CUT *", "P_DL_WIDTH": "DELAY WIDTH *",
    "P_DL_PING": "DELAY PING-PONG *", "P_RV_ON": "REVERB ON *", "P_RV_SEND": "REVERB SEND *", "P_RV_DECAY": "REVERB DECAY *",
    "P_RV_DAMP": "REVERB DAMP *", "P_RV_PRE": "REVERB PRE-DELAY *", "P_RV_DIFF": "REVERB DIFFUSION *",
    "P_RV_WIDTH": "REVERB WIDTH *",
    "P_LFO_SYNC": "LFO SYNC (FREE, 8/1 .. 1/32T) *", "P_CLK_DIV": "CLOCK: valore di nota (8/1 .. 1/32T) *",
    "P_CLK_WIDTH": "CLOCK GATE (durata dell'impulso) *",
    "P_NOISE": "NOISE: il rumore bianco nel VCF *",
}
JIN = ["1V/OCT", "PWM", "CUTOFF", "VCA CV", "LFO RATE", "GATE", "FOLD IN", "S+H IN (senza cavo: NOISE)", "S+H GATE", "ONE", "TWO", "ATTN",
       "(MULT: non c'è più, ignorata)"]
JSRC = ["nessun cavo", None, "KB CV", "VCO", "LFO", "EG", "S+H", "ONE", "ONE+TWO", "ATTN", None, None, "VELO *",
        "MODWHL *", "PRESS *", "CLOCK *", "DUMMY (spina sola: 0 V, niente normalizzazione) *", "NOISE (rumore bianco) *"]   # (None: not offered)


def names():
    out = {P[k]: v for k, v in NAME.items()}
    for i, j in enumerate(JIN):
        out[P["P_JACK"] + i] = f"PATCHBAY: il cavo in {j}"
    return out


def main():
    rows = [line.split(" ", 2) for line in Path("build/params.txt").read_text().splitlines()]
    nm = names()
    if len(rows) != P["NPARAM"] or len(nm) != P["NPARAM"]:
        raise SystemExit(f"{len(nm)} names, {len(rows)} rows for {P['NPARAM']} parameters: update tools/cc_table.py")
    print("# FMav-1: CC e NRPN\n")
    print("Ogni parametro del suono ha una CC diretta e un numero NRPN (CC 99 = 0, CC 98 = numero, CC 6 = valore "
          "0..127, scalato sul suo intervallo). CC fisse: 74 CUTOFF, 71 RESONANCE, 73 ATTACK, 72 RELEASE, 75 DECAY, "
          "5 GLIDE, 93 CHORUS SEND, 92 DELAY SEND, 91 REVERB SEND, 1 mod wheel, 7 volume della traccia, 64 sustain, "
          "120 / 123 tutto spento. Le altre dalla CC 20 in su. Ogni traccia sul suo canale (N, N+1, N+2, N+3). "
          "Con * le estensioni che il pannello di riferimento non ha.\n")
    print("| CC | NRPN | Parametro |")
    print("| --- | --- | --- |")
    for i, cc, _ in rows:
        n = int(i)
        print(f"| {cc if cc != '0' else '-'} | {n} | {nm[n]} |")
    print("\n## I cavi via MIDI\n")
    jcc = [cc for i, cc, _ in rows if P["P_JACK"] <= int(i) < P["P_JACK"] + P["NJIN"] - 1]
    print(f"La CC di una presa ({jcc[0]}-{jcc[-1]}) sceglie da quale uscita arriva il suo cavo. Il valore da mandare per ciascuna "
          "uscita (lo stesso per tutte le prese; dal Digitakt un p-lock sulla CC cambia il cavo a quel passo):\n")
    print("| Uscita | Valore CC |")
    print("| --- | --- |")
    mx = MAX[P["P_JACK"]]
    for s, name in enumerate(JSRC):
        if name:
            print(f"| {name} | {cc_value(P['P_JACK'], s, mx)} |")
    print("\nUn valore che una presa non accetta (un'uscita del suo stesso modulo, VCA OUT, i vecchi MULT) toglie il cavo.")


if __name__ == "__main__":
    main()
