#!/usr/bin/env python3
"""Report the numbers a frame has to be judged by, so "it looks flat" can become "why".

Why this exists
---------------
Every art pass in this project starts with the same question and it is never the one that
gets asked: not "is it pretty" but "which of the five things that make a picture readable is
missing". Value range, hue spread, detail distribution and the front-to-back ladder are all
measurable, and a frame that fails one of them cannot be rescued by tuning a material.

This prints those numbers for a rendered frame. It is a *reading*, not a pass/fail gate -
the gate is tools/checks/measure-frame.py, which scores the world disc against art.md SS2.1.
Use this one when the gate passes and the picture still looks wrong.

What it reports
---------------
  value      mean / median / p05 / p95, and the luminance histogram in three bands
  hue        warm / green / cool share of the pixels that have any chroma at all
  detail     Laplacian energy per region - where the frame has texture and where it is a fill
  ladder     region means from sky to near sea, i.e. whether depth is legible

Reading it
----------
Thresholds that have held up on this project (1080p landscape framing):

  dark  (lum < 0.30)   want 15-30%    under 8%  = no shadows, everything floats
  mid   (0.50-0.70)    want < 50%     over 55%  = the classic "flat render" signature
  high  (lum > 0.80)   want 2-8%      under 1%  = no highlights, no sun
  warm  (hue < 75)     want 3-15%     under 1%  = cold picture, reads as unlit 3D
  detail per region    sky < sea < island        the subject must be the busiest thing

Usage
-----
  python tools/checks/analyze-frame.py tmp/review/frame.png
  python tools/checks/analyze-frame.py a.png b.png        # compare two frames side by side
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PIL import Image

# Region boxes are fractions of the frame so the report works at any resolution.
REGIONS = [
    ("sky top",    0.00, 0.00, 1.00, 0.17),
    ("sky mid",    0.00, 0.17, 1.00, 0.41),
    ("horizon",    0.00, 0.41, 1.00, 0.47),
    ("island",     0.16, 0.42, 0.61, 0.56),
    ("sea far",    0.00, 0.47, 1.00, 0.58),
    ("sea mid",    0.00, 0.58, 1.00, 0.75),
    ("sea near",   0.00, 0.75, 1.00, 1.00),
]

# (name, lo, hi, target text, floor, ceil, why). floor..ceil is the acceptable share; for a
# band that should be *scarce* the ceiling is what matters, so mid gets ceil = 50.
BANDS = [
    ("dark", 0.00, 0.30, "15-30%",  8.0, 30.0, "no shadows, everything floats"),
    ("mid",  0.50, 0.70, "<50%",    0.0, 50.0, "the flat-render signature"),
    ("high", 0.80, 1.00, "2-8%",    2.0,  8.0, "no highlights, no sun"),
]


def load(path: Path) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """RGB in 0..1, luminance, HSV hue in degrees and saturation in 0..1."""
    image = Image.open(path).convert("RGB")
    rgb = np.asarray(image, dtype=np.float32) / 255.0
    lum = rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722
    hsv = np.asarray(image.convert("HSV"), dtype=np.float32)
    hue = hsv[..., 0] * 360.0 / 255.0
    sat = hsv[..., 1] / 255.0
    return rgb, lum, hue, sat


def laplacian_energy(lum: np.ndarray) -> float:
    """Mean squared 4-neighbour Laplacian, x1e4 so the numbers are readable.

    Squared, not absolute: a frame with one hard edge and nothing else should score low, and
    the absolute value treats that the same as a frame full of texture.
    """
    if lum.shape[0] < 3 or lum.shape[1] < 3:
        return 0.0
    lap = (4.0 * lum[1:-1, 1:-1]
           - lum[:-2, 1:-1] - lum[2:, 1:-1]
           - lum[1:-1, :-2] - lum[1:-1, 2:])
    return float((lap * lap).mean() * 1e4)


def report(path: Path) -> dict[str, float]:
    rgb, lum, hue, sat = load(path)
    height, width = lum.shape
    total = float(lum.size)

    hist, _ = np.histogram(lum, bins=10, range=(0.0, 1.0))
    hist = hist * 100.0 / total

    # Chroma-gated: a grey pixel has a hue of 0 and would otherwise count as "warm".
    chroma = sat > 0.08
    warm = float((((hue < 75.0) | (hue > 330.0)) & chroma).sum()) * 100.0 / total
    green = float(((hue >= 75.0) & (hue < 160.0) & chroma).sum()) * 100.0 / total
    cool = float(((hue >= 160.0) & (hue <= 330.0) & chroma).sum()) * 100.0 / total

    print(f"\n=== {path.name}  {width}x{height} ===")
    print(f"  value     mean {lum.mean():.3f}  median {np.median(lum):.3f}  "
          f"p05 {np.percentile(lum, 5):.3f}  p95 {np.percentile(lum, 95):.3f}")
    print(f"  hue       warm {warm:5.1f}%   green {green:5.1f}%   cool {cool:5.1f}%")
    print(f"  rgb mean  {rgb[..., 0].mean():.3f} {rgb[..., 1].mean():.3f} "
          f"{rgb[..., 2].mean():.3f}   R-B {rgb[..., 0].mean() - rgb[..., 2].mean():+.3f}")

    print("\n  luminance bands")
    for name, lo, hi, target, floor, ceil, why in BANDS:
        share = float(hist[int(lo * 10):int(hi * 10)].sum())
        flag = "OK  " if floor <= share <= ceil else "OUT "
        print(f"    {flag} {name:5s} {lo:.2f}-{hi:.2f}  {share:5.1f}%   want {target:7s}  {why}")

    print("\n  detail and value by region")
    print(f"    {'region':10s} {'lum':>6s} {'p05':>6s} {'p95':>6s} {'sat':>6s} {'detail':>8s}")
    out: dict[str, float] = {}
    for name, fx0, fy0, fx1, fy1 in REGIONS:
        x0, x1 = int(fx0 * width), int(fx1 * width)
        y0, y1 = int(fy0 * height), int(fy1 * height)
        sub = lum[y0:y1, x0:x1]
        detail = laplacian_energy(sub)
        print(f"    {name:10s} {sub.mean():6.3f} {np.percentile(sub, 5):6.3f} "
              f"{np.percentile(sub, 95):6.3f} {sat[y0:y1, x0:x1].mean():6.3f} {detail:8.2f}")
        out[name] = detail

    island = out.get("island", 0.0)
    sea = max(out.get("sea near", 0.0), out.get("sea mid", 0.0), 1e-6)
    print(f"\n  island / sea detail ratio {island / sea:5.1f}x"
          f"   (want > 3x - the subject has to be the busiest thing in the frame)")
    return out


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 1
    for arg in argv[1:]:
        path = Path(arg)
        if not path.exists():
            print(f"missing: {path}")
            return 1
        report(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
