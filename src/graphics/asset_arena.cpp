#include "../../include/enjin2/graphics/asset_arena.hpp"

#include <cstring>
#include <new>

#include "../../include/enjin2/core/psram.h"

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

    // Prefer PSRAM: asset data is cold and bulky, and internal DRAM is scarce.
    base_ = static_cast<uint8_t*>(psramAlignedAlloc(ALIGNMENT, bytes));
    if (base_ == nullptr) return false;

    capacity_ = bytes;
    used_ = 0;
    peak_ = 0;
    return true;
}

void AssetArena::releaseBacking() {
    if (base_ == nullptr) return;
    psramFree(base_);
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
