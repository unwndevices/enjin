/**
 * @file asset_arena.hpp
 * @brief Bounded, 32-byte-aligned per-applet arena for immutable asset data
 *        (ADR-0004 §Storage and memory, Tomodachi #95).
 *
 * A layered sprite is loaded by copying its active pixel and metadata records
 * out of the (possibly flash-backed) `.njn` file into one contiguous,
 * 32-byte-aligned region. That region is an `AssetArena`: a bump allocator with
 * a fixed capacity, a high-water mark, and whole-arena reclamation. One arena
 * backs every layered asset an applet loads; the applet's context owns it and
 * reclaims it in full at teardown, so one applet cannot leak asset bytes into
 * the next.
 *
 * The arena owns its backing store (preferring PSRAM on device) and never frees
 * an individual allocation. `mark()`/`rollback()` make a multi-record copy
 * transactional: a loader can reserve a whole block, and if anything fails it
 * restores the exact `used()` it started from, leaving prior assets untouched.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace enjin2 {

/**
 * @brief Bounded bump arena for per-applet immutable asset data.
 *
 * Not thread-safe; the owning applet context drives it from the main thread
 * during load and teardown.
 */
class AssetArena {
public:
    /// Provisional per-applet asset budget (ADR-0004: raises the old 64 KiB
    /// limit to 512 KiB until measurements fix the final value).
    static constexpr size_t DEFAULT_CAPACITY = 512u * 1024u;

    /// Alignment of the arena base and of every allocation from it.
    static constexpr size_t ALIGNMENT = 32u;

    AssetArena() = default;
    ~AssetArena();

    AssetArena(const AssetArena&) = delete;
    AssetArena& operator=(const AssetArena&) = delete;

    /**
     * @brief Allocate the backing store.
     * @param capacityBytes Total bytes to reserve (default 512 KiB).
     * @return true if a 32-byte-aligned backing block is now owned.
     *
     * On ESP32 the block is requested from PSRAM and falls back to internal
     * heap if PSRAM is unavailable. Allocating again while a backing store is
     * live is a no-op and returns true.
     */
    bool allocateBacking(size_t capacityBytes = DEFAULT_CAPACITY);

    /// Release the backing store and reset all accounting.
    void releaseBacking();

    /// Whether a backing store is owned.
    bool valid() const { return base_ != nullptr; }

    /// Total capacity of the backing store in bytes.
    size_t capacity() const { return capacity_; }

    /// Bytes currently handed out.
    size_t used() const { return used_; }

    /// High-water mark of `used()` since the last backing allocation.
    size_t peak() const { return peak_; }

    /// Bytes still available for allocation.
    size_t remaining() const { return capacity_ - used_; }

    /**
     * @brief Reserve @p size bytes aligned to @p alignment.
     * @param size      Byte count (0 returns the current aligned cursor).
     * @param alignment Power-of-two alignment; defaults to `ALIGNMENT`.
     * @return Pointer to the reserved block, or nullptr if it does not fit or
     *         no backing store is owned. On failure `used()` is unchanged.
     */
    uint8_t* allocate(size_t size, size_t alignment = ALIGNMENT);

    /**
     * @brief Dry-run capacity check for `allocate(size, alignment)`.
     *
     * Does not mutate any accounting; the loader uses it to complete all
     * validation and capacity checks before publishing anything.
     */
    bool canAllocate(size_t size, size_t alignment = ALIGNMENT) const;

    /// Opaque checkpoint of the allocation cursor.
    struct Mark {
        size_t used;
    };

    /// Snapshot the current cursor for a later rollback().
    Mark mark() const { return Mark{used_}; }

    /// Restore the cursor to @p m. Never touches prior allocations' bytes.
    void rollback(Mark m);

    /// Reclaim every allocation (cursor back to zero) without dropping backing.
    void reclaimAll();

private:
    uint8_t* base_ = nullptr;
    size_t capacity_ = 0;
    size_t used_ = 0;
    size_t peak_ = 0;

    static size_t alignUp(size_t value, size_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace enjin2
