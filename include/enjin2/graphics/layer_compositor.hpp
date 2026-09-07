#pragma once

#include "canvas.hpp"
#include "remap.hpp"
#include "presenter.hpp"
#include <cstring>

namespace enjin2 {

/**
 * @brief Compile-time layer count for the multi-layer compositor.
 *
 * Valid range: 1-8. Change and rebuild to adjust the layer stack.
 * Layer 0 = backmost (background), layer N-1 = frontmost (top).
 *
 * Platform values:
 *   - ESP32-S3 (PSRAM): 4 layers (see the `ifdef ESP32` block below)
 *   - SDL3 / WASM:      5 layers (4 user + 1 debug via engine.debug.*)
 */
#ifdef ESP32
// ESP32-S3 with 8MB PSRAM: 320x240 x 4-bit = ~38KB per layer buffer.
// 4 layers = ~152KB total framebuffer memory. PSRAM (8MB) provides
// ample headroom for layer buffers without touching SRAM (512KB).
// Without PSRAM, reduce to 2-3 layers to stay within SRAM limits.
constexpr uint8_t ENJIN_LAYER_COUNT = 4;
#else
// Desktop (SDL3) and WASM: 5 layers (4 user-facing + 1 debug layer
// accessible only via engine.debug.* bindings in sdl_main.cpp).
constexpr uint8_t ENJIN_LAYER_COUNT = 5;
#endif
static_assert(ENJIN_LAYER_COUNT >= 1 && ENJIN_LAYER_COUNT <= 8,
              "ENJIN_LAYER_COUNT must be between 1 and 8 (inclusive)");

/**
 * @brief Multi-layer canvas compositor.
 *
 * Holds ENJIN_LAYER_COUNT Canvas4 layer buffers plus one output Canvas4.
 * Compositing uses painter's order (layer 0 = back, layer N-1 = front).
 * Pixel index 15 is the transparency passthrough value — any pixel with
 * index 15 on an upper layer lets lower layers show through.
 *
 * Typical frame cycle:
 *   1. clearAll()        — reset all layers for a new frame
 *   2. draw to layers[]  — components write to their assigned layer
 *   3. composite()       — merge layers into output
 *   4. blit output       — send output.getBuffer() to display
 *
 * @tparam W Canvas width in pixels
 * @tparam H Canvas height in pixels
 */
template <uint16_t W, uint16_t H>
struct LayerCompositor {
    /// Layer buffers: index 0 = background, index N-1 = foreground
    Canvas4<W, H> layers[ENJIN_LAYER_COUNT];

    /// Composited output buffer (read-only after composite() call)
    Canvas4<W, H> output;

    /// Per-layer visibility. Hidden layers are skipped during composite().
    bool visible[ENJIN_LAYER_COUNT];

    /**
     * @brief Default constructor — initialises all visibility flags to true.
     */
    LayerCompositor() {
        for (uint8_t i = 0; i < ENJIN_LAYER_COUNT; ++i) {
            visible[i] = true;
        }
        resetDirtyState();
    }

    /**
     * @brief Clear all layer buffers for a new frame.
     *
     * Layer 0 is cleared to Pixel4(0) (black/opaque background).
     * Layers 1..N-1 are cleared to Pixel4(15) (transparent passthrough).
     */
    void clearAll() {
        layers[0].clear(Pixel4(0));
        for (uint8_t i = 1; i < ENJIN_LAYER_COUNT; ++i) {
            layers[i].clear(Pixel4(15));
        }
    }

    /**
     * @brief Composite all visible layers into the output buffer.
     *
     * Uses painter's order: layer 0 is the base; each subsequent layer
     * overwrites pixels that are not index 15 (transparent).
     * The raw PackedPixel4 buffer is walked for performance — no virtual
     * getPixel/setPixel calls inside the hot loop.
     */
    void composite() {
        const size_t BUF_SIZE = layers[0].getBufferSize();

        // Seed output from layer 0
        if (visible[0]) {
            memcpy(output.getBuffer(), layers[0].getBuffer(),
                   BUF_SIZE * sizeof(PackedPixel4));
        } else {
            output.clear(Pixel4(0));
        }

        // Painter's order: merge layers 1..N-1 onto output
        for (uint8_t l = 1; l < ENJIN_LAYER_COUNT; ++l) {
            if (!visible[l]) continue;

            const PackedPixel4* src = layers[l].getBuffer();
            PackedPixel4*       out = output.getBuffer();

            for (size_t i = 0; i < BUF_SIZE; ++i) {
                uint8_t src_byte = src[i].getByte();
                uint8_t out_byte = out[i].getByte();

                // Low nibble: pixels at even x coordinates
                uint8_t src_low = src_byte & 0x0F;
                if (src_low != 0x0F) {
                    out_byte = (out_byte & 0xF0) | src_low;
                }

                // High nibble: pixels at odd x coordinates
                uint8_t src_high = (src_byte >> 4) & 0x0F;
                if (src_high != 0x0F) {
                    out_byte = (out_byte & 0x0F) | (src_high << 4);
                }

                out[i] = PackedPixel4(out_byte);
            }
        }
    }

    // ============================================================
    // DIRTY-TILE PATH: restore sources, per-layer tint, presenter
    // ============================================================
    //
    // The frame loop a host drives:
    //   comp.beginFrame();          // restore last frame's tiles from source
    //   ... draw to comp.layers ... // primitives mark dirty tiles
    //   comp.compositeDirty();      // write only dirty tiles into output
    //   comp.present(presenter);    // hand the frame to the adapter (once)
    //   comp.endFrame();            // snapshot host draws, clear dirty
    //
    // A layer's restore source decides what its dirty tiles reset to before it
    // redraws: None (persist — a static layer, drawn once, settles to 0 tiles),
    // Backdrop (fill a colour — gfx.clear sugar) or a tilemap Callback.

    using Layer = Canvas4<W, H>;

    static constexpr uint16_t TILE_SIZE = Layer::TILE_SIZE;
    static constexpr uint16_t TILES_X = Layer::TILES_X;
    static constexpr uint16_t TILES_Y = Layer::TILES_Y;
    static constexpr uint16_t TILE_COUNT = Layer::TILE_COUNT;
    static constexpr size_t DIRTY_BYTES = (TILE_COUNT + 7) / 8;

    /// @brief What a layer's dirty tiles are restored to before it redraws.
    struct RestoreSource {
        enum class Kind : uint8_t { None, Backdrop, Callback };
        Kind kind = Kind::None;
        Pixel4 color = Pixel4(0);
        /// @brief Tilemap restore: paint tile (tx,ty) of `layer` from the source.
        void (*fn)(void* ctx, Layer& layer, uint16_t tx, uint16_t ty) = nullptr;
        void* ctx = nullptr;
    };

    /// Per-layer tint applied while compositing (identity by default).
    Remap tint[ENJIN_LAYER_COUNT];
    /// Per-layer restore source (None by default — persistent).
    RestoreSource restore[ENJIN_LAYER_COUNT];
    /// Tiles each layer's host drew last frame (drives this frame's restore).
    uint8_t restoreSet[ENJIN_LAYER_COUNT][DIRTY_BYTES];
    /// Tiles written into `output` this frame (the merged dirty bitmap).
    uint8_t mergedDirty[DIRTY_BYTES];
    /// Tiles forced to recomposite regardless of any layer's dirty state, e.g.
    /// after a visibility toggle. Merged then cleared each compositeDirty().
    uint8_t pendingDirty[DIRTY_BYTES];
    /// Palette handed to the presenter (may be null — adapter falls back).
    const Palette* palette = nullptr;

    /// @brief Reset the dirty-tile state (call once after construction/setup).
    /// Layer 0 is opaque black (the base); layers 1..N-1 are transparent (index
    /// 15) so an untouched upper layer lets lower layers show through.
    void resetDirtyState() {
        layers[0].clear(Pixel4(0));
        for (uint8_t l = 1; l < ENJIN_LAYER_COUNT; ++l) {
            layers[l].clear(Pixel4(15));
        }
        for (uint8_t l = 0; l < ENJIN_LAYER_COUNT; ++l) {
            tint[l] = Remap::identity();
            restore[l] = RestoreSource{};
            memset(restoreSet[l], 0, DIRTY_BYTES);
            layers[l].clearDirty();
        }
        memset(mergedDirty, 0, DIRTY_BYTES);
        memset(pendingDirty, 0, DIRTY_BYTES);
    }

    /// @brief Show or hide a layer. A visibility change forces the whole frame
    /// to recomposite next frame (the affected tiles must pull from the new set
    /// of visible layers), so route toggles through here, not `visible[]` raw.
    void setVisible(uint8_t layer, bool v) {
        if (layer >= ENJIN_LAYER_COUNT || visible[layer] == v) return;
        visible[layer] = v;
        memset(pendingDirty, 0xFF, DIRTY_BYTES);
    }

    /// @brief Set a layer's backdrop colour — the gfx.clear(c) sugar. Fills the
    /// layer now and records Backdrop as its restore source.
    void setBackdrop(uint8_t layer, Pixel4 color) {
        if (layer >= ENJIN_LAYER_COUNT) return;
        restore[layer].kind = RestoreSource::Kind::Backdrop;
        restore[layer].color = color;
        layers[layer].clear(color); // fills + marks every tile dirty
    }

    /// @brief Set a layer's restore source to a tilemap callback.
    void setRestoreCallback(uint8_t layer,
                            void (*fn)(void*, Layer&, uint16_t, uint16_t),
                            void* ctx) {
        if (layer >= ENJIN_LAYER_COUNT) return;
        restore[layer].kind = RestoreSource::Kind::Callback;
        restore[layer].fn = fn;
        restore[layer].ctx = ctx;
    }

    /// @brief Clear a layer's restore source (persist — no per-frame restore).
    void clearRestore(uint8_t layer) {
        if (layer >= ENJIN_LAYER_COUNT) return;
        restore[layer] = RestoreSource{};
    }

    /// @brief Set a layer's tint remap. Repaints the whole layer next composite.
    void setTint(uint8_t layer, const Remap& remap) {
        if (layer >= ENJIN_LAYER_COUNT) return;
        tint[layer] = remap;
        layers[layer].invalidateAll();
    }

    /// @brief Mark a pixel rect on a layer dirty (explicit invalidate).
    void invalidate(uint8_t layer, int16_t x, int16_t y, int16_t w, int16_t h) {
        if (layer >= ENJIN_LAYER_COUNT) return;
        layers[layer].invalidate(x, y, w, h);
    }

    /// @brief Mark a whole layer dirty.
    void invalidateLayer(uint8_t layer) {
        if (layer >= ENJIN_LAYER_COUNT) return;
        layers[layer].invalidateAll();
    }

    /// @brief Restore each layer's last-frame tiles from its restore source.
    /// Call at frame start, before host draws. The restore paint deliberately
    /// does NOT mark the layer dirty — restored tiles are tracked in
    /// restoreSet[l] and merged at composite time — so any external
    /// invalidation already marked (setBackdrop's seed fill, a mid-setup
    /// setTint) survives to this frame's composite, and a backdrop layer that
    /// stops changing settles to zero recomposited tiles.
    void beginFrame() {
        for (uint8_t l = 0; l < ENJIN_LAYER_COUNT; ++l) {
            restorePaint(l);
        }
    }

    /// @brief Write only the changed tiles into `output`, applying per-layer
    /// tint. Populates `mergedDirty` (the tiles present() will hand over).
    void compositeDirty() {
        // Merge each layer's contribution: restored tiles (restore != None) plus
        // this frame's host draws and external invalidations.
        memset(mergedDirty, 0, DIRTY_BYTES);
        for (size_t b = 0; b < DIRTY_BYTES; ++b) {
            mergedDirty[b] |= pendingDirty[b];
        }
        for (uint8_t l = 0; l < ENJIN_LAYER_COUNT; ++l) {
            if (!visible[l]) continue;
            const uint8_t* host = layers[l].dirtyBitmap();
            const bool restores = restore[l].kind != RestoreSource::Kind::None;
            for (size_t b = 0; b < DIRTY_BYTES; ++b) {
                mergedDirty[b] |= host[b];
                if (restores) {
                    mergedDirty[b] |= restoreSet[l][b];
                }
            }
        }
        memset(pendingDirty, 0, DIRTY_BYTES);

        // Recomposite each merged-dirty tile across all visible layers.
        for (uint16_t ty = 0; ty < TILES_Y; ++ty) {
            for (uint16_t tx = 0; tx < TILES_X; ++tx) {
                const uint16_t t = ty * TILES_X + tx;
                if ((mergedDirty[t >> 3] & (1u << (t & 7))) == 0) {
                    continue;
                }
                recompositeTile(tx, ty);
            }
        }
    }

    /// @brief Hand the composited frame to a presenter. Call once per frame.
    template <typename Presenter>
    void present(Presenter& presenter) {
        Frame<W, H> frame{&output, mergedDirty, TILES_X, TILES_Y, palette};
        presenter.present(frame);
    }

    /// @brief Snapshot host draws into restoreSet (next frame's restore) and
    /// clear the layers' dirty grids. Call at frame end, after present.
    void endFrame() {
        for (uint8_t l = 0; l < ENJIN_LAYER_COUNT; ++l) {
            memcpy(restoreSet[l], layers[l].dirtyBitmap(), DIRTY_BYTES);
            layers[l].clearDirty();
        }
    }

    /// @brief Count of tiles written into output on the last compositeDirty().
    uint16_t mergedDirtyCount() const {
        uint16_t count = 0;
        for (uint16_t ty = 0; ty < TILES_Y; ++ty) {
            for (uint16_t tx = 0; tx < TILES_X; ++tx) {
                const uint16_t t = ty * TILES_X + tx;
                if ((mergedDirty[t >> 3] & (1u << (t & 7))) != 0) ++count;
            }
        }
        return count;
    }

private:
    /// @brief Repaint the layer's restoreSet tiles from its restore source,
    /// without disturbing the layer's dirty grid (see beginFrame's rationale).
    void restorePaint(uint8_t l) {
        const RestoreSource& src = restore[l];
        if (src.kind == RestoreSource::Kind::None) {
            return;
        }
        // A tilemap callback paints through the normal (marking) primitives, so
        // snapshot the dirty grid and restore it afterwards to drop those marks.
        uint8_t saved[DIRTY_BYTES];
        const bool callback = (src.kind == RestoreSource::Kind::Callback);
        if (callback) {
            memcpy(saved, layers[l].dirtyBitmap(), DIRTY_BYTES);
        }
        for (uint16_t ty = 0; ty < TILES_Y; ++ty) {
            for (uint16_t tx = 0; tx < TILES_X; ++tx) {
                const uint16_t t = ty * TILES_X + tx;
                if ((restoreSet[l][t >> 3] & (1u << (t & 7))) == 0) {
                    continue;
                }
                if (src.kind == RestoreSource::Kind::Backdrop) {
                    layers[l].fillTileNoMark(tx, ty, src.color);
                } else if (callback && src.fn) {
                    src.fn(src.ctx, layers[l], tx, ty);
                }
            }
        }
        if (callback) {
            layers[l].setDirtyBitmap(saved);
        }
    }

    /// @brief Recomposite one 16×16 tile across all visible layers into output,
    /// applying each layer's tint. Source index 15 is transparent (the tint's
    /// entry 15 is ignored by the compositor).
    void recompositeTile(uint16_t tx, uint16_t ty) {
        const int16_t x0 = static_cast<int16_t>(tx * TILE_SIZE);
        const int16_t y0 = static_cast<int16_t>(ty * TILE_SIZE);
        const int16_t x1 = static_cast<int16_t>(x0 + TILE_SIZE > W ? W : x0 + TILE_SIZE);
        const int16_t y1 = static_cast<int16_t>(y0 + TILE_SIZE > H ? H : y0 + TILE_SIZE);
        for (int16_t py = y0; py < y1; ++py) {
            for (int16_t px = x0; px < x1; ++px) {
                uint8_t out = 0; // default background
                for (uint8_t l = 0; l < ENJIN_LAYER_COUNT; ++l) {
                    if (!visible[l]) continue;
                    const uint8_t s = layers[l].getPixel(px, py).value;
                    if (s == 0x0F) continue; // transparent source
                    out = tint[l].apply(s);
                }
                output.setPixel(px, py, Pixel4(out));
            }
        }
    }
};

} // namespace enjin2
