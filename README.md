# FMav-1

**A semi-modular monophonic synthesizer for the M-VAVE FM-1.** One VCO (saw > pulse, pulse width, PWM), a four-pole
transistor ladder filter that self-oscillates, an LFO (0.1 .. 550 Hz, triangle > square, or locked to the MIDI clock),
an ADSR, a VCA with its MODE switch (EG / ON), a keyboard with low-note priority, legato triggering, glide and KB SCALE,
a wave folder, a sample & hold (on white noise), a two-channel mixer, an attenuator, a white-noise source, and a
patchbay of 12 inputs with their normals,
every cable running a sample at a time (the LFO, the S+H and the VCO patch into the pitch, the pulse width, the cutoff
and the folder at audio rate).

- **Four monophonic tracks**, each its own sound, on MIDI channels N .. N+3 (fixed; ALGORITHM picks the panel's).
- **The MIDI clock as a patch source** (TRS or USB): the LFO's SYNC (a note value, phase-locked, START realigns it), a
  CLOCK output any input can take (S+H GATE, GATE, CUTOFF, VCA CV ...), the arpeggiator on the clock's grid, a
  tempo-synced delay. Without a MIDI clock: TEMPO.
- **DUMMY cables**: a plug on its own breaks an input's normal (0 V), as on a real patchbay.
- **Chorus, delay and plate reverb**, shared as sends; **REC switches all of them off and on**, as they were.
- **90 factory programs** (91-99 INIT): basses, leads, patchbay tricks, clocked sounds, pads, keys, drums and drones.
- Played from outside (TRS MIDI IN or USB-MIDI: notes, bend, aftertouch, mod wheel, sustain, program change, clock, a CC
  for every parameter, the cables included, and NRPN) or from the keys. No sequencer: sequence it from a Digitakt or
  anything that sends MIDI.

The panel: a button opens a section (pressed again: its next page), SELECT turned runs through its pages, the four knobs
are the page's, SELECT pressed opens UTL (folder, mixer, attenuator, noise). Every knob has one place (PERFORM repeats
the most played on purpose). An input jack is a knob that picks the output its cable comes from: SEQ shows the whole
patchbay with its cables, then every input on the knobs. The screen is our own drawing (white print on black,
vertical module tabs, a 13-button keyboard), animated (the LFO LED, the tracks' lamps, the cables' swing, the boot: a
cable plugged into VCA OUT).

Identity `FM-1_3628`. The full guide (Italian) is [LEGGIMI.md](LEGGIMI.md), the MIDI chart [CC-NRPN.md](CC-NRPN.md),
the changes [CHANGELOG.md](CHANGELOG.md).

> **Use at your own risk.** FMav-1 is free software with **no warranty**: the author is not liable for a bricked FM-1,
> lost data or anything else. See [DISCLAIMER.md](DISCLAIMER.md) and [LICENSE](LICENSE).

## Install

From The Firmware Hub (one click, Chrome / Edge), or with the installer from a release:

```sh
python3 tools/fm1_install.py fmav1-0.4.fwsc --yes      # FM-1 on USB, switched on
```

It installs over most FM-1 firmwares (Felucca, SLOOP, OberVave and their forks) and keeps their stored programs
(FMav-1 stores its own at 0xD8000). If the FM-1 does not start: hold OCT- while switching it on (update mode) and run
the installer again.

## Build

```sh
sh build.sh --release 0.4                              # build/fmav1-0.4.fwsc
OBV_DEFS=-DMFM_NTRK=1u sh build.sh                     # the one-track build
sh tests/run_host.sh tests/mfm_programs.txt programs   # on a computer: build/host/programs.wav and the screens
sh tests/run_tests.sh                                  # every host test, sanitizers and a parameter fuzz (~25 min)
QUICK=1 sh tests/run_tests.sh                          # the same, the 90 programs not under the sanitizers (~5 min)
```

The JieLi toolchain (`tools/get_toolchain.sh`) and the AC79 SDK are needed for the device build; the host tests need
only a C compiler and Python with Pillow. Integers only (the chip has no FPU); see `docs/RULES.md` and `docs/API.md` for
the platform.

## Credits and licence

GPL-3.0-only, see [LICENSE](LICENSE) and [LICENSING.md](LICENSING.md). FMav-1 is built on
[OberVave](https://github.com/zvenson/obervave) 3.3.2 (zvenson) with its saves-in-silence
fixes and fixed MIDI channels, which stands on [Dinghy](https://github.com/zvenson/dinghy) and the FM-1 platform of
[Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton Instruments) with the changes of
[SLOOP](https://github.com/isod89/sloop-fm1) (isod89). The ladder filter and the fast-knob decoder follow
[FMini-1](https://github.com/ruppert/fmini-1) (Ruppert).

FMav-1 is an independent work, not made, endorsed or supported by M-VAVE or by any synthesizer maker. "M-VAVE" and
"FM-1" belong to their owners.
