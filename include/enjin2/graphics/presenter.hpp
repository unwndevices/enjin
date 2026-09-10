#pragma once

#include "canvas.hpp"

namespace enjin2 {

struct Palette; // forward decl — the presenter only needs a pointer

/**
 * @brief A composited frame handed to a presenter: the canonical output canvas,
 *        the merged dirty-tile bitmap describing which 16×16 tiles changed, and
 *        the palette to expand indices through.
 *
 * The compositor emits one `Frame` per `present()` call. A presenter adapter
 * (QSPI panel, WASM canvas, SDL window, test fake) reads the changed tiles from
 * `output` and pushes them to its target; everything else — LUT expansion,
 * buffer placement, window merging, back-pressure — is the adapter's business.
 *
 * `dirtyTiles` is a bitmap of `tilesX * tilesY` bits in row-major order
 * (bit `ty*tilesX + tx`). A set bit means tile (tx,ty) changed this frame and
 * must be pushed; an all-zero bitmap means nothing changed (present is a no-op
 * for the target, though the adapter is still called once).
 */
template <uint16_t W, uint16_t H>
struct Frame {
    const Canvas4<W, H>* output;   ///< Canonical composited frame (never null).
    const uint8_t*       dirtyTiles; ///< Merged dirty-tile bitmap (never null).
    uint16_t             tilesX;    ///< Tile columns = ceil(W/16).
    uint16_t             tilesY;    ///< Tile rows = ceil(H/16).
    const Palette*       palette;   ///< Palette for index→RGB; may be null (use g_palette).

    /// @brief Test whether tile (tx,ty) is marked dirty in this frame.
    /// @brief Derive a 10-bit band mask for the CO5300 480x480 panel from dirtyTiles.
    /// Logical canvas 160x160, tile size 16 (10 tile rows). Panel scale x3 means
    /// 1 logical tile row = exactly 1 48-row panel band. Returns 10-bit mask.
    uint16_t deriveBandMask() const {
        uint16_t mask = 0;
        for (uint16_t ty = 0; ty < tilesY && ty < 10; ++ty) {
            for (uint16_t tx = 0; tx < tilesX; ++tx) {
                if (isTileDirty(tx, ty)) {
                    mask |= (1 << ty);
                    break;
                }
            }
        }
        return mask;
    }

    bool isTileDirty(uint16_t tx, uint16_t ty) const {
        if (tx >= tilesX || ty >= tilesY) {
            return false;
        }
        const uint16_t t = ty * tilesX + tx;
        return (dirtyTiles[t >> 3] & static_cast<uint8_t>(1u << (t & 7))) != 0;
    }
};

/**
 * @brief The seam between the compositor and whatever puts pixels on a display.
 *
 * Enjin drives the frame sequence (restore → draw → composite → present); a host
 * supplies exactly one adapter implementing `present()`. The interface is
 * templated on canvas size because a build fixes its canvas dimensions.
 */
template <uint16_t W, uint16_t H>
class IPresenter {
public:
    virtual ~IPresenter() = default;

    /**
     * @brief Present one composited frame. Called once per frame, synchronously.
     * @param frame The composited output, dirty-tile bitmap and palette.
     */
    virtual void present(const Frame<W, H>& frame) = 0;
};

} // namespace enjin2
