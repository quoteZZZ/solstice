"""Re-grade the current frame with three evidence-based palettes.

This is a *tone preview*, not a render: it keeps the luminance structure of the
existing frame and replaces hue/saturation band by band, so the only thing that
changes between A/B/C is the palette itself. Colours are the ones extracted from
Wind Waker / Genshin / Ghibli / Your Name (see docs/art/PALETTE.md).
"""

import os
import numpy as np
from PIL import Image

SRC = "tmp/review/shot.png"
OUT = "tmp/review/palette"
os.makedirs(OUT, exist_ok=True)


def hx(s):
    s = s.lstrip("#")
    return np.array([int(s[i:i + 2], 16) for i in (0, 2, 4)], dtype=np.float32) / 255.0


PALETTES = {
    "A_mediterranean_noon": {
        "sky_zenith": "#2E7FC2", "sky_mid": "#62A9DC", "sky_horizon": "#CFE6F2",
        "sun": "#FFF3D0", "sun_glow": "#FFD98A",
        "sea_shallow": "#57C8C8", "sea_mid": "#2E93B8", "sea_deep": "#17527E",
        "foam": "#F4FBF7",
        "sand_dry": "#F0DCB0", "sand_wet": "#C9A97A",
        "grass_light": "#A8D45A", "grass_mid": "#6FA83C", "grass_dark": "#3E6B2E",
        "rock_light": "#C9B79A", "rock_dark": "#6E6152",
        "foliage_dark": "#2F5A3A", "foliage_lit": "#7FBF52",
    },
    "B_hokkaido_early_summer": {
        "sky_zenith": "#5E97C4", "sky_mid": "#9CC4DE", "sky_horizon": "#E4EFEF",
        "sun": "#FFF6E2", "sun_glow": "#FFE0A8",
        "sea_shallow": "#8FD3C4", "sea_mid": "#4E9E9B", "sea_deep": "#2B5F6B",
        "foam": "#FBFDF8",
        "sand_dry": "#F2E3C8", "sand_wet": "#CBB18D",
        "grass_light": "#C8DB8A", "grass_mid": "#8FAE5C", "grass_dark": "#55703F",
        "rock_light": "#D6CEC0", "rock_dark": "#7A7268",
        "foliage_dark": "#3C5B45", "foliage_lit": "#9CC46A",
    },
    "C_golden_hour": {
        "sky_zenith": "#3B4E86", "sky_mid": "#8A6FA8", "sky_horizon": "#F5A76B",
        "sun": "#FFE9B0", "sun_glow": "#FF9E5E",
        "sea_shallow": "#7FB2C4", "sea_mid": "#3F6E8C", "sea_deep": "#22384F",
        "foam": "#FFEFE0",
        "sand_dry": "#EFD2A8", "sand_wet": "#B98E68",
        "grass_light": "#C9C070", "grass_mid": "#8A8A4E", "grass_dark": "#4A5238",
        "rock_light": "#C48F72", "rock_dark": "#5B3F42",
        "foliage_dark": "#333F35", "foliage_lit": "#8FA05A",
    },
}


def lerp(a, b, t):
    return a + (b - a) * np.clip(t, 0.0, 1.0)[..., None]


def two_stop(dark, mid, light, t):
    """t in [0,1]: 0 -> dark, 0.5 -> mid, 1 -> light."""
    lo = lerp(dark, mid, t * 2.0)
    hi = lerp(mid, light, (t - 0.5) * 2.0)
    return np.where((t < 0.5)[..., None], lo, hi)


def rgb_to_hsv(arr):
    r, g, b = arr[..., 0], arr[..., 1], arr[..., 2]
    mx = arr.max(axis=-1)
    mn = arr.min(axis=-1)
    d = mx - mn
    h = np.zeros_like(mx)
    mask = d > 1e-6
    idx = mask & (mx == r)
    h[idx] = ((g[idx] - b[idx]) / d[idx]) % 6
    idx = mask & (mx == g)
    h[idx] = (b[idx] - r[idx]) / d[idx] + 2
    idx = mask & (mx == b)
    h[idx] = (r[idx] - g[idx]) / d[idx] + 4
    h = h / 6.0
    s = np.where(mx > 1e-6, d / np.maximum(mx, 1e-6), 0.0)
    return h, s, mx


def build(src_rgb, P):
    H, W, _ = src_rgb.shape
    r, g, b = src_rgb[..., 0], src_rgb[..., 1], src_rgb[..., 2]
    lum = src_rgb @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
    hue, sat, val = rgb_to_hsv(src_rgb)

    # ---- horizon: the brightest *row* is the pale band the sea meets ----
    # Per-column detection fails here: the upper sky is blue too, so the first
    # "blue" pixel is at the top of the frame. The horizon band is the only row
    # that is bright across its whole width (the sun only brightens a slice).
    row_mean = lum.mean(axis=1)
    top_region = row_mean[: int(H * 0.7)]
    if top_region.size:
        band = int(np.argmax(top_region))
        horizon = band + max(2, int(H * 0.006))
    else:
        horizon = int(H * 0.42)
    horizon = max(20, min(horizon, int(H * 0.75)))

    yy, xx = np.mgrid[0:H, 0:W]

    # ---- land mask: not blue, above the frame bottom, inside the island box ----
    land = (~(b > r + 0.045)) & (yy > horizon - 6) & (yy < H * 0.62) & (lum > 0.10)
    land &= (np.abs(xx - W * 0.47) < W * 0.30)
    sky = yy < horizon
    sea = (~sky) & (~land)

    out = np.zeros_like(src_rgb)

    # ---- sky: three stops, horizon band forced bright ----
    # 0 at the horizon -> sky_horizon (near white), 0.30 -> sky_mid, 1.0 -> sky_zenith.
    t_sky = np.clip((horizon - yy) / max(1.0, float(horizon)), 0.0, 1.0)
    sky_col = np.where(
        (t_sky < 0.30)[..., None],
        lerp(hx(P["sky_horizon"]), hx(P["sky_mid"]), t_sky / 0.30),
        lerp(hx(P["sky_mid"]), hx(P["sky_zenith"]), (t_sky - 0.30) / 0.70),
    )
    # The celestial body is deliberately NOT reproduced: the current frame draws it
    # as two concentric rings, which is a geometry bug that the render pass fixes.
    # Previewing the palette does not need it, so the sky gradient simply covers it.
    out[sky] = sky_col[sky]

    # ---- sea: near -> far (bottom is deep, horizon is shallow) ----
    t_sea = np.clip((yy - horizon) / max(1.0, (H - horizon) * 1.05), 0.0, 1.0)
    sea_col = two_stop(hx(P["sea_shallow"]), hx(P["sea_mid"]), hx(P["sea_deep"]), t_sea)
    # keep the frame's own ripple contrast, gently
    sea_lum = lum[sea]
    if sea_lum.size:
        rel = np.clip(sea_lum / max(1e-4, float(np.median(sea_lum))), 0.82, 1.18)
        sea_col[sea] = sea_col[sea] * rel[:, None]
    # aerial perspective: the far water must meet the horizon band, not cut into it
    fade = np.clip(1.0 - (yy - horizon) / max(1.0, (H - horizon) * 0.16), 0.0, 1.0)
    fade = (fade ** 1.6) * 0.85
    sea_col = lerp(sea_col, np.broadcast_to(hx(P["sky_horizon"]), sea_col.shape), fade)
    out[sea] = sea_col[sea]

    # ---- land: classify by hue, then band by luminance inside each class ----
    if land.any():
        l_lum = lum[land]
        p20, p55, p85 = (np.percentile(l_lum, q) for q in (20, 55, 85))
        h_l, s_l, l_ = hue[land], sat[land], lum[land]

        is_grass = (h_l > 0.17) & (h_l < 0.46) & (s_l > 0.12)
        is_sand = (h_l >= 0.05) & (h_l <= 0.17) & (s_l > 0.10) & (l_ > p55)
        is_rock = ~is_grass & ~is_sand

        col = np.zeros((h_l.size, 3), dtype=np.float32)
        # grass: 3 bands, hard-ish
        tg = np.clip((l_ - p20) / max(1e-4, p85 - p20), 0.0, 1.0)
        col[is_grass] = two_stop(hx(P["grass_dark"]), hx(P["grass_mid"]), hx(P["grass_light"]), tg[is_grass])
        # sand
        ts = np.clip((l_ - p55) / max(1e-4, p85 - p55), 0.0, 1.0)
        col[is_sand] = lerp(hx(P["sand_wet"]), hx(P["sand_dry"]), ts[is_sand])
        # rock
        tr = np.clip((l_ - p20) / max(1e-4, p85 - p20), 0.0, 1.0)
        col[is_rock] = lerp(hx(P["rock_dark"]), hx(P["rock_light"]), tr[is_rock])

        out[land] = col

    return np.clip(out, 0.0, 1.0), horizon


def main():
    src = np.asarray(Image.open(SRC).convert("RGB")).astype(np.float32) / 255.0
    for name, P in PALETTES.items():
        graded, horizon = build(src, P)
        Image.fromarray((graded * 255).astype(np.uint8)).save(os.path.join(OUT, f"{name}.png"))
        print(f"{name}: horizon={horizon}")


if __name__ == "__main__":
    main()
