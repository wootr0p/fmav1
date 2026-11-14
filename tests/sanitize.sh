#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The app on the host under AddressSanitizer + UndefinedBehaviorSanitizer (every check, a signed overflow too): every
# script in tests/ and a fuzz of every parameter (tools/fuzz_script.py, random and extreme values by NRPN while notes
# play), several seeds. Any report fails. Linux (gcc or clang); run from anywhere.
set -e
cd "$(dirname "$0")/.."
OUT=build/host/sanitize
mkdir -p "$OUT"
fail=0
${CC:-cc} -g -O1 -w -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer \
    -DOBV_HOST -DAPP_VER='"dev"' -Ifirmware/src -Ifirmware/gen -o "$OUT/sfm" firmware/src/fmav1.c
cc -Ifirmware/src -Ifirmware/gen -o build/nrpn tools/nrpn.c && build/nrpn > build/params.txt
for s in 1 2 3 4; do python3 tools/fuzz_script.py $s > "$OUT/fuzz$s.txt"; done
for t in "" tests/*.txt "$OUT"/fuzz*.txt; do
    n=$(basename "${t:-default}" .txt)
    [ -n "$QUICK" ] && [ "$n" = mfm_programs ] && continue   # (QUICK=1: not the 90 programs, ~10 min under the sanitizers)
    if ! ASAN_OPTIONS=detect_leaks=0 "$OUT/sfm" "$OUT/san-$n" $t >"$OUT/san-$n.log" 2>&1 \
       || grep -qE "runtime error|AddressSanitizer" "$OUT/san-$n.log"; then
        echo "FAIL $n:"; grep -m3 -E "runtime error|AddressSanitizer" "$OUT/san-$n.log"; fail=1
    fi
done
# 0.2: the default build has four tracks (above: every script, the fuzz on one channel); here the fuzz on four channels,
# and the one-track build (MFM_NTRK=1, $OUT/sfm4 by its old name) on the programs, the panel and a fuzz
${CC:-cc} -g -O1 -w -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer \
    -DOBV_HOST -DMFM_NTRK=1u -DAPP_VER='"dev"' -Ifirmware/src -Ifirmware/gen -o "$OUT/sfm4" firmware/src/fmav1.c
for s in 5 6; do python3 tools/fuzz_script.py $s 4 > "$OUT/fuzz4ch$s.txt"; done
for t in "$OUT"/fuzz4ch*.txt; do
    n=4ch-$(basename "$t" .txt)
    if ! ASAN_OPTIONS=detect_leaks=0 "$OUT/sfm" "$OUT/san-$n" $t >"$OUT/san-$n.log" 2>&1 \
       || grep -qE "runtime error|AddressSanitizer" "$OUT/san-$n.log"; then
        echo "FAIL $n:"; grep -m3 -E "runtime error|AddressSanitizer" "$OUT/san-$n.log"; fail=1
    fi
done
for t in tests/mfm_ui.txt "$OUT"/fuzz1.txt; do   # (0.3: the 90 programs under the sanitizers once, above: ~10 min each)
    n=1trk-$(basename "$t" .txt)
    if ! ASAN_OPTIONS=detect_leaks=0 "$OUT/sfm4" "$OUT/san-$n" $t >"$OUT/san-$n.log" 2>&1 \
       || grep -qE "runtime error|AddressSanitizer" "$OUT/san-$n.log"; then
        echo "FAIL $n:"; grep -m3 -E "runtime error|AddressSanitizer" "$OUT/san-$n.log"; fail=1
    fi
done
[ $fail = 0 ] && echo "sanitizers: every script and the fuzz clean (one track and four)"
exit $fail
