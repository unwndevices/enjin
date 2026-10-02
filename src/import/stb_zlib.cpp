/**
 * @file stb_zlib.cpp
 * @brief Bounded zlib and PNG decode through vendored stb_image (#291, #307)
 *
 * STB_IMAGE_STATIC keeps every stb symbol local to this translation unit, so a
 * binary that also compiles stb_image (the libtomo golden-image tests) links.
 */
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

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

bool isPng(const uint8_t* data, size_t size) {
    static constexpr uint8_t signature[] = {137,80,78,71,13,10,26,10};
    return data && size >= 8 && std::memcmp(data, signature, 8) == 0;
}

Error decodePng(const uint8_t* data, size_t size, const SpriteImportLimits& limits,
                uint16_t& width, uint16_t& height, bool& indexed,
                std::vector<uint8_t>& pixels, std::vector<std::array<uint8_t, 4>>& palette) {
    if (size > limits.maxFileBytes || size > INT_MAX) return tooLarge("PNG exceeds the input-size cap");
    if (!isPng(data, size) || size < 33) return malformed("truncated PNG header");
    auto be32 = [](const uint8_t* p) {
        return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
    };
    if (be32(data + 8) != 13 || std::memcmp(data + 12, "IHDR", 4)) return malformed("missing PNG IHDR");
    const uint32_t w = be32(data + 16), h = be32(data + 20);
    if (!w || !h) return malformed("empty PNG");
    if (w > limits.maxCanvasSide || h > limits.maxCanvasSide || w > 65535 || h > 65535 ||
        uint64_t(w) * h * 4 > limits.maxPixelBytes) return tooLarge("PNG exceeds the canvas or decoded-pixel cap");
    indexed = data[25] == 3;
    if (!indexed && data[25] != 6) return unsupported("PNG must be indexed or RGBA");
    if (!indexed && data[24] != 8) return unsupported("RGBA PNG must be 8-bit");
    if (data[26] || data[27]) return malformed("invalid PNG compression or filter method");
    std::vector<uint8_t> compressed;
    bool end = false, idat = false, plte = false, trns = false;
    for (size_t pos = 8; pos < size;) {
        if (size - pos < 12) return malformed("truncated PNG chunk");
        const uint32_t n = be32(data + pos);
        if (n > size - pos - 12) return malformed("truncated PNG chunk data");
        const uint8_t* type = data + pos + 4;
        const uint8_t* payload = data + pos + 8;
        if (!std::memcmp(type, "CgBI", 4)) return unsupported("Apple CgBI PNG is unsupported");
        if (!std::memcmp(type, "IHDR", 4) && pos != 8) return malformed("duplicate PNG header");
        if (!std::memcmp(type, "PLTE", 4)) {
            if (plte || idat || !n || n > 768 || n % 3) return malformed("invalid PNG palette");
            plte = true;
            palette.resize(n / 3);
            for (size_t i = 0; i < palette.size(); ++i) {
                palette[i] = {payload[3*i], payload[3*i+1], payload[3*i+2], 255};
            }
        } else if (!std::memcmp(type, "tRNS", 4) && indexed) {
            if (!plte || idat || trns || n > palette.size()) return malformed("invalid PNG transparency table");
            trns = true;
            for (size_t i = 0; i < n; ++i) palette[i][3] = payload[i];
        } else if (!std::memcmp(type, "IDAT", 4)) {
            idat = true;
            compressed.insert(compressed.end(), data + pos + 8, data + pos + 8 + n);
        } else if (!std::memcmp(type, "IEND", 4)) {
            if (n || !idat || pos + 12 != size) return malformed("invalid PNG end");
            end = true;
        }
        pos += size_t(n) + 12;
    }
    if (!end || (indexed && !plte)) return malformed("incomplete PNG");
    const unsigned depth = data[24], channelsOnDisk = indexed ? 1 : 4;
    if ((indexed && depth != 1 && depth != 2 && depth != 4 && depth != 8) || data[28] > 1)
        return malformed("invalid PNG bit depth or interlace");
    if (indexed && palette.size() > (size_t(1) << depth)) return malformed("PNG palette exceeds its bit depth");
    auto passBytes = [&](uint32_t pw, uint32_t ph) -> size_t {
        return pw && ph ? size_t(ph) * (1 + (size_t(pw) * channelsOnDisk * depth + 7) / 8) : 0;
    };
    size_t rawBytes = passBytes(w, h);
    if (data[28]) {
        static constexpr unsigned x0[] = {0,4,0,2,0,1,0}, y0[] = {0,0,4,0,2,0,1};
        static constexpr unsigned dx[] = {8,8,4,4,2,2,1}, dy[] = {8,8,8,4,4,2,2};
        rawBytes = 0;
        for (size_t i = 0; i < 7; ++i) rawBytes += passBytes(w > x0[i] ? (w - x0[i] + dx[i] - 1) / dx[i] : 0,
                                                          h > y0[i] ? (h - y0[i] + dy[i] - 1) / dy[i] : 0);
    }
    if (rawBytes >= INT_MAX) return tooLarge("PNG scanlines exceed the decoder limit");
    // Use stb's filters/Adam7 reconstruction directly, before palette expansion:
    // original indices remain available for range validation and duplicate colours
    // cannot collapse them. Inflation has a fixed output size, never a growing heap.
    std::vector<uint8_t> raw(rawBytes);
    if (!inflateZlib(compressed.data(), compressed.size(), raw.data(), raw.size()))
        return malformed("PNG inflated pixels do not match its dimensions");
    stbi__context context{};
    context.img_x = w; context.img_y = h;
    context.img_n = int(channelsOnDisk); context.img_out_n = int(channelsOnDisk);
    stbi__png png{};
    png.s = &context;
    const bool ok = stbi__create_png_image(&png, raw.data(), uint32_t(raw.size()), int(channelsOnDisk),
                                          int(depth), data[25], data[28]) != 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decoded(png.out, stbi_image_free);
    if (!ok || !decoded) return malformed("PNG pixels do not decode");
    width = uint16_t(w); height = uint16_t(h);
    const size_t count = size_t(w) * h;
    if (!indexed) pixels.assign(decoded.get(), decoded.get() + count * 4);
    else {
        pixels.resize(count);
        for (size_t i = 0; i < count; ++i) {
            const uint8_t index = decoded.get()[i];
            if (index >= palette.size()) return malformed("PNG pixel exceeds its palette");
            const uint8_t alpha = palette[index][3];
            if (alpha != 0 && alpha != 255) return unsupported("indexed PNG needs binary alpha");
            if (alpha && index > 14) return unsupported("indexed PNG opaque index exceeds slot 14");
            pixels[i] = alpha ? index : 15;
        }
    }
    return {};
}

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
