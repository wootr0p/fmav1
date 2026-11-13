#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# FMav-1's host tests (no hardware). Run from the repo root after ./build.sh:
#   sh build.sh && sh tests/run_tests.sh
# The app on the host (every factory program, every page, the panel's switches, four tracks, the CPU worst case), the
# sanitizers with a fuzz, the platform pieces kept from OberVave (MASTER glide, CPU meter), the update path (ota.c, the
# loader, tools/fm1_install.py).
set -e
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"   # (fm1pkg_make.py: the SDK files of a package)
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
CC="${CC:-cc} -O1 -Wall -Wno-unused-function"
fail=0
run() { echo "== $1"; shift; "$@" || fail=1; }

[ -f build/fmav1.fwsc ] || { echo "run ./build.sh first"; exit 1; }

run "the app on the host: every factory program plays (build/host/programs.wav)" sh tests/run_host.sh tests/mfm_programs.txt programs
run "every section, every page, a note held (build/host/nav-*.png)" sh tests/run_host.sh tests/mfm_nav.txt nav
run "the panel: SELECT pressed on each page, cables in and out, undo, random, save, presets, hold, arp (build/host/ui-*.png)" sh tests/run_host.sh tests/mfm_ui.txt ui
run "four tracks: channels N .. N+3, ALGORITHM, the mixer (build/host/tracks-*.png)" sh tests/run_host.sh tests/mfm_tracks.txt tracks
run "the worst case on four tracks: everything at audio rate on each, chorus, delay and reverb" sh tests/run_host.sh tests/mfm_cpu.txt cpu4
run "the screens for the manual: a lead's pages, the patchbay (build/host/screens-*.png)" sh tests/run_host.sh tests/mfm_screens.txt screens
run "the one-track build (MFM_NTRK=1): every factory program plays" env MFM_NTRK=1 sh tests/run_host.sh tests/mfm_programs.txt programs1
run "sanitizers (ASan + UBSan) on every script and a parameter fuzz, one track and four" sh tests/sanitize.sh

$CC -o "$OUT/enc_test" tests/enc_test.c
run "the knobs: fast turns (up to 300 clicks/s) every click counted, a still knob never steps" "$OUT/enc_test"
$CC -o "$OUT/master_gain_test" tests/master_gain_test.c
run "MASTER gain: steady as before, a glide when the level moves (no click a knob step)" "$OUT/master_gain_test"
$CC -o "$OUT/cpu_meter_test" tests/cpu_meter_test.c
run "CPU meter: a slow half held 2 s on the GLOBAL page, the late halves counted" "$OUT/cpu_meter_test"

$CC -o "$OUT/ota_test" tests/ota_test.c
run "M-UPGRADE entry (FM-1)" "$OUT/ota_test" build/fmav1.fwsc
head -c 60000 build/fmav1.bin > "$OUT/old_app.bin"
python3 tools/fm1pkg_make.py "$OUT/old_app.bin" build/loader/ota.bin "$OUT/old.fwsc" >/dev/null
$CC -o "$OUT/ldr_screen_test" tests/ldr_screen_test.c
run "update loader's screen: UPDATE MODE, WRITING and its bar, UPDATE FAILED" "$OUT/ldr_screen_test" "$OUT"
$CC -o "$OUT/ldr_test" tests/ldr_test.c
run "update loader (FM-1): other app -> this build" "$OUT/ldr_test" "$OUT/old.fwsc" build/fmav1.fwsc
run "installer (tools/fm1_install.py): install and the update-mode resume check the model" python3 tests/install_test.py

[ "$fail" = 0 ] && echo "ALL TESTS PASSED" || echo "SOME TESTS FAILED"
exit "$fail"
