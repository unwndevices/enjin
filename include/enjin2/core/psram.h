#pragma once
/**
 * @file psram.h
 * @brief The one PSRAM-first allocator and the one PSRAM .bss macro.
 *
 * Every PSRAM preference in enjin, libtomo and the firmware goes through here
 * (Tomodachi #572); libtomo's `TOMO_PSRAM_BSS` and `libtomo::psramAlloc` are
 * aliases. C-includable: the macro is plain C, the allocator C++ only.
 *
 * - `ENJIN2_PSRAM_BSS` puts a static's zero-initialised storage in
 *   `.ext_ram.bss` (external PSRAM) on device; it needs
 *   `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y`. Elsewhere it is nothing
 *   and the symbol stays in ordinary `.bss`.
 *
 * - `psramAlloc` / `psramRealloc` / `psramAlignedAlloc` prefer PSRAM and fall
 *   back to internal RAM; `psramFree` frees any of them. They return nullptr
 *   on failure, never throw. Off device they are the plain heap.
 *
 * Under ADR-0028 (`CONFIG_SPIRAM_USE_MALLOC`, 1 KB always-internal threshold)
 * a plain `malloc` above 1 KB already goes PSRAM first, so `psramAlloc` of a
 * large block behaves as plain malloc. Use it anyway where it says a buffer is
 * cold whatever its size, and for blocks of 1 KB or less, which plain malloc
 * would keep in internal RAM.
 *
 * The device test is `ESP_PLATFORM || ESP32`: IDF components get the former,
 * enjin's plain CMake targets only the latter.
 */
#if defined(ESP_PLATFORM) || defined(ESP32)
#define ENJIN2_PSRAM_DEVICE 1
#include "esp_attr.h"
#include "esp_heap_caps.h"
#define ENJIN2_PSRAM_BSS EXT_RAM_BSS_ATTR
#else
#define ENJIN2_PSRAM_DEVICE 0
#define ENJIN2_PSRAM_BSS
#endif

#ifdef __cplusplus
#include <cstddef>
#include <cstdlib>

namespace enjin2 {

/// Raw bytes, PSRAM first, then internal RAM. nullptr on failure.
inline void* psramAlloc(std::size_t bytes) {
#if ENJIN2_PSRAM_DEVICE
    void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
#else
    return std::malloc(bytes);
#endif
}

/// Grow or shrink a block from this file (or nullptr), PSRAM first. On
/// failure returns nullptr and leaves `ptr` as it was.
inline void* psramRealloc(void* ptr, std::size_t bytes) {
#if ENJIN2_PSRAM_DEVICE
    void* p = heap_caps_realloc(ptr, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : heap_caps_realloc(ptr, bytes, MALLOC_CAP_8BIT);
#else
    return std::realloc(ptr, bytes);
#endif
}

/// A block whose address is a multiple of `alignment` (a power of two),
/// PSRAM first. Free with psramFree.
inline void* psramAlignedAlloc(std::size_t alignment, std::size_t bytes) {
#if ENJIN2_PSRAM_DEVICE
    void* p = heap_caps_aligned_alloc(alignment, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : heap_caps_aligned_alloc(alignment, bytes, MALLOC_CAP_8BIT);
#else
    // aligned_alloc wants the size a multiple of the alignment.
    const std::size_t rounded = (bytes + alignment - 1) & ~(alignment - 1);
    return std::aligned_alloc(alignment, rounded ? rounded : alignment);
#endif
}

/// Free a block from any allocator above (nullptr is fine).
inline void psramFree(void* p) {
#if ENJIN2_PSRAM_DEVICE
    heap_caps_free(p);
#else
    std::free(p);
#endif
}

} // namespace enjin2
#endif
