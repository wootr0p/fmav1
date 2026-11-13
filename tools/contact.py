#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A contact sheet of host screenshots: tools/contact.py OUT.png COLS SHOT.png ... (each at 240 x 240, captioned)"""
import sys
from PIL import Image, ImageDraw
out, cols, files = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
rows = (len(files) + cols - 1) // cols
sheet = Image.new("RGB", (cols * 244, rows * 256), (60, 60, 60))
d = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    im = Image.open(f).resize((240, 240), Image.NEAREST)
    x, y = (i % cols) * 244 + 2, (i // cols) * 256 + 14
    sheet.paste(im, (x, y))
    d.text((x, y - 12), f.split("/")[-1], fill=(255, 255, 255))
sheet.save(out)
