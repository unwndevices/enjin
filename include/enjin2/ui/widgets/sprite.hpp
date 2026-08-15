#pragma once

#include "../component.hpp"
#include "../reflect.hpp"
#include "../components.hpp"
#include "../system.hpp"
#include "../../core/types.hpp"
#include "../../graphics/canvas.hpp"
#include "../../graphics/sprite.hpp" // AnimMode (Once/Loop/PingPong)
#include <cstdint>

/**
 * @file sprite.hpp
 * @brief Animated grayscale sprite-sheet widget on the ui ECS (unwn #205)
 *
 * The animated sibling of @ref IconComponent: a borrowed sprite-sheet plane
 * (`.njn` v2, `cols`×`rows` frames laid out frame-major, one byte per pixel with
 * the icon transparent sentinel) plus a frame cursor driven two ways —
 *
 *  - **Autonomous playback**: when @ref fps > 0 the @ref SpriteSystem advances
 *    @ref frame with a delta-time accumulator per @ref AnimMode, reusing the
 *    legacy C_Sprite state machine (Once freezes on the last frame, Loop wraps,
 *    PingPong reverses at each end).
 *  - **Bindable frame**: with @ref fps <= 0 the system never advances, so an
 *    authored value — or one written each tick by a `frame` binding through the
 *    ParamRegistry resolver — stands as the current frame.
 *
 * `icon` stays the zero-cost static path; a sprite is only paid for when a scene
 * actually animates. Like the icon the sheet's cell size is intrinsic, so the
 * entity pairs the component with just a PositionComponent (top-left).
 */

namespace enjin2 {

/**
 * @brief Data-only state for an animated sprite-sheet widget
 *
 * Borrows a frame-major byte-per-pixel plane (values 0..15, transparent pixels
 * carry @ref matte) whose cell size and grid travel with it. The blit and the
 * animation advance are the @ref SpriteSystem's job; @ref sampleAt exposes the
 * per-frame pixel read as a pure seam. Runtime playback state (@ref accumSec,
 * @ref forward, @ref done) is deliberately *not* reflected — only the authored
 * fields serialize.
 */
struct SpriteComponent : public Component<SpriteComponent> {
    const uint8_t* bitmap = nullptr; ///< Borrowed sheet plane, frame-major (not owned)
    uint16_t width = 0;              ///< Cell (frame) width in pixels
    uint16_t height = 0;             ///< Cell (frame) height in pixels
    uint16_t cols = 1;              ///< Sheet columns
    uint16_t rows = 1;              ///< Sheet rows
    uint16_t frame = 0;             ///< Current frame index (bindable)
    float fps = 8.0f;              ///< Playback rate; <= 0 ⇒ frame is externally driven
    AnimMode mode = AnimMode::Loop; ///< Autonomous playback mode
    uint8_t matte = kTransparent;   ///< Pixel value treated as transparent
    bool visible = true;            ///< Skipped by the system when false

    /// @brief Sentinel pixel value the blit skips (Eisei's clear color, matches icon).
    static constexpr uint8_t kTransparent = 16;

    // Runtime playback state — not serialized (absent from the field list).
    float accumSec = 0.0f; ///< Accumulated seconds since the last frame advance
    bool forward = true;   ///< PingPong direction (true = forward)
    bool done = false;     ///< True once Once-mode playback has frozen

    /// @brief Total frames in the sheet (cols*rows).
    uint16_t frameCount() const { return static_cast<uint16_t>(cols * rows); }

    /// @brief Point at a new sheet plane and reset playback to frame 0.
    void load(const uint8_t* data, uint16_t w, uint16_t h, uint16_t c, uint16_t r) {
        bitmap = data;
        width = w;
        height = h;
        cols = c;
        rows = r;
        frame = 0;
        accumSec = 0.0f;
        forward = true;
        done = false;
    }

    /// @brief Show or hide the sprite.
    void setVisible(bool v) { visible = v; }

    /**
     * @brief Pixel value of the current frame at a cell coordinate (pure)
     * @param x Column in [0, width)
     * @param y Row in [0, height)
     * @return The stored value, or @ref matte for an out-of-range/absent pixel
     *
     * Frame N occupies the plane span [N*width*height, (N+1)*width*height); a
     * frame past the sheet's end clamps to the last frame.
     */
    uint8_t sampleAt(int x, int y) const {
        if (!bitmap || x < 0 || y < 0 || x >= width || y >= height) return matte;
        const uint16_t total = frameCount();
        if (total == 0) return matte;
        uint16_t fr = frame < total ? frame : static_cast<uint16_t>(total - 1);
        const size_t base = static_cast<size_t>(fr) * width * height;
        return bitmap[base + static_cast<size_t>(y) * width + x];
    }

    /// @brief Whether a cell coordinate paints in the current frame (pure).
    bool isOpaqueAt(int x, int y) const { return sampleAt(x, y) != matte; }

    /**
     * @brief Advance one frame per @ref mode (mutates frame/forward/done)
     *
     * The C_Sprite state machine, byte-for-byte: Once freezes on the last frame
     * (setting @ref done), Loop wraps modulo the frame count, PingPong reverses
     * direction at each end.
     */
    void advanceFrame() {
        const uint16_t total = frameCount();
        if (total == 0) return;
        switch (mode) {
        case AnimMode::Once:
            if (frame < total - 1) ++frame;
            else done = true; // freeze on last frame
            break;
        case AnimMode::Loop:
            frame = static_cast<uint16_t>((frame + 1) % total);
            break;
        case AnimMode::PingPong:
            if (forward) {
                if (frame < total - 1) {
                    ++frame;
                } else {
                    forward = false;
                    if (total > 1) --frame; // step back from the last frame
                }
            } else {
                if (frame > 0) {
                    --frame;
                } else {
                    forward = true;
                    ++frame; // step forward from the first frame
                }
            }
            break;
        }
    }
};

/// @brief Serializable properties of @ref SpriteComponent (see reflect.hpp).
/// The bitmap serializes as an AssetRegistry name reference (the content hash);
/// width/height/cols/rows/frame/fps/mode/matte/visible travel as plain fields.
#define ENJIN2_SPRITE_COMPONENT_FIELDS(FIELD, PROP) \
    FIELD(bitmap)                                   \
    FIELD(width)                                    \
    FIELD(height)                                   \
    FIELD(cols)                                     \
    FIELD(rows)                                     \
    FIELD(frame)                                    \
    FIELD(fps)                                       \
    FIELD(mode)                                     \
    FIELD(matte)                                    \
    FIELD(visible)

ENJIN2_REFLECT_COMPONENT(SpriteComponent, 13, "sprite", ENJIN2_SPRITE_COMPONENT_FIELDS)

/**
 * @brief Advances + blits every SpriteComponent entity to a Pixel4 canvas
 * @tparam TWorld World composing SpriteComponent and PositionComponent
 * @tparam TCanvas Pixel4 canvas type (e.g. Canvas4<W,H>)
 *
 * Each update: autonomous sprites (fps > 0) advance their frame with a
 * delta-time accumulator, then every visible sprite's current frame is blitted
 * at its entity position with matte-valued pixels left transparent. Off-canvas
 * pixels are clipped by the canvas itself.
 */
template<typename TWorld, typename TCanvas>
class SpriteSystem : public System<SpriteSystem<TWorld, TCanvas>> {
public:
    /**
     * @brief Construct against the world it draws and the canvas it draws to
     * @param world World holding the sprite entities (borrowed, not owned)
     * @param canvas Target Pixel4 canvas (borrowed, not owned)
     */
    SpriteSystem(TWorld* world, TCanvas* canvas) : world_(world), canvas_(canvas) {}

    /**
     * @brief Advance and blit every sprite
     * @param dt Time since last update in seconds (drives autonomous playback)
     */
    void update(float dt) override {
        if (!world_ || !canvas_) return;
        for (Entity e : world_->template query<SpriteComponent, PositionComponent>())
            drawEntity(e, dt);
    }

    /// @brief Advance + blit one sprite entity (the per-entity seam the scene
    /// player's z-sorted pass drives, unwn #243). Each sprite is visited once a
    /// frame, so its animation still advances exactly once per @p dt.
    void drawEntity(Entity e, float dt) {
        if (!world_ || !canvas_) return;
        auto* sprite = world_->template get<SpriteComponent>(e);
        auto* pos = world_->template get<PositionComponent>(e);
        if (!sprite || !pos) return;
        animate(*sprite, dt);
        draw(*sprite, *pos);
    }

    /// @brief Sprites render alongside icons/labels, above scene content.
    int getPriority() const override { return 900; }

private:
    // Delta-time accumulator: advance whole frames while the accumulator holds a
    // full frame period, carrying the sub-frame remainder (matches C_Sprite).
    void animate(SpriteComponent& s, float dt) {
        if (!s.bitmap || s.fps <= 0.0f || s.done || s.frameCount() == 0) return;
        s.accumSec += dt;
        const float frameSec = 1.0f / s.fps;
        while (s.accumSec >= frameSec) {
            s.accumSec -= frameSec;
            s.advanceFrame();
            if (s.done) break; // Once mode: stop advancing after the last frame
        }
    }

    void draw(const SpriteComponent& s, const PositionComponent& pos) {
        if (!s.visible || !s.bitmap) return;
        const Point origin = pos.renderOrigin(Size(s.width, s.height));
        const int originX = origin.x;
        const int originY = origin.y;
        for (int y = 0; y < s.height; ++y) {
            for (int x = 0; x < s.width; ++x) {
                const uint8_t v = s.sampleAt(x, y);
                if (v == s.matte) continue;
                canvas_->setPixel(static_cast<int16_t>(originX + x),
                                  static_cast<int16_t>(originY + y),
                                  Pixel4(static_cast<uint8_t>(v & 0x0F)));
            }
        }
    }

    TWorld* world_;
    TCanvas* canvas_;
};

} // namespace enjin2
