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

#include "../instrumentation/clock.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace enjin2 {

/// How a part image's palette indices are stored in the arena (ADR-0004, #98).
///
/// The on-disk `LIMG` payload is always one palette byte per pixel; the loader
/// chooses the arena representation at load time. The two paths trade arena
/// bytes against per-pixel blit work, and the measurement harness in
/// `benchmarks/bench_layered.cpp` compares them before the default is frozen.
enum class PixelStorage : uint8_t {
    Unpacked8 = 0,  ///< One palette byte per pixel (low nibble).
    Packed4bpp = 1, ///< Two palette nibbles per byte, row-major.
};

/// One immutable cropped part image. `pixels` is arena-owned and has a
/// 32-byte-aligned start; its byte count depends on the owning asset's
/// `PixelStorage` (`w*h` unpacked, `(w*h+1)/2` packed).
struct LayeredImage {
    uint16_t w;
    uint16_t h;
    const uint8_t* pixels;
};

/// Read one palette index out of a part image under @p storage.
///
/// `index` is the row-major pixel index (`y * w + x`) and must be `< w * h`.
inline uint8_t layeredPixelAt(const LayeredImage& img, PixelStorage storage,
                              uint32_t index) {
    if (storage == PixelStorage::Packed4bpp) {
        const uint8_t byte = img.pixels[index >> 1];
        return (index & 1u) ? static_cast<uint8_t>(byte >> 4)
                            : static_cast<uint8_t>(byte & 0x0Fu);
    }
    return static_cast<uint8_t>(img.pixels[index] & 0x0Fu);
}

/// Byte count the arena stores for a @p w × @p h part image under @p storage.
inline size_t layeredPixelBytes(uint16_t w, uint16_t h, PixelStorage storage) {
    const size_t count = static_cast<size_t>(w) * static_cast<size_t>(h);
    return storage == PixelStorage::Packed4bpp ? (count + 1) / 2 : count;
}

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
    PixelStorage storage;             ///< Arena pixel representation (#98).
};

/**
 * @brief Point-in-time instrumentation snapshot for one `LayeredAssetStore`
 *        (ADR-0004, #98).
 *
 * Reports the metrics the spec requires: loaded asset count, current and peak
 * arena bytes, allocation failures, and load time. All byte counts are arena
 * facts, so `currentBytes == arena.used()` and `peakBytes == arena.peak()`.
 */
struct LayeredAssetMetrics {
    int assetCount = 0;                 ///< Registry slots in use (live/retained).
    size_t currentBytes = 0;            ///< Arena bytes currently in use.
    size_t peakBytes = 0;               ///< Arena high-water mark.
    uint32_t allocationFailures = 0;    ///< Loads rejected for lack of capacity.
    uint32_t loadCount = 0;             ///< Successful loads.
    uint64_t loadMicrosTotal = 0;       ///< Summed successful-load duration.
    uint64_t loadMicrosMax = 0;         ///< Longest successful load.
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

    /// Frozen default arena pixel representation (#98). Chosen from the
    /// `bench_layered` measurements and recorded in ADR-0005.
    static constexpr PixelStorage DEFAULT_STORAGE = PixelStorage::Unpacked8;

    /**
     * @param arena    Bounded arena that owns all asset bytes.
     * @param storage  Arena pixel representation to use for this store.
     *                 Defaults to @ref DEFAULT_STORAGE; the measurement
     *                 harness constructs one store per representation.
     */
    explicit LayeredAssetStore(AssetArena& arena,
                               PixelStorage storage = DEFAULT_STORAGE)
        : arena_(arena), storage_(storage) {}

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

    /// Arena pixel representation this store loads into.
    PixelStorage storage() const { return storage_; }

    /**
     * @brief Install the microsecond time source for load timing.
     *
     * Timing is disabled until a clock is installed, so shipping builds pay no
     * clock calls. `nullptr` disables it again; passing a clock accumulates
     * durations until @ref resetMetrics.
     */
    void setClock(MicrosFn clock) { loadTiming_.clock = clock; }

    /// Zero the load-time accumulators and failure/load counters.
    void resetMetrics();

    /// Snapshot of all instrumentation for this store (ADR-0004, #98).
    LayeredAssetMetrics metrics() const;

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
    PixelStorage storage_;
    Slot slots_[MAX_ASSETS];
    uint32_t allocationFailures_ = 0;
    uint32_t loadCount_ = 0;
    TimingAccumulator loadTiming_;
};

} // namespace enjin2
