#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The app on the host under AddressSanitizer + UndefinedBehaviorSanitizer (every check, a signed overflow too): every
# script in tests/ and a fuzz of every parameter (tools/fuzz_script.py, random and extreme values by NRPN while notes
# play), several seeds; the fuzz on four channels; the one-track build on the panel and a fuzz. Any report fails.
# Linux (gcc or clang); run from anywhere. (0.5) The runs go in parallel, one a core (JOBS=n to change it), the
# longest first, each printing "[done/total]" as it ends.
set -e
cd "$(dirname "$0")/.."
OUT=build/host/sanitize
if [ "$1" = --job ]; then                      # (one run: "binary|script|name", the total)
    bin=${2%%|*}; rest=${2#*|}; scr=${rest%%|*}; n=${rest#*|}
    [ "$scr" = - ] && scr=
    t0=$(date +%s)
    if ASAN_OPTIONS=detect_leaks=0 "$bin" "$OUT/san-$n" $scr >"$OUT/san-$n.log" 2>&1 \
       && ! grep -qE "runtime error|AddressSanitizer" "$OUT/san-$n.log"; then
        r=ok; touch "$OUT/san-$n.done"
    else
        r=FAIL; touch "$OUT/san-$n.done" "$OUT/san-$n.fail"
    fi
    k=$(ls "$OUT"/*.done | wc -l)
    echo "   [$k/$3] $((100 * k / $3))% $r $n ($(( $(date +%s) - t0 )) s)"
    [ $r = ok ] || grep -m3 -E "runtime error|AddressSanitizer" "$OUT/san-$n.log" | sed 's/^/      /'
    exit 0
fi
mkdir -p "$OUT"
rm -f "$OUT"/*.done "$OUT"/*.fail
SAN="-g -O1 -w -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer"
${CC:-cc} $SAN -DOBV_HOST -DAPP_VER='"dev"' -Ifirmware/src -Ifirmware/gen -o "$OUT/sfm" firmware/src/fmav1.c &
${CC:-cc} $SAN -DOBV_HOST -DMFM_NTRK=1u -DAPP_VER='"dev"' -Ifirmware/src -Ifirmware/gen -o "$OUT/sfm4" firmware/src/fmav1.c &
wait
cc -Ifirmware/src -Ifirmware/gen -o build/nrpn tools/nrpn.c && build/nrpn > build/params.txt
for s in 1 2 3 4; do python3 tools/fuzz_script.py $s > "$OUT/fuzz$s.txt"; done
for s in 5 6; do python3 tools/fuzz_script.py $s 4 > "$OUT/fuzz4ch$s.txt"; done
# (0.5) the 90 programs in 8 parts run side by side (one long run took over 10 min): each part starts on the last
# program of the one before, so every change from a program to the next is still played under the sanitizers
python3 - "$OUT" 8 <<'EOF'
import sys
from pathlib import Path
out, parts = Path(sys.argv[1]), int(sys.argv[2])
lines = [l for l in Path("tests/mfm_programs.txt").read_text().splitlines()
         if l.strip() and not l.startswith("#") and not l.endswith(" end")]
progs, cur = [], []
for l in lines:                                 # (a program: its program change and the phrase after it)
    if " 0xC0 " in l and cur:
        progs.append(cur)
        cur = []
    cur.append(l)
progs.append(cur)
per = -(-len(progs) // parts)
for f in out.glob("progs*.txt"):
    f.unlink()
for k in range(parts):
    g = progs[max(0, k * per - 1):(k + 1) * per]   # (the part before's last program first)
    if k * per >= len(progs):
        break
    rows = [l for p in g for l in p]
    t0 = int(rows[0].split()[0]) - 1500
    body = [f"{int(l.split(' ', 1)[0]) - t0} {l.split(' ', 1)[1]}" for l in rows]
    end = int(body[-1].split()[0]) + 3000
    (out / f"progs{k + 1}.txt").write_text("\n".join(body + [f"{end} end"]) + "\n")
EOF
# the runs: the default build (four tracks) on the 90 programs' parts (first: the longest), every other script and
# the fuzz on one channel; the fuzz on four channels; the one-track build (MFM_NTRK=1, sfm4 by its old name) on the
# panel and a fuzz
L="$OUT/jobs.txt"
: > "$L"
if [ -z "$QUICK" ]; then                       # (QUICK=1: not the 90 programs)
    for t in "$OUT"/progs*.txt; do echo "$OUT/sfm|$t|$(basename "$t" .txt)" >> "$L"; done
fi
echo "$OUT/sfm|-|default" >> "$L"
for t in tests/*.txt "$OUT"/fuzz[0-9].txt; do
    n=$(basename "$t" .txt)
    [ "$n" = mfm_programs ] && continue
    echo "$OUT/sfm|$t|$n" >> "$L"
done
for t in "$OUT"/fuzz4ch*.txt; do echo "$OUT/sfm|$t|4ch-$(basename "$t" .txt)" >> "$L"; done
for t in tests/mfm_ui.txt "$OUT"/fuzz1.txt; do echo "$OUT/sfm4|$t|1trk-$(basename "$t" .txt)" >> "$L"; done
N=$(wc -l < "$L")
J=${JOBS:-$(nproc 2>/dev/null || echo 4)}
echo "   $N runs under the sanitizers, $J at a time"
xargs -P "$J" -I{} sh "$0" --job "{}" "$N" < "$L"
if ls "$OUT"/*.fail >/dev/null 2>&1; then
    echo "sanitizers: FAILED:"; for f in "$OUT"/*.fail; do echo "   $(basename "$f" .fail)"; done
    exit 1
fi
echo "sanitizers: every script and the fuzz clean (one track and four)"
