#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# FMav-1's host tests (no hardware). Run from the repo root after ./build.sh:
#   sh build.sh && sh tests/run_tests.sh      (QUICK=1: the sanitizers skip the 90 programs)
# (0.5) Each step prints "[k/N] its share done, the time so far" and how long it took; the sanitizers run in parallel
# (tests/sanitize.sh, one run a core) and print their own "[done/total]".
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
N=$(grep -c '^run ' "$0")                       # (the steps: every line that starts with run)
k=0
T0=$(date +%s)
mmss() { echo "$(($1 / 60))m$(printf %02d $(($1 % 60)))s"; }
run() {
    k=$((k + 1)); t=$(date +%s)
    echo "== [$k/$N] $((100 * (k - 1) / N))%, $(mmss $((t - T0))): $1"; shift
    if "$@"; then echo "   ok ($(mmss $(($(date +%s) - t))))"; else echo "   FAILED ($(mmss $(($(date +%s) - t))))"; fail=1; fi
}

[ -f build/fmav1.fwsc ] || { echo "run ./build.sh first"; exit 1; }

run "the app on the host: every factory program plays (build/host/programs.wav)" sh tests/run_host.sh tests/mfm_programs.txt programs
run "every section, every page, a note held (build/host/nav-*.png)" sh tests/run_host.sh tests/mfm_nav.txt nav
run "the panel: SELECT pressed on each page, cables in and out, undo, random, save, presets, hold, arp (build/host/ui-*.png)" sh tests/run_host.sh tests/mfm_ui.txt ui
det() {   # (0.5) two histories, then the same keys: the same screens, byte for byte (tools/gen_ui_tests.py)
    sh tests/run_host.sh tests/mfm_det_a.txt det_a >/dev/null && sh tests/run_host.sh tests/mfm_det_b.txt det_b >/dev/null || return 1
    for s in 7000 7300 8000 8700 9000 9300; do cmp -s "$OUT/det_a-$s.png" "$OUT/det_b-$s.png" || { echo "page differs at $s ms"; return 1; }; done
}
run "the navigation is deterministic: the same keys, the same page, whatever came before" det
ccv() {   # (0.5.2) REC held: the CCs; let go: as before, the effects untouched; tapped twice: off, then as before
    sh tests/run_host.sh tests/mfm_cc_a.txt cc_a >/dev/null && sh tests/run_host.sh tests/mfm_cc_b.txt cc_b >/dev/null || return 1
    for s in 4200 5300 7800; do cmp -s "$OUT/cc_a-$s.png" "$OUT/cc_b-$s.png" || { echo "REC changed the screen at $s ms"; return 1; }; done
    for s in 4900 5700; do cmp -s "$OUT/cc_a-$s.png" "$OUT/cc_b-$s.png" && { echo "REC showed nothing at $s ms"; return 1; }; done
    return 0
}
run "REC held: the MIDI CCs on the screen, the effects untouched; a tap: the effects off / on (build/host/cc_b-*.png)" ccv
run "the screen reader (0.6): what a reader hears, page by page, knob by knob, every message" sh tests/run_host.sh tests/mfm_sr.txt sr
sra() {   # (0.6) fm1-reader's audit of every page, value and message (needs ../fm1-reader, Node.js, a word list)
    sh tests/sr_audit.sh > "$OUT/sr_audit.txt" 2>&1; r=$?
    [ $r = 2 ] && { echo "   SKIPPED: $(tail -1 "$OUT/sr_audit.txt")"; return 0; }
    [ $r = 0 ] || tail -30 "$OUT/sr_audit.txt"
    return $r
}
run "the screen reader's audit: real words, no repeats, short enough (build/host/sr_audit.txt)" sra
run "the FM-1's own voice and fm1-reader say the same words, every one in its dictionary" sh tests/voice_words.sh
voice() {   # (0.6) PLAY held 2 s: the voice on, its texts; the outputs speak, then quiet once it is off
    sh tests/run_host.sh tests/mfm_voice.txt voice > "$OUT/voice.txt" 2>&1 || { grep FAIL "$OUT/voice.txt"; return 1; }
    python3 tools/voice_wav.py "$OUT/voice.wav" 2.3-4.1:quiet 4.3-5.5:speech 10.3-11.5:speech 30.2-31.4:quiet
}
run "the FM-1's own voice: PLAY held 2 s on and off, the outputs speak (build/host/voice.wav)" voice
run "four tracks: channels N .. N+3, ALGORITHM, the mixer (build/host/tracks-*.png)" sh tests/run_host.sh tests/mfm_tracks.txt tracks
run "the worst case on four tracks: everything at audio rate on each, chorus, delay and reverb" sh tests/run_host.sh tests/mfm_cpu.txt cpu4
run "the screens for the manual: a lead's pages, the patchbay (build/host/screens-*.png)" sh tests/run_host.sh tests/mfm_screens.txt screens
run "the one-track build (MFM_NTRK=1): every factory program plays" env MFM_NTRK=1 sh tests/run_host.sh tests/mfm_programs.txt programs1

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

# (0.5, the user) the sanitizers last: they need none of the steps above, and they are the long one
run "sanitizers (ASan + UBSan) on every script and a parameter fuzz, one track and four" sh tests/sanitize.sh

[ "$fail" = 0 ] && echo "ALL TESTS PASSED in $(mmss $(($(date +%s) - T0)))" || echo "SOME TESTS FAILED ($(mmss $(($(date +%s) - T0))))"
exit "$fail"
