# Licensing

FMav-1 (built on OberVave) is free software under the GNU General Public License, version 3
only (`GPL-3.0-only`, full text in `LICENSE`). You may use, study, change and share it. If you give someone a firmware
built from it, give them its complete source under the same licence too. It comes with **no warranty**: see
`DISCLAIMER.md`.

## Who made what

| | |
| --- | --- |
| The platform: `firmware/hal/`, `firmware/loader/`, `crt0.S`, `app.ld`, `firmware/src/` (`usb.c`, `midi_uart.c` (the TRS MIDI IN; its fix from Felucca Salt by ChanceTheMaker, found by keremimo), `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `uac_fir.h`), `tools/` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| The platform's wiring `platform.c`, the host mode `tests/`, the docs, the build (`build.sh`, `tools/build.py`) | zvenson, as [Dinghy](https://github.com/zvenson/dinghy) |
| The update loader's screen: `firmware/loader/ldr_screen.c`, `tests/ldr_screen_test.c` | Copyright (C) 2026 zvenson (the Firmware Hub's look); the idea, a loader that draws, its 5 x 7 letters and the order of things from [Skiff](https://github.com/illuminus5/skiff) by illuminus5, GPL-3.0 |
| The synth: `firmware/src/fmav1.c`, `mfm_*.h`, `firmware/gen/mfm_tables.h` and `tools/gen_tables.py`, `tools/make_fonts.py`, the programs, the screen (its panel, knobs, jacks, graphs: our own drawing), `tools/ana_wav.py`, `tools/ana_spec.py`, `tools/mfm_params.py`, `tools/nrpn.c`, `tools/cc_table.py`, `tools/fuzz_script.py`, `tools/fm1_cpu.py`, `tools/gen_cpu_test.py`, `tools/gen_ui_tests.py`, `tools/level_programs.py`, `tools/try_patches.py`, `tools/zc_track.py`, `tools/contact.py`, `tests/mfm_*.txt`, `tests/enc_test.c`, `tests/run_tests.sh`, `tests/sanitize.sh` | Copyright (C) 2026 wootr0p (the FMav-1 author), on OberVave 3.3.2 (its DSP blocks, knob drawing, engine structure: Copyright (C) 2026 zvenson) |
| The ladder VCF's model and laws (`ladder_coef` / `ladder_tick` in `mfm_dsp.h`, the LAD_* tables of `tools/gen_tables.py`): four differential pairs, the emphasis network's lag pole, the loop predicted from the states, the emphasis and passband laws | Copyright (C) 2026 Ruppert, [FMini-1](https://github.com/ruppert/fmini-1) (`eng_fmini.c`, `tools/fmini_filter.py`), GPL-3.0; its calibration from the CA-72 project's measurements (GPL-3.0-or-later) |
| The encoders' decoder taking every frame's state and a 2-state jump from the detent the way of the last click (`fm1__frame` in `firmware/hal/fm1_input.h`: fast turns lost clicks), its turned-knob model in `tests/enc_test.c` | after [FMini-1](https://github.com/ruppert/fmini-1)'s #126 and `tests/input_test.c` (Ruppert), GPL-3.0, on Felucca's decoder |
| The effects (`mfm_fx.h`: the chorus, the delay, the plate after J. Dattorro, 1997) | from OberVave (zvenson), in fixed point |
| The SMK-37 board layer: `firmware/hal/smk_input.h`, `fm1_flash_map.h`, the SMK-37 parts of `fm1_cc.h`, `fm1_flash.h`, `fm1_lcd_hw.h`, `lcd.c`, `ota.c`, `bootguard.h`, `firmware/loader/`, `tools/fm1pkg_make.py`, `tools/fm1_install.py`, `tools/build.py`; `firmware/src/smk_wheel.h`; `tests/ota_test.c`, `ldr_test.c`, `install_test.py`, `smk_wheel_test.c`, `smk_pkg_test.py` | Copyright (C) 2026 M. Geisler (genkigenki/makkuro), from [genkigenki/sloop-fm1](https://github.com/genkigenki/sloop-fm1) branch `smk37` (the reference port of the [fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37) kit), on the platform above |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Terminus font 8x16 (ter-u16n), rendered into `firmware/gen/felucca_font.h` | SIL OFL 1.1 | `assets/fonts/Terminus-LICENSE.txt` |
| Inter Tight by The Inter Project Authors (the screen's text), rasterised into `firmware/gen/mfm_fonts.h` by `tools/make_fonts.py` (the generated tables are not offered as a font; the font declares no Reserved Font Name) | SIL OFL 1.1 | `assets/fonts/InterTight-wght.ttf`, `assets/fonts/InterTight-OFL.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Names

FMav-1 is an independent work. It is not made, endorsed or supported by M-VAVE or by any synthesizer maker, and it
contains none of their code, sounds or graphics; its factory programs are its own. "Felucca" and "Hügelton Instruments"
are names of Hügelton Instruments; "M-VAVE", "FM-1", "SMK-37", "MKE-P37" and "Starrykey" belong to their owners;
"Digitakt" belongs to Elektron. Any other name mentioned belongs to its owner and is used only to describe where code
came from.
