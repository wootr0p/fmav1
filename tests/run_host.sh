#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run firmware/src/fmav1.c on this computer (tests/host.c): build/host/fmav1.wav and the screens as PNG.
#   tests/run_host.sh [script] [out-name]      (MFM_NTRK=4: the four-track build)
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host
N=${2:-fmav1}
cc -O2 -Wall -Wno-unused-function -Wno-missing-field-initializers -DOBV_HOST ${MFM_NTRK:+-DMFM_NTRK=${MFM_NTRK}u} -DAPP_VER="\"${APP_VER:-dev}\"" -Ifirmware/src -Ifirmware/gen -o build/host/fmav1 firmware/src/fmav1.c
build/host/fmav1 build/host/$N ${1:+"$1"}
python3 - "$N" <<'PY2'
import sys
from pathlib import Path
from PIL import Image
for p in sorted(Path("build/host").glob(sys.argv[1] + "-*.ppm")):
    Image.open(p).resize((480, 480), Image.NEAREST).save(p.with_suffix(".png"))
    p.unlink()
    print("screen ->", p.with_suffix(".png"))
PY2
