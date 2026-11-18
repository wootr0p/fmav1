#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# (0.6) The FM-1's own voice and fm1-reader say the same: every word FMav-1 says (every page, value, program and
# message: tests/sr_audit.sh gathers them) is in the voice's dictionary (tools/voice_words.txt), and, with Node.js and
# ../fm1-reader, the reader page says them word for word (tools/voice_check.py).
set -e
cd "$(dirname "$0")/.."
if sh tests/sr_audit.sh --spoken > build/sr/spoken.txt 2>/dev/null; then sp=build/sr/spoken.txt; else sp=; echo "voice_words: no Node.js or ../fm1-reader: the page not compared"; fi
python3 tools/voice_check.py tools/voice_words.txt build/sr/all.tsv $sp
