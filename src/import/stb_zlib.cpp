/**
 * @file stb_zlib.cpp
 * @brief zlib inflate for compressed `.aseprite` cels, from vendored stb_image (#291)
 *
 * STB_IMAGE_STATIC keeps every stb symbol local to this translation unit, so a
 * binary that also compiles stb_image (the libtomo golden-image tests) links.
 */
#include <climits>
#include <cstddef>
#include <cstdint>

#include "aseprite_file.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include "../../vendor/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace enjin2 {
namespace ase {

// stb does not check the Adler-32 trailer; the exact output size catches
// corrupt streams.
bool inflateZlib(const uint8_t* in, size_t inLen, uint8_t* out, size_t outLen) {
    if (inLen > INT_MAX || outLen >= INT_MAX) return false;
    // stb fails (rather than grows) when the stream holds more than outLen.
    const int n = stbi_zlib_decode_buffer(reinterpret_cast<char*>(out), static_cast<int>(outLen),
                                          reinterpret_cast<const char*>(in), static_cast<int>(inLen));
    return n >= 0 && static_cast<size_t>(n) == outLen;
}

} // namespace ase
} // namespace enjin2
