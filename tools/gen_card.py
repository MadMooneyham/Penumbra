#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Penumbra's card picture for The Firmware Hub (fmhub.json "image": square, 240 to 1024 px, 300 KB at most).

  tools/gen_card.py [corona|drone|forge] [OUT.png] [SIZE]

Drawn, not captured: a total eclipse with its corona over a field of stars, and the name. Three looks:
  corona  the eclipse whole, centred, the name under it
  drone   the eclipse rising at the top, the five drone voices as beating threads of light under it
  forge   the corona round the Lunar Eclipse Forge mark (assets/logo.jpg), as on the boot screen
Everything comes from a fixed seed, so the picture is the same each time."""
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

SRC = Path(__file__).resolve().parents[1]
FONT = SRC / "assets/fonts/BarlowSemiCondensed-SemiBold.ttf"
FONT_M = SRC / "assets/fonts/BarlowSemiCondensed-Medium.ttf"
SS = 2                                                 # drawn at twice the size, then reduced: smooth edges

BG_TOP, BG_BOT = np.array([12, 11, 26]), np.array([3, 4, 9])
EMBER = np.array([255, 150, 70])                       # the screen's accent (ui.c, the landing page)
HOT = np.array([255, 236, 214])
VIOLET = np.array([92, 70, 170])


def ground(n, rng):
    y = np.linspace(0, 1, n)[:, None, None]
    img = BG_TOP * (1 - y) + BG_BOT * y
    img = np.repeat(img, n, axis=1).astype(np.float64)
    # stars: many faint, a few bright, a little colour
    for _ in range(int(n * n / 900)):
        x, yy = rng.integers(0, n, 2)
        b = rng.random() ** 3 * 220 + 20
        tint = np.array([1.0, 0.95 + 0.05 * rng.random(), 0.9 + 0.2 * rng.random()])
        r = 1 if b < 160 else 2
        img[max(0, yy - r + 1):yy + r, max(0, x - r + 1):x + r] += b * tint
    return img


def corona(img, cx, cy, R, rng, reach=2.6, glow=1.0):
    n = img.shape[0]
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float64)
    dx, dy = xx - cx, yy - cy
    r = np.hypot(dx, dy)
    th = np.arctan2(dy, dx)
    # streamers: the corona's rays, uneven in angle, longer along a tilted axis like the real one
    s = np.zeros_like(th)
    for k in range(3, 40):
        s += rng.random() / k ** 0.7 * np.cos(k * th + rng.random() * 2 * np.pi)
    s = (s - s.min()) / (s.max() - s.min())
    fine = np.zeros_like(th)                                      # fine filaments over the broad rays
    for k in range(60, 220, 7):
        fine += rng.random() * np.cos(k * th + rng.random() * 2 * np.pi)
    fine = (fine - fine.min()) / (fine.max() - fine.min())
    s = 0.75 * s + 0.25 * fine
    axis = 0.55 + 0.45 * np.abs(np.cos(th - 0.5)) ** 2
    out = np.clip(r - R, 0, None) / R
    falloff = np.exp(-out / (0.16 + 0.55 * s * axis)) * (r > R * 0.995)
    inner = np.exp(-out / 0.045) * (r > R * 0.995)
    halo = np.exp(-out / (reach * 0.5)) * 0.18 * (r > R)     # the penumbra: a wide, faint violet haze
    i = np.clip(falloff * (0.55 + 0.6 * s) * glow, 0, 1.5)
    col = (HOT * np.clip(inner * 1.2, 0, 1)[..., None]
           + EMBER * np.clip(i - inner * 0.4, 0, 1)[..., None]
           + VIOLET * halo[..., None])
    img += col
    # the moon: black, with the faintest earthshine
    disc = r <= R
    img[disc] = img[disc] * 0.0 + np.array([6, 6, 12])
    return img


def bead(img, cx, cy, R, ang, size):
    """the diamond ring: one bright bead where the sun breaks the rim"""
    n = img.shape[0]
    bx, by = cx + R * math.cos(ang), cy + R * math.sin(ang)
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float64)
    d = np.hypot(xx - bx, yy - by)
    img += HOT * np.exp(-d / size)[..., None] * 1.4
    # four soft spikes
    for a in (0.0, math.pi / 2):
        u = (xx - bx) * math.cos(a) + (yy - by) * math.sin(a)
        v = -(xx - bx) * math.sin(a) + (yy - by) * math.cos(a)
        img += HOT * (np.exp(-np.abs(v) / (size * 0.12)) * np.exp(-np.abs(u) / (size * 7)))[..., None] * 0.6
    return img


def threads(img, y0, y1, rng):
    """the five drone voices: slow sines a few cents apart, so they beat, drawn as threads of light"""
    n = img.shape[0]
    layer = Image.new("L", (n, n), 0)
    d = ImageDraw.Draw(layer)
    xs = np.arange(0, n, 2, dtype=np.float64)
    for v in range(5):
        yc = y0 + (y1 - y0) * (v + 0.5) / 5
        f1 = (2.5 + v * 0.9) * 2 * np.pi / n
        f2 = f1 * (1.0 + 0.3 + 0.08 * v)
        amp = (y1 - y0) / 7
        env = 0.5 + 0.5 * np.cos((f2 - f1) * xs + rng.random() * 6)       # the beat
        ys = yc + amp * env * np.sin(f1 * xs + rng.random() * 6)
        lvl = int(150 + 80 * (v / 4))
        d.line(list(zip(xs, ys)), fill=lvl, width=max(1, n // 300))
    soft = np.asarray(layer.filter(ImageFilter.GaussianBlur(n / 140)), dtype=np.float64) / 255
    sharp = np.asarray(layer, dtype=np.float64) / 255
    img += EMBER * (soft * 1.6)[..., None] + HOT * (sharp * 0.55)[..., None]
    return img


def mark(img, cx, cy, h):
    """the Lunar Eclipse Forge mark, moon-white, over the disc"""
    logo = Image.open(SRC / "assets/logo.jpg").convert("L")
    w = int(logo.width * h / logo.height)
    logo = logo.resize((w, int(h)), Image.LANCZOS)
    a = 1 - np.asarray(logo, dtype=np.float64) / 255                  # black ink -> alpha
    x0, y0 = int(cx - w / 2), int(cy - h / 2)
    moon = np.array([226, 224, 236])
    sub = img[y0:y0 + a.shape[0], x0:x0 + a.shape[1]]
    sub[:] = sub * (1 - a[..., None]) + moon * a[..., None]
    return img


def words(im, n, title_y, sub, align="center", x=None, size=0.13):
    d = ImageDraw.Draw(im)
    f = ImageFont.truetype(str(FONT), int(n * size))
    fs = ImageFont.truetype(str(FONT_M), int(n * 0.042))
    t = "PENUMBRA"
    track = n * size * 0.16
    widths = [d.textlength(c, font=f) for c in t]
    tw = sum(widths) + track * (len(t) - 1)
    x0 = (n - tw) / 2 if align == "center" else x
    cx = x0
    for c, w in zip(t, widths):
        d.text((cx, title_y), c, font=f, fill=(244, 241, 250))
        cx += w + track
    sw = d.textlength(sub, font=fs) + n * 0.042 * 0.25 * (len(sub) - 1)
    sx = (n - sw) / 2 if align == "center" else x
    for c in sub:
        d.text((sx, title_y + n * size * 1.18), c, font=fs, fill=(255, 168, 104))
        sx += d.textlength(c, font=fs) + n * 0.042 * 0.25


def make(look, size):
    n = size * SS
    rng = np.random.default_rng(7342)
    img = ground(n, rng)
    sub = "SPACE DRONE  ·  M-VAVE FM-1"
    if look == "corona":
        cx, cy, R = n * 0.5, n * 0.4, n * 0.17
        img = corona(img, cx, cy, R, rng)
        img = bead(img, cx, cy, R, -0.78, n * 0.012)
        title_y, align, x = n * 0.71, "center", None
    elif look == "drone":
        cx, cy, R = n * 0.5, n * 0.3, n * 0.15
        img = corona(img, cx, cy, R, rng, glow=0.9)
        img = bead(img, cx, cy, R, -2.3, n * 0.01)
        img = threads(img, n * 0.56, n * 0.8, rng)
        title_y, align, x = n * 0.80, "center", None
        sub = "SPACE DRONE  ·  FM-1"
    elif look == "forge":
        cx, cy, R = n * 0.5, n * 0.41, n * 0.2
        img = corona(img, cx, cy, R, rng, glow=1.1)
        img = mark(img, cx + R * 0.03, cy, R * 1.45)
        title_y, align, x = n * 0.73, "center", None
    else:
        raise SystemExit(f"unknown look {look}: corona, drone or forge")
    im = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGB")
    words(im, n, title_y, sub, align, x, 0.115 if look == "drone" else 0.13)
    return im.resize((size, size), Image.LANCZOS)


if __name__ == "__main__":
    look = sys.argv[1] if len(sys.argv) > 1 else "corona"
    out = sys.argv[2] if len(sys.argv) > 2 else "assets/card.png"
    size = int(sys.argv[3]) if len(sys.argv) > 3 else 480
    im = make(look, size)
    im.save(out, optimize=True)
    print(f"card: {out} ({look}, {size} px, {Path(out).stat().st_size} B)")
