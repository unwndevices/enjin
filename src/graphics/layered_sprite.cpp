#include "../../include/enjin2/graphics/layered_sprite.hpp"

#include <cstring>

namespace enjin2 {

// ---------------------------------------------------------------------------
// Binding
// ---------------------------------------------------------------------------

void LayeredSprite::bind(const LayeredAsset* asset) {
    int clipIndex = 0;
    if (asset != nullptr && asset->numClips > 0) {
        for (uint16_t i = 0; i < asset->numClips; ++i) {
            if (asset->clips[i].numFrames > 0) {
                clipIndex = static_cast<int>(i);
                break;
            }
        }
    }
    rebind(asset, clipIndex);
}

void LayeredSprite::unbind() {
    asset_ = nullptr;
    clipIndex_ = 0;
    playMode_ = NjnLoopMode::Loop;
    paused_ = true;
    progress_ = 0.0f;
    resetPlaybackState();
}

void LayeredSprite::rebind(const LayeredAsset* asset, int clipIndex) {
    unbind();
    asset_ = asset;
    clipIndex_ = clipIndex;
    if (asset_ != nullptr && clipIndex >= 0 && clipIndex < asset_->numClips) {
        playMode_ = asset_->clips[clipIndex].loopMode;
    }
    resetPlaybackState();
}

// ---------------------------------------------------------------------------
// Clip queries
// ---------------------------------------------------------------------------

uint16_t LayeredSprite::clipCount() const {
    if (asset_ == nullptr) return 0;
    if (asset_->numClips > 0) return asset_->numClips;
    return asset_->numFrames > 0 ? 1 : 0;  // the synthesized default clip
}

const char* LayeredSprite::clipName(uint16_t index) const {
    if (asset_ == nullptr) return nullptr;
    if (index < asset_->numClips) return asset_->clips[index].name;
    if (index == asset_->numClips && asset_->numClips == 0 && asset_->numFrames > 0) {
        return "default";
    }
    return nullptr;
}

int LayeredSprite::findClip(const char* name) const {
    if (asset_ == nullptr || name == nullptr) return -1;
    for (uint16_t i = 0; i < asset_->numClips; ++i) {
        if (std::strncmp(asset_->clips[i].name, name, NAME_BYTES - 1) == 0) {
            return static_cast<int>(i);
        }
    }
    if (asset_->numClips == 0 && asset_->numFrames > 0 &&
        std::strncmp(name, "default", NAME_BYTES - 1) == 0) {
        return 0;
    }
    return -1;
}

bool LayeredSprite::selectClip(const char* name) {
    const int index = findClip(name);
    if (index < 0) return false;
    return selectClip(static_cast<uint16_t>(index));
}

bool LayeredSprite::selectClip(uint16_t index) {
    if (asset_ == nullptr || index >= clipCount()) return false;
    const LayeredClip* clip = (index < asset_->numClips) ? &asset_->clips[index] : nullptr;
    if (clip != nullptr && clip->numFrames == 0) return false;
    clipIndex_ = static_cast<int>(index);
    playMode_ = (clip != nullptr) ? clip->loopMode : NjnLoopMode::Loop;
    resetPlaybackState();
    return true;
}

uint16_t LayeredSprite::selectedClip() const {
    if (asset_ == nullptr) return NO_CLIP;
    return static_cast<uint16_t>(clipIndex_);
}

const char* LayeredSprite::selectedClipName() const {
    if (asset_ == nullptr) return nullptr;
    const LayeredClip* clip = activeClip();
    return (clip != nullptr) ? clip->name : "default";
}

uint16_t LayeredSprite::clipFrameCount() const {
    return activeFrameCount();
}

// ---------------------------------------------------------------------------
// Explicit frame selection
// ---------------------------------------------------------------------------

void LayeredSprite::setFrame(uint16_t animationFrame) {
    if (asset_ == nullptr || asset_->numFrames == 0) return;
    if (animationFrame >= asset_->numFrames) animationFrame = asset_->numFrames - 1;
    frame_ = animationFrame;
    paused_ = true;
    scrubbing_ = false;
    done_ = false;
    forward_ = true;
    accumMs_ = 0.0f;

    // Move the clip cursor onto the pose when the clip references it, so a
    // later play() resumes from this frame. A pose outside the clip restarts it.
    const uint16_t n = activeFrameCount();
    bool found = false;
    for (uint16_t i = 0; i < n; ++i) {
        if (activeFrameIndex(i) == animationFrame) {
            clipFrame_ = i;
            found = true;
            break;
        }
    }
    if (!found) clipFrame_ = 0;

    justAdvanced_ = false;
    justCompleted_ = false;
    frameEvent_ = 0;
}

// ---------------------------------------------------------------------------
// Timed playback
// ---------------------------------------------------------------------------

void LayeredSprite::play() {
    if (asset_ == nullptr || activeFrameCount() == 0) return;
    if (done_) {
        clipFrame_ = 0;
        accumMs_ = 0.0f;
        forward_ = true;
    }
    paused_ = false;
    scrubbing_ = false;
    done_ = false;
    justAdvanced_ = false;
    justCompleted_ = false;
    frameEvent_ = 0;  // events fire on frame changes, not on (re)starting the clock
    frame_ = activeFrameIndex(clipFrame_);
}

// ---------------------------------------------------------------------------
// Scrubbed playback
// ---------------------------------------------------------------------------

void LayeredSprite::setProgress(float progress) {
    if (asset_ == nullptr) return;
    const uint16_t n = activeFrameCount();
    if (n == 0) return;

    float t = progress;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    uint16_t pos = 0;
    if (n > 1) {
        uint64_t total = 0;
        for (uint16_t i = 0; i < n; ++i) total += activeDuration(i);
        if (total == 0) {
            pos = (t <= 0.0f) ? 0 : (t >= 1.0f) ? static_cast<uint16_t>(n - 1)
                                                 : static_cast<uint16_t>(t * (n - 1) + 0.5f);
        } else if (t <= 0.0f) {
            pos = 0;
        } else if (t >= 1.0f) {
            pos = static_cast<uint16_t>(n - 1);
        } else {
            const double target = static_cast<double>(t) * static_cast<double>(total);
            uint64_t cum = 0;
            pos = static_cast<uint16_t>(n - 1);
            for (uint16_t i = 0; i < n; ++i) {
                cum += activeDuration(i);
                if (target < static_cast<double>(cum)) {
                    pos = i;
                    break;
                }
            }
        }
    }

    clipFrame_ = pos;
    frame_ = activeFrameIndex(pos);
    progress_ = t;
    scrubbing_ = true;
    paused_ = true;
    done_ = false;
    forward_ = true;
    accumMs_ = 0.0f;
    justAdvanced_ = false;
    justCompleted_ = false;
    frameEvent_ = 0;  // events are suppressed while scrubbing
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void LayeredSprite::update(float dtSeconds) {
    justAdvanced_ = false;
    justCompleted_ = false;
    frameEvent_ = 0;

    if (asset_ == nullptr) return;
    const uint16_t n = activeFrameCount();
    if (n == 0) return;
    if (paused_ || scrubbing_ || done_) return;

    accumMs_ += dtSeconds * 1000.0f;

    for (;;) {
        const uint16_t dur = activeDuration(clipFrame_);
        if (dur == 0 || accumMs_ < static_cast<float>(dur)) break;
        accumMs_ -= static_cast<float>(dur);
        advanceClipCursor();
        if (done_) break;
    }
}

void LayeredSprite::advanceClipCursor() {
    const uint16_t n = activeFrameCount();
    if (n == 0) return;
    const uint16_t previousFrame = frame_;

    switch (playMode_) {
        case NjnLoopMode::Once:
            if (clipFrame_ + 1 < n) {
                ++clipFrame_;
                emitCurrent(previousFrame);
            } else {
                done_ = true;
                justCompleted_ = true;
            }
            break;
        case NjnLoopMode::Loop:
            if (clipFrame_ + 1 < n) {
                ++clipFrame_;
            } else {
                clipFrame_ = 0;
                justCompleted_ = true;
            }
            emitCurrent(previousFrame);
            break;
        case NjnLoopMode::PingPong:
            if (n <= 1) {
                emitCurrent(previousFrame);
                break;
            }
            if (forward_) {
                if (clipFrame_ + 1 < n) {
                    ++clipFrame_;
                } else {
                    forward_ = false;
                    --clipFrame_;
                }
            } else {
                if (clipFrame_ > 0) {
                    --clipFrame_;
                } else {
                    forward_ = true;
                    ++clipFrame_;
                }
            }
            // A full there-and-back cycle ends when we land back on frame 0.
            if (clipFrame_ == 0) justCompleted_ = true;
            emitCurrent(previousFrame);
            break;
    }
}

void LayeredSprite::emitCurrent(uint16_t previousFrame) {
    frame_ = activeFrameIndex(clipFrame_);
    const uint8_t ev = activeEventId(clipFrame_);
    if (ev != 0) frameEvent_ = ev;  // latch non-zero events across multi-frame steps
    if (frame_ != previousFrame) justAdvanced_ = true;
}

void LayeredSprite::resetPlaybackState() {
    clipFrame_ = 0;
    frame_ = activeFrameIndex(0);
    forward_ = true;
    done_ = false;
    scrubbing_ = false;
    accumMs_ = 0.0f;
    justAdvanced_ = false;
    justCompleted_ = false;
    frameEvent_ = 0;
}

// ---------------------------------------------------------------------------
// Clip frame accessors
// ---------------------------------------------------------------------------

const LayeredClip* LayeredSprite::activeClip() const {
    if (asset_ == nullptr || clipIndex_ < 0 || clipIndex_ >= asset_->numClips) return nullptr;
    return &asset_->clips[clipIndex_];
}

uint16_t LayeredSprite::activeFrameCount() const {
    if (asset_ == nullptr) return 0;
    if (const LayeredClip* clip = activeClip()) return clip->numFrames;
    return asset_->numFrames;
}

uint16_t LayeredSprite::activeFrameIndex(uint16_t pos) const {
    if (const LayeredClip* clip = activeClip()) {
        if (clip->numFrames == 0) return 0;
        if (pos >= clip->numFrames) pos = static_cast<uint16_t>(clip->numFrames - 1);
        return clip->frames[pos].frameIndex;
    }
    if (asset_ == nullptr || asset_->numFrames == 0) return 0;
    if (pos >= asset_->numFrames) pos = static_cast<uint16_t>(asset_->numFrames - 1);
    return pos;
}

uint16_t LayeredSprite::activeDuration(uint16_t pos) const {
    if (const LayeredClip* clip = activeClip()) {
        if (clip->numFrames == 0) return 0;
        if (pos >= clip->numFrames) pos = static_cast<uint16_t>(clip->numFrames - 1);
        return clip->frames[pos].durationMs;
    }
    if (asset_ == nullptr || asset_->numFrames == 0) return 0;
    if (pos >= asset_->numFrames) pos = static_cast<uint16_t>(asset_->numFrames - 1);
    return asset_->durations[pos];
}

uint8_t LayeredSprite::activeEventId(uint16_t pos) const {
    const LayeredClip* clip = activeClip();
    if (clip == nullptr || clip->numFrames == 0) return 0;
    if (pos >= clip->numFrames) pos = static_cast<uint16_t>(clip->numFrames - 1);
    return clip->frames[pos].eventId;
}

// ---------------------------------------------------------------------------
// Reconstruction
// ---------------------------------------------------------------------------

void LayeredSprite::draw(ICanvas<Pixel4>& target) const {
    // The position places the asset's static pivot. Under a flip the pivot pixel
    // mirrors around the authored extent (canvasW-1-pivotX / canvasH-1-pivotY),
    // so the origin subtracts the flipped pivot to keep it on the position. An
    // absent pivot is (0,0), which anchors the authored top-left as before.
    int16_t anchorX = 0;
    int16_t anchorY = 0;
    if (asset_ != nullptr) {
        const int16_t canvasW = static_cast<int16_t>(asset_->canvasW);
        const int16_t canvasH = static_cast<int16_t>(asset_->canvasH);
        anchorX = hflip_ ? static_cast<int16_t>(canvasW - 1 - asset_->pivotX)
                         : asset_->pivotX;
        anchorY = vflip_ ? static_cast<int16_t>(canvasH - 1 - asset_->pivotY)
                         : asset_->pivotY;
    }
    blit(target, static_cast<int16_t>(x_ - anchorX),
         static_cast<int16_t>(y_ - anchorY));
}

void LayeredSprite::blit(ICanvas<Pixel4>& target, int16_t originX, int16_t originY) const {
    if (asset_ == nullptr || asset_->numFrames == 0 || asset_->numParts == 0) return;

    uint16_t frameIndex = frame_;
    if (frameIndex >= asset_->numFrames) frameIndex = static_cast<uint16_t>(asset_->numFrames - 1);
    const NjnFramePartRef* refs = asset_->refs + static_cast<size_t>(frameIndex) * asset_->numParts;

    const int16_t canvasW = static_cast<int16_t>(asset_->canvasW);
    const int16_t canvasH = static_cast<int16_t>(asset_->canvasH);

    // Parts are stored bottom-to-top: painting them in order reproduces the
    // authored source-layer painter order. Every mirrored/offset coordinate is
    // clipped to the authored extent so the painted footprint is exactly the
    // reported width x height.
    for (uint16_t p = 0; p < asset_->numParts; ++p) {
        const NjnFramePartRef& ref = refs[p];
        if (ref.imageIndex == NJN2_LAYERED_INVISIBLE) continue;
        if (ref.imageIndex >= asset_->numImages) continue;
        const LayeredImage& img = asset_->images[ref.imageIndex];
        if (img.pixels == nullptr || img.w == 0 || img.h == 0) continue;

        const int16_t iw = static_cast<int16_t>(img.w);
        const int16_t ih = static_cast<int16_t>(img.h);
        for (int16_t j = 0; j < ih; ++j) {
            const int16_t dy = vflip_ ? static_cast<int16_t>(canvasH - ref.offsetY - ih + j)
                                      : static_cast<int16_t>(ref.offsetY + j);
            if (dy < 0 || dy >= canvasH) continue;
            const int16_t srcY = vflip_ ? static_cast<int16_t>(ih - 1 - j) : j;
            const uint32_t rowIndex = static_cast<uint32_t>(srcY) * img.w;
            for (int16_t i = 0; i < iw; ++i) {
                const int16_t dx = hflip_ ? static_cast<int16_t>(canvasW - ref.offsetX - iw + i)
                                          : static_cast<int16_t>(ref.offsetX + i);
                if (dx < 0 || dx >= canvasW) continue;
                const int16_t srcX = hflip_ ? static_cast<int16_t>(iw - 1 - i) : i;
                const uint8_t px = layeredPixelAt(
                    img, asset_->storage, rowIndex + static_cast<uint32_t>(srcX));
                if (px == 15) continue;  // index 15 is transparent
                target.setPixel(static_cast<int16_t>(originX + dx),
                                static_cast<int16_t>(originY + dy), Pixel4(px));
            }
        }
    }
}

} // namespace enjin2
