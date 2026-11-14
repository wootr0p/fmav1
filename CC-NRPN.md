# FMav-1: CC e NRPN

Ogni parametro del suono ha una CC diretta e un numero NRPN (CC 99 = 0, CC 98 = numero, CC 6 = valore 0..127, scalato sul suo intervallo). CC fisse: 74 CUTOFF, 71 RESONANCE, 73 ATTACK, 72 RELEASE, 75 DECAY, 5 GLIDE, 93 CHORUS SEND, 92 DELAY SEND, 91 REVERB SEND, 1 mod wheel, 7 volume della traccia, 64 sustain, 120 / 123 tutto spento. Le altre dalla CC 20 in su. Ogni traccia sul suo canale (N, N+1, N+2, N+3). Con * le estensioni che il pannello di riferimento non ha.

| CC | NRPN | Parametro |
| --- | --- | --- |
| 74 | 0 | VCF CUTOFF |
| 71 | 1 | VCF RESONANCE |
| 20 | 2 | VCF MOD AMT (64 = 0) |
| 21 | 3 | VCF MOD MIX (0 EG .. 127 LFO) |
| 73 | 4 | EG ATTACK |
| 75 | 5 | EG DECAY |
| 22 | 6 | EG SUSTAIN |
| 72 | 7 | EG RELEASE |
| 23 | 8 | VCO WAVE (0 saw .. 127 pulse) |
| 24 | 9 | VCO PULSE WIDTH (0 = 5 %, 127 = 50 %) |
| 25 | 10 | VCO PITCH (64 = 0, un semitono ogni ~1,06) |
| 26 | 11 | VCO MOD MIX (0 EG .. 127 LFO) |
| 27 | 12 | VCO PITCH MOD AMT |
| 28 | 13 | VCO PWM AMT |
| 29 | 14 | LFO RATE (0,1 .. 550 Hz) |
| 30 | 15 | LFO WAVE (0 tri .. 127 square) |
| 31 | 16 | VCA VOLUME |
| 32 | 17 | VCA MODE (0-63 EG, 64-127 ON) |
| 5 | 18 | KB GLIDE |
| 33 | 19 | KB SCALE |
| 34 | 20 | UTL FOLD |
| 35 | 21 | UTL ONE LVL |
| 36 | 22 | UTL ATTENUATOR |
| 37 | 23 | VCO FINE (64 = 0) * |
| 38 | 24 | PATCHBAY: il cavo in 1V/OCT |
| 39 | 25 | PATCHBAY: il cavo in PWM |
| 40 | 26 | PATCHBAY: il cavo in CUTOFF |
| 41 | 27 | PATCHBAY: il cavo in VCA CV |
| 42 | 28 | PATCHBAY: il cavo in LFO RATE |
| 43 | 29 | PATCHBAY: il cavo in GATE |
| 44 | 30 | PATCHBAY: il cavo in FOLD IN |
| 45 | 31 | PATCHBAY: il cavo in S+H IN (senza cavo: NOISE) |
| 46 | 32 | PATCHBAY: il cavo in S+H GATE |
| 47 | 33 | PATCHBAY: il cavo in ONE |
| 48 | 34 | PATCHBAY: il cavo in TWO |
| 49 | 35 | PATCHBAY: il cavo in ATTN |
| 50 | 36 | PATCHBAY: il cavo in (MULT: non c'è più, ignorata) |
| 51 | 37 | KEYBOARD PRIORITY (LOW / LAST / HIGH) * |
| 52 | 38 | KEYBOARD RETRIG (LEGATO / ALWAYS) * |
| 53 | 39 | PITCH BEND RANGE * |
| 54 | 40 | ANALOG (la deriva) * |
| 55 | 41 | ARP MODE * |
| 56 | 42 | ARP OCTAVES * |
| 57 | 43 | ARP RATE * |
| 58 | 44 | ARP GATE * |
| 59 | 45 | CHORUS ON * |
| 60 | 46 | CHORUS TYPE (I, II, I+II, FREE) * |
| 61 | 47 | CHORUS RATE * |
| 62 | 48 | CHORUS DEPTH * |
| 93 | 49 | CHORUS SEND * |
| 63 | 50 | DELAY ON * |
| 92 | 51 | DELAY SEND * |
| 65 | 52 | DELAY TIME L (valori di nota) * |
| 66 | 53 | DELAY TIME R * |
| 67 | 54 | DELAY FEEDBACK * |
| 68 | 55 | DELAY LOW CUT * |
| 69 | 56 | DELAY HIGH CUT * |
| 70 | 57 | DELAY WIDTH * |
| 76 | 58 | DELAY PING-PONG * |
| 77 | 59 | REVERB ON * |
| 91 | 60 | REVERB SEND * |
| 78 | 61 | REVERB DECAY * |
| 79 | 62 | REVERB DAMP * |
| 80 | 63 | REVERB PRE-DELAY * |
| 81 | 64 | REVERB DIFFUSION * |
| 82 | 65 | REVERB WIDTH * |
| 83 | 66 | LFO SYNC (FREE, 8/1 .. 1/32T) * |
| 84 | 67 | CLOCK: valore di nota (8/1 .. 1/32T) * |
| 85 | 68 | CLOCK GATE (durata dell'impulso) * |
| 86 | 69 | NOISE: il rumore bianco nel VCF * |

## I cavi via MIDI

La CC di una presa (38-49) sceglie da quale uscita arriva il suo cavo. Il valore da mandare per ciascuna uscita (lo stesso per tutte le prese; dal Digitakt un p-lock sulla CC cambia il cavo a quel passo):

| Uscita | Valore CC |
| --- | --- |
| nessun cavo | 0 |
| KB CV | 12 |
| VCO | 19 |
| LFO | 27 |
| EG | 34 |
| S+H | 42 |
| ONE | 49 |
| ONE+TWO | 57 |
| ATTN | 64 |
| VELO * | 86 |
| MODWHL * | 94 |
| PRESS * | 101 |
| CLOCK * | 109 |
| DUMMY (spina sola: 0 V, niente normalizzazione) * | 116 |
| NOISE (rumore bianco) * | 124 |

Un valore che una presa non accetta (un'uscita del suo stesso modulo, VCA OUT, i vecchi MULT) toglie il cavo.
