"""sRGB ↔ Oklab colour conversions (Björn Ottosson's Oklab).

Quantisation matches purchased-tileset colours to the engine's 15-index palette
by nearest distance in **Oklab**, whose Euclidean metric is far more perceptually
uniform than sRGB or CIELAB for the saturated pixel-art colours we deal with
(the #52/#53 decision: own Oklab quantiser, not ImageMagick/Aseprite CIELAB).

All functions are vectorised over numpy arrays: an ``(..., 3)`` array of sRGB
bytes (0–255) or linear/Oklab floats, preserving the leading shape.
"""

from __future__ import annotations

import numpy as np

# Linear-sRGB → LMS matrix (Ottosson).
_M1 = np.array(
    [
        [0.4122214708, 0.5363325363, 0.0514459929],
        [0.2119034982, 0.6806995451, 0.1073969566],
        [0.0883024619, 0.2817188376, 0.6299787005],
    ],
    dtype=np.float64,
)

# LMS' (cube-rooted) → Oklab matrix (Ottosson).
_M2 = np.array(
    [
        [0.2104542553, 0.7936177850, -0.0040720468],
        [1.9779984951, -2.4285922050, 0.4505937099],
        [0.0259040371, 0.7827717662, -0.8086757660],
    ],
    dtype=np.float64,
)


def srgb_to_linear(srgb: np.ndarray) -> np.ndarray:
    """Convert sRGB bytes (0–255) to linear-light floats (0–1).

    Accepts any array whose last axis holds the channels; the gamma curve is
    applied per channel.
    """
    c = np.asarray(srgb, dtype=np.float64) / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def linear_to_oklab(linear: np.ndarray) -> np.ndarray:
    """Convert linear-sRGB floats (0–1), shape ``(..., 3)``, to Oklab ``(..., 3)``."""
    lin = np.asarray(linear, dtype=np.float64)
    lms = lin @ _M1.T
    # Cube root, sign-safe (linear inputs are non-negative, but guard anyway).
    lms_ = np.cbrt(lms)
    return lms_ @ _M2.T


def srgb_to_oklab(srgb: np.ndarray) -> np.ndarray:
    """Convert sRGB bytes (0–255), shape ``(..., 3)``, straight to Oklab ``(..., 3)``."""
    return linear_to_oklab(srgb_to_linear(srgb))


def delta_e(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    """Euclidean ΔE in Oklab (ΔEOK) between two ``(..., 3)`` Oklab arrays.

    Broadcasts like numpy: pass one ``(N, 3)`` and one ``(3,)`` to get ``(N,)``.
    """
    diff = np.asarray(a, dtype=np.float64) - np.asarray(b, dtype=np.float64)
    return np.sqrt(np.sum(diff * diff, axis=-1))
