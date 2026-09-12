#include "../../include/enjin2/graphics/asset_arena.hpp"

#include <cstring>
#include <new>

#ifdef ESP32
#include <esp_heap_caps.h>
#endif

namespace enjin2 {

AssetArena::~AssetArena() {
    releaseBacking();
}

bool AssetArena::allocateBacking(size_t capacityBytes) {
    if (base_ != nullptr) return true;
    if (capacityBytes == 0) return false;

    // Align the requested capacity so the base alignment holds for every
    // subsequent aligned allocation.
    const size_t bytes = alignUp(capacityBytes, ALIGNMENT);

#ifdef ESP32
    // Prefer PSRAM: asset data is cold and bulky, and internal DRAM is scarce.
    // Fall back to internal heap when PSRAM is absent.
    base_ = static_cast<uint8_t*>(
        heap_caps_aligned_alloc(ALIGNMENT, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (base_ == nullptr) {
        base_ = static_cast<uint8_t*>(
            heap_caps_aligned_alloc(ALIGNMENT, bytes, MALLOC_CAP_8BIT));
    }
    if (base_ == nullptr) return false;
#else
    base_ = static_cast<uint8_t*>(
        ::operator new(bytes, std::align_val_t(ALIGNMENT), std::nothrow));
    if (base_ == nullptr) return false;
#endif

    capacity_ = bytes;
    used_ = 0;
    peak_ = 0;
    return true;
}

void AssetArena::releaseBacking() {
    if (base_ == nullptr) return;
#ifdef ESP32
    heap_caps_free(base_);
#else
    ::operator delete(base_, std::align_val_t(ALIGNMENT));
#endif
    base_ = nullptr;
    capacity_ = 0;
    used_ = 0;
    peak_ = 0;
}

bool AssetArena::canAllocate(size_t size, size_t alignment) const {
    if (base_ == nullptr || alignment == 0) return false;
    const size_t aligned = alignUp(used_, alignment);
    // Subtraction-only bounds check: no add can wrap.
    if (aligned > capacity_) return false;
    return size <= capacity_ - aligned;
}

uint8_t* AssetArena::allocate(size_t size, size_t alignment) {
    if (base_ == nullptr || alignment == 0) return nullptr;
    const size_t aligned = alignUp(used_, alignment);
    if (aligned > capacity_ || size > capacity_ - aligned) return nullptr;
    uint8_t* result = base_ + aligned;
    used_ = aligned + size;
    if (used_ > peak_) peak_ = used_;
    return result;
}

void AssetArena::rollback(Mark m) {
    if (m.used <= used_) used_ = m.used;
}

void AssetArena::reclaimAll() {
    used_ = 0;
}

} // namespace enjin2
