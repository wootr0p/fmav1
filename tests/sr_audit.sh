#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# (0.6) The screen reader's audit: fm1-reader's tools/audit.mjs checks what FMav-1 says against the rules in its skill
# (RULES.md). In: every page, knob value and program as said (the simulator's "speakdump") and every message the
# scenario tests made (FMAV1_SAID_LOG), put into words with this build's words file exactly as the reader page does.
# Needs fm1-reader next to this repository (../fm1-reader), Node.js (in this shell, or Windows' from WSL) and an
# English word list (SR_DICT, else /usr/share/dict/words).
#   sh tests/sr_audit.sh            the audit (exit 1 on errors)
#   sh tests/sr_audit.sh --spoken   everything as it will be heard, to read
#   sh tests/sr_audit.sh --all      every finding, not the first few of each
set -e
cd "$(dirname "$0")/.."
R=../fm1-reader
O=build/sr
mkdir -p $O
printf '2100 speakdump\n2200 end\n' > $O/dump.txt
sh tests/run_host.sh $O/dump.txt srdump > $O/dump.out
awk -F '\t' 'NF == 2 && $1 ~ /^[A-Z]+$/' $O/dump.out | sort -u > $O/dump.tsv
rm -f $O/said.tsv
for f in tests/mfm_sr.txt tests/mfm_ui.txt tests/mfm_tracks.txt tests/mfm_nav.txt tests/mfm_voice.txt tests/mfm_vo_out.txt; do
    FMAV1_SAID_LOG=$PWD/$O/said.tsv build/host/fmav1 build/host/srlog $f > /dev/null || :   # (its said checks: the reader was listening already)
done
sort -u $O/said.tsv | cat $O/dump.tsv - > $O/all.tsv
[ -f $R/tools/audit.mjs ] || { echo "sr_audit: no ../fm1-reader (git clone https://github.com/charlesvestal/fm1-reader ../fm1-reader)"; exit 2; }
W=docs/words/$(sed -n 's/.*words\/\([0-9a-f]*\)\.json.*/\1/p' firmware/gen/ann_words.h).json
D="${SR_DICT:-/usr/share/dict/words}"
if [ "$1" != --spoken ] && [ ! -s "$D" ]; then
    echo "sr_audit: no English word list ($D; SR_DICT=...): the audit can't tell words from abbreviations"; exit 2
fi
if command -v node > /dev/null; then
    node $R/tools/audit.mjs $O/all.tsv --words "$W" --dict "$D" --allow tests/sr_allow.txt "$@"
elif [ -x "/mnt/c/Program Files/nodejs/node.exe" ]; then   # (WSL, Node.js on Windows: it reads Windows paths only)
    T="$(wslpath "$(cmd.exe /c 'echo %TEMP%' 2> /dev/null | tr -d '\r')")/fmav1-sr"
    rm -rf "$T"; mkdir -p "$T/reader"
    cp -r $R/tools $R/web "$T/reader/"
    cp $O/all.tsv "$W" tests/sr_allow.txt "$T/"
    [ -s "$D" ] && cp "$D" "$T/dict.txt"
    cd "$T"
    if [ -s dict.txt ]; then "/mnt/c/Program Files/nodejs/node.exe" reader/tools/audit.mjs all.tsv --words "$(basename "$W")" --dict dict.txt --allow sr_allow.txt "$@"
    else "/mnt/c/Program Files/nodejs/node.exe" reader/tools/audit.mjs all.tsv --words "$(basename "$W")" "$@"; fi
else
    echo "sr_audit: no Node.js"; exit 2
fi
