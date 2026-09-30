#include "../../include/enjin2/ui/style.hpp"
#include "../../include/enjin2/core/name_index.hpp"

namespace enjin2 {

bool styleSlotFromName(const char* name, StyleSlot& out) {
    const int i = nameIndex(name, kStyleSlotNames);
    if (i < 0) return false;
    out = static_cast<StyleSlot>(i);
    return true;
}

}  // namespace enjin2
