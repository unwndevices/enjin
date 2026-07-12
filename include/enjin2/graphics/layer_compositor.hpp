#pragma once

#include "canvas.hpp"
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
};

} // namespace enjin2
