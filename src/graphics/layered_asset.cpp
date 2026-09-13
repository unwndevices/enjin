#include "../../include/enjin2/graphics/layered_asset.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#ifdef ESP32
#include <esp_heap_caps.h>
#endif

namespace enjin2 {

namespace {

/// File-read scratch buffer, preferring PSRAM on device so the transient copy
/// never competes for scarce internal DRAM (#100).
uint8_t* allocScratch(size_t bytes) {
#ifdef ESP32
    uint8_t* p = static_cast<uint8_t*>(
        heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (p == nullptr) {
        p = static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_8BIT));
    }
    return p;
#else
    return static_cast<uint8_t*>(std::malloc(bytes));
#endif
}

void freeScratch(uint8_t* p) {
#ifdef ESP32
    if (p != nullptr) heap_caps_free(p);
#else
    std::free(p);
#endif
}

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
 * @param storage Arena pixel representation to lay out.
 * @param outView On commit, receives the view pointer.
 * @return The exact byte count the layout requires.
 */
size_t buildLayered(uint8_t* base, const NjnLayered& decoded, PixelStorage storage,
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
        view->storage = storage;

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
        const size_t count =
            static_cast<size_t>(decoded.images[i].w) * decoded.images[i].h;
        const size_t pixelBytes =
            layeredPixelBytes(decoded.images[i].w, decoded.images[i].h, storage);
        uint8_t* pixels = b.reserve(pixelBytes);
        if (base != nullptr) {
            // The decoder either materialized the pixels or left a non-owning
            // view into the source container (#100); either way there is exactly
            // one source copy to read from.
            const uint8_t* src = decoded.images[i].pixels.empty()
                                     ? decoded.images[i].rawPixels
                                     : decoded.images[i].pixels.data();
            if (src == nullptr) continue;  // unreachable for a validated asset
            images[i].w = decoded.images[i].w;
            images[i].h = decoded.images[i].h;
            images[i].pixels = pixels;
            if (storage == PixelStorage::Packed4bpp) {
                // Pack two row-major low-nibble indices per byte, low pixel
                // first. The source is always one byte per pixel.
                for (size_t px = 0; px < count; px += 2) {
                    const uint8_t lo = static_cast<uint8_t>(src[px] & 0x0F);
                    const uint8_t hi =
                        (px + 1 < count) ? static_cast<uint8_t>(src[px + 1] & 0x0F) : 0;
                    pixels[px >> 1] = static_cast<uint8_t>(lo | (hi << 4));
                }
            } else {
                std::memcpy(pixels, src, pixelBytes);
            }
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

    const size_t size = static_cast<size_t>(fileSize);
    uint8_t* bytes = allocScratch(size);
    if (bytes == nullptr) {
        std::fclose(fp);
        if (error != nullptr) *error = "out of memory for layered asset buffer";
        return INVALID_HANDLE;
    }
    const size_t read = std::fread(bytes, 1, size, fp);
    std::fclose(fp);
    if (read != size) {
        freeScratch(bytes);
        if (error != nullptr) *error = "incomplete layered asset read";
        return INVALID_HANDLE;
    }
    const Handle handle = loadFromMemory(bytes, size, error);
    freeScratch(bytes);
    return handle;
}

LayeredAssetStore::Handle LayeredAssetStore::loadFromMemory(const uint8_t* data,
                                                            size_t size,
                                                            std::string* error) {
    const uint64_t startMicros = loadTiming_.start();

    // --- 1. Parse and validate (no arena or registry mutation). ---
    NjnV2Reader reader;
    if (!reader.open(data, size)) {
        if (error != nullptr) *error = "invalid or corrupt layered container";
        return INVALID_HANDLE;
    }
    NjnLayered decoded;
    const char* decodeError = nullptr;
    // Decode as non-owning views into `data`: the pixels are copied straight
    // into the arena below, so a large asset never needs a second full
    // heap-resident copy (which on device is scarce internal DRAM). The caller
    // guarantees `data` outlives this call.
    if (!njn2DecodeLayered(reader, decoded, &decodeError,
                           /*materializePixels=*/false)) {
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
    const size_t required = buildLayered(nullptr, decoded, storage_, nullptr);

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
    buildLayered(block, decoded, storage_, &view);

    Slot& slot = slots_[slotIndex];
    slot.used = true;
    slot.live = true;
    slot.refCount = 0;
    slot.asset = view;
    slot.arenaStart = before.used;
    slot.arenaEnd = arena_.used();
    ++loadCount_;
    loadTiming_.stop(startMicros);
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

void LayeredAssetStore::resetMetrics() {
    allocationFailures_ = 0;
    loadCount_ = 0;
    loadTiming_.reset();
}

LayeredAssetMetrics LayeredAssetStore::metrics() const {
    LayeredAssetMetrics m;
    m.assetCount = assetCount();
    m.currentBytes = arena_.used();
    m.peakBytes = arena_.peak();
    m.allocationFailures = allocationFailures_;
    m.loadCount = loadCount_;
    m.loadMicrosTotal = loadTiming_.total;
    m.loadMicrosMax = loadTiming_.max;
    return m;
}

} // namespace enjin2
