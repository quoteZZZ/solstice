"""Measure a screenshot against the Cel style criteria (art/art.md SS2.1 / SS2.4).

The point is to turn "the picture looks grey" into numbers someone can pass or fail:

    python tools/checks/measure-frame.py tmp/review/r2_01.png
    python tools/checks/measure-frame.py tmp/review/r2_01.png --box 330,150,760,430

Three checks, all from the same pixel sample:

    saturation   mean HSL saturation over the world region      PASS when > 0.25
    midpoint     median lightness                               PASS when 0.45..0.60
    headroom     share of samples at lightness >= 0.90          PASS when < 0.10

The `--box` argument exists because a full editor capture is mostly panels: measuring the
HUD would happily report saturation 0.4 and prove nothing. Crop to the world first.
"""

from __future__ import annotations

import argparse
import colorsys
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required: pip install pillow")


CRITERIA = {
    "saturation": ("> 0.25", lambda v: v > 0.25),
    "midpoint": ("0.45 - 0.60", lambda v: 0.45 <= v <= 0.60),
    "headroom": ("< 0.10", lambda v: v < 0.10),
}


def measure(path: str, box: tuple[int, int, int, int] | None) -> dict:
    image = Image.open(path).convert("RGB")
    if box:
        image = image.crop(box)

    raw = image.tobytes()
    pixels = [tuple(raw[i:i + 3]) for i in range(0, len(raw), 3)]
    if not pixels:
        raise SystemExit("empty image region")

    step = max(1, len(pixels) // 40000)
    sample = pixels[::step]

    rgb_mean = [sum(p[i] for p in sample) / len(sample) for i in range(3)]

    lightness = []
    saturation = []
    for r, g, b in sample:
        _, l, s = colorsys.rgb_to_hls(r / 255.0, g / 255.0, b / 255.0)
        lightness.append(l)
        saturation.append(s)
    lightness.sort()

    n = len(lightness)
    histogram = [0] * 10
    for l in lightness:
        histogram[min(9, int(l * 10))] += 1

    return {
        "pixels": len(sample),
        "rgb_mean": rgb_mean,
        "lightness_p05": lightness[int(n * 0.05)],
        "lightness_median": lightness[n // 2],
        "lightness_max": lightness[-1],
        "saturation_mean": sum(saturation) / n,
        "headroom_share": histogram[9] / n,
        "histogram": histogram,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Measure a frame against the Cel criteria.")
    parser.add_argument("image")
    parser.add_argument("--box", default="", help="crop as left,top,right,bottom")
    args = parser.parse_args()

    box = None
    if args.box:
        parts = [int(v) for v in args.box.split(",")]
        if len(parts) != 4:
            raise SystemExit("--box wants four integers: left,top,right,bottom")
        box = tuple(parts)

    m = measure(args.image, box)

    print("image        : %s" % args.image)
    if box:
        print("box          : %s" % (box,))
    print("samples      : %d" % m["pixels"])
    print("rgb mean     : %.0f %.0f %.0f" % tuple(m["rgb_mean"]))
    print("lightness    : p05 %.3f  median %.3f  max %.3f"
          % (m["lightness_p05"], m["lightness_median"], m["lightness_max"]))
    print("saturation   : %.3f" % m["saturation_mean"])
    print("histogram    : %s" % m["histogram"])
    print()

    failed = 0
    for name, (wanted, test) in CRITERIA.items():
        value = {
            "saturation": m["saturation_mean"],
            "midpoint": m["lightness_median"],
            "headroom": m["headroom_share"],
        }[name]
        ok = test(value)
        failed += 0 if ok else 1
        print("%-12s %-12s want %-12s %s"
              % (name, "%.3f" % value, wanted, "PASS" if ok else "FAIL"))

    print()
    if failed:
        print("result       : %d check(s) failed (art/art.md SS2.1)" % failed)
        return 1
    print("result       : all checks pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
