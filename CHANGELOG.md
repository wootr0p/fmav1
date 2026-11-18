# Changelog

## 0.5.2 (2026-11-18)

- **ATTACK as the reference's** (the user, by ear: the reference's ATTACK at 10 o'clock sounds as 0.5.1's noon):
  153 ms at 10 o'clock (it was at noon); 44 ms at 9 and 1.15 s at 3 (the scope's) stay; noon is ~350 ms now. The
  factory programs and the programs saved by 0.5 / 0.5.1 keep their attack time (ATT_FROM_051; the saves' tag is
  MFM7 now): only the knob's position moves.
- **DECAY 1.2 s at noon** (the user; was 668 ms); 437 ms at 9 and the ends stay. The programs keep their decay
  time as above (DEC_FROM_051).
- **RELEASE 458 ms at 9 o'clock** (the user; was 330 ms); noon 627 ms and the ends stay; the programs keep their
  release time the same way (REL_FROM_051). No program changes its sound.
- **REC held shows the MIDI CCs** (the user): after half a second every knob on the screen shows its CC in a filled
  badge (`CC 74`) instead of its value, the PATCHBAY each input's CC instead of its name; `NO CC` for the globals.
  Let go: the values again, the effects untouched. A short tap (under half a second) still turns every effect off /
  on, now on the release.
- **CC 7 is the track's mixer LEVEL** (the user: the four levels with a CC): on each track's channel it moves its
  LEVEL knob, on its scale (0-127: 100 = unity, 80 the default with four tracks), shown and saved with the
  settings. It was a separate gain multiplying the LEVEL, unseen.
- **CC 10 is the track's mixer PAN** (the user): 0 all left, 64 the centre, 127 all right; shown and saved the same
  way.

## 0.5.1 (2026-11-16)

- **KEYBOARD moved to SELECT pressed** (the user), its second page after UTL; HOME is the MIXER (levels, pans),
  VOICE and GLOBAL.
- **A new patch source, GATE**: +5 V while a note is held (the FM-1's keys or MIDI, the arpeggiator's notes too),
  0 V when let go. It takes the slot the CLOCK output left in 0.5, so no other cable's CC value moves (GATE: 109);
  a cable from CLOCK in a program saved by 0.4.1 or before still becomes no cable.
- **PRESS is called AFTERTC** (the MIDI aftertouch).
- **The external sources' cables** (VELO, MODWHL, AFTERTC, GATE) share one colour, khaki, apart from the panel's
  outputs; two coming into one column enter from their own points and bow apart.

## 0.5 (2026-11-16)

Every knob calibrated against the reference instrument (the user's recordings, scope captures and photos of its panel),
the 90 programs rebuilt from INIT so they can be set up on it, and the panel reorganised.

### The sound, calibrated to the reference instrument

- **The knobs turn as the reference panel's**: drawn from 7 to 5 o'clock (300 degrees, was 270), eleven ticks on the
  hours (the big knobs: a tick every 10 degrees). A knob set at 2 o'clock on the panel is set here at its 2 o'clock
  tick. Every law is in one table, `KNOB` at the top of `tools/gen_tables.py` (a value at 7 o'clock, at any hours in
  between, at 5 o'clock, and "exp" or "lin").
- **A more faithful ladder**: each stage saturates its own state as well as its input, as the transistor pairs do
  (dV/dt ~ tanh(Vin) - tanh(V): Huovilainen 2004, D'Angelo & Valimaki 2013); ~8 % more CPU in the worst case. Fitted
  to the reference's recordings (its INIT, a C2 saw, five CUTOFF / RESONANCE settings): the -3 dB corner at ~24, 289
  and ~4800 Hz at 9 o'clock, noon and 3; the whistle at 2.12 times the corner; RESONANCE starts to whistle halfway
  between 3 and 4 o'clock. Checked on the FM-1 itself (its USB audio): the harmonics within 0.7 dB open and 1.3 dB at
  RESONANCE noon, the whistle 589 Hz (the reference's 583). CUTOFF at 10:28 is 95 Hz (a second recording).
- **The ladder sings from silence**, as the hardware's: past the singing point it starts with nothing in it (a hiss
  of one 16-bit step a block); under it, silence.
- **CUTOFF fully counterclockwise is silent, with no click** (the pulse's DC no longer reaches the VCF).
- **The EG**: ATTACK 0.8 ms .. 2.5 s on an RC curve aiming at 1.11 times the top, restarting from 0 at each new gate
  as the reference's; DECAY 3 ms .. 16 s and RELEASE 3 ms .. 18 s in the reference's seconds; at noon ATTACK 153 ms,
  DECAY 668 ms, RELEASE 627 ms; at 9 o'clock ATTACK 44 ms, DECAY 437 ms, RELEASE 330 ms; SUSTAIN 13 % at 9 o'clock,
  48 % at noon, linear above.
- **VCF MOD AMT** follows |x|^1.5 (was the square: 3.3 octaves at 2:48).
- **The LFO**: 0.165 .. 550 Hz, 4.7 Hz at noon; between triangle and square as the reference's (the square high while
  the triangle rises: halfway it steps at the peaks), both skewed 58 / 42.
- **The pulse width runs 0 .. 50 %** (knob, PWM AMT and the PWM jack; it was 1 .. 99 %): fully right is always a square,
  and a modulation under 0 % silences the pulse, as the reference does.
- **S+H IN's normal is the VCO again**, as the reference panel says (S+H (VCO)); programs saved by 0.4 .. 0.5's
  first builds that sampled the noise get a NOISE > S+H IN cable and sound as before.
- **The CLOCK output is gone** (the reference panel has none); LFO SYNC, the arpeggiator and the delay still follow the
  clock. A saved cable from CLOCK becomes no cable; CC 84 and 85 do nothing.

### The programs

- **The 90 factory programs rebuilt from INIT**: each lists only the knobs its sound needs (every other one at INIT's
  where the simulator heard no difference) and uses only what the reference panel has, the effects aside (no white
  noise, no VELO / MODWHL / PRESS cable, no FINE, ANALOG, arp or LFO SYNC), so each can be set up on the reference from
  its INIT by turning only the knobs listed (`firmware/src/mfm_patches.h`, in the panel's order). Their sounds are
  0.4.1's, each knob moved to the same Hz, time and level on the new curves, the levels as 0.4.1's. The noise programs
  (37 NOISE HIT, 69 CLOCK HATS, 79 WIND, 81 SNARE, 82 HI-HAT, 83 CLAP) make their noise with the VCO and the LFO
  modulating each other at audio rate; the rhythmic ones have their LFO free at the rate it had at 120 bpm; 42 ARP
  PLUCK and 70 SYNC ARP no longer arpeggiate, 59 WHEEL LEAD no longer takes the wheel.
- **A new order**: 01 BRASS LEAD, 02 CLASSIC LEAD, 03 CLASSIC BASS, 04 PWM LEAD, 05 FOLDED TRI, 06 DOTTED LEAD, then
  the others in their old order. The four tracks start on 01, 11, 21, 31 (first power-on, the memory's reset).
- **CLASSIC BASS** on the reference's own knobs (a photo, and the laws fitted to its recording); **BRASS LEAD** with
  the user's EG and LFO RATE.
- **INIT** (91-99, SAVE + F3) as the reference's: the saw, PULSE WIDTH 50 %, both MOD MIXes on EG, the LFO a triangle,
  CUTOFF open, RESONANCE 0, ATTACK, DECAY, RELEASE 0, SUSTAIN full.

### The panel

- **The buttons reorganised**: HOME opens the MIXER (levels, then pans), then KEYBOARD, VOICE, GLOBAL; EDIT the VCO;
  GLO the VCF; the PERFORM pages are gone.
- **Deterministic navigation** (for the blind, and muscle memory): a section's button opens its first page, pressed
  again its next one (pressed N times = page N); SELECT stops at the first and the last page; EDIT and SAVE act on
  their release however long they were held. The guide has the map of every page.
- **FX**: the delay first, then the reverb, then the chorus.
- **CUT FINE** (GLO, the VCF's third knob) and **RATE FINE** (LFO, fourth knob): +-2 semitones of fine tuning
  (CC 87 / NRPN 70, CC 88 / NRPN 71). Not on the reference panel.
- **ARP pressed again** no longer turns the arp on / off: its MODE knob does.
- **The memory's reset**: SAVE held + G5 asks "RESET ALL MEMORY?", OCT+ confirms (anything else, or 8 s, cancels):
  every saved program and setting goes and the FM-1 restarts as on its first install.
- **The LEDs**: HOLD lights PLAY green; the LFO button no longer blinks with the LFO (its LED is on the screen); fixed:
  REC's LED (the effects on) stayed dark after power-on.
- **The patchbay cables bow across their line**: a cable between two jacks of one column no longer runs straight
  over the others (the sides alternate), and parallel cables never coincide.
- **The version on the boot screen.**

## 0.4.1 (2026-11-14)

- **The clock's source**: GLOBAL > **CLOCK** = **INTERNAL** (the TEMPO knob; every MIDI clock and Start / Stop is
  ignored), **USB** or **TRS** (the clock and Start / Stop / Continue only from that one, the other ignored). Until 0.4
  both counted: a sequencer sending its clock on USB and TRS at once doubled it. A fresh start (or an update from 0.4)
  is INTERNAL.
- With USB or TRS **the arpeggiator follows that clock only**: its steps on the clock's ticks; with the clock stopped
  (or none yet) it plays the first note and waits, never at the TEMPO knob's pace.
- With USB or TRS **TEMPO is locked** (grey; turned, it says so) and shows the tempo the sync reads (TRS 125; --- until
  measured). The tempo is measured after one quarter already (then refined every four, as before).
- GLOBAL's knobs: MIDI CH, TUNE, CLOCK, TEMPO, then KEY VEL, LIGHTS. The CLOCK page says USB CLOCK / TRS CLOCK,
  (STOPPED), or NO CLOCK.
- The sound is unchanged.

## 0.4 (2026-11-14)

- **White noise**: a generator a track; a **NOISE** level mixes it into the VCF's input (with the VCO or the folder),
  and a **NOISE** output on the patchbay takes it to any input. With DUMMY in FOLD IN the VCF hears the noise alone.
- **The S+H samples the white noise** when S+H IN has no cable (it sampled the VCO). Programs saved by earlier versions
  that use the S+H get the cable VCO > S+H IN, so they sound as they did.
- **No knob twice** but on PERFORM: the cables only on SEQ (the modules' INPUTS pages and the S+H page are gone), UTL one
  page (FOLD, ONE LVL, ATTENUATOR, NOISE), TEMPO only on GLOBAL.
- **A section's button pressed again turns to its next page**, as SELECT (ARP again: the arp on / off).
- **18 factory programs remade with the noise** (drums, wind, the random S+H sounds, a breath on a few leads and pads, a
  noise riser); the other 72 render bit-identical to 0.3.1.
- The programs' name has a fixed place (new parameters need no conversion from now on); 0.1 .. 0.3.1 saves converted.
- The patch cables' CC values change (one more source: see CC-NRPN.md); NOISE is CC 86.

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
