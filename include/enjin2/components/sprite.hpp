/**
 * @file sprite.hpp
 * @brief Sprite component with SpriteSheet and frame animation
 *
 * Provides ECS integration for bitmap sprite rendering with a full
 * animation state machine supporting Once, Loop, and PingPong modes.
 * Based on original Enjin C_Sprite, updated for the SpriteSheet API.
 */
#pragma once
#include "drawable.hpp"
#include "../graphics/sprite.hpp"
#include "../core/object.hpp"

namespace enjin2 {

/**
 * @brief Sprite component with SpriteSheet and frame animation.
 *
 * Wraps a SpriteSheet and drives frame animation automatically via
 * lateUpdate(). Supports Once, Loop, and PingPong animation modes.
 *
 * Transparency: palette index 15 is always skipped at blit time (compile-time constant).
 */
class C_Sprite : public C_Drawable {
public:
    /**
     * @param owner The object that owns this component
     * @param width  Drawable width (passed to C_Drawable for sort/anchor math)
     * @param height Drawable height
     */
    C_Sprite(Object* owner, uint8_t width, uint8_t height)
        : C_Drawable(owner, width, height)
        , _sheet()
        , _fps(8.0f)
        , _accumMs(0.0f)
        , _frame(0)
        , _mode(AnimMode::Loop)
        , _forward(true)
        , _done(false)
    {}

    /** Replace the sprite sheet. Resets frame to 0 and animation state. */
    void setSheet(const SpriteSheet& sheet) {
        _sheet = sheet;
        _frame = 0;
        _accumMs = 0.0f;
        _forward = true;
        _done = false;
    }

    /** Set frames-per-second playback rate. Must be > 0. */
    void setFPS(float fps) { _fps = fps; }

    /** Set animation loop mode. */
    void setMode(AnimMode mode) {
        _mode = mode;
        _done = false;
        _forward = true;
    }

    /** Directly set the current frame. Clamped to valid range [0, frameCount-1]. */
    void setFrame(uint8_t index) {
        const uint8_t total = _sheet.frameCount();
        if (total == 0) return;
        _frame = (index >= total) ? static_cast<uint8_t>(total - 1) : index;
        _accumMs = 0.0f;
    }

    /** Get the current frame index. */
    uint8_t getFrame() const { return _frame; }

    /** True when Once mode animation has completed (frozen on last frame). */
    bool isDone() const { return _done; }

    /**
     * @brief Draw the current frame to canvas at the component's position.
     *
     * Uses GetOffsetPosition() from C_Drawable for position plumbing.
     * Skips draw if not visible or sheet has no data.
     */
    void draw(ICanvas<Pixel4>& canvas) override {
        if (!is_visible || !_sheet.data) return;
        Point pos = GetOffsetPosition();
        _sheet.draw(canvas, _frame, pos.x, pos.y);
    }

    /**
     * @brief Advance animation by deltaTimeMs milliseconds.
     *
     * Uses delta-time accumulator: accumulator += dt, advance when >= frame_duration.
     * Subtracts frame duration rather than zeroing to preserve carry-over.
     */
    void lateUpdate(uint16_t deltaTimeMs) override {
        if (!_sheet.data || _fps <= 0.0f || _done) return;
        _accumMs += static_cast<float>(deltaTimeMs);
        const float frameMs = 1000.0f / _fps;
        while (_accumMs >= frameMs) {
            _accumMs -= frameMs;  // preserve sub-frame carry-over
            advanceFrame();
            if (_done) break;    // Once mode: stop advancing after last frame
        }
    }

    bool continueToDraw() const override {
        return !owner->isQueuedForRemoval();
    }

private:
    SpriteSheet _sheet;   ///< Sprite sheet data (value copy, caller owns pixel data lifetime)
    float       _fps;     ///< Frames per second
    float       _accumMs; ///< Accumulated milliseconds since last frame advance
    uint8_t     _frame;   ///< Current frame index
    AnimMode    _mode;    ///< Animation loop mode
    bool        _forward; ///< Ping-pong direction flag (true = forward)
    bool        _done;    ///< True when Once mode has completed

    void advanceFrame() {
        const uint8_t total = _sheet.frameCount();
        if (total == 0) return;
        switch (_mode) {
            case AnimMode::Once:
                if (_frame < total - 1) {
                    ++_frame;
                } else {
                    _done = true;  // freeze on last frame
                }
                break;
            case AnimMode::Loop:
                _frame = static_cast<uint8_t>((_frame + 1) % total);
                break;
            case AnimMode::PingPong:
                if (_forward) {
                    if (_frame < total - 1) {
                        ++_frame;
                    } else {
                        _forward = false;
                        if (total > 1) --_frame;  // step back from last
                    }
                } else {
                    if (_frame > 0) {
                        --_frame;
                    } else {
                        _forward = true;
                        ++_frame;  // step forward from first
                    }
                }
                break;
        }
    }
};

} // namespace enjin2
