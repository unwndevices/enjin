/**
 * @file sprite.hpp
 * @brief Sprite component with SpriteSheet and clip-driven frame animation
 *
 * Provides ECS integration for bitmap sprite rendering. Two playback models
 * coexist:
 *
 *   - **Clips (ADR-0003 §5)** — the retained model. A clip is a per-frame list
 *     `{frameIndex, durationMs, eventId}` plus a loop mode (Once / Loop /
 *     PingPong), decoded from a `.njn` v2 CLIP chunk (@ref NjnClip). play() a
 *     clip by name; lateUpdate() advances it by real per-frame durations, not a
 *     single sheet FPS. Frame events are **polled** — justAdvanced(),
 *     justCompleted(), frameEvent() — never callbacks.
 *   - **Legacy whole-sheet FPS** — the original model, kept for back-compat
 *     (setFPS / setMode / setFrame). Active only while no clip is playing.
 *
 * Flips (hflip / vflip) go through the flip-aware @ref SpriteSheet::draw; there
 * is deliberately **no runtime rot90** — rotation is authored frames, scrubbed
 * by angle via setFrameForAngle() (the flipper) or `<base>_<NN>` facings.
 *
 * Transparency: palette index 15 is always skipped at blit time.
 */
#pragma once
#include "drawable.hpp"
#include "../graphics/sprite.hpp"
#include "../graphics/njn2.hpp"
#include "../core/object.hpp"
#include <vector>
#include <cstring>
#include <cmath>

namespace enjin2 {

/**
 * @brief Retained sprite component: SpriteSheet + clip animation (ADR-0003 §5).
 *
 * Attach from Lua with `obj:add("C_Sprite")` (the sole attach verb, §2). The
 * default width/height are 0 and are refreshed from the sheet cell size in
 * setSheet(), so the registry's argument-free `addComponent<C_Sprite>()` works.
 */
class C_Sprite : public C_Drawable {
public:
    /**
     * @param owner  The object that owns this component.
     * @param width  Drawable width (defaults to 0; refreshed by setSheet()).
     * @param height Drawable height (defaults to 0; refreshed by setSheet()).
     */
    C_Sprite(Object* owner, uint8_t width = 0, uint8_t height = 0)
        : C_Drawable(owner, width, height)
        , _sheet()
        , _fps(8.0f)
        , _accumSec(0.0f)
        , _frame(0)
        , _mode(AnimMode::Loop)
        , _forward(true)
        , _done(false)
    {}

    // ── Sheet ────────────────────────────────────────────────────────────────

    /** Replace the sprite sheet. Resets frame + animation state and adopts the
     *  sheet's cell size as the drawable width/height.
     *  @param sheet New sprite sheet to use */
    void setSheet(const SpriteSheet& sheet) {
        _sheet = sheet;
        _frame = 0;
        _accumSec = 0.0f;
        _accumMs = 0.0f;
        _forward = true;
        _done = false;
        _scrub = false;
        _clipIndex = -1;   // stop any clip playback; setClips()+play() re-arm it
        _clipFrame = 0;
        if (sheet.cellW) width = sheet.cellW;
        if (sheet.cellH) height = sheet.cellH;
    }

    // ── Clips (ADR-0003 §5) ───────────────────────────────────────────────────

    /** Replace the clip table (copied in). Does not start playback — call
     *  play() to begin. A sprite with clips set but none playing renders its
     *  current frame statically.
     *  @param clips Decoded clips (e.g. from njn2DecodeClip). */
    void setClips(const std::vector<NjnClip>& clips) {
        _clips = clips;
        _clipIndex = -1;   // nothing playing until play()
    }

    /** Number of loaded clips. */
    size_t clipCount() const { return _clips.size(); }

    /** Start playing a named clip from its first frame. Clears scrub mode and
     *  the done flag. No-op returning false if the name is unknown or the clip
     *  is empty.
     *  @param name Clip name (matched against NjnClip::name).
     *  @return true if the clip was found and started. */
    bool play(const char* name) {
        if (!name) return false;
        for (size_t i = 0; i < _clips.size(); ++i) {
            if (std::strncmp(_clips[i].name, name, sizeof(_clips[i].name)) == 0) {
                if (_clips[i].frames.empty()) return false;
                _clipIndex = static_cast<int>(i);
                _clipFrame = 0;
                _accumMs   = 0.0f;
                _forward   = true;
                _done      = false;
                _scrub     = false;
                _frame     = _clips[i].frames[0].frameIndex;
                _justAdvanced = false;
                _justCompleted = false;
                _frameEvent = 0;
                return true;
            }
        }
        return false;
    }

    /** Name of the clip currently playing, or nullptr if none. */
    const char* currentClip() const {
        return (_clipIndex >= 0) ? _clips[static_cast<size_t>(_clipIndex)].name : nullptr;
    }

    // ── Polled clip events (ADR-0003 §5) ──────────────────────────────────────
    // All three reflect the most recent lateUpdate() tick and reset at the top
    // of the next one. Poll them after update, never register a callback.

    /** True on the tick the clip frame changed. */
    bool justAdvanced() const { return _justAdvanced; }
    /** True on the tick the clip reached the end of a cycle (Once freeze on the
     *  last frame; Loop/PingPong on wrap). */
    bool justCompleted() const { return _justCompleted; }
    /** The eventId of the frame entered this tick (0 = none / no advance). */
    uint8_t frameEvent() const { return _frameEvent; }

    // ── Flips (ADR-0003 §5 — flip-aware SpriteSheet::draw, no runtime rot90) ──

    void setHFlip(bool h) { _hflip = h; }
    void setVFlip(bool v) { _vflip = v; }
    void setFlip(bool h, bool v) { _hflip = h; _vflip = v; }
    bool getHFlip() const { return _hflip; }
    bool getVFlip() const { return _vflip; }

    // ── Scrub-by-angle (ADR-0003 §5 — the flipper) ────────────────────────────

    /** Drive the frame directly from an angle instead of a clip timer. Maps
     *  @p angle linearly across the frames of the active clip (or the whole
     *  sheet if none), clamped to `[minAngle, maxAngle]`, and enters scrub mode
     *  so lateUpdate() no longer auto-advances. Call play() to return to timed
     *  playback.
     *  @param angle    Current angle (any unit; must match min/max).
     *  @param minAngle Angle mapped to the first frame.
     *  @param maxAngle Angle mapped to the last frame. */
    void setFrameForAngle(float angle, float minAngle, float maxAngle) {
        const uint16_t n = clipOrSheetFrameCount();
        if (n == 0) return;
        _scrub = true;
        _done = false;
        float t = 0.0f;
        if (maxAngle != minAngle) {
            t = (angle - minAngle) / (maxAngle - minAngle);
        }
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        const uint16_t idx = static_cast<uint16_t>(std::lround(t * (n - 1)));
        if (_clipIndex >= 0) {
            _clipFrame = idx;
            _frame = _clips[static_cast<size_t>(_clipIndex)].frames[idx].frameIndex;
        } else {
            _frame = idx;
        }
    }

    // ── Legacy whole-sheet FPS animation (back-compat) ────────────────────────

    /** Set frames-per-second playback rate for the legacy (no-clip) path. */
    void setFPS(float fps) { _fps = fps; }

    /** Set legacy loop mode. */
    void setMode(AnimMode mode) {
        _mode = mode;
        _done = false;
        _forward = true;
    }

    /** Directly set the current sheet frame. Clamped to [0, frameCount-1].
     *  Leaves any active clip's bookkeeping alone (used for manual control). */
    void setFrame(uint16_t index) {
        const uint16_t total = _sheet.frameCount();
        if (total == 0) return;
        _frame = (index >= total) ? static_cast<uint16_t>(total - 1) : index;
        _accumSec = 0.0f;
    }

    /** Current sheet cell index being drawn. */
    uint16_t getFrame() const { return _frame; }

    /** True when a Once animation (clip or legacy) has frozen on its last frame. */
    bool isDone() const { return _done; }

    // ── Rendering ─────────────────────────────────────────────────────────────

    void draw(ICanvas<Pixel4>& canvas) override {
        if (!is_visible || !_sheet.data) return;
        Point pos = GetOffsetPosition();
        if (_hflip || _vflip) {
            _sheet.draw(canvas, _frame, pos.x, pos.y, _hflip, _vflip, Remap::identity());
        } else {
            _sheet.draw(canvas, _frame, pos.x, pos.y);
        }
    }

    void lateUpdate(float dt) override {
        // Polled events reflect only this tick.
        _justAdvanced = false;
        _justCompleted = false;
        _frameEvent = 0;

        if (!_sheet.data || _scrub) return;

        if (_clipIndex >= 0) {
            updateClip(dt);
        } else {
            updateLegacy(dt);
        }
    }

    bool continueToDraw() const override {
        return !owner->isQueuedForRemoval();
    }

private:
    SpriteSheet _sheet;    ///< Sprite sheet (value copy; caller owns pixel data lifetime)
    float       _fps;      ///< Legacy frames per second
    float       _accumSec; ///< Legacy seconds accumulator
    uint16_t    _frame;    ///< Current sheet cell index being drawn
    AnimMode    _mode;     ///< Legacy loop mode
    bool        _forward;  ///< Ping-pong direction flag (true = forward)
    bool        _done;     ///< True when a Once animation has frozen

    // Clip state
    std::vector<NjnClip> _clips;      ///< Owned clip table
    int         _clipIndex = -1;      ///< Active clip (-1 = legacy FPS path)
    uint16_t    _clipFrame = 0;       ///< Index INTO the active clip's frame list
    float       _accumMs   = 0.0f;    ///< Clip millisecond accumulator

    // Flip + scrub
    bool _hflip = false;
    bool _vflip = false;
    bool _scrub = false;              ///< setFrameForAngle disables time advance

    // Polled events (valid for the tick that produced them)
    bool    _justAdvanced  = false;
    bool    _justCompleted = false;
    uint8_t _frameEvent    = 0;

    /// Frame count of the active clip, or the whole sheet if none.
    uint16_t clipOrSheetFrameCount() const {
        if (_clipIndex >= 0) {
            return static_cast<uint16_t>(_clips[static_cast<size_t>(_clipIndex)].frames.size());
        }
        return _sheet.frameCount();
    }

    void updateClip(float dt) {
        const NjnClip& clip = _clips[static_cast<size_t>(_clipIndex)];
        const uint16_t n = static_cast<uint16_t>(clip.frames.size());
        if (n == 0 || _done) return;

        _accumMs += dt * 1000.0f;

        // Advance as many frames as the elapsed time covers. A zero-duration
        // frame holds forever (no auto-advance) rather than spinning.
        for (;;) {
            const uint16_t dur = clip.frames[_clipFrame].durationMs;
            if (dur == 0 || _accumMs < static_cast<float>(dur)) break;
            _accumMs -= static_cast<float>(dur);
            advanceClipFrame(clip, n);
            if (_done) break;  // Once mode froze on the last frame
        }
    }

    /// Step the clip cursor once per its loop mode, latching poll events.
    void advanceClipFrame(const NjnClip& clip, uint16_t n) {
        switch (clip.loopMode) {
            case NjnLoopMode::Once:
                if (_clipFrame + 1 < n) {
                    ++_clipFrame;
                    emitFrame(clip);
                } else {
                    _done = true;
                    _justCompleted = true;  // reached the end; freeze on last frame
                }
                break;
            case NjnLoopMode::Loop:
                if (_clipFrame + 1 < n) {
                    ++_clipFrame;
                } else {
                    _clipFrame = 0;
                    _justCompleted = true;  // wrapped a full cycle
                }
                emitFrame(clip);
                break;
            case NjnLoopMode::PingPong:
                if (n <= 1) { emitFrame(clip); break; }
                if (_forward) {
                    if (_clipFrame + 1 < n) {
                        ++_clipFrame;
                    } else {
                        _forward = false;
                        --_clipFrame;  // step back from the last frame
                    }
                } else {
                    if (_clipFrame > 0) {
                        --_clipFrame;
                        if (_clipFrame == 0) _justCompleted = true;  // back to start
                    } else {
                        _forward = true;
                        ++_clipFrame;
                    }
                }
                emitFrame(clip);
                break;
        }
    }

    /// Point the sheet cursor + poll fields at the current clip frame.
    void emitFrame(const NjnClip& clip) {
        const NjnFrameEntry& f = clip.frames[_clipFrame];
        _frame = f.frameIndex;
        _frameEvent = f.eventId;
        _justAdvanced = true;
    }

    void updateLegacy(float dt) {
        if (_fps <= 0.0f || _done) return;
        _accumSec += dt;
        const float frameSec = 1.0f / _fps;
        while (_accumSec >= frameSec) {
            _accumSec -= frameSec;
            advanceLegacyFrame();
            if (_done) break;
        }
    }

    void advanceLegacyFrame() {
        const uint16_t total = _sheet.frameCount();
        if (total == 0) return;
        switch (_mode) {
            case AnimMode::Once:
                if (_frame < total - 1) {
                    ++_frame;
                } else {
                    _done = true;
                }
                break;
            case AnimMode::Loop:
                _frame = static_cast<uint16_t>((_frame + 1) % total);
                break;
            case AnimMode::PingPong:
                if (_forward) {
                    if (_frame < total - 1) {
                        ++_frame;
                    } else {
                        _forward = false;
                        if (total > 1) --_frame;
                    }
                } else {
                    if (_frame > 0) {
                        --_frame;
                    } else {
                        _forward = true;
                        ++_frame;
                    }
                }
                break;
        }
    }
};

} // namespace enjin2
