#!/usr/bin/env python3
"""Generate the ground/rock/sand texture set as tileable PNGs.

Why this exists
---------------
The whole world is painted with one flat colour per tile. That is the "plastic model"
look: a hundred square metres of #7FBF5F with no grain, no wear, no change of soil.
Detail has to come from somewhere, and the two options are (a) buy or paint textures,
(b) generate them. There is no artist on this project, so (b).

Everything here is *tileable* by construction: the noise lattice is periodic, so the
texture wraps without a seam in either axis. That matters because the ground is mapped
by world position and tiles every few metres.

Output goes to tools/art/textures/. `embed-textures.py` turns those PNGs into a C++
header; the game decodes them into transient Texture2D objects at startup. Nothing here
produces a uasset on purpose - the repo does not store binary assets, and importing
PNGs needs an editor pass that cannot run headless on this machine.

Usage:
    python tools/art/gen-textures.py
"""

from __future__ import annotations

import math
from pathlib import Path

import numpy as np
from PIL import Image

SIZE = 512
OUT = Path(__file__).resolve().parent / "textures"


def _periodic_value_noise(size: int, cells: int, rng: np.random.Generator) -> np.ndarray:
    """Bilinear value noise on a `cells x cells` lattice that wraps at the edges."""
    lattice = rng.random((cells, cells))
    u = np.arange(size) * cells / size
    i0 = np.floor(u).astype(np.int64) % cells
    i1 = (i0 + 1) % cells
    t = u - np.floor(u)
    t = t * t * (3.0 - 2.0 * t)          # smoothstep, so cells do not read as a grid

    g00 = lattice[np.ix_(i0, i0)]
    g01 = lattice[np.ix_(i0, i1)]
    g10 = lattice[np.ix_(i1, i0)]
    g11 = lattice[np.ix_(i1, i1)]

    tx = t[None, :]
    ty = t[:, None]
    top = g00 * (1.0 - tx) + g01 * tx
    bot = g10 * (1.0 - tx) + g11 * tx
    return top * (1.0 - ty) + bot * ty


def fbm(size: int, cells: int, octaves: int, rng: np.random.Generator, gain: float = 0.5) -> np.ndarray:
    """Fractal sum of periodic value noise, normalised to 0..1."""
    total = np.zeros((size, size), dtype=np.float64)
    amplitude = 1.0
    weight = 0.0
    for octave in range(octaves):
        total += amplitude * _periodic_value_noise(size, max(2, cells * (2 ** octave)), rng)
        weight += amplitude
        amplitude *= gain
    out = total / weight
    return (out - out.min()) / max(1e-9, out.max() - out.min())


def normal_from_height(height: np.ndarray, strength: float) -> np.ndarray:
    """Sobel gradient -> tangent-space normal map, encoded 0..1."""
    # np.roll keeps the map tileable: the edge pixels read across to the other side.
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5
    nx = -dx * strength
    ny = -dy * strength
    nz = np.ones_like(height)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    out = np.stack([nx / length, ny / length, nz / length], axis=-1)
    return out * 0.5 + 0.5


def save(name: str, rgb: np.ndarray) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    data = np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
    Image.fromarray(data, mode="RGB").save(OUT / name, optimize=True)
    print(f"  {name:24s} {data.shape[1]}x{data.shape[0]}")


# ---------------------------------------------------------------------------
# grass
# ---------------------------------------------------------------------------
def grass(seed: int) -> None:
    rng = np.random.default_rng(seed)

    broad = fbm(SIZE, 3, 5, rng)      # ~ metres: which patch of the hillside this is
    mid = fbm(SIZE, 10, 4, rng)       # ~ tens of cm: clumps
    fine = fbm(SIZE, 40, 3, rng)      # blade-scale speckle
    dry = fbm(SIZE, 5, 3, rng)        # where the soil shows through

    # Base is a *lit* green, not the palette's #7FBF5F: this is albedo and the sun will
    # multiply it. Too bright here and the meadow clips.
    base = np.array([0.40, 0.58, 0.29])

    light = 1.0 + (broad - 0.5) * 0.34 + (mid - 0.5) * 0.26 + (fine - 0.5) * 0.18
    light = np.clip(light, 0.55, 1.35)

    # Dry patches shift toward straw rather than simply getting darker - that is what
    # stops the field from reading as one green with a noise filter on it.
    t = np.clip((dry - 0.52) * 2.6, 0.0, 1.0)[..., None]
    dry_colour = np.array([0.52, 0.50, 0.26])
    lush = np.array([0.32, 0.54, 0.30])
    colour = lush * (1.0 - t) + dry_colour * t

    rgb = colour * light[..., None]

    # A little blade-level structure: short streaks along y, which is what grass looks
    # like from a distance even when it is a flat texture.
    streak = fbm(SIZE, 96, 2, rng)
    streak = (streak - 0.5) * 0.10
    rgb *= (1.0 + streak)[..., None]

    save("ground_albedo.png", rgb)

    height = mid * 0.55 + fine * 0.45
    save("ground_normal.png", normal_from_height(height, 2.2))


# ---------------------------------------------------------------------------
# rock
# ---------------------------------------------------------------------------
def rock(seed: int) -> None:
    rng = np.random.default_rng(seed)

    strata = fbm(SIZE, 3, 4, rng)
    strata = np.sin((strata * 7.0 + np.linspace(0, 1, SIZE)[:, None] * 5.0) * math.pi) * 0.5 + 0.5
    crack = fbm(SIZE, 8, 5, rng)
    grain = fbm(SIZE, 60, 3, rng)

    # Ridged: 1 - |2n-1| turns the noise into cracks rather than blobs.
    ridge = 1.0 - np.abs(crack * 2.0 - 1.0)
    ridge = ridge ** 2.0

    light = 0.72 + (strata - 0.5) * 0.30 + (grain - 0.5) * 0.16
    light = np.clip(light * (0.62 + 0.50 * ridge), 0.45, 1.25)

    # 0.56, not 0.44: at 0.44 the rock band over the summit rendered as a dark hole.
    base = np.array([0.56, 0.53, 0.48])
    rgb = base * light[..., None]

    save("rock_albedo.png", rgb)
    save("rock_normal.png", normal_from_height(ridge * 0.7 + grain * 0.3, 3.4))


# ---------------------------------------------------------------------------
# sand
# ---------------------------------------------------------------------------
def sand(seed: int) -> None:
    rng = np.random.default_rng(seed)

    ripple = fbm(SIZE, 26, 3, rng)
    ripple = np.sin((ripple * 6.0 + np.linspace(0, 1, SIZE)[None, :] * 9.0) * math.pi) * 0.5 + 0.5
    grain = fbm(SIZE, 128, 2, rng)

    light = 0.86 + (ripple - 0.5) * 0.16 + (grain - 0.5) * 0.12
    base = np.array([0.86, 0.79, 0.63])
    rgb = base * np.clip(light, 0.60, 1.10)[..., None]

    save("sand_albedo.png", rgb)
    save("sand_normal.png", normal_from_height(ripple * 0.6 + grain * 0.4, 1.3))


# ---------------------------------------------------------------------------
# water detail normal: the fine chop that the shader's four waves cannot supply
# ---------------------------------------------------------------------------
def water(seed: int) -> None:
    rng = np.random.default_rng(seed)

    a = fbm(SIZE, 16, 4, rng)
    b = fbm(SIZE, 48, 3, rng)
    height = a * 0.6 + b * 0.4
    save("water_normal.png", normal_from_height(height, 2.8))


def main() -> None:
    print(f"generating {SIZE}x{SIZE} tileable textures -> {OUT}")
    grass(20261018)
    rock(20261019)
    sand(20261020)
    water(20261021)
    print("done")


if __name__ == "__main__":
    main()
