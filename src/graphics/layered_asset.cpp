#include "../../include/enjin2/graphics/layered_asset.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace enjin2 {

namespace {

/// Overflow-checked byte-count multiply. Saturates to SIZE_MAX so the arena
/// capacity check rejects rather than wrapping to a small "successful" size.
size_t safeMul(size_t a, size_t b) {
    if (a != 0 && b > std::numeric_limits<size_t>::max() / a) {
        return std::numeric_limits<size_t>::max();
    }
    return a * b;
}

/// A cursor that lays out 32-byte-aligned records either into a real block
/// (commit) or over a null block (pure sizing). One code path, so the reserved
/// byte count and the committed layout can never drift.
struct LayeredBuilder {
    uint8_t* base;
    size_t cursor = 0;

    uint8_t* reserve(size_t bytes) {
        constexpr size_t kMax = std::numeric_limits<size_t>::max();
        if (cursor > kMax - (AssetArena::ALIGNMENT - 1)) {
            cursor = kMax;  // alignment would wrap
            return nullptr;
        }
        cursor = (cursor + AssetArena::ALIGNMENT - 1) & ~(AssetArena::ALIGNMENT - 1);
        if (bytes > kMax - cursor) {
            cursor = kMax;  // record would wrap
            return nullptr;
        }
        uint8_t* at = base != nullptr ? base + cursor : nullptr;
        cursor += bytes;
        return at;
    }
};

/**
 * @brief Size and (when @p base is non-null) fill an arena block with the
 *        decoded layered asset.
 * @param base    Destination block, or nullptr to only measure.
 * @param decoded Validated source records (heap-owned).
 * @param outView On commit, receives the view pointer.
 * @return The exact byte count the layout requires.
 */
size_t buildLayered(uint8_t* base, const NjnLayered& decoded,
                    const LayeredAsset** outView) {
    LayeredBuilder b{base};

    const size_t nImages = decoded.images.size();
    const size_t nParts = decoded.parts.size();
    const size_t nRefs = decoded.refs.size();
    const size_t nDurations = decoded.durations.size();
    const size_t nClips = decoded.clips.size();
    size_t nClipFrames = 0;
    for (const auto& clip : decoded.clips) nClipFrames += clip.frames.size();

    auto* view = reinterpret_cast<LayeredAsset*>(b.reserve(sizeof(LayeredAsset)));
    auto* images = reinterpret_cast<LayeredImage*>(
        b.reserve(safeMul(sizeof(LayeredImage), nImages)));
    auto* parts = reinterpret_cast<LayeredPart*>(
        b.reserve(safeMul(sizeof(LayeredPart), nParts)));
    auto* refs = reinterpret_cast<NjnFramePartRef*>(
        b.reserve(safeMul(sizeof(NjnFramePartRef), nRefs)));
    auto* durations = reinterpret_cast<uint16_t*>(
        b.reserve(safeMul(sizeof(uint16_t), nDurations)));
    auto* clips = reinterpret_cast<LayeredClip*>(
        b.reserve(safeMul(sizeof(LayeredClip), nClips)));
    auto* clipFrames = reinterpret_cast<NjnFrameEntry*>(
        b.reserve(safeMul(sizeof(NjnFrameEntry), nClipFrames)));

    if (base != nullptr) {
        view->canvasW = decoded.canvasW;
        view->canvasH = decoded.canvasH;
        view->numFrames = static_cast<uint16_t>(nDurations);
        view->numParts = static_cast<uint16_t>(nParts);
        view->numImages = static_cast<uint16_t>(nImages);
        view->images = images;
        view->parts = parts;
        view->refs = refs;
        view->durations = durations;
        view->clips = clips;
        view->numClips = static_cast<uint16_t>(nClips);

        for (size_t i = 0; i < nParts; ++i) {
            std::memcpy(parts[i].name, decoded.parts[i].name, sizeof(parts[i].name));
        }
        for (size_t i = 0; i < nRefs; ++i) refs[i] = decoded.refs[i];
        for (size_t i = 0; i < nDurations; ++i) durations[i] = decoded.durations[i];

        size_t frameCursor = 0;
        for (size_t i = 0; i < nClips; ++i) {
            const NjnClip& src = decoded.clips[i];
            std::memcpy(clips[i].name, src.name, sizeof(clips[i].name));
            clips[i].loopMode = src.loopMode;
            clips[i].numFrames = static_cast<uint16_t>(src.frames.size());
            clips[i].frames = clipFrames + frameCursor;
            for (size_t f = 0; f < src.frames.size(); ++f) {
                clipFrames[frameCursor + f] = src.frames[f];
            }
            frameCursor += src.frames.size();
        }
    }

    for (size_t i = 0; i < nImages; ++i) {
        const size_t pixelBytes =
            static_cast<size_t>(decoded.images[i].w) * decoded.images[i].h;
        uint8_t* pixels = b.reserve(pixelBytes);
        if (base != nullptr) {
            images[i].w = decoded.images[i].w;
            images[i].h = decoded.images[i].h;
            images[i].pixels = pixels;
            std::memcpy(pixels, decoded.images[i].pixels.data(), pixelBytes);
        }
    }

    if (base != nullptr && outView != nullptr) *outView = view;
    return b.cursor;
}

} // namespace

LayeredAssetStore::Slot* LayeredAssetStore::slotFor(Handle handle) {
    if (handle < 0 || handle >= MAX_ASSETS) return nullptr;
    Slot& s = slots_[handle];
    return s.used ? &s : nullptr;
}

const LayeredAssetStore::Slot* LayeredAssetStore::slotFor(Handle handle) const {
    if (handle < 0 || handle >= MAX_ASSETS) return nullptr;
    const Slot& s = slots_[handle];
    return s.used ? &s : nullptr;
}

void LayeredAssetStore::clearSlot(Slot* slot) {
    if (slot->arenaEnd != 0 && slot->arenaEnd == arena_.used()) {
        arena_.rollback(AssetArena::Mark{slot->arenaStart});
    }
    *slot = Slot{};
}

LayeredAssetStore::Handle LayeredAssetStore::load(const std::string& path,
                                                  std::string* error) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (fp == nullptr) {
        if (error != nullptr) *error = "could not open layered asset file";
        return INVALID_HANDLE;
    }
    std::fseek(fp, 0, SEEK_END);
    const long fileSize = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (fileSize <= 0) {
        std::fclose(fp);
        if (error != nullptr) *error = "empty layered asset file";
        return INVALID_HANDLE;
    }

    std::vector<uint8_t> bytes(static_cast<size_t>(fileSize));
    const size_t read = std::fread(bytes.data(), 1, bytes.size(), fp);
    std::fclose(fp);
    if (read != bytes.size()) {
        if (error != nullptr) *error = "incomplete layered asset read";
        return INVALID_HANDLE;
    }
    return loadFromMemory(bytes.data(), bytes.size(), error);
}

LayeredAssetStore::Handle LayeredAssetStore::loadFromMemory(const uint8_t* data,
                                                            size_t size,
                                                            std::string* error) {
    // --- 1. Parse and validate (no arena or registry mutation). ---
    NjnV2Reader reader;
    if (!reader.open(data, size)) {
        if (error != nullptr) *error = "invalid or corrupt layered container";
        return INVALID_HANDLE;
    }
    NjnLayered decoded;
    const char* decodeError = nullptr;
    if (!njn2DecodeLayered(reader, decoded, &decodeError)) {
        if (error != nullptr) {
            *error = decodeError != nullptr ? decodeError : "malformed layered asset";
        }
        return INVALID_HANDLE;
    }

    // --- 2. Reserve a registry slot before committing anything. ---
    int slotIndex = -1;
    for (int i = 0; i < MAX_ASSETS; ++i) {
        if (!slots_[i].used) {
            slotIndex = i;
            break;
        }
    }
    if (slotIndex < 0) {
        if (error != nullptr) *error = "layered asset store full";
        return INVALID_HANDLE;
    }

    // --- 3. Compute the exact byte requirement (dry run). ---
    const size_t required = buildLayered(nullptr, decoded, nullptr);

    // --- 4. Capacity check completes before any handle is published. ---
    if (!arena_.canAllocate(required, AssetArena::ALIGNMENT)) {
        ++allocationFailures_;
        if (error != nullptr) *error = "layered arena capacity exceeded";
        return INVALID_HANDLE;
    }

    // --- 5. Commit: one aligned block, then copy. ---
    const AssetArena::Mark before = arena_.mark();
    uint8_t* block = arena_.allocate(required, AssetArena::ALIGNMENT);
    if (block == nullptr) {
        arena_.rollback(before);
        ++allocationFailures_;
        if (error != nullptr) *error = "layered arena allocation failed";
        return INVALID_HANDLE;
    }
    const LayeredAsset* view = nullptr;
    buildLayered(block, decoded, &view);

    Slot& slot = slots_[slotIndex];
    slot.used = true;
    slot.live = true;
    slot.refCount = 0;
    slot.asset = view;
    slot.arenaStart = before.used;
    slot.arenaEnd = arena_.used();
    ++loadCount_;
    return slotIndex;
}

const LayeredAsset* LayeredAssetStore::get(Handle handle) const {
    const Slot* slot = slotFor(handle);
    if (slot == nullptr || !slot->live) return nullptr;
    return slot->asset;
}

const LayeredAsset* LayeredAssetStore::retain(Handle handle) {
    Slot* slot = slotFor(handle);
    if (slot == nullptr || !slot->live) return nullptr;
    ++slot->refCount;
    return slot->asset;
}

void LayeredAssetStore::release(Handle handle) {
    Slot* slot = slotFor(handle);
    if (slot == nullptr) return;
    if (slot->refCount > 0) --slot->refCount;
    if (slot->refCount == 0 && !slot->live) clearSlot(slot);
}

void LayeredAssetStore::free(Handle handle) {
    Slot* slot = slotFor(handle);
    if (slot == nullptr || !slot->live) return;
    slot->live = false;
    if (slot->refCount == 0) clearSlot(slot);
}

void LayeredAssetStore::reclaimAll() {
    for (auto& slot : slots_) slot = Slot{};
    arena_.reclaimAll();
}

int LayeredAssetStore::assetCount() const {
    int count = 0;
    for (const auto& slot : slots_) {
        if (slot.used) ++count;
    }
    return count;
}

} // namespace enjin2
