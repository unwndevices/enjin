/**
 * @file sprite_import_fixtures.hpp
 * @brief Shared fixtures for the sprite importer tests (Tomodachi #291, #297):
 *        the ASSERT counters, the `.aseprite` builder and `.njn` readers.
 */
#pragma once

#include <enjin2/import/sprite_import.hpp>
#include <enjin2/graphics/njn2.hpp>
#include <enjin2/graphics/palette.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace enjin2;

inline int passes   = 0;
inline int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); \
            ++failures; \
        } else { \
            ++passes; \
        } \
    } while (0)

using Bytes = std::vector<uint8_t>;

// ---------------------------------------------------------------------------
// `.aseprite` builder (spec: aseprite/docs/ase-file-specs.md)
// ---------------------------------------------------------------------------

inline void put8(Bytes& b, uint32_t v) { b.push_back(static_cast<uint8_t>(v)); }
inline void put16(Bytes& b, uint32_t v) { put8(b, v); put8(b, v >> 8); }
inline void put32(Bytes& b, uint32_t v) { put16(b, v); put16(b, v >> 16); }
inline void putStr(Bytes& b, const std::string& s) {
    put16(b, static_cast<uint32_t>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
}
inline void putZeros(Bytes& b, size_t n) { b.insert(b.end(), n, 0); }

/// zlib stream of stored (uncompressed) deflate blocks.
inline Bytes zlibStored(const Bytes& data) {
    Bytes z = {0x78, 0x01};
    size_t pos = 0;
    do {
        const size_t n = std::min<size_t>(65535, data.size() - pos);
        put8(z, pos + n == data.size() ? 1 : 0);
        put16(z, static_cast<uint32_t>(n));
        put16(z, static_cast<uint32_t>(~n & 0xFFFF));
        z.insert(z.end(), data.begin() + static_cast<long>(pos), data.begin() + static_cast<long>(pos + n));
        pos += n;
    } while (pos < data.size());
    uint32_t a = 1, b = 0;
    for (uint8_t v : data) { a = (a + v) % 65521; b = (b + a) % 65521; }
    const uint32_t adler = (b << 16) | a;
    put8(z, adler >> 24); put8(z, adler >> 16); put8(z, adler >> 8); put8(z, adler);
    return z;
}

struct Tag {
    uint16_t from, to;
    uint8_t dir;        // 0 fwd, 1 reverse, 2 ping-pong, 3 ping-pong reverse
    uint16_t repeat;
    std::string name;
};

struct AseBuilder {
    uint16_t w = 4, h = 4;
    uint16_t depth = 8;          // 8 indexed, 32 RGBA
    uint32_t flags = 1;          // layer opacity valid
    uint16_t speed = 100;
    uint8_t  transparent = 0;
    struct Frame { uint16_t duration = 100; std::vector<Bytes> chunks; };
    std::vector<Frame> frames;

    AseBuilder(uint16_t w_, uint16_t h_, uint16_t depth_, size_t n = 1) : w(w_), h(h_), depth(depth_) {
        frames.resize(n);
    }

    static Bytes chunk(uint16_t type, const Bytes& body) {
        Bytes c;
        put32(c, static_cast<uint32_t>(body.size() + 6));
        put16(c, type);
        c.insert(c.end(), body.begin(), body.end());
        return c;
    }
    static Bytes layer(const std::string& name, bool visible = true, uint16_t type = 0,
                       uint16_t child = 0, uint16_t blend = 0, uint8_t opacity = 255) {
        Bytes b;
        put16(b, visible ? 1 : 0); put16(b, type); put16(b, child);
        put16(b, 0); put16(b, 0); put16(b, blend); put8(b, opacity); putZeros(b, 3);
        putStr(b, name);
        if (type == 2) put32(b, 0);  // tileset index
        return chunk(0x2004, b);
    }
    static Bytes celHeader(uint16_t layerIdx, int16_t x, int16_t y, uint16_t type,
                           uint8_t opacity, int16_t z) {
        Bytes b;
        put16(b, layerIdx); put16(b, static_cast<uint16_t>(x)); put16(b, static_cast<uint16_t>(y));
        put8(b, opacity); put16(b, type); put16(b, static_cast<uint16_t>(z)); putZeros(b, 5);
        return b;
    }
    static Bytes cel(uint16_t layerIdx, int16_t x, int16_t y, uint16_t cw, uint16_t ch,
                     const Bytes& px, bool compressed = false, uint8_t opacity = 255, int16_t z = 0) {
        Bytes b = celHeader(layerIdx, x, y, compressed ? 2 : 0, opacity, z);
        put16(b, cw); put16(b, ch);
        const Bytes payload = compressed ? zlibStored(px) : px;
        b.insert(b.end(), payload.begin(), payload.end());
        return chunk(0x2005, b);
    }
    static Bytes linked(uint16_t layerIdx, int16_t x, int16_t y, uint16_t frame) {
        Bytes b = celHeader(layerIdx, x, y, 1, 255, 0);
        put16(b, frame);
        return chunk(0x2005, b);
    }
    static Bytes tilemapCel(uint16_t layerIdx) {
        Bytes b = celHeader(layerIdx, 0, 0, 3, 255, 0);
        put16(b, 1); put16(b, 1); put16(b, 32);
        put32(b, 0x1fffffff); put32(b, 0x20000000); put32(b, 0x40000000); put32(b, 0x80000000);
        putZeros(b, 10);
        const Bytes z = zlibStored(Bytes(4, 0));
        b.insert(b.end(), z.begin(), z.end());
        return chunk(0x2005, b);
    }
    static Bytes tags(const std::vector<Tag>& ts) {
        Bytes b;
        put16(b, static_cast<uint32_t>(ts.size())); putZeros(b, 8);
        for (const auto& t : ts) {
            put16(b, t.from); put16(b, t.to); put8(b, t.dir); put16(b, t.repeat);
            putZeros(b, 6); putZeros(b, 3); put8(b, 0);
            putStr(b, t.name);
        }
        return chunk(0x2018, b);
    }
    /// New palette chunk with entries first..first+n-1 (RGBA each).
    static Bytes newPalette(uint32_t first, const std::vector<std::array<uint8_t, 4>>& cols) {
        Bytes b;
        put32(b, first + static_cast<uint32_t>(cols.size()));
        put32(b, first); put32(b, first + static_cast<uint32_t>(cols.size()) - 1);
        putZeros(b, 8);
        for (const auto& c : cols) { put16(b, 0); for (uint8_t v : c) put8(b, v); }
        return chunk(0x2019, b);
    }
    /// Old palette chunk: one packet, skipping @p skip, then RGB colours.
    static Bytes oldPalette(uint8_t skip, const std::vector<std::array<uint8_t, 3>>& cols) {
        Bytes b;
        put16(b, 1); put8(b, skip); put8(b, cols.size() == 256 ? 0 : static_cast<uint32_t>(cols.size()));
        for (const auto& c : cols) for (uint8_t v : c) put8(b, v);
        return chunk(0x0004, b);
    }

    void add(size_t frame, const Bytes& c) { frames.at(frame).chunks.push_back(c); }

    Bytes build() const {
        Bytes body;
        for (const auto& f : frames) {
            Bytes fb;
            size_t sz = 16;
            for (const auto& c : f.chunks) sz += c.size();
            put32(fb, static_cast<uint32_t>(sz)); put16(fb, 0xF1FA);
            put16(fb, static_cast<uint32_t>(f.chunks.size())); put16(fb, f.duration);
            putZeros(fb, 2); put32(fb, static_cast<uint32_t>(f.chunks.size()));
            for (const auto& c : f.chunks) fb.insert(fb.end(), c.begin(), c.end());
            body.insert(body.end(), fb.begin(), fb.end());
        }
        Bytes out;
        put32(out, static_cast<uint32_t>(128 + body.size())); put16(out, 0xA5E0);
        put16(out, static_cast<uint32_t>(frames.size())); put16(out, w); put16(out, h);
        put16(out, depth); put32(out, flags); put16(out, speed);
        put32(out, 0); put32(out, 0); put8(out, transparent); putZeros(out, 3);
        put16(out, 0); put8(out, 1); put8(out, 1);
        putZeros(out, 128 - out.size());
        out.insert(out.end(), body.begin(), body.end());
        return out;
    }
};

inline SpriteImportResult run(const Bytes& b, const SpriteImportOptions& o = {}) {
    return importSprite(b.data(), b.size(), o);
}

/// The palette used by RGBA fixtures: slot i = (i*10, i*10, i*10).
inline std::array<RGB, PALETTE_MAX_ENTRIES> greyPalette() {
    std::array<RGB, PALETTE_MAX_ENTRIES> p{};
    for (uint8_t i = 0; i < PALETTE_MAX_ENTRIES; ++i) p[i] = RGB(i * 10, i * 10, i * 10);
    return p;
}
inline SpriteImportOptions greyOpts(SpriteKind k = SpriteKind::Auto) {
    SpriteImportOptions o;
    o.kind = k;
    o.palette = greyPalette();
    return o;
}

struct Sheet {
    bool ok = false;
    uint8_t cellW = 0, cellH = 0, cols = 0, rows = 0;
    Bytes pixl;
    std::vector<NjnClip> clips;
};
inline Sheet readSheet(const Bytes& njn) {
    Sheet s;
    NjnV2Reader rd;
    if (!rd.open(njn.data(), njn.size())) return s;
    if (!njn2DecodeMeta(rd.find(NJN2_CHUNK_META), s.cellW, s.cellH, s.cols, s.rows)) return s;
    const NjnV2Chunk* p = rd.find(NJN2_CHUNK_PIXL);
    if (!p) return s;
    s.pixl.assign(p->data, p->data + p->size);
    if (!njn2DecodeClip(rd.find(NJN2_CHUNK_CLIP), s.clips)) return s;
    s.ok = true;
    return s;
}
inline Sheet readSheet(const SpriteImportResult& r) { return readSheet(r.njn); }
inline bool readLayered(const Bytes& njn, NjnLayered& out) {
    NjnV2Reader rd;
    if (!rd.open(njn.data(), njn.size())) return false;
    const char* err = nullptr;
    return njn2DecodeLayered(rd, out, &err);
}
inline std::vector<uint16_t> frameIdx(const NjnClip& c) {
    std::vector<uint16_t> v;
    for (const auto& f : c.frames) v.push_back(f.frameIndex);
    return v;
}
inline bool contains(const std::string& s, const char* needle) {
    return s.find(needle) != std::string::npos;
}

/// An indexed, one-layer, @p n-frame sheet: frame i fills pixel (0,0) with index i+1.
inline AseBuilder indexedFrames(size_t n) {
    AseBuilder a(2, 2, 8, n);
    a.add(0, AseBuilder::layer("body"));
    for (size_t i = 0; i < n; ++i) {
        a.add(i, AseBuilder::cel(0, 0, 0, 2, 2, {static_cast<uint8_t>(i + 1), 0, 0, 0}));
    }
    return a;
}

inline Bytes readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
