/**
 * @file layered_asset.hpp
 * @brief Shared C++ layered-asset owner/loader over a bounded per-applet arena
 *        (ADR-0004, Tomodachi #95).
 *
 * `LayeredAssetStore` is the single runtime seam for layered `.njn` v2 assets,
 * independent of the Lua bindings. It parses and validates a layered asset via
 * the `njn2` codec, copies the decoded records and pixels into an `AssetArena`,
 * and exposes them as one immutable `LayeredAsset`.
 *
 * ## Transactional loading
 *
 * Nothing is published until every fallible step has succeeded:
 *
 *   1. the file is read and the container/chunks are fully validated (heap
 *      temporaries only);
 *   2. the exact arena byte requirement is computed and a free registry slot is
 *      reserved;
 *   3. the whole asset is then reserved as one aligned block and copied.
 *
 * If parsing, validation, slot reservation, or capacity fails, the arena
 * accounting, registry, existing assets, and retained instances are unchanged
 * and the call reports a clear failure (handle `INVALID_HANDLE`). Allocation
 * failures are counted in `allocationFailures()`.
 *
 * ## Ownership
 *
 * `load*()` returns a public handle. `retain(handle)` hands a retained instance
 * a pointer to the immutable asset and increments its owner count;
 * `release(handle)` decrements it. `free(handle)` invalidates the public handle
 * but never the bytes: a retained instance keeps a valid view until it
 * releases. The arena is reclaimed as a unit by `reclaimAll()` (applet
 * teardown), so no individual free can dangle a live instance.
 *
 * The arena is a bump allocator and does not compact. A freed asset whose block
 * is the arena tip returns its bytes immediately; any other freed asset leaves
 * a gap until `reclaimAll()`. Within one applet lifetime, free/reload loops
 * should therefore free in load order (or reload the same asset) to conserve
 * the budget.
 *
 * The first cut hosts layered sprites only; flat sheets and tilesets keep their
 * existing path.
 */
#pragma once

#include "asset_arena.hpp"
#include "njn2.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace enjin2 {

/// One immutable cropped part image. `pixels` is arena-owned (`w*h` bytes,
/// low nibble = palette index) and has a 32-byte-aligned start.
struct LayeredImage {
    uint16_t w;
    uint16_t h;
    const uint8_t* pixels;
};

/// One named sprite part in bottom-to-top painter order. The name is
/// diagnostics metadata, never part identity.
struct LayeredPart {
    char name[16];
};

/// One named animation clip. `frames` is arena-owned (`numFrames` entries);
/// each entry's `frameIndex` references an animation frame (0..numFrames-1).
struct LayeredClip {
    char name[16];
    NjnLoopMode loopMode;
    uint16_t numFrames;
    const NjnFrameEntry* frames;
};

/// Immutable, arena-owned view of one loaded layered asset.
struct LayeredAsset {
    uint16_t canvasW;                 ///< Authored canvas extent (stable).
    uint16_t canvasH;
    uint16_t numFrames;               ///< Animation frame count.
    uint16_t numParts;                ///< Sprite part count.
    uint16_t numImages;               ///< Part-image pool size.
    const LayeredImage* images;       ///< numImages records.
    const LayeredPart* parts;         ///< numParts records, painter order.
    const NjnFramePartRef* refs;      ///< numFrames × numParts, frame-major.
    const uint16_t* durations;        ///< numFrames milliseconds.
    const LayeredClip* clips;         ///< numClips records.
    uint16_t numClips;
};

/**
 * @brief Owns and loads layered assets into a bounded `AssetArena`.
 *
 * Zero heap allocation past load time: fixed slots, arena-only asset bytes.
 */
class LayeredAssetStore {
public:
    /// Fixed registry size. One applet loads a handful of layered sprites.
    static constexpr int MAX_ASSETS = 16;

    /// Public handle type; `INVALID_HANDLE` means the load failed.
    using Handle = int;
    static constexpr Handle INVALID_HANDLE = -1;

    explicit LayeredAssetStore(AssetArena& arena) : arena_(arena) {}

    LayeredAssetStore(const LayeredAssetStore&) = delete;
    LayeredAssetStore& operator=(const LayeredAssetStore&) = delete;

    /**
     * @brief Load a layered asset from a `.njn` file path. Transactional.
     * @param path   Filesystem path to the asset.
     * @param error  Optional out-param receiving a static failure reason.
     * @return A live handle, or `INVALID_HANDLE` on any failure.
     */
    Handle load(const std::string& path, std::string* error = nullptr);

    /**
     * @brief Load a layered asset from an in-memory container. Transactional.
     * @param data   Pointer to the `.njn` bytes.
     * @param size   Byte count.
     * @param error  Optional out-param receiving a static failure reason.
     * @return A live handle, or `INVALID_HANDLE` on any failure.
     */
    Handle loadFromMemory(const uint8_t* data, size_t size,
                          std::string* error = nullptr);

    /// Immutable data for a live public handle, or nullptr.
    const LayeredAsset* get(Handle handle) const;

    /**
     * @brief Increment the owner count and return the immutable asset.
     * @return The asset view, or nullptr if `handle` is not live.
     */
    const LayeredAsset* retain(Handle handle);

    /**
     * @brief Decrement the owner count. When both the public handle is freed
     *        and the last owner released, the registry slot is reclaimed.
     */
    void release(Handle handle);

    /**
     * @brief Invalidate the public handle without touching asset bytes.
     *
     * Retained instances keep a valid immutable view; the slot (and, when it is
     * the arena's tip, its bytes) is reclaimed once the last owner releases.
     */
    void free(Handle handle);

    /// Release every asset and reclaim the whole arena. Keeps the backing store.
    void reclaimAll();

    /// Number of registry slots currently in use (live or retained).
    int assetCount() const;

    /// Arena bytes currently in use (== `arena.used()`).
    size_t currentBytes() const { return arena_.used(); }

    /// Arena high-water mark in bytes (== `arena.peak()`).
    size_t peakBytes() const { return arena_.peak(); }

    /// Count of loads rejected because the arena lacked capacity.
    uint32_t allocationFailures() const { return allocationFailures_; }

    /// Count of successful loads.
    uint32_t loadCount() const { return loadCount_; }

private:
    struct Slot {
        bool used = false;         ///< A view is (or was) published here.
        bool live = false;         ///< The public handle is still valid.
        int refCount = 0;          ///< Retained instances.
        const LayeredAsset* asset = nullptr;
        size_t arenaStart = 0;     ///< Cursor before this asset's block.
        size_t arenaEnd = 0;       ///< Cursor after this asset's block.
    };

    Slot* slotFor(Handle handle);
    const Slot* slotFor(Handle handle) const;
    void clearSlot(Slot* slot);

    AssetArena& arena_;
    Slot slots_[MAX_ASSETS];
    uint32_t allocationFailures_ = 0;
    uint32_t loadCount_ = 0;
};

} // namespace enjin2
