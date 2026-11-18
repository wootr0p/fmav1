#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(0.6) The FM-1's own voice and fm1-reader say the same (tests/voice_words.sh).

    voice_check.py tools/voice_words.txt build/sr/all.tsv [build/sr/spoken.txt]

all.tsv: every text FMav-1 says (KIND<TAB>text: every page, value, program and message, tests/sr_audit.sh). Every
word in it must be in the voice's dictionary (a word not there would be spelt by the FM-1, said by the page). With
spoken.txt (the same lines as the reader page says them, fm1-reader's audit.mjs --spoken), the page's words must be
FMav-1's own, word for word: its words file must not rewrite them. Exit 1 on either."""
import re
import sys


def words(t):
    return re.findall(r"[a-z]+", t.lower())


def said(t):
    """FMav-1's text as the voice says it: % and + in words (mfm_voice.h vo_text)"""
    return words(t.replace("%", " percent ").replace("+", " plus "))


def main(dic, alltsv, spoken=None):
    known = set()
    for ln in open(dic, encoding="ascii"):
        if ln.strip() and not ln.startswith("#"):
            known.add(ln.split("\t")[0].lower())
    lines = [ln.rstrip("\n").split("\t", 1) for ln in open(alltsv, encoding="ascii") if "\t" in ln]
    bad = 0
    missing = {}
    for _, text in lines:
        for w in words(text):
            if len(w) > 1 and w not in known:
                missing.setdefault(w, text)
    for w, ex in sorted(missing.items()):
        print(f"not in tools/voice_words.txt: {w!r} (\"{ex}\")")
        bad = 1
    if spoken:
        page = [ln.rstrip("\n").split("\t", 1) for ln in open(spoken, encoding="utf-8") if "\t" in ln]
        if len(page) != len(lines):
            print(f"spoken: {len(page)} lines for {len(lines)}")
            return 1
        diff = {}
        for (_, a), (_, b) in zip(lines, page):
            if said(a) != words(b):
                diff.setdefault(" ".join(said(a)), " ".join(words(b)))
        for a, b in list(diff.items())[:20]:
            print(f"the page says it otherwise:\n   FMav-1: {a}\n   page:   {b}")
        if diff:
            print(f"{len(diff)} texts the page says otherwise")
            bad = 1
    print(f"voice: {len(lines)} texts, {'every word in the dictionary' if not missing else f'{len(missing)} words missing'}"
          + ("" if not spoken else ", the page's words the same" if bad == 0 else ""))
    return bad


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:4]))
