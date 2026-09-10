"""Nearest-in-Oklab quantisation of RGBA pixels to the 15-index palette.

Purchased tilesets ship as RGBA PNG with an artist palette; the engine renders
4bpp indices against a fixed 15-colour ramp. This module maps each opaque pixel
to the nearest palette colour in Oklab (dithering off, per the #53 decision) and
reports the ΔEOK error so a human can judge quantise quality (the ``--preview``
histogram in the CLIs). Transparent pixels (alpha ≤ threshold) map straight to
the reserved transparent index, never by colour.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from . import oklab
from .palette import TRANSPARENT_INDEX, Palette

#: Default alpha cutoff: pixels at or below this are transparent (#53: α≤128→15).
DEFAULT_ALPHA_THRESHOLD = 128


@dataclass
class QuantizeResult:
    """Indices plus per-opaque-pixel ΔEOK error.

    Attributes:
        indices: uint8 array (same leading shape as input, channels dropped) of
            palette indices; transparent pixels hold :data:`TRANSPARENT_INDEX`.
        delta_e: 1-D float array of ΔEOK values, one per *opaque* pixel (the
            transparent pixels contribute no colour error). May be empty.
    """

    indices: np.ndarray
    delta_e: np.ndarray


def quantize_rgba(
    rgba: np.ndarray,
    palette: Palette,
    alpha_threshold: int = DEFAULT_ALPHA_THRESHOLD,
) -> QuantizeResult:
    """Quantise an ``(..., 4)`` uint8 RGBA array to palette indices.

    Args:
        rgba: RGBA bytes, last axis = (R, G, B, A). Any leading shape is kept.
        palette: The target :class:`Palette`.
        alpha_threshold: Pixels with ``A <= alpha_threshold`` become transparent.

    Returns:
        A :class:`QuantizeResult`; ``indices`` has the input's leading shape.
    """
    arr = np.asarray(rgba, dtype=np.uint8)
    if arr.shape[-1] != 4:
        raise ValueError(f"expected RGBA (last axis 4), got shape {arr.shape}")

    lead = arr.shape[:-1]
    flat = arr.reshape(-1, 4)
    alpha = flat[:, 3]
    opaque_mask = alpha > alpha_threshold

    indices = np.full(flat.shape[0], TRANSPARENT_INDEX, dtype=np.uint8)
    delta_e = np.zeros(0, dtype=np.float64)

    if np.any(opaque_mask):
        opaque_rgb = flat[opaque_mask, :3]
        px_oklab = oklab.srgb_to_oklab(opaque_rgb)  # (K, 3)
        # Pairwise ΔEOK to each of the 15 palette colours → (K, 15).
        dists = oklab.delta_e(px_oklab[:, None, :], palette.oklab[None, :, :])
        best = np.argmin(dists, axis=1)
        indices[opaque_mask] = best.astype(np.uint8)
        delta_e = dists[np.arange(dists.shape[0]), best]

    return QuantizeResult(indices=indices.reshape(lead), delta_e=delta_e)


def histogram(delta_e: np.ndarray, bins: int = 8, max_de: float = 0.16) -> str:
    """Render an ASCII ΔEOK histogram for the ``--preview`` report.

    ΔEOK below ~0.02 is imperceptible and ~0.05 is a just-noticeable step, so the
    default range 0..0.16 spans "perfect" to "clearly shifted". Returns a
    multi-line string ending with count/mean/max summary stats.
    """
    de = np.asarray(delta_e, dtype=np.float64)
    lines = []
    if de.size == 0:
        return "ΔEOK: (no opaque pixels)"
    edges = np.linspace(0.0, max_de, bins + 1)
    counts, _ = np.histogram(np.clip(de, 0.0, max_de), bins=edges)
    peak = max(1, counts.max())
    for i in range(bins):
        lo, hi = edges[i], edges[i + 1]
        bar = "#" * int(round(40 * counts[i] / peak))
        lines.append(f"  [{lo:.3f},{hi:.3f}) {counts[i]:6d} {bar}")
    lines.append(
        f"  ΔEOK  n={de.size}  mean={de.mean():.4f}  max={de.max():.4f}  p95={np.percentile(de, 95):.4f}"
    )
    return "\n".join(lines)
