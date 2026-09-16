/**
 * @file layered_sprite.hpp
 * @brief Retained layered-sprite instance: reconstruction + animation control
 *        (ADR-0004, Tomodachi #96).
 *
 * A `LayeredSprite` binds to one immutable `LayeredAsset` (arena-owned, shared
 * by every instance over that asset) and adds the per-instance state an applet
 * controls: position, whole-object flips, the selected animation clip, and the
 * playback cursor. Source layers never become Enjin compositor layers:
 * @ref draw reconstructs the current frame by painting visible sprite parts in
 * source-layer painter order onto one caller-supplied layer.
 *
 * ## Reconstruction
 *
 * The object's position is the top-left of the **authored canvas**. Every
 * frame-part reference is offset from that origin, and every pixel is clipped
 * to the authored extent before it is placed, so @ref width / @ref height
 * always describe exactly the painted footprint — for every frame, no matter
 * which parts are visible. A whole-object flip mirrors parts and offsets around
 * that stable extent (not around each cropped image), so a cropped detail
 * flipped on a wider canvas lands where the authored assembly would mirror to.
 *
 * ## Animation control
 *
 * Clips come from the asset (`CLIP` chunk) plus a synthesized looping `default`
 * clip when the asset carries none. @ref update advances timed playback by the
 * authored frame durations with the selected clip's once/loop/ping-pong mode.
 * @ref setFrame selects a pose explicitly; @ref setProgress enters scrubbed
 * playback and maps a normalized `0..1` value across the clip weighted by
 * authored durations (endpoint-exact, no interpolation, monotone in either
 * direction). Calling @ref play resumes timed playback from the current frame,
 * including a frame reached by scrubbing. Frame events are **polled**
 * (@ref justAdvanced / @ref justCompleted / @ref frameEvent), are latched only
 * when the rendered frame actually changes, and are suppressed while scrubbing.
 *
 * The class never allocates and holds no asset bytes of its own. Bind only a
 * view obtained through `LayeredAssetStore::retain(handle)` and `release()` it
 * when the last sprite using it is gone; the store's arena is then free to
 * reclaim the bytes. Two sprites may share one asset with fully independent
 * position and animation state.
 */
#pragma once

#include "canvas.hpp"
#include "layered_asset.hpp"

#include <cstdint>

namespace enjin2 {

/**
 * @brief Retained instance over one immutable `LayeredAsset` (ADR-0004, #96).
 */
class LayeredSprite {
public:
    /// Sentinel returned by @ref selectedClip before an asset is bound.
    static constexpr uint16_t NO_CLIP = 0xFFFF;

    /// Number of bytes in a clip/part name, including the null terminator.
    static constexpr size_t NAME_BYTES = 16;

    LayeredSprite() = default;

    // ── Binding ──────────────────────────────────────────────────────────────

    /**
     * @brief Bind to an immutable asset and reset playback to its first clip.
     *
     * The asset must remain valid for the sprite's lifetime (the store's
     * `retain()`/`release()` protocol guarantees this). If the asset carries
     * clips the first non-empty one is selected; otherwise the synthesized
     * `default` clip is used. Playback starts paused on the first frame.
     */
    void bind(const LayeredAsset* asset);

    /// Release the binding and reset all instance state.
    void unbind();

    /// The bound immutable asset, or nullptr.
    const LayeredAsset* asset() const { return asset_; }

    /// Whether an asset is bound.
    bool isBound() const { return asset_ != nullptr; }

    // ── Position (places the asset's pivot anchor) ───────────────────────────
    //
    // The position is the screen coordinate the asset's static pivot is drawn
    // at. With the default pivot (0,0) this is the authored canvas top-left, so
    // an asset without an `LPIV` chunk positions exactly as before. A non-zero
    // pivot shifts the drawn frame so the pivot pixel lands on the position, and
    // a flip mirrors the pivot around the canvas extent (see @ref pivotX).

    void setPosition(int16_t x, int16_t y) { x_ = x; y_ = y; }
    int16_t positionX() const { return x_; }
    int16_t positionY() const { return y_; }

    /// Authored static pivot of the bound asset — the positioning anchor before
    /// flips. 0 when unbound; an absent `LPIV` chunk decodes to (0,0).
    int16_t pivotX() const { return asset_ != nullptr ? asset_->pivotX : 0; }
    int16_t pivotY() const { return asset_ != nullptr ? asset_->pivotY : 0; }

    /// Authored canvas width — stable for every frame. 0 when unbound.
    uint16_t width() const { return asset_ != nullptr ? asset_->canvasW : 0; }
    /// Authored canvas height — stable for every frame. 0 when unbound.
    uint16_t height() const { return asset_ != nullptr ? asset_->canvasH : 0; }

    // ── Whole-object flips (around the authored canvas extent) ───────────────

    void setHFlip(bool h) { hflip_ = h; }
    void setVFlip(bool v) { vflip_ = v; }
    void setFlip(bool h, bool v) { hflip_ = h; vflip_ = v; }
    bool getHFlip() const { return hflip_; }
    bool getVFlip() const { return vflip_; }

    // ── Clips ────────────────────────────────────────────────────────────────

    /// Number of addressable clips, including the synthesized `default` clip.
    uint16_t clipCount() const;

    /// Name of clip @p index, or nullptr when out of range.
    const char* clipName(uint16_t index) const;

    /// Index of the named clip (including `"default"`), or -1 when absent.
    int findClip(const char* name) const;

    /**
     * @brief Select a clip by name, resetting to its first frame.
     *
     * The playback mode is adopted from the clip's authored loop mode (the
     * default clip loops). Playback pause state is unchanged; call @ref play to
     * begin.
     * @return true if the clip was found.
     */
    bool selectClip(const char* name);

    /// Select a clip by index, resetting to its first frame. Out-of-range
    /// returns false and changes nothing.
    bool selectClip(uint16_t index);

    /// Selected clip index, or `NO_CLIP` before an asset is bound.
    uint16_t selectedClip() const;

    /// Selected clip name; nullptr before an asset is bound.
    const char* selectedClipName() const;

    /// Number of frames in the selected clip.
    uint16_t clipFrameCount() const;

    /// Current position within the selected clip's frame list.
    uint16_t clipFrame() const { return clipFrame_; }

    // ── Explicit frame selection ─────────────────────────────────────────────

    /**
     * @brief Select a pose by animation-frame index (clamped to the asset).
     *
     * Pauses timed playback and leaves scrubbed playback. When the pose appears
     * in the selected clip, the clip cursor moves to it so a later @ref play
     * resumes there; a pose the clip never references restarts the clip.
     */
    void setFrame(uint16_t animationFrame);

    /// Animation-frame index currently being rendered.
    uint16_t frame() const { return frame_; }

    // ── Timed playback ───────────────────────────────────────────────────────

    /// Override the selected clip's loop mode for timed playback.
    void setPlayMode(NjnLoopMode mode) { playMode_ = mode; }

    /// Current timed playback mode.
    NjnLoopMode playMode() const { return playMode_; }

    /// Resume timed playback from the current frame (leaves scrubbed playback).
    void play();

    /// Suspend timed playback, holding the current frame and elapsed time.
    void pause() { paused_ = true; }

    /// True while timed playback is running (not paused and not done).
    bool isPlaying() const { return !paused_ && !done_ && !scrubbing_; }

    /// True while timed playback is suspended.
    bool isPaused() const { return paused_; }

    /// True once a `Once` clip has frozen on its terminal frame.
    bool isDone() const { return done_; }

    // ── Scrubbed playback ────────────────────────────────────────────────────

    /**
     * @brief Enter scrubbed playback, selecting a clip frame from `0..1`.
     *
     * `0` selects the first clip frame and `1` the last; intermediates are
     * weighted by authored durations with no interpolation, so increasing and
     * decreasing values traverse the clip in either direction. Timed
     * advancement is suspended until @ref play. Frame events are suppressed.
     */
    void setProgress(float progress);

    /// Last normalized progress supplied to @ref setProgress.
    float progress() const { return progress_; }

    /// True while control is normalized-progress rather than timed.
    bool isScrubbing() const { return scrubbing_; }

    // ── Tick ─────────────────────────────────────────────────────────────────

    /// Advance timed playback by @p dtSeconds. No-op while paused/scrubbing/done.
    void update(float dtSeconds);

    // ── Polled frame events (valid for the tick that produced them) ──────────

    /// True on the tick the rendered animation frame changed.
    bool justAdvanced() const { return justAdvanced_; }

    /// True on the tick a once clip finished or a loop/ping-pong clip wrapped.
    bool justCompleted() const { return justCompleted_; }

    /// Event id of a frame entered this tick (0 = none / no advance).
    uint8_t frameEvent() const { return frameEvent_; }

    // ── Reconstruction ───────────────────────────────────────────────────────

    /// Reconstruct the current frame at the object's position onto @p target.
    void draw(ICanvas<Pixel4>& target) const;

private:
    /// Bind an asset and an addressable clip index.
    void rebind(const LayeredAsset* asset, int clipIndex);

    /// The selected named clip, or nullptr for the synthesized default/unbound.
    const LayeredClip* activeClip() const;

    /// Clip frame count for the active clip (named or synthesized).
    uint16_t activeFrameCount() const;

    /// Animation-frame index of clip position @p pos.
    uint16_t activeFrameIndex(uint16_t pos) const;

    /// Authored duration (ms) of clip position @p pos.
    uint16_t activeDuration(uint16_t pos) const;

    /// Event id of clip position @p pos (always 0 for the default clip).
    uint8_t activeEventId(uint16_t pos) const;

    /// Reset the cursor/event state and sync the rendered frame to position 0.
    void resetPlaybackState();

    /// Advance the clip cursor once, latching polled events.
    void advanceClipCursor();

    /// Sync the rendered frame to the clip cursor, latching its event only when
    /// the rendered frame actually changed since @p previousFrame.
    void emitCurrent(uint16_t previousFrame);

    /// Reconstruct the current frame at an explicit origin.
    void blit(ICanvas<Pixel4>& target, int16_t originX, int16_t originY) const;

    const LayeredAsset* asset_ = nullptr;
    int16_t x_ = 0;
    int16_t y_ = 0;
    bool hflip_ = false;
    bool vflip_ = false;

    int clipIndex_ = 0;          ///< Addressable clip index (named or default).
    uint16_t clipFrame_ = 0;     ///< Position in the selected clip's frame list.
    uint16_t frame_ = 0;         ///< Rendered animation frame.
    NjnLoopMode playMode_ = NjnLoopMode::Loop;

    bool paused_ = true;
    bool done_ = false;
    bool forward_ = true;
    bool scrubbing_ = false;
    float accumMs_ = 0.0f;
    float progress_ = 0.0f;

    bool justAdvanced_ = false;
    bool justCompleted_ = false;
    uint8_t frameEvent_ = 0;
};

} // namespace enjin2
