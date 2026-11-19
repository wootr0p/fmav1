# FMav-1

A firmware for the M-VAVE FM-1: a small semi-modular mono synth, control for control and jack for jack after the user's
manual of its reference instrument (the user's request, 2026-11-13: as faithful as possible, intuitive, the controls as
the popular firmwares, easy sound design, four tracks as Felucca's, no sequencer: they sequence it from a Digitakt 2
over the TRS MIDI IN). GPL-3.0. 0.1-0.2 had a working name; **0.3 named it FMav-1** (the user: no trademark in a
public firmware; the reference instrument is named nowhere in this repo, keep it so). Identity **`FM-1_3628`** (F M A V
on a phone's keys) for every version (the version is APP_VER); 0.1-0.2 used FM-1_988, which is Quartet's on The
Firmware Hub.

Started 2026-11-13 from the author's own earlier, unreleased OberVave-based tree (never name it: the user, 0.3; where
it lives is in the private notes). The platform (`platform.c`, `usb.c`, `ota.c`, `midi_uart.c`, `lcd.c`, `gfx.c`, hal/,
the loader, `master_gain.h`, `cpu_meter.h`) is OberVave 3.3.2 + PR #32 + the fixed MIDI channels, unchanged here except
the names and the programs' window (0xD8000, 3 sectors, past the other firmwares' stores).

The source of truth for fidelity: the reference instrument's user's manual (2022, 53 pages). Its facts the code follows:
VCO 8 Hz .. 16 kHz, PITCH 8 Hz .. 8 kHz on the low C, WAVE saw > pulse mix, PULSE WIDTH narrow > square, VCO MOD MIX EG
(CCW) > LFO (CW) into PITCH MOD AMT and PWM AMT; CUTOFF 30 Hz .. 20 kHz, RESONANCE self-oscillates at the top, VCF MOD
MIX EG > LFO, VCF MOD AMT the only bipolar knob; LFO 0.1 .. 550 Hz (0.5: from 0.165 Hz, the user's scope; 4.7 Hz at noon, the user's recording), WAVE triangle > square (0.5: skewed 58 / 42), its LED flashes each cycle;
EG ATTACK 0.8 ms .. 5.5 s (0.5: .. 2.5 s, the user's ear on the real unit), DECAY / RELEASE 3 ms .. 18 s (0.5: DECAY .. 16 s in the reference's seconds: a DECAY time = 3.53 time constants, TAUS in gen_tables.py), SUSTAIN 0..8 V; VCA MODE off = EG + VCA CV, on = constant;
keyboard low-note priority, legato triggering with glide, GLIDE up to ~9 s, KB SCALE 1 .. 5 octaves; the patchbay's
inputs and normals (S+H IN <- VCO, S+H GATE <- LFO, ONE <- -5 V, ATTN <- +5 V, FOLD IN unpatched = the folder out of
the path, patched = it replaces the VCO into the VCF); the mixer: ONE x ONE LVL + TWO (unity), outputs +-8 V; PWM jack:
10 Vpp at mid width sweeps ~5 .. 50 %; GATE thresholds 1.5 / 3.5 V. Not in the manual, chosen: the jack/knob scalings
of the mod amounts (1 V an octave at full, square law on PITCH MOD AMT; VCF MOD AMT |x|^1.5, VCF_AMT_LAW, from the user's recording), the folder's law (triangle
folds at 5 / 15 / 25 V, rounded peaks, 2x oversampled).

## Hard rules

- Name only released projects in credits (OberVave, Dinghy, Felucca, SLOOP, FMini-1, Skiff, genkigenki's SMK-37
  layer): the user's unreleased firmwares appear nowhere.
- Faithful to the reference's ARCHITECTURE only: never its maker's panel art, logo or patchbook patches; the screen is
  our own black-panel drawing; the programs are our own. No trademark of a synth maker anywhere (code, comments, docs,
  screen, program names): say "the reference instrument / panel / manual".
- Four tracks by default (`MFM_NTRK 4u` in mfm_engine.h): channels N .. N+3 fixed, ALGORITHM the panel's track, HOME's
  MIXER (levels from 80, pans), four track colours; the top-right lamps say 1 2 3 4 (the user, 0.3: keep them so).
  `-DMFM_NTRK=1u` (build: `OBV_DEFS=-DMFM_NTRK=1u sh build.sh`; host: `MFM_NTRK=1 sh tests/run_host.sh ...`) builds one
  (ALGORITHM then the CUTOFF; levels at 100: the programs are levelled so).
- The effects: all three are SENDS shared by the tracks (`mfm_fx.h`), each sound sends with its own SEND and ON, the
  character is the first track's that has it on. Fixed CCs 93 / 92 / 91. **REC** (0.3) switches every effect off / on
  (`set.fx_off` -> `ctl_fx_off`: the sounds' ONs untouched; lit = on). 0.5.2 (the user): a tap (let go within
  REC_HOLD_MS, 500) toggles them on the release; held longer, `cc_view`: knob_view and the patchbay's inputs show
  each parameter's CC in a badge (`cc_of`, cc_map's inverse; V_LVL CC 7, V_PAN CC 10; `NO CC` for the V_ globals), the
  effects untouched. CC 7 = the mixer's LEVEL (cc_level -> set.lvl / ctl_lvl; the old ctl_cc7 gain is gone), CC 10
  its PAN (cc_pan: 0..63 -> -64..-1, 64..127 -> 0..+64).
  (Asked the user: short tap / hold rather than two buttons.) Tests: mfm_cc_a / _b (run_tests.sh compares them).
- The user's corrections, keep them: fast knob turns count every click (the HAL decoder takes every frame's state, after
  FMini-1 #126; `tests/enc_test.c`); a jack never offers its own module's outputs, VCA OUT (the audio out) or the mult
  (gone in 0.3) (`JIN_DENY` / `jack_ok`; `patch_fix_jacks` converts older programs: a cable from a mult comes from the
  mult's source); INIT is the manual's INITIAL PATCH (0.5, the user: WAVE 0 (saw), PW 127 (50 %), VCO / VCF MOD MIX 0 (EG), LFO WAVE 0 (triangle), CUTOFF 127, RESONANCE 0, ATTACK / DECAY / RELEASE 0, SUSTAIN 127), the factory programs start from 0.1's defaults (`FACT_BASE_01`)
  so programs 1-40 sound bit-identical to 0.1 / 0.2 (checked against renders).
- The panel as the user wants it on every FM-1 firmware here (memory: fm1-panel-navigation): in a page opened with its
  button, only SELECT turned (every page of the section, across its items) and the four knobs. **0.3: no switch on
  SELECT pressed** (the user: "toglierei il tasto SEL in ogni pagina"): SELECT pressed is UTL's button (WAVE FOLDER,
  MIXER, ATTENUATOR); what it switched is a knob. No secret combos as the only way; the page dots map the whole
  section; the program's number has the track's colour. Every jack is on its module's pages and on SEQ (the whole
  patchbay as one item, its inputs on knobs as another (SEQ again switches; `g_jacks` lists their cables).
  **0.5: deterministic** (the user, for the blind and muscle memory): a section's button opens its FIRST page,
  pressed again the next (wrapping: pressed N times = page N); SELECT stops at both ends (`page_step(e, wrap)`);
  EDIT / GLO / SAVE act on release whatever the hold if not used as a modifier; ARP again does nothing more (MODE
  turns the arp off); FX order DELAY, REVERB, CHORUS. tests/mfm_det_a/_b.txt check it (run_tests.sh `det`).
  0.5, the user: HOME = MIXER, VOICE, GLOBAL (IT_HOME, HOME_GLOBAL its CPU meter; no PERFORM; 0.5.1: KEYBOARD on
  SELECT pressed, IT_UTL's second item; J_AFTER is AFTERTC (was PRESS); J_KGATE = GATE, t->kgate as 0 / +5 V, in
  J_CLOCK's old slot so the cables' CC values stay; `patch_drop_clock` for pre-0.5 saves); EDIT = the
  VCO; GLO = the VCF (S_VCF, on press). HOLD: PLAY green (`led_play_green`: the FM-1's PLAY green at fm1_led[8]
  bit 1). The LFO button lit only on its page. leds(): the first pass sets every LED (REC was dark at power-on).
  The programs' order: 51, 11, 1, 17, 58, 66 first (build/presets/reorder.py, reorder_map.txt).
- The clock (0.3): `clock_block` in mfm_engine.h keeps one place for everything (Q20 MIDI clocks mod 2304, every note
  value divides it): at the tempo (`quarter_len`: the MIDI clock's measured, else TEMPO), held between the last MIDI
  clock and the next while one runs (`ctl_clk_ticks`, `ctl_clk_run`: the main loop counts 0xF8, 0xFA = Start puts it at
  0, the first clock after it = tick 0, 0xFC / 300 ms silence = runs on free). LFO SYNC (phase from the place; with a
  cable on LFO RATE: `p8_of_inc`, free around the synced rate), the arp on its grid while a MIDI clock runs.
  (0.3-0.4.1 also had a CLOCK patch output; 0.5 removed it, the user: not on the reference panel, not asked: J_CLOCK
  keeps its id, denied in JD_ALL, o[J_CLOCK] = 0; P_CLK_DIV / P_CLK_WIDTH stay unused.) DUMMY (J_DUMMY) = 0 V, the normal broken.
- The screen: our own drawing: white print on black, a module's box with its name on a vertical tab, the panel's knob
  names, big knobs with a dense ring, signs at the scales' ends, toggles for VCA MODE and the ONs, the 13-button
  keyboard, the patchbay with outputs in reverse print (VCA OUT's cable off the panel, MIDI sources from above, a dummy
  hanging). Animated: the LFO LED (steady over ~8 Hz on screen, ~20 Hz on the button), the tracks' lamps, the keys, a
  turned knob's light (inside its box: `ui_knob`'s cell stops short of the frame), the cables' swing, the splash (a
  cable into VCA OUT). OCT- / OCT+ LEDs: the octave (lit, slow, fast blink).
- The chip: JieLi AC791N, no FPU, no libgcc: integers only, no 64-bit division, no 64-bit products in sample loops
  (a 32-bit hardware divide exists: a few per block are fine), no hex literal that looks like a register window, no
  left shift of a negative value (UBSan in tests/sanitize.sh). RAM <= 96 KB, big buffers in `.pool`. Audio ISR: 256
  frames at 44.1 kHz = 5805 us.

## Where things are

- `firmware/src/fmav1.c`: the app (sections and items with per-page subtitles and graphs; virtual knobs `V_*`, value
  formatting `param_value`, MIDI in with the clock, storage, drawing what changed, the LEDs).
- `mfm_params.h`: the parameter table (70 in a 96-byte patch, the 12-character name at its fixed place `PNAME_AT` = 84,
  spare slots 0xFF = not set -> the default: append parameters up to 84 with no conversion; 0.1 / 0.2 / 0.3 had the name
  after 45 / 66 / 69 parameters: `patch_from_01/02/03`, tags "MFMP" / "MFM2" / "MFM3" -> "MFM4"). **The order is the
  MIDI CC order** (CC 20 up, skipping 64, 71-75, 91-93, 98, 99) and the NRPN number: append new parameters at the end.
  The jacks `P_JACK + I_*` hold a source `J_*` (0 = no cable; `JORDER` the knob's order: ---, the panel's, CLOCK, the MIDI
  ones, DUMMY last), never one `JIN_DENY` refuses. I_MULT stays as a dead slot (the CC numbers keep their meaning).
- `mfm_dsp.h`: `pitch_inc`, the VCO `mvco_tick` (polyBLEP), the LFO `mlfo_tick`, the folder `fold1` / `fold_tick`, the
  ladder `ladder_coef` / `ladder_tick` (after FMini-1's, per sample).
- `mfm_engine.h`: `track_t` (one voice), the keyboard, the EG (per block, interpolated), `clock_block`, `track_block`:
  the patchbay a sample at a time in the circuit's order, outputs in `t->o[J_*]` (volts Q12). An idle track skips the
  loop but keeps the LFO and the VCO running. `engine_audio`: levels, sends (off with REC), the DC blocker, the output
  tanh, the scope.
- `mfm_ui.h`: `ui_top`, `ui_prog`, `ui_box`, `ui_knob_at`, `ui_keys`, `ui_toast`, `ui_splash`. Canvas 240 x 192.
- `mfm_graph.h`: the graphs (`G_*`), `ui_patchbay` (`PB[7][3]`, SEQ's first page), `g_jacks` (an INPUTS page),
  `g_clk` (the clock).
- `mfm_speak.h` (0.6): what a screen reader is told (fm1-reader, issue #1): `speak()` once a loop notes what changed
  (the page `sp_ctx`, the knob turned last `knob_t` / `an_focus`, the program, the save slot) and says it only once
  no encoder has turned for SP_SETTLE_MS = 500 (the user: "mentre lo sto cambiando non leggere il valore";
  encoders() sets `sp_turn_t`, and a message said during a turn, the track or TEMPO locked, waits in `sp_msg`). A new
  page (the user): `sp_title` (with its subtitle and, the user, first its place among all its button's pages: "1 OF 7. DELAY OFF", FX's
  REVERB "4 OF 7"; a button with one page: the title alone), then a message of its own (a longer pause)
  `sp_knobs(0)`: the names only, a switch (K_ONOFF, VCA MODE) as "DELAY SWITCH"; WHERE too (the user: no values at
  the start). Every message goes through `sp_out()`: SP_WORDS says the short words in full (the user's list: TIME L
  "TIME LEFT", EG "ENVELOPE" (the user: one word, as the sections), PRIO "LEGATO PRIORITY", ONE (-5) "ONE NORMALIZED TO MINUS FIVE", ..), so
  it is right without the words file (which then holds only gen_words.py's common words); the chorus TYPE 1, 2,
  1 PLUS 2, FREE. REC's toast "EFFECTS ENGINE ON / OFF" (the user). Never the footer's hint (the user). `ann_toast` (from
  `say()` / `warn()`, warnings ANN_KEEP) joins a two-line message (`sp_line`: "|" a space, a stop or a comma).
  `sp_knob` says a knob as its page shows it (units apart: "1.2 s", "508 Hz"; a jack the way the cable runs, the
  user: "LFO TO FOLD IN", "NORMAL VCO TO S+H (VCO)", "NO CABLE TO PWM"; on its page without the title's word). `speak_dump` (host): every page, value, program. The protocol is
  `firmware/src/announce.c` (fm1-base's, MPL-2.0: never edit the copy; `../fm1-reader/tools/sync_announce.sh .` copies
  it and `tools/gen_words.py`, `--check` compares), included after platform.c; platform.c's `ann_wire_send` and the
  frames read before `ota_service`; usb.c SXQ 128 (a whole SAY). The words: `firmware/words.tsv` -> `docs/words/<id>.json`
  (commit every one, never change or delete one: a device points at it) and `firmware/gen/ann_words.h` (the link,
  https://wootr0p.github.io/fmav1/words/<id>.json: GitHub Pages from main /docs), by build.py and run_host.sh.
- `mfm_voice.h` (0.6, the user): the FM-1's own voice, Bigorna's speech.c (Nuno Zimas, GPL-3.0-only) without its
  screen capture: Klatt formants at 22050 Hz doubled, a phoneme ring (main loop -> audio), `vo_say(text, flags)`
  (ANN_NOW cuts unless a warning is still being said), `vo_mix` in `app_audio_out` (platform.c calls it after
  uac_tap: the outputs only; MASTER's level; the music ducked 6 dB) and the CPU meter counts it. Words:
  `tools/voice_words.txt` (Bigorna's + ours) -> `firmware/gen/voice_dict.h` by `tools/gen_speech.py` (build.py,
  run_host.sh). ONE text for both ways out: `sp_send(sp_out(), flags)` in mfm_speak.h; sp_out also does what the
  reader page used to ("1 OF 7", "1 TO 4", "PLUS 1", note values in words, "C C"; SEQ / RESO / REC said in
  full so the words file's common ones change nothing). `tests/voice_words.sh` (tools/voice_check.py): every word
  said is in the dictionary, and the page (audit.mjs --spoken) says them word for word. `set.voice` (a spare
  settings byte), PLAY held 2 s (the user; was 3) (`voice_toggle`; a tap is HOLD on the release). (The user) the page's knobs numbered
  ("1 CUTOFF, 2 RESONANCE"); SAVE `say_as` (the screen's toast, the user's longer words); its keys' hint said only
  when SAVE is held 0.6 s (`save_hint_t`); the voice's comma VO_COMMA 170 ms, a stop VO_STOP. (The user, then) a
  switch said by its whole name (sp_name(id, 0)); effects' titles without ON / OFF; `sp_reread` (go_section) says
  a one-page section's page again; `sp_cc()`: REC held shows the CCs, but with the voice on it reads the values
  (sp_knobs(1)), and REC let go says nothing. ARP: `P_ARP_ON` (appended, CC 89, NRPN 72; an old save's 0xFF slot
  converted in patch_fix_values: MODE 0 = off, else on with MODE - 1), `P_ARP_MODE` AR_UP + v (0..6), GATE on a
  second ARP page. (The user, round 9) REC held: CC badges over the knobs (`knob_badge`, kv->cc; the patchbay: over
  the jacks), the value in view, never said; a section's button acts on its release, held REC_HOLD_MS, 0.5 s as REC (`secb_hold`,
  `sp_values`) its page with values; a knob's value said only when turned with REC held (`sp_turn_rec`, and
  `rec_used`: no effects toggle), a switch always, HOME > READER's always; an end at once (`sp_knob_end`);
  after a page is said, what follows queues (`after_page`). HOME > READER: V_VOVOL / V_VOSPD / V_VOPITCH, settings
  vo_set/vo_vol/vo_speed/vo_pitch (SET_SIZE_05: the older save still loads, vo_set not 2 -> defaults VOLUME 8, SPEED 4 of 1..9
  (130 %: the user, slower speeds too), 110 Hz). HOME mixer held: PLAYING after a track whose meter shows (g_trk_h).
  A knob turned (a new turn) stops the page's reading, said or pending (`sp_hush`: a blank NOW SAY to fm1-reader,
  the voice's ring cut). REC held: `sp_tell("TURN A KNOB TO KNOW ITS VALUE")`. READER's picture G_READ (`g_read`): the
  big "FOR ACTIVATE THE SCREEN READER, HOLD PLAY/STOP". (The user, then) a page says its title only (held: all its
  knobs with values; the patchbay's overview: its cables); a knob touched first on the page, or another than the
  last, says its name at once (`sp_knob_touch`, `sp_touch_k` reset when a page is entered). A knob of few values
  (`sp_discrete`: a jack, or pmax <= 24) says each value as it changes (`sp_knob_change`; the user: "24 or so"),
  any other its end with its value at once; FOLD said WAVE FOLD (sp_name). Titles: EDIT "OSCILLATOR", GLO "FILTER", ARP
  "ARPEGGIATOR", without their subtitles (the user).
  KEYBOARD HOLD ON / OFF. Cost: ~16 KB flash, 1.2 KB RAM,
  +122 mean / +141 max instructions a sample on the host while it speaks (the 4-track worst case is 2422: about
  +5 points on the chip, to be measured there with fm1_cpu.py).
- `mfm_patches.h`: the 90 programs (1-40 as 0.2; 41-90 levelled by `build/level_new.py`-style passes of
  `tools/level_programs.py`: render, measure, VOLUME x 10^((target - peak) / 40)). `tools/try_patches.py` tries sounds by
  NRPN on the host.
- Storage: settings in the globals sector 0xFC000 (SET_MAGIC "MFM1"; 0.3 appended `fx_off`: a 0.2 save, shorter, still
  loads; 0.4.1 took a spare byte for `clk_src`, 0 = INTERNAL), the 99 programs in 3 sectors at **0xD8000** (40 a sector, tag "MFM3").
- Authors (0.5, the user): "wootrop & gpix", lowercase everywhere (fmhub.json, LICENSING.md); the commits stay wootr0p's noreply.
- GitHub: <https://github.com/wootr0p/fmav1> (public, branch main). Work here in WSL; publish by copying the tree
  without .git / build into the Windows clone (the user's release folder's `github/`) and committing there with the
  Windows git (author wootr0p, the GitHub noreply email: never the user's own email). The hub release: a tag vX.Y and
  `fmav1-X.Y.fwsc` on its GitHub release, made after the user's tests on the device.
- Publishing on The Firmware Hub: `fmhub.json` (what, author, kinds [a list], image `docs/card.png`, color, sounds), `DISCLAIMER.md`,
  `CHANGELOG.md`; the hub wants the source in a public GitHub repo at a tag and the `.fwsc` on its release, tested on a
  real FM-1 from V15 and from another firmware.

## Measured

Host (OBV_PERF=1; ~1.2 us a half per host instruction on the chip). **On the device (0.2)**: idle 7 %, a plain voice
~12 %, four tracks of the worst patch (tools/gen_cpu_test.py) 77 % without effects, 92 % (5359 us) with all three, no
late half (chorus +3, delay +5, plate +8 points). Tried and dropped (callgrind): pitch / cutoff / slow LFO at the control
rate saved ~10 % of a plain voice and cost the worst patch +3.5 %; the utilities only when patched ~2 %. The voice's
cost is spread (the ladder ~25 %), so the per-sample engine stays. If margin is ever needed: the plate (~550 instr a
tick) on one power-of-two ring, FV-1 style (bit-exact, ~3 points). Levels: the programs peak -8.5 .. -5 dBFS with the
mixer at 100 (levelled with the one-track build).

## Tests

`sh tests/run_tests.sh` after `sh build.sh`: the programs, every page (`mfm_nav.txt`), the panel's actions
(`mfm_ui.txt`: knobs for VCA MODE and the ONs, REC, UTL, DUMMY, LFO SYNC and CLOCK under a TRS clock with a USB one
ignored, the arp, undo,
random, save, presets, octave, hold; both generated by `tools/gen_ui_tests.py`), four tracks, the CPU worst case on one
and four, the sanitizers on every script plus a parameter fuzz, the platform tests kept from OberVave. (0.5, the
user) Each step prints [k/N], its share and the time; the sanitizers are the last step and run in parallel, one run
a core (`sanitize.sh --job`, JOBS=n), the 90 programs in 8 parts each starting on the last program of the one
before (every program change still played): the whole suite ~2.5 min on 8 cores (it was ~25). Host scripts
must be in time order and at most ~60 MIDI messages a millisecond; nothing before ~1400 ms (the splash).
`tests/mfm_screens.txt` makes the manual's screens and the hub's card.
(0.6) The screen reader: `tests/mfm_sr.txt` (host commands `listen`, `where`, `said TEXT`, `said_none`, `speakdump`;
FMAV1_SAID_LOG=FILE logs every SAY) and `tests/sr_audit.sh` (fm1-reader's `tools/audit.mjs` on the dump and the
messages; needs ../fm1-reader, Node.js, here Windows' from WSL, copied to %TEMP%, and an English word list, else
the step says SKIPPED). `sh tests/sr_audit.sh --spoken` prints everything as it will be heard: read it.

## Status

- 0.1, 0.2 (2026-11-13, under the working name): installed on the user's FM-1 (port "Felucca 1", `fm1_install.py --yes`).
- 0.3 (2026-11-13), the user's asks after trying 0.2: REC = the effects off / on; the MIDI clock (LFO SYNC, a CLOCK
  output, the arp on its grid); DUMMY cables; no mult, VCA OUT not patchable (the splash: a cable into it); SEQ = the
  whole patchbay then the inputs on knobs; SELECT pressed = UTL, the switches as knobs; 90 programs; OCT LEDs; the LFO
  lights steady when fast; a lit knob's border fixed; MIDI cables from outside the panel; the name FMav-1, identity
  FM-1_3628, licence and disclaimer, the hub's files. Installed (from V15, from Felucca), on GitHub, release v0.3 made by
  the user (the hub's check then passed but for fmhub.json "kinds", fixed); the hub form in progress.
- 0.3.1 (2026-11-14): DUMMY last in JORDER; MIDI CH 1 by default; SEQ = two items (PATCHBAY, INPUTS: SEQ again
  switches); SAVE held lights F3 / G3 / A3, EDIT held lights OCT- / OCT+. Sound bit-identical to 0.3.
- 0.4 (2026-11-14): white noise (P_NOISE level into the VCF input, J_NOISE output, per-track xorshift, run only when
  heard; `noise_one` draws one for the S+H's normal); S+H IN normalled to NOISE (`patch_keep_sh_vco` for older saves);
  no knob twice but PERFORM (cables only on SEQ, UTL one page, TEMPO only GLOBAL, no S+H page); a section's button again
  = SELECT +1 (ARP again: toggle); the name at PNAME_AT 84, spare slots 0xFF (`patch_fix_values`), tag "MFM4"; 18
  programs remade with the noise (the other 72 bit-identical to 0.3.1, each rendered alone).
- 0.4.1 (2026-11-14), the user: "il tempo posso impostarlo su interno oppure sincronizzarlo su usb o midi trs".
  GLOBAL > CLOCK (`V_CLKSRC`, `set.clk_src`: CLK_INT / CLK_USB / CLK_TRS): midi_in drops 0xF8..0xFC unless they come
  from the source chosen (platform.c `midi_rx_cable`: the TRS's realtime bytes are cable 1, midi_uart.c; USB's cable 0;
  the host: "T trs ST D1 D2"); another source clears the measured tempo. GLOBAL page 1 MIDI CH, TUNE, CLOCK, TEMPO;
  page 2 KEY VEL, LIGHTS. After the user's try ("l'arpeggio deve anche seguire il clock"; TEMPO "disabilitata" showing
  the sync's tempo): `ctl_clk_ext` (USB / TRS chosen) makes arp_tick step only on the clock's grid (no clock running:
  the first note, then it waits); TEMPO locked (`knob_view_t.locked`: grey; turned: a toast), pget/param_value show the
  measured tempo ("TRS 125", "TRS ---"); a first measure after 24 clocks, then every 96 as before.
- 0.6.2 (2026-11-19): HOME > READER > OUTPUT (V_VOOUT, settings `vo_out`, the old `spare`: 0 = MAIN;
  VO_OUT_MAIN / PHONES / USB). platform.c's ISR: `app_audio_usb(o)` before uac_tap (MAIN: vo_mix into o, both
  hear it; USB: vo_mix into a copy `ub` that only uac_tap reads; the route latched per half in `vo_route`), then
  `app_audio_out(o)` after (HEADPHONES: vo_mix into o; the CPU meter). tests/host.c does the same and, with
  FMAV1_USB_WAV=1, writes OUT-usb.wav; tests/mfm_vo_out.txt + tools/voice_wav.py check each route. No SPEAKER: the
  speaker and the jack share the DAC and no amp-enable / mute line is known (fmini-1 found none either). knob_value:
  a value too wide even with -1 tracking falls back to FA8 (HEADPHONES overflowed and left a stale S).
  (The user) an input's normal ("S+H (VCO)") said only on NO CABLE: `sp_jack_name(id, title, normal)` cuts the
  " (...)" otherwise (a cable, sp_patchbay, a list of names). EFFECTS  ENGINE ON / OFF with two spaces on the screen
  (FA12's space is 2 px: "EFFECTSENGINE"), said "EFFECTS. ON" / "EFFECTS. OFF" only. (The user's rule) discrete = 25 values or
  fewer (`sp_discrete`: pmax + 1 <= 25, any jack; BEND's 0..24 in), continuous otherwise. A discrete knob says each
  value alone (`sp_value`; `sp_knob`, name and value, with the first value after `sp_knob_touch`, `sp_touch_new`),
  at an end its value again each detent, never MINIMUM / MAXIMUM; a continuous one its name on touch and "MAXIMUM,
  <value>" at an end (queued after the name when just touched). Lists (page with values, patchbay) keep `sp_knob`. fmhub.json: "Made for blind players too ...".
- 0.6.1 (2026-11-19): "50/50" said "50 50" (SP_WORDS; the audit's repeat check splits on any mark but lets digits
  repeat, so "FIFTY-FIFTY" failed too); `tests/sr_allow.txt` extended. Audit (wamerican installed): 0 errors, 122
  warnings: readout-short (the cables "X TO Y NORMALIZED TO Z" as the user asked, MOD MIX "L F O 68 PERCENT"; continuous
  knobs are not said while turning anyway) and warning-says-what (NOTHING TO UNDO, RESET / SAVE CANCELLED).
- 0.6 (2026-11-19): screen reader support (issue #1, fm1-reader; see `mfm_speak.h` above). Messages that did not
  happen are `warn()` (ANN_KEEP); TEMPO under a clock says "TEMPO NOT CHANGED|THE USB CLOCK SETS IT". ~4 KB flash,
  ~1 KB RAM. Before release: the words file published (GitHub Pages, `curl -sI` the link: 200), the audit clean,
  tried on the device (`../fm1-reader/tools/fm1_reader.py --where --seconds 20`), FMav-1 added to fm1-reader's list.
- 0.5.2 (2026-11-18): ATTACK 153 ms at 10 o'clock (KNOB; was noon, the user by ear on the reference); ATT_FROM_051
  for the 0.5 / 0.5.1 saves (MFM5 / MFM6 -> MFM7; MFM6 keeps its GATE cables) and the factory programs' attacks;
  DECAY 1.2 s at noon and RELEASE 458 ms at 9 (the user), DEC_FROM_051 / REL_FROM_051 the same way (every program,
  CLASSIC BASS too: the user, "dimentica la foto").
  fmhub.json: a generic description, no calibration claim (the user: the calibration on the real unit is still
  going on).
- 0.5.1 (2026-11-16, the user): KEYBOARD on SELECT pressed; PRESS called AFTERTC; a GATE patch source (the note
  held, keys or MIDI) in the CLOCK output's old slot; the external sources' cables khaki and bowed apart; the test
  suite with progress and parallel sanitizers last (~2.5 min).
- 0.5 (2026-11-16): the CLOCK output and its page removed (the user: "nel [riferimento] non c'è e non te l'ho
  chiesto"); programs 62, 63, 64, 69 on the LFO synced (build/cmp042.py: same levels, envelopes alike); the LFO page's
  subtitle says the clock's state while SYNC is on. Lesson: add nothing the reference panel lacks without asking.
  The knobs drawn over 300 degrees, 7 to 5 o'clock (the user measured the reference's travel; its panel has ticks
  only): 11 ticks on the hours (big knobs every 10 degrees). The user: CUTOFF at 7 o'clock silent on the reference, ours
  sounded and clicked: the VCO's pulse offset subtracted at the VCF input (exact, by width and wave; no lag) plus a
  ~7 Hz AC coupling (acx / acy), a woken track's VCO sample refreshed (`woke`); left: a ~25 Hz swell at -46 dBFS (the
  ladder frozen while idle; running it idle would cost the CPU). ATTACK first tried from 24 ms, then (the user) 0.8 ms .. 2.2, then 2.5 s
  (ATT_HI in gen_tables.py); saves "MFM5" (MFM4 and older: ATT_FROM_04, the same time). The factory programs are
  left as they were (the user: re-tune them all at the end of the calibration; build/lvl042b.py re-levels). Next asked: the same knob position = the same sound on
  the reference (its mid-travel curves: to be measured on the user's unit; the manual gives only the ends).
- The knobs' calibration (0.5, the user: "facile correggere e tarare questi valori minimi e massimi dei knob"): `KNOB`
  at the top of tools/gen_tables.py, each knob's value at 7 o'clock, noon (None: the law's middle; else two halves)
  and 5 o'clock, "exp" or "lin"; it makes CUT_P8 (int32: MIDI x 256 passes int16), PITCH_P8, ATT / DEC / REL tables,
  GLIDE, LFO, the ladder's dial, DUTY_* / PW_* and KBS_* defines. Run it, then build; the factory programs keep their
  values (re-tune them once the calibration is done: the user's plan).
- The memory's reset (0.5, the user): SAVE held + G5 (key 26, the last; it was GLO + G5, GLO is the VCF's button now)
  arms `reset_armed`; confirmed, `memory_reset` writes the cleared programs and the default settings at once and
  `fm1_reboot()`s (the user: as on the first install; the host resets in place); TPROG_DEF 01, 11, 21, 31; only OCT+ confirms; any other button (swallowed) or 8 s cancels.
- The ladder (0.5, the user: "un algoritmo più fedele"): LADDER_DIFF 1 (mfm_dsp.h ladder_tick): each TPT stage
  integrates tanh(its input) - tanh(its state) (the transistor ladder's equation, Huovilainen / D'Angelo-Valimaki),
  the loop still predicted linearly; identical while small, ~8 % more CPU in the worst case (host perf 1978 against
  1830 instructions a sample). Fitted to the user's recordings of the reference's INIT (build/cal/fitB.py, tuneB.py):
  CUTOFF points 50.4 / 612.9 / 10249.5 Hz (the small-signal whistle; the corner 289 Hz at noon), LAD_RATIO 2.12,
  then checked on the FM-1 itself (registrazioni/cattura_fm1.py records its USB audio; build/cal/compare_dev.py):
  RESONANCE {12: 4.0, 3.5: 8.0} top 9.0, LADDER_DRIVE_DB -0.5, RES_LEVEL_DB 0 ("B2": the closest of A, B, B2).
  The reference's MP3s are quiet (-60 dBFS): above ~2.5 kHz the MP3 drops the harmonics, so the 3 o'clock point is
  not measurable from them. Packages fmav1-0.5-ladderA / -ladderB / -ladderB2.
- 0.5, the factory programs (the user): rebuilt from INIT with only the knobs each sound needs and only what the
  reference has (effects aside), so they can be set up on it. build/presets (not published): prep.sh (0.4.1's and
  0.5's programs dumped whole, dump.c), convert.py (0.4.1 -> 0.5 curves by physical value; strips the extensions;
  LFO SYNC -> the free rate at 120 bpm), mk_host.sh (a one-track simulator reading program 1 from $CAND), dist.c
  (band-by-band difference over time), minimize.py (each knob back at INIT's if < 0.8 dB mean / 4 dB worst / 0.6 dB
  level), design.py (the noise programs: VCO <-> LFO cross-modulation grid against 0.4.1's renders), level041.py
  (VOLUME to 0.4.1's level), write_patches.py (the PF_ lists, INIT plus changes, the panel's order). S+H IN's
  normal is the VCO again (MFM6; MFM4 / MFM5 saves get NOISE > S+H IN); the ladder's hiss (`lseed`) lets it sing
  from silence.
- **Always: the user flashes and tries a build on the FM-1 before any push, release or hub update** (the user, 0.3.1).

## Style

As OberVave / Dinghy: C99, 4 spaces, short comments that say why, unity build. Commit messages: one line what
changed.
