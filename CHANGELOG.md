# Changelog

## 0.3.1 (2026-11-14)

- **DUMMY** last in every input's list (after the MIDI sources).
- **MIDI channel 1** by default (tracks 1-4 on channels 1-4) on a fresh start; a saved channel stays.
- **SEQ** has two items: **PATCHBAY** (every cable) and **INPUTS** (the 12 inputs on knobs, 3 pages); SEQ pressed again
  switches between them.
- The keys a held combination can press are lit: SAVE held lights F3 / G3 / A3 (INIT, RANDOM, MUTATE), EDIT held lights
  OCT- / OCT+ (undo / redo).
- The sound is unchanged: the 90 programs render bit-identical to 0.3.

## 0.3 (2026-11-13): FMav-1

- The name: **FMav-1**, identity **FM-1_3628** (0.1-0.2 had a working name and FM-1_988, which is another
  firmware's on The Firmware Hub). Programs saved by 0.1 and 0.2 are converted.
- **REC**: every effect off / on together, each sound's settings kept (lit = the effects play; remembered).
- **The MIDI clock** (TRS or USB) as a source: **LFO SYNC** (FREE, 8/1 .. 1/32T, phase-locked, START realigns), a
  **CLOCK** output (note value, GATE length) any input can take, the **arpeggiator** on the clock's grid. TEMPO without
  a clock.
- **DUMMY** cable on every input (the normal broken, 0 V), one detent after "---".
- The **mult** is gone; **VCA OUT** is the audio out (no cable from it). Old cables from the mult are taken from its
  source.
- **SEQ**: the whole patchbay first (full screen, external MIDI cables from outside the panel, a dummy hanging loose),
  then all the inputs on the knobs with a list of their cables. **SELECT pressed opens UTL** (folder, mixer,
  attenuator); no page has a switch on SELECT any more (VCA MODE and the effects' ON are knobs).
- **90 factory programs** (50 new); programs 1-40 sound bit-identical to 0.2.
- OCT- / OCT+ LEDs show the octave (lit, slow blink, fast blink).
- The LFO's lights are steady when the LFO is faster than they can show.
- Fixed: a lit knob erased part of its box's border; MIDI-source cables were drawn with a plug at their far end.

## 0.2 (2026-11-13)

Four tracks; chorus / delay / plate reverb as shared sends; an animated UI of our own drawing; fast knob turns counted
(the FMini-1 #126 decoder); INIT = the reference manual's initial patch; jacks never offer their own module's outputs.

## 0.1 (2026-11-13)

First version: one track, the whole voice and patchbay, 40 programs.
