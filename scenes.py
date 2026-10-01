#!/usr/bin/env python3
"""
Procedural photo-style weather scenes for a 280x240 ST7789 (RGB565).
Every scene is a complete picture (sky, sun/moon, clouds, land, rain...), so the
sketch only has to load ONE file per weather condition.

usage: python scenes.py [outdir]
"""
import sys, os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from scipy.ndimage import map_coordinates, gaussian_filter, shift as ndshift

W, H = 280, 240          # screen (landscape)
S = 2                    # supersampling
w, h = W * S, H * S
HOR = 0.70               # horizon (fraction of height)
SUN = (70 * S, 60 * S)   # sun / moon position (upper left, text lives on the right)

# ------------------------------------------------------------------ helpers
def ss(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)

def col(c):
    return np.array(c, np.float32)

def mix(a, b, t):
    return a + (b - a) * t

_tex_cache = {}
def fbm(width, height, scale, octaves=6, pers=0.5, seed=0, ax=1.0, ay=1.0, tile=False):
    key = (width, height, scale, octaves, pers, seed, ax, ay, tile)
    if key in _tex_cache:
        return _tex_cache[key]
    r = np.random.default_rng(seed)
    out = np.zeros((height, width), np.float32)
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        gw = max(2, int(width / scale * ax * 2 ** o))
        gh = max(2, int(height / scale * ay * 2 ** o))
        g = r.random((gh, gw)).astype(np.float32)
        if tile:                                   # periodic: tile 3x3, resize, keep the middle
            big = np.tile(g, (3, 3))
            big = np.asarray(Image.fromarray(big).resize((width * 3, height * 3), Image.BICUBIC))
            out += big[height:2 * height, width:2 * width] * amp
        else:
            out += np.asarray(Image.fromarray(g).resize((width, height), Image.BICUBIC)) * amp
        tot += amp
        amp *= pers
    out /= tot
    lo, hi = np.percentile(out, [2, 98])
    out = np.clip((out - lo) / (hi - lo), 0, 1)
    _tex_cache[key] = out
    return out

YY, XX = np.mgrid[0:h, 0:w].astype(np.float32)
YF = YY / h

# ------------------------------------------------------------------ sky
def sky(zen, hor):
    t = np.clip(YY / (h * HOR), 0, 1) ** 1.5
    return mix(col(zen)[None, None, :], col(hor)[None, None, :], t[..., None])

def sun_glow(img, warm=(1.0, 0.86, 0.60), strength=1.0):
    d = np.hypot(XX - SUN[0], YY - SUN[1])
    core = np.exp(-d / (9 * S)) * 1.6
    mid = np.exp(-d / (30 * S)) * 0.55
    wide = np.exp(-d / (110 * S)) * 0.30
    disc = ss(9.5 * S, 7.5 * S, d)
    g = (core + mid) * strength
    img += g[..., None] * col((1.0, 0.95, 0.85))
    img += (wide * strength)[..., None] * col(warm)
    img += disc[..., None] * col((2.0, 2.0, 1.9))
    return img

# ------------------------------------------------------------------ stars / moon
def stars(img, seed=5, n=420, visibility=None):
    r = np.random.default_rng(seed)
    layer = np.zeros((h, w), np.float32)
    tint = np.zeros((h, w, 3), np.float32)
    for _ in range(n):
        x = r.uniform(0, w)
        y = r.uniform(0, h * HOR * 0.98)
        mag = r.random() ** 3.2                       # few bright, many faint
        sig = 0.55 + 0.9 * mag
        c = np.array([1.0, 1.0, 1.0]) * 0.5 + r.random(3) * np.array([0.10, 0.12, 0.25])
        x0, y0 = int(x), int(y)
        for dy in range(-3, 4):
            for dx in range(-3, 4):
                px, py = x0 + dx, y0 + dy
                if 0 <= px < w and 0 <= py < h:
                    v = mag * 0.95 * np.exp(-((px - x) ** 2 + (py - y) ** 2) / (2 * sig ** 2)) + 0.05 * (mag > 0.4)
                    layer[py, px] += v
                    tint[py, px] += c * v
    # faint milky way band
    band = np.exp(-(((XX - w * 0.55) * 0.55 + (YY - h * 0.15) * 0.83) / (55 * S)) ** 2)
    mw = fbm(w, h, 60, 5, 0.55, seed + 9) ** 2.0 * band * 0.10
    vis = 1.0 if visibility is None else visibility
    img += (tint * 1.15 + mw[..., None] * col((0.55, 0.62, 0.95))) * (vis[..., None] if hasattr(vis, 'shape') else vis)
    return img

def moon(img, R=27 * S, seed=4, light=(0.80, -0.30, 0.52), glow=1.0):
    cx, cy = SUN
    size = int(2 * R + 10)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    nx, ny = (xx - size / 2) / R, (yy - size / 2) / R
    r2 = nx ** 2 + ny ** 2
    nz = np.sqrt(np.clip(1 - r2, 0, 1))
    rr = np.sqrt(r2)

    tex = fbm(size, size, R * 0.9, 5, 0.55, seed)
    dark = 1 - ss(0.40, 0.62, tex)                        # maria
    alb = 0.88 * (1 - 0.40 * dark)
    alb *= 0.90 + 0.20 * fbm(size, size, R * 0.12, 4, 0.6, seed + 1)

    rg = np.random.default_rng(seed)
    for k in range(120):
        a = rg.uniform(0, 2 * np.pi)
        rad = np.sqrt(rg.uniform(0, 0.92))
        ccx, ccy = rad * np.cos(a), rad * np.sin(a)
        rc = float(np.clip(rg.lognormal(np.log(0.065), 0.55), 0.02, 0.24))
        d = np.hypot(nx - ccx, ny - ccy) / rc
        bowl = (d < 1) * (-0.10 * (1 - d ** 2))
        rim = 0.16 * np.exp(-((d - 1.0) / 0.13) ** 2)
        alb *= 1 + bowl + rim
    for (ccx, ccy, rc) in [(-0.25, 0.62, 0.075), (0.42, -0.35, 0.06)]:     # two "ray" craters
        d = np.hypot(nx - ccx, ny - ccy)
        ang = np.arctan2(ny - ccy, nx - ccx)
        rays = (0.5 + 0.5 * np.cos(ang * 14 + 1.3)) ** 6 * np.exp(-d / 0.55) * 0.22
        alb *= 1 + rays * (d > rc) + 0.25 * np.exp(-(d / rc) ** 2)

    L = np.array(light) / np.linalg.norm(light)
    diff = np.clip(nx * L[0] + ny * L[1] + nz * L[2], 0, 1) ** 0.62
    earth = 0.035
    lum = alb * (earth + (1 - earth) * diff)
    rgbm = lum[..., None] * col((1.02, 0.99, 0.92)) * 1.18
    alpha = ss(1.0, 0.965, rr)

    # halo (drawn first, additive)
    halo = np.zeros((h, w), np.float32)
    x0, y0 = int(cx - size / 2), int(cy - size / 2)
    inside = np.zeros((h, w), np.float32)
    inside[y0:y0 + size, x0:x0 + size] = (rr < 1)
    hl = gaussian_filter(inside, R * 0.55) * 0.9 + gaussian_filter(inside, R * 1.8) * 0.9
    img += (hl * glow)[..., None] * col((0.30, 0.38, 0.62)) * 0.55

    patch = img[y0:y0 + size, x0:x0 + size]
    a = alpha[..., None]
    img[y0:y0 + size, x0:x0 + size] = patch * (1 - a) + rgbm * a
    return img

# ------------------------------------------------------------------ clouds
def cloud_density(coverage, seed, scale=230, soft=0.22, ax=1.0, ay=1.0, top=0.0, detail=6, dmin=55):
    """Cloud layer seen in perspective: it shrinks towards the horizon."""
    T = fbm(1024, 1024, scale, detail, 0.52, seed, ax, ay, tile=True)
    horizon = h * HOR
    d = np.maximum(horizon - YY, dmin)                      # distance above the horizon (px)
    u = (XX - w / 2) / d
    v = 420.0 / d
    P = 380.0
    coords = np.array([(v * P) % 1024, (u * P + 300) % 1024])
    Dn = map_coordinates(T, coords, order=1, mode='wrap')
    dens = np.clip((Dn - (1 - coverage)) / soft, 0, 1)
    dens = dens * dens * (3 - 2 * dens)
    # thin out towards the horizon and slightly at the very top
    dens *= ss(horizon * 1.02, horizon * 0.80, YY) if False else (1 - ss(horizon * 0.86, horizon * 1.0, YY))
    dens *= 1 - top * ss(0.0, h * 0.10, h * 0.10 - YY)
    return dens.astype(np.float32)

def lit_clouds(img, dens, lit, shadow, sun_dir=(-0.62, -0.78), absorb=2.4, alpha_gain=1.0,
               hole=None, rim=0.0, ambient_light=None):
    dx, dy = sun_dir
    occl = np.zeros_like(dens)
    steps = (3, 7, 13, 22)
    for s in steps:
        occl += ndshift(dens, (-dy * s * S, -dx * s * S), order=1, mode='nearest')
    occl /= len(steps)
    trans = np.exp(-absorb * occl)                                    # 1 = lit side
    c = mix(col(shadow)[None, None, :], col(lit)[None, None, :], trans[..., None])
    if rim:
        edge = (1 - dens) * dens * 4.0
        c = c + (edge * rim * trans)[..., None] * col((1.0, 0.95, 0.85))
    if ambient_light is not None:
        c = c + ambient_light[..., None]
    a = np.clip(dens * alpha_gain, 0, 1)
    if hole is not None:
        a = a * hole
    img = img * (1 - a[..., None]) + c * a[..., None]
    return img, a

# ------------------------------------------------------------------ land
def ridge(seed, base, amp, scale):
    r = np.random.default_rng(seed)
    xs = np.arange(w, dtype=np.float32)
    y = np.zeros(w, np.float32)
    a, tot = 1.0, 0.0
    for o in range(5):
        k = int(w / scale * 2 ** o) + 3
        vals = r.random(k)
        y += np.interp(xs, np.linspace(0, w, k), vals).astype(np.float32) * a
        tot += a
        a *= 0.5
    y /= tot
    y = (y - y.min()) / (y.max() - y.min())
    return base + amp * (y - 0.5)

def firs(mask_img, ridge_y, seed, density, hmin, hmax):
    r = np.random.default_rng(seed)
    d = ImageDraw.Draw(mask_img)
    x = 0.0
    while x < w:
        x += r.uniform(3, 3 + 26 / density)
        xi = int(min(x, w - 1))
        base = ridge_y[xi] + r.uniform(2, 8)
        th = r.uniform(hmin, hmax)
        tw = th * r.uniform(0.28, 0.38)
        pts = [(x, base - th)]
        tiers = 5
        for i in range(1, tiers + 1):
            yy = base - th + th * i / tiers
            ww = tw * (i / tiers) * (1.0 if i % 2 else 0.78)
            pts.append((x + ww, yy))
            pts.append((x + ww * 0.45, yy - th * 0.03))
        for i in range(tiers, 0, -1):
            yy = base - th + th * i / tiers
            ww = tw * (i / tiers) * (1.0 if i % 2 else 0.78)
            pts.append((x - ww * 0.45, yy - th * 0.03))
            pts.append((x - ww, yy))
        d.polygon(pts, fill=255)

def land(img, far, mid, near, haze_col, haze_amt, seed=11, trees=True, snow=False):
    hor = h * HOR
    # far ridge (blue haze), mid ridge (with firs), near ground
    layers = [
        (ridge(seed, hor - 26 * S, 34 * S, 210), far, 0.0, 0.6),
        (ridge(seed + 1, hor + 2 * S, 30 * S, 150), mid, 0.5, 0.35),
        (ridge(seed + 2, hor + 34 * S, 26 * S, 110), near, 1.0, 0.0),
    ]
    tex_hi = fbm(w, h, 5 * S, 4, 0.6, seed + 20)
    tex_lo = fbm(w, h, 40 * S, 5, 0.55, seed + 21)
    for idx, (ry, base, closeness, haze) in enumerate(layers):
        m = (YY > ry[None, :].repeat(h, 0)).astype(np.float32)
        if trees and idx == 1:
            tm = Image.new('L', (w, h), 0)
            firs(tm, ry, seed + 30, 1.4, 8 * S, 20 * S)
            m = np.maximum(m, np.asarray(tm, np.float32) / 255.0)
        if trees and idx == 2:
            tm = Image.new('L', (w, h), 0)
            firs(tm, ry - 3 * S, seed + 31, 0.5, 18 * S, 38 * S)
            m = np.maximum(m, np.asarray(tm, np.float32) / 255.0)
        m = gaussian_filter(m, 0.7)
        depth = np.clip((YY - ry.min()) / (h - ry.min()), 0, 1)
        c = col(base)[None, None, :] * (0.78 + 0.5 * tex_lo[..., None] * 0.6 + 0.22 * (tex_hi[..., None] - 0.5))
        if snow:
            c = col(base)[None, None, :] * (0.90 + 0.14 * tex_lo[..., None] + 0.06 * (tex_hi[..., None] - 0.5))
        c = c * (1 - 0.18 * depth[..., None] * closeness)
        c = mix(c, col(haze_col)[None, None, :], (haze * haze_amt)[..., None] if np.ndim(haze * haze_amt) else haze * haze_amt)
        img = img * (1 - m[..., None]) + c * m[..., None]
    # low mist along the horizon
    mist = np.exp(-((YY - hor) / (14 * S)) ** 2) * haze_amt * 0.55
    img = mix(img, col(haze_col)[None, None, :], mist[..., None])
    return img

# ------------------------------------------------------------------ weather
def rain_layer(img, seed, n_far=1400, n_near=170, angle=0.20, tint=(0.78, 0.83, 0.92), gain=1.0):
    r = np.random.default_rng(seed)
    a = np.zeros((h, w), np.float32)
    for n, (lo, hi, wid, val) in [(n_far, (10, 26, 1, 0.16)), (n_near, (26, 60, 2, 0.30))]:
        im = Image.new('L', (w, h), 0)
        d = ImageDraw.Draw(im)
        for _ in range(n):
            x = r.uniform(-30, w + 30)
            y = r.uniform(-30, h * 0.98)
            L = r.uniform(lo, hi) * (S / 2)
            d.line([(x, y), (x - L * angle, y + L)], fill=int(255 * val * r.uniform(0.5, 1.0)), width=int(wid))
        im = im.filter(ImageFilter.GaussianBlur(0.7 + (wid - 1) * 0.6))
        a += np.asarray(im, np.float32) / 255.0
    img = img + (a * gain)[..., None] * col(tint)
    return img

def snow_layer(img, seed):
    r = np.random.default_rng(seed)
    total = np.zeros((h, w), np.float32)
    for n, rad, blur, val in [(700, 0.8, 0.5, 0.55), (230, 1.6, 0.9, 0.65), (40, 5.0, 3.0, 0.30), (10, 10.0, 6.0, 0.20)]:
        im = Image.new('L', (w, h), 0)
        d = ImageDraw.Draw(im)
        for _ in range(n):
            x, y = r.uniform(0, w), r.uniform(0, h * 0.99)
            rr = rad * S / 2 * r.uniform(0.7, 1.3)
            d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=int(255 * val * r.uniform(0.6, 1.0)))
        im = im.filter(ImageFilter.GaussianBlur(blur))
        total += np.asarray(im, np.float32) / 255.0
    return img + np.clip(total, 0, 1)[..., None] * col((0.98, 0.99, 1.0)) * 0.9

def lightning(img, seed=8, x0=118 * S):
    r = np.random.default_rng(seed)
    im = Image.new('L', (w, h), 0)
    d = ImageDraw.Draw(im)

    def bolt(x, y, ang, length, width, depth):
        pts = [(x, y)]
        steps = int(length / (5 * S))
        for i in range(steps):
            ang += r.normal(0, 0.28)
            ang *= 0.86
            x += np.sin(ang) * 6 * S
            y += np.cos(ang) * 6 * S * 1.15
            pts.append((x, y))
            if depth < 2 and r.random() < 0.10:
                bolt(x, y, ang + r.choice([-1, 1]) * r.uniform(0.5, 1.0), length * 0.35, max(1, width - 1), depth + 1)
        d.line(pts, fill=255, width=width, joint='curve')
    bolt(x0, -5, 0.05, h * HOR * 0.95, 3, 0)
    core = np.asarray(im, np.float32) / 255.0
    glow = gaussian_filter(core, 3.2 * S) * 3.2 + gaussian_filter(core, 16 * S) * 5.5
    img = img + glow[..., None] * col((0.42, 0.50, 0.95)) * 0.9
    core = gaussian_filter(core, 0.6)
    img = img + core[..., None] * col((1.6, 1.6, 1.8))
    return img

# ------------------------------------------------------------------ finishing
def finish(img, grain=0.012, scrim=0.55, seed=1):
    # soft shoulder instead of hard clipping (sun / lightning highlights)
    img = np.where(img > 0.78, 0.78 + 0.22 * np.tanh((img - 0.78) / 0.22), img)
    # gentle S-curve for a photographic contrast
    img = np.clip(img, 0, 1)
    img = img + 0.10 * (img - 0.5) * (1 - np.abs(2 * img - 1))
    # vignette
    v = 1 - 0.20 * (((XX / w - 0.5) * 1.5) ** 2 + ((YY / h - 0.45) * 1.2) ** 2)
    img = img * np.clip(v, 0.6, 1)[..., None]
    # dark scrim behind the text rows at the bottom
    img = img * (1 - scrim * ss(0.62, 1.0, YF))[..., None]
    # film grain
    g = np.random.default_rng(seed).normal(0, grain, (h, w, 1)).astype(np.float32)
    img = img + g
    img = np.clip(img, 0, 1)
    # 2x2 box down-sample
    return img.reshape(H, S, W, S, 3).mean(axis=(1, 3))

def to565(img01):
    """Floyd-Steinberg dither to RGB565. returns (uint16 array, decoded preview uint8)."""
    a = img01 * 255.0
    hh, ww, _ = a.shape
    out = np.zeros((hh, ww), np.uint16)
    lv = (31.0, 63.0, 31.0)
    a = a.astype(np.float64).tolist()
    for y in range(hh):
        row = a[y]
        nxt = a[y + 1] if y + 1 < hh else None
        for x in range(ww):
            px = row[x]
            q = [0, 0, 0]
            for c in range(3):
                v = min(255.0, max(0.0, px[c]))
                qi = int(round(v / 255.0 * lv[c]))
                qv = qi * 255.0 / lv[c]
                e = v - qv
                q[c] = qi
                if x + 1 < ww:
                    row[x + 1][c] += e * 7 / 16
                if nxt is not None:
                    if x > 0:
                        nxt[x - 1][c] += e * 3 / 16
                    nxt[x][c] += e * 5 / 16
                    if x + 1 < ww:
                        nxt[x + 1][c] += e * 1 / 16
            out[y, x] = (q[0] << 11) | (q[1] << 5) | q[2]
    r = ((out >> 11) & 31).astype(np.uint16)
    g = ((out >> 5) & 63).astype(np.uint16)
    b = (out & 31).astype(np.uint16)
    prev = np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], -1).astype(np.uint8)
    return out, prev

# ------------------------------------------------------------------ scenes
def clear_day():
    img = sky((0.10, 0.32, 0.78), (0.66, 0.82, 0.98))
    img = sun_glow(img)
    dens = cloud_density(0.30, 101, scale=170, soft=0.5, ax=0.5, ay=1.8, detail=5) * 0.30       # faint cirrus
    img, _ = lit_clouds(img, dens, (1.0, 0.99, 0.97), (0.80, 0.86, 0.96), absorb=1.0)
    img = land(img, (0.50, 0.64, 0.76), (0.16, 0.34, 0.30), (0.10, 0.26, 0.10), (0.76, 0.86, 0.96), np.float32(0.9) * (1 - ss(0.0, 1.0, YF)) + 0.15)
    return img

def clear_night():
    img = sky((0.004, 0.010, 0.05), (0.05, 0.09, 0.20))
    img = stars(img, 5)
    img = moon(img)
    img = land(img, (0.05, 0.08, 0.16), (0.02, 0.035, 0.07), (0.008, 0.015, 0.02), (0.06, 0.10, 0.20), np.float32(0.8) * (1 - ss(0.0, 1.0, YF)) + 0.1)
    return img

def partly_day():
    img = sky((0.10, 0.32, 0.78), (0.66, 0.82, 0.98))
    img = sun_glow(img, strength=1.0)
    d_sun = np.hypot(XX - SUN[0], YY - SUN[1])
    hole = 1 - 0.92 * np.exp(-(d_sun / (44 * S)) ** 2)
    dens = cloud_density(0.54, 202, scale=200, soft=0.18, ay=1.30)
    img, _ = lit_clouds(img, dens, (1.02, 0.99, 0.95), (0.46, 0.55, 0.71), absorb=3.3, hole=hole, rim=0.5)
    img = land(img, (0.50, 0.64, 0.76), (0.16, 0.34, 0.30), (0.10, 0.26, 0.10), (0.76, 0.86, 0.96), np.float32(0.9) * (1 - ss(0.0, 1.0, YF)) + 0.15)
    return img

def partly_night():
    img = sky((0.004, 0.010, 0.05), (0.05, 0.09, 0.20))
    dens = cloud_density(0.50, 303, scale=200, soft=0.24, ay=1.35)
    img = stars(img, 6, visibility=(1 - np.clip(dens * 1.5, 0, 1)))
    img = moon(img)
    d_sun = np.hypot(XX - SUN[0], YY - SUN[1])
    hole = 1 - 0.90 * np.exp(-(d_sun / (40 * S)) ** 2)
    img, _ = lit_clouds(img, dens, (0.46, 0.53, 0.68), (0.04, 0.06, 0.12), absorb=2.4, hole=hole, rim=0.35)
    img = land(img, (0.05, 0.08, 0.16), (0.02, 0.035, 0.07), (0.008, 0.015, 0.02), (0.06, 0.10, 0.20), np.float32(0.8) * (1 - ss(0.0, 1.0, YF)) + 0.1)
    return img

def overcast(day):
    if day:
        img = sky((0.40, 0.44, 0.50), (0.74, 0.77, 0.80))
        lit, shadow = (0.90, 0.91, 0.93), (0.44, 0.47, 0.53)
    else:
        img = sky((0.035, 0.05, 0.09), (0.15, 0.18, 0.25))
        lit, shadow = (0.36, 0.40, 0.50), (0.06, 0.08, 0.13)
    d_sun = np.hypot(XX - SUN[0], YY - SUN[1])
    dens = cloud_density(0.93, 404, scale=210, soft=0.42, ax=0.8, ay=1.5, detail=6)
    img, a = lit_clouds(img, dens, lit, shadow, absorb=1.7)
    dens2 = cloud_density(0.85, 405, scale=120, soft=0.5, ax=0.8, ay=1.6, detail=6) * 0.7
    img, a = lit_clouds(img, dens2, lit, shadow, absorb=1.3)
    spot = np.exp(-(d_sun / (42 * S)) ** 2)
    img = img + spot[..., None] * col((0.30, 0.29, 0.26) if day else (0.16, 0.20, 0.30))
    if day:
        img = land(img, (0.56, 0.61, 0.65), (0.27, 0.33, 0.34), (0.11, 0.17, 0.11), (0.72, 0.75, 0.78), np.float32(0.9) * (1 - ss(0.0, 1.0, YF)) + 0.2)
    else:
        img = land(img, (0.13, 0.16, 0.22), (0.05, 0.06, 0.09), (0.015, 0.02, 0.03), (0.15, 0.18, 0.25), np.float32(0.8) * (1 - ss(0.0, 1.0, YF)) + 0.15)
    return img

def rain():
    img = sky((0.20, 0.23, 0.28), (0.52, 0.55, 0.60))
    dens = cloud_density(0.96, 505, scale=200, soft=0.40, ax=0.8, ay=1.5)
    img, _ = lit_clouds(img, dens, (0.62, 0.65, 0.70), (0.20, 0.23, 0.28), absorb=2.2)
    dens2 = cloud_density(0.88, 506, scale=110, soft=0.5, ax=0.8, ay=1.6) * 0.65
    img, _ = lit_clouds(img, dens2, (0.55, 0.58, 0.63), (0.17, 0.20, 0.25), absorb=1.6)
    img = land(img, (0.40, 0.44, 0.49), (0.19, 0.23, 0.25), (0.06, 0.10, 0.09), (0.55, 0.58, 0.62), np.float32(1.0) * (1 - ss(0.0, 1.0, YF)) + 0.30)
    img = rain_layer(img, 7)
    fog = np.exp(-((YY - h * HOR) / (40 * S)) ** 2) * 0.22
    img = mix(img, col((0.62, 0.65, 0.70))[None, None, :], fog[..., None])
    return img

def snow():
    img = sky((0.58, 0.62, 0.68), (0.86, 0.88, 0.91))
    dens = cloud_density(0.95, 606, scale=210, soft=0.42, ax=0.8, ay=1.5)
    img, _ = lit_clouds(img, dens, (0.95, 0.96, 0.98), (0.62, 0.66, 0.73), absorb=1.5)
    img = land(img, (0.76, 0.80, 0.87), (0.40, 0.47, 0.58), (0.90, 0.93, 0.97), (0.86, 0.88, 0.91), np.float32(0.8) * (1 - ss(0.0, 1.0, YF)) + 0.25, snow=True)
    img = snow_layer(img, 9)
    return img

def storm():
    img = sky((0.07, 0.07, 0.11), (0.28, 0.26, 0.34))
    dens = cloud_density(0.97, 707, scale=190, soft=0.30, ax=0.8, ay=1.4)
    img, _ = lit_clouds(img, dens, (0.46, 0.43, 0.54), (0.06, 0.055, 0.09), absorb=3.2, rim=0.15)
    dens2 = cloud_density(0.90, 708, scale=100, soft=0.4, ax=0.8, ay=1.5) * 0.8
    img, _ = lit_clouds(img, dens2, (0.40, 0.38, 0.48), (0.05, 0.05, 0.08), absorb=2.4)
    img = lightning(img)
    img = land(img, (0.22, 0.21, 0.28), (0.09, 0.09, 0.13), (0.03, 0.03, 0.04), (0.28, 0.26, 0.34), np.float32(1.0) * (1 - ss(0.0, 1.0, YF)) + 0.3)
    img = rain_layer(img, 12, n_far=2000, n_near=260, angle=0.30, tint=(0.70, 0.74, 0.90), gain=1.1)
    return img

SCENES = {
    'CLRDAY':  clear_day,
    'CLRNITE': clear_night,
    'PTLDAY':  partly_day,
    'PTLNITE': partly_night,
    'OVCDAY':  lambda: overcast(True),
    'OVCNITE': lambda: overcast(False),
    'RAIN':    rain,
    'SNOW':    snow,
    'STORM':   storm,
}

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'sd'
    os.makedirs(out, exist_ok=True)
    os.makedirs(out + '/preview', exist_ok=True)
    only = sys.argv[2:] or list(SCENES)
    for name in only:
        img = SCENES[name]().astype(np.float32)
        small = finish(img, scrim=0.55, seed=hash(name) % 1000)
        q, prev = to565(small)
        open(f'{out}/{name}.BIN', 'wb').write(q.astype('>u2').tobytes())
        Image.fromarray(prev).save(f'{out}/preview/{name}.png')
        print(name, q.size * 2, 'bytes')
