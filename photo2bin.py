#!/usr/bin/env python3
"""
photo2bin.py - turn ANY picture (real photo, Canva / AI image ...) into a scene file
for the weather station: 280x240, RGB565, big-endian, no header.

usage:  python photo2bin.py photo.jpg CLRDAY.BIN
        python photo2bin.py photo.jpg RAIN.BIN --no-scrim

Scene file names the sketch looks for (SD card root):
  CLRDAY  CLRNITE   clear day / night
  PTLDAY  PTLNITE   partly cloudy day / night
  OVCDAY  OVCNITE   overcast day / night
  RAIN    SNOW    STORM

Tips: use landscape pictures. Keep the top-right area (where the temperature is
drawn) fairly calm, the bottom third is darkened automatically for the text rows.
Needs: pip install pillow numpy
"""
import sys
import numpy as np
from PIL import Image

W, H = 280, 240

def cover(im):
    """resize + centre-crop to fill 280x240"""
    s = max(W / im.width, H / im.height)
    im = im.resize((max(W, round(im.width * s)), max(H, round(im.height * s))), Image.LANCZOS)
    x, y = (im.width - W) // 2, (im.height - H) // 2
    return im.crop((x, y, x + W, y + H))

def scrim(a, amount=0.55):
    t = np.clip((np.arange(H) / H - 0.62) / 0.38, 0, 1)
    t = t * t * (3 - 2 * t)
    return a * (1 - amount * t)[:, None, None]

def to565(a):
    """Floyd-Steinberg dither to RGB565 (no banding in smooth skies)"""
    a = (a * 255.0).astype(np.float64).tolist()
    out = np.zeros((H, W), np.uint16)
    lv = (31.0, 63.0, 31.0)
    for y in range(H):
        row = a[y]
        nxt = a[y + 1] if y + 1 < H else None
        for x in range(W):
            px = row[x]
            q = [0, 0, 0]
            for c in range(3):
                v = min(255.0, max(0.0, px[c]))
                qi = int(round(v / 255.0 * lv[c]))
                e = v - qi * 255.0 / lv[c]
                q[c] = qi
                if x + 1 < W:
                    row[x + 1][c] += e * 7 / 16
                if nxt is not None:
                    if x > 0:
                        nxt[x - 1][c] += e * 3 / 16
                    nxt[x][c] += e * 5 / 16
                    if x + 1 < W:
                        nxt[x + 1][c] += e * 1 / 16
            out[y, x] = (q[0] << 11) | (q[1] << 5) | q[2]
    return out

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(1)
    src, dst = sys.argv[1], sys.argv[2]
    a = np.asarray(cover(Image.open(src).convert('RGB')), np.float32) / 255.0
    if '--no-scrim' not in sys.argv:
        a = scrim(a)
    q = to565(a)
    open(dst, 'wb').write(q.astype('>u2').tobytes())
    print(dst, q.size * 2, 'bytes (expected 134400)')
