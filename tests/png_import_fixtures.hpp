#pragma once
#include "sprite_import_fixtures.hpp"

inline void png32(Bytes& b, uint32_t n) {
    put8(b, n >> 24); put8(b, n >> 16); put8(b, n >> 8); put8(b, n);
}
inline void pngChunk(Bytes& b, const char* type, const Bytes& data) {
    png32(b, static_cast<uint32_t>(data.size()));
    const size_t start = b.size();
    b.insert(b.end(), type, type + 4);
    b.insert(b.end(), data.begin(), data.end());
    uint32_t crc = 0xffffffff;
    for (size_t i = start; i < b.size(); ++i) {
        crc ^= b[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    png32(b, ~crc);
}
inline Bytes pngImage(uint32_t w, uint32_t h, bool indexed, const Bytes& pixels, uint8_t depth = 8, bool interlace = false) {
    Bytes b = {137,80,78,71,13,10,26,10}, header;
    png32(header, w); png32(header, h);
    header.insert(header.end(), {depth, uint8_t(indexed ? 3 : 6), 0, 0, uint8_t(interlace)});
    pngChunk(b, "IHDR", header);
    if (indexed) {
        Bytes palette;
        // Deliberately duplicate colours: preserving indices cannot use colour matching.
        const size_t entries = std::min<size_t>(16, size_t(1) << depth);
        for (size_t i = 0; i < entries; ++i) palette.insert(palette.end(), {23,42,67});
        pngChunk(b, "PLTE", palette);
        Bytes alpha(entries, 255); if (entries == 16) alpha[15] = 0;
        pngChunk(b, "tRNS", alpha);
    }
    Bytes rows;
    const size_t stride = indexed ? (size_t(w) * depth + 7) / 8 : size_t(w) * 4;
    if (interlace) rows = pixels; // Explicit Adam7 filtered pass bytes supplied by the test.
    else for (uint32_t y = 0; y < h; ++y) {
        rows.push_back(0);
        rows.insert(rows.end(), pixels.begin() + y * stride, pixels.begin() + (y + 1) * stride);
    }
    pngChunk(b, "IDAT", zlibStored(rows));
    pngChunk(b, "IEND", {});
    return b;
}
