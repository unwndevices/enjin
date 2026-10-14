/**
 * @file shape_corpus.hpp
 * @brief The Shape bit-identity corpus: one scripted pet run, a layer CRC per
 *        frame (ADR-0016, Tomodachi #337)
 *
 * ADR-0016 makes a Shape group bit-identical on every host: fixed point, no
 * float, no UB. The corpus is how that is checked. It builds a fixed Look on
 * a pack and drives it through the tick seam (tick() then flush(), 30 fps)
 * with a fixed script, and CRCs the pet's layer after every frame. The native
 * suite, the WASM build and the device (Tomodachi #338) run this same code on
 * the same pack and compare against one committed golden file.
 *
 * **The fixed Look.** Piece 1 in every slot (the stand-in Star Kit), colours
 * body 9, accent 4, ink 0; feet line at (80, 140) on a 160×160 layer cleared
 * to 15 (transparent).
 *
 * **The script** (frame numbers; each phase ends with the jelly asleep):
 *
 * | phase   | frames    | input before the frame's tick                            |
 * |---------|-----------|----------------------------------------------------------|
 * | draw    | 0         | none: no tick, the first flush                           |
 * | rest    | 1..10     | none                                                     |
 * | kick    | 11..80    | 11: impulse at feet + (8, -40), (-500, 400) px/s         |
 * | drag    | 81..170   | 81..110: moveTo(80 + f - 80, 140), so to (110, 140)      |
 * | pull    | 171..270  | 171: grab at feet + (0, -20); 171..190: pull to the grab |
 * |         |           | point + (k, -2k), k = min(f - 170, 10); 191: release at  |
 * |         |           | (-300, 300) px/s                                         |
 * | breathe | 271..320  | 271: breathe(Breath{}) (3 s, 4 steps, 1 px, feet): two   |
 * |         |           | breath steps, the jelly asleep throughout                |
 *
 * Every frame is dt = 33334 µs. A frame's record is taken after its flush.
 * On the Star the jelly sleeps 20+ frames before each phase ends (run
 * shape_corpus_test with SHAPE_VERBOSE=1 for the margins).
 *
 * **The CRC** (shapeLayerCrc) is CRC-32/ISO-HDLC (the zlib / PNG one:
 * reflected polynomial 0xEDB88320, init 0xFFFFFFFF, final xor 0xFFFFFFFF)
 * over the layer's 25600 pixels, one byte each holding the palette index
 * (0..15), row by row from the top-left. It is the CRC the enjin Shape
 * goldens (shape_jelly_test, shape_grab_test) use. Check value: a blank
 * layer (all 15) is 0x85A04443, Python's zlib.crc32(bytes([15]) * 25600).
 */
#pragma once

#include "../graphics/asset_arena.hpp"
#include "../graphics/canvas.hpp"
#include "shape_pack_group.hpp"

#include <cstddef>
#include <cstdint>

namespace enjin2 {
namespace shape {

/// The corpus layer: the Shell's one resolution.
using ShapeCorpusLayer = Canvas4<160, 160>;

/// CRC-32/ISO-HDLC of the layer, a byte per pixel (see the file comment).
uint32_t shapeLayerCrc(const ShapeCorpusLayer& layer);

enum class ShapeCorpusPhase : uint8_t { Draw, Rest, Kick, Drag, Pull, Breathe };

/// "draw", "rest", "kick", "drag", "pull", "breathe": the golden file's names.
const char* shapeCorpusPhaseName(ShapeCorpusPhase p);

/// One frame of the corpus, after its flush.
struct ShapeCorpusFrame {
    uint16_t         index = 0;
    ShapeCorpusPhase phase = ShapeCorpusPhase::Draw;
    bool             redrew = false;  ///< flush() drew this frame.
    bool             asleep = false;  ///< The jelly, after the tick.
    uint32_t         crc = 0;         ///< shapeLayerCrc of the layer.

    // The frame's cost in the scratch clock's ticks (Tomodachi #338), all 0
    // without a clock. Not part of the golden line.
    uint32_t simTicks = 0;     ///< The tick (0 on frame 0, which does not tick).
    uint32_t rasterTicks = 0;  ///< The redraw less its paint (0 without a redraw).
    uint32_t paintTicks = 0;   ///< The redraw's pixel writes (0 without a redraw).

    /// Room for formatShapeCorpusFrame's longest line and its NUL.
    static constexpr size_t kLineMax = 32;
};

/**
 * @brief The frame as a golden-file line: `<frame> <phase> <CRC> <redraw> <asleep>`,
 *        e.g. `172 pull 0365399B 1 0` (decimal frame, the phase's name, the
 *        CRC as 8 upper-case hex digits, then 1/0 flags), no newline.
 * @return its length; 0 (and an empty string) when `n` is too small.
 */
size_t formatShapeCorpusFrame(const ShapeCorpusFrame& f, char* out, size_t n);

/**
 * @brief The scripted run, one frame per step(): a host can run it flat out
 *        (tests) or one frame per display frame (the device, timing each).
 *        Not copyable; holds the pet (keep it off small stacks: the group is
 *        a few KB).
 */
class ShapeCorpus {
public:
    static constexpr uint16_t kFrames = 321;
    static constexpr uint32_t kFrameMicros = 33334;

    ShapeCorpus() = default;
    ShapeCorpus(const ShapeCorpus&) = delete;
    ShapeCorpus& operator=(const ShapeCorpus&) = delete;

    /// Build the fixed Look from `pack` (it must outlive the corpus) in
    /// `arena`. False, with `err` set, when the pack lacks a piece of it.
    bool begin(ShapePack& pack, AssetArena& arena, ShapeError* err);

    /// Run the next frame onto `layer` (frame 0 clears it first) and report
    /// it in `out`, timed when `scratch.clock` is set (the CRC is not timed).
    /// False once all kFrames ran, before begin(), or when the
    /// script's grab finds nothing drawn (failed() then says so).
    bool step(ShapeCorpusLayer& layer, ShapeDrawScratch& scratch, ShapeCorpusFrame& out);

    /// The step() that stopped the run early: the grab missed the pet.
    bool failed() const { return failed_; }
    uint16_t frame() const { return next_; }
    const ShapePackGroup& pet() const { return pet_; }

private:
    ShapePackGroup pet_;
    int            grab_ = -1;
    FixedVec2      grabAt_{};
    uint16_t       next_ = 0;
    bool           begun_ = false;
    bool           failed_ = false;
};

}  // namespace shape
}  // namespace enjin2
