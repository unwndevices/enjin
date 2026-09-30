#pragma once

#include <cstddef>
#include <cstring>

namespace enjin2 {

/**
 * @brief Index of `name` in a name array, or -1 when absent (or null).
 *
 * A binding that accepts a closed set of names keeps them in one array indexed
 * by its enum, matches arguments with this, and publishes the same array in
 * its Lua API descriptor (lua_api.hpp), so the accepted and documented names
 * cannot drift apart.
 */
template <size_t N>
int nameIndex(const char* name, const char* const (&names)[N]) {
    if (!name) return -1;
    for (size_t i = 0; i < N; ++i) {
        if (std::strcmp(name, names[i]) == 0) return static_cast<int>(i);
    }
    return -1;
}

} // namespace enjin2
