/**
 * @file sprite_import_test.cpp
 * @brief Tests for the `.aseprite` → `.njn` v2 sprite importer (Tomodachi #291)
 *
 * Fixtures are hand-built `.aseprite` byte streams (AseBuilder below), checked
 * against the spec, not against the Python converter's output.
 *
 * Test IDs:
 *   SPI-01  Indexed, one layer, untagged → sheet; default loop clip; 15 = clear.
 *   SPI-02  Tag forward / reverse / ping-pong / ping-pong-reverse, repeat 0.
 *   SPI-03  Tag repeat 1 → once; repeat N > 1 → N passes unrolled, once.
 *   SPI-04  0 ms frames fall back to the header speed, then to 100 ms.
 *   SPI-05  Tags clamp to the frame range; all-empty tags → default clip.
 *   SPI-06  Clip names: printable ASCII, 15 chars, empty → clipN; the summary
 *           keeps each clip's source tag name.
 *   SPI-07  Indexed opaque index > 14 rejected.
 *   SPI-08  Old (0x0004) and new (0x2019) palette chunks both read.
 *   SPI-09  RGBA exact palette match imports to indices.
 *   SPI-10  RGBA mismatch: colour and alpha issues, counts, nearest slot.
 *   SPI-11  RGBA issues ignore off-canvas pixels and count a linked cel once.
 *   SPI-12  Two visible layers → layered: parts, crop, dedupe, invisible
 *           refs, bottom-centre pivot; decodes through njn2DecodeLayered.
 *   SPI-13  Kind override: sheet flattens layers, layered takes one layer.
 *   SPI-14  Flat z-index: order layer+z, ties broken by z (spec NOTE.5).
 *   SPI-15  Layered rejects nonzero z-index, cel and layer opacity.
 *   SPI-16  Hidden layers ignored and reported; their tilemap cels skipped.
 *   SPI-17  Visible group or tilemap layer rejected.
 *   SPI-18  Compressed cels inflate; a wrong-size stream is rejected.
 *   SPI-19  Linked cel without a source, or out of range, rejected.
 *   SPI-20  Size caps and format limits → TooLarge.
 *   SPI-21  Malformed headers, frames and chunks rejected.
 *   SPI-22  tomo_tune2.aseprite imports with the "tomo" palette and matches
 *           the committed tomo_tune2.njn part for part.
 *   SPI-23  tomo_tune2.aseprite against the system palette → PaletteMismatch.
 *   SPI-24  Truncation/corruption corpus: never crashes; every ok result
 *           re-reads (run under ASan by scripts/importer-check.sh).
 *   SPI-25  An unrolled clip over 255 frames → TooLarge.
 *   SPI-26  More sheet frames than META's 255x255 grid → TooLarge, even with
 *           the frame cap raised.
 *
 * `sprite_import_test --verify-layered <file.njn>` checks a CLI output file
 * (the sprite_import_cli_* ctest pair).
 */

#include <enjin2/import/sprite_import.hpp>
#include <enjin2/graphics/njn2.hpp>
#include <enjin2/graphics/palette.hpp>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace enjin2;

static int passes   = 0;
static int failures = 0;

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

static void put8(Bytes& b, uint32_t v) { b.push_back(static_cast<uint8_t>(v)); }
static void put16(Bytes& b, uint32_t v) { put8(b, v); put8(b, v >> 8); }
static void put32(Bytes& b, uint32_t v) { put16(b, v); put16(b, v >> 16); }
static void putStr(Bytes& b, const std::string& s) {
    put16(b, static_cast<uint32_t>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
}
static void putZeros(Bytes& b, size_t n) { b.insert(b.end(), n, 0); }

/// zlib stream of stored (uncompressed) deflate blocks.
static Bytes zlibStored(const Bytes& data) {
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

static SpriteImportResult run(const Bytes& b, const SpriteImportOptions& o = {}) {
    return importSprite(b.data(), b.size(), o);
}

/// The palette used by RGBA fixtures: slot i = (i*10, i*10, i*10).
static std::array<RGB, PALETTE_MAX_ENTRIES> greyPalette() {
    std::array<RGB, PALETTE_MAX_ENTRIES> p{};
    for (uint8_t i = 0; i < PALETTE_MAX_ENTRIES; ++i) p[i] = RGB(i * 10, i * 10, i * 10);
    return p;
}
static SpriteImportOptions greyOpts(SpriteKind k = SpriteKind::Auto) {
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
static Sheet readSheet(const SpriteImportResult& r) {
    Sheet s;
    NjnV2Reader rd;
    if (!rd.open(r.njn.data(), r.njn.size())) return s;
    if (!njn2DecodeMeta(rd.find(NJN2_CHUNK_META), s.cellW, s.cellH, s.cols, s.rows)) return s;
    const NjnV2Chunk* p = rd.find(NJN2_CHUNK_PIXL);
    if (!p) return s;
    s.pixl.assign(p->data, p->data + p->size);
    if (!njn2DecodeClip(rd.find(NJN2_CHUNK_CLIP), s.clips)) return s;
    s.ok = true;
    return s;
}
static bool readLayered(const Bytes& njn, NjnLayered& out) {
    NjnV2Reader rd;
    if (!rd.open(njn.data(), njn.size())) return false;
    const char* err = nullptr;
    return njn2DecodeLayered(rd, out, &err);
}
static std::vector<uint16_t> frameIdx(const NjnClip& c) {
    std::vector<uint16_t> v;
    for (const auto& f : c.frames) v.push_back(f.frameIndex);
    return v;
}
static bool contains(const std::string& s, const char* needle) {
    return s.find(needle) != std::string::npos;
}

/// An indexed, one-layer, @p n-frame sheet: frame i fills pixel (0,0) with index i+1.
static AseBuilder indexedFrames(size_t n) {
    AseBuilder a(2, 2, 8, n);
    a.add(0, AseBuilder::layer("body"));
    for (size_t i = 0; i < n; ++i) {
        a.add(i, AseBuilder::cel(0, 0, 0, 2, 2, {static_cast<uint8_t>(i + 1), 0, 0, 0}));
    }
    return a;
}

static Bytes readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test01_indexedSheetUntagged() {
    AseBuilder a(3, 2, 8, 2);
    a.transparent = 0;
    a.frames[0].duration = 80;
    a.frames[1].duration = 120;
    a.add(0, AseBuilder::layer("body"));
    a.add(0, AseBuilder::cel(0, 0, 0, 3, 2, {1, 0, 2, 3, 4, 0}));
    a.add(1, AseBuilder::cel(0, 1, 1, 1, 1, {14}));
    const auto r = run(a.build());
    ASSERT(r.ok(), "SPI-01 indexed sheet imports");
    ASSERT(r.kind == SpriteKind::Sheet, "SPI-01 one visible layer → sheet");
    const Sheet s = readSheet(r);
    ASSERT(s.ok, "SPI-01 META/PIXL/CLIP decode");
    ASSERT(s.cellW == 3 && s.cellH == 2 && s.cols == 2 && s.rows == 1, "SPI-01 META = canvas cell, one cell per frame");
    const Bytes want = {1, 15, 2, 3, 4, 15,   15, 15, 15, 15, 14, 15};
    ASSERT(s.pixl == want, "SPI-01 PIXL frame-major, source transparent → 15");
    ASSERT(s.clips.size() == 1, "SPI-01 one clip");
    if (s.clips.size() == 1) {
        ASSERT(std::strcmp(s.clips[0].name, "default") == 0, "SPI-01 clip named default");
        ASSERT(s.clips[0].loopMode == NjnLoopMode::Loop, "SPI-01 default clip loops");
        ASSERT((frameIdx(s.clips[0]) == std::vector<uint16_t>{0, 1}), "SPI-01 default clip covers all frames");
        ASSERT(s.clips[0].frames[0].durationMs == 80 && s.clips[0].frames[1].durationMs == 120, "SPI-01 per-frame durations");
    }
    ASSERT(r.canvasW == 3 && r.canvasH == 2 && r.frames == 2, "SPI-01 summary");
    ASSERT(r.clips.size() == 1 && r.clips[0].tag.empty() && !r.clips[0].fromTag, "SPI-01 the default clip follows no tag");
}

static const NjnClip* clipNamed(const std::vector<NjnClip>& clips, const char* name) {
    for (const auto& c : clips) if (std::strcmp(c.name, name) == 0) return &c;
    return nullptr;
}

static void test02_tagDirections() {
    AseBuilder a = indexedFrames(5);
    a.add(0, AseBuilder::tags({
        {1, 3, 0, 0, "fwd"}, {1, 3, 1, 0, "rev"}, {1, 3, 2, 0, "pp"}, {1, 3, 3, 0, "ppr"},
    }));
    const Sheet s = readSheet(run(a.build()));
    ASSERT(s.ok && s.clips.size() == 4, "SPI-02 four tags → four clips");
    const NjnClip* fwd = clipNamed(s.clips, "fwd");
    const NjnClip* rev = clipNamed(s.clips, "rev");
    const NjnClip* pp  = clipNamed(s.clips, "pp");
    const NjnClip* ppr = clipNamed(s.clips, "ppr");
    ASSERT(fwd && frameIdx(*fwd) == (std::vector<uint16_t>{1, 2, 3}) && fwd->loopMode == NjnLoopMode::Loop, "SPI-02 forward → in order, loop");
    ASSERT(rev && frameIdx(*rev) == (std::vector<uint16_t>{3, 2, 1}) && rev->loopMode == NjnLoopMode::Loop, "SPI-02 reverse → reversed, loop");
    ASSERT(pp && frameIdx(*pp) == (std::vector<uint16_t>{1, 2, 3}) && pp->loopMode == NjnLoopMode::PingPong, "SPI-02 ping-pong → pingpong");
    ASSERT(ppr && frameIdx(*ppr) == (std::vector<uint16_t>{3, 2, 1}) && ppr->loopMode == NjnLoopMode::PingPong, "SPI-02 ping-pong-reverse → reversed + pingpong");
}

static void test03_tagRepeat() {
    AseBuilder a = indexedFrames(4);
    a.add(0, AseBuilder::tags({
        {0, 2, 0, 1, "once"}, {0, 1, 0, 3, "fwd3"}, {0, 2, 1, 2, "rev2"},
        {0, 2, 2, 1, "pp1"}, {0, 2, 2, 3, "pp3"}, {0, 2, 3, 2, "ppr2"},
    }));
    const Sheet s = readSheet(run(a.build()));
    ASSERT(s.ok && s.clips.size() == 6, "SPI-03 six clips");
    auto check = [&](const char* name, std::vector<uint16_t> want, const char* msg) {
        const NjnClip* c = clipNamed(s.clips, name);
        ASSERT(c && frameIdx(*c) == want && c->loopMode == NjnLoopMode::Once, msg);
    };
    check("once", {0, 1, 2}, "SPI-03 repeat 1 → once");
    check("fwd3", {0, 1, 0, 1, 0, 1}, "SPI-03 forward repeat 3 → three passes, once");
    check("rev2", {2, 1, 0, 2, 1, 0}, "SPI-03 reverse repeat 2 → two reversed passes, once");
    check("pp1", {0, 1, 2}, "SPI-03 ping-pong repeat 1 → one forward pass, once");
    check("pp3", {0, 1, 2, 1, 0, 1, 2}, "SPI-03 ping-pong repeat 3 → alternating passes, turn frames once");
    check("ppr2", {2, 1, 0, 1, 2}, "SPI-03 ping-pong-reverse repeat 2 → reverse then forward");
}

static void test04_zeroDuration() {
    AseBuilder a = indexedFrames(2);
    a.speed = 70;
    a.frames[0].duration = 0;
    a.frames[1].duration = 30;
    Sheet s = readSheet(run(a.build()));
    ASSERT(s.ok && s.clips.size() == 1 && s.clips[0].frames[0].durationMs == 70, "SPI-04 0 ms → header speed");
    ASSERT(s.ok && s.clips[0].frames[1].durationMs == 30, "SPI-04 nonzero kept");
    a.speed = 0;
    s = readSheet(run(a.build()));
    ASSERT(s.ok && s.clips[0].frames[0].durationMs == 100, "SPI-04 0 ms and speed 0 → 100 ms");
}

static void test05_tagClamp() {
    AseBuilder a = indexedFrames(3);
    a.add(0, AseBuilder::tags({{1, 9, 0, 0, "tail"}, {5, 7, 0, 0, "gone"}, {2, 1, 0, 0, "inverted"}}));
    Sheet s = readSheet(run(a.build()));
    ASSERT(s.ok && s.clips.size() == 1, "SPI-05 out-of-range tags dropped");
    ASSERT(s.ok && s.clips.size() == 1 && frameIdx(s.clips[0]) == (std::vector<uint16_t>{1, 2}), "SPI-05 tag clamped to last frame");
    AseBuilder b = indexedFrames(3);
    b.add(0, AseBuilder::tags({{5, 7, 0, 0, "gone"}}));
    s = readSheet(run(b.build()));
    ASSERT(s.ok && s.clips.size() == 1 && std::strcmp(s.clips[0].name, "default") == 0, "SPI-05 all tags empty → default clip");
}

static void test06_clipNames() {
    AseBuilder a = indexedFrames(2);
    a.add(0, AseBuilder::tags({
        {0, 0, 0, 0, "a_very_long_clip_name_indeed"}, {0, 0, 0, 0, "caf\xC3\xA9 \x01"}, {0, 1, 0, 0, ""},
    }));
    const auto r = run(a.build());
    const Sheet s = readSheet(r);
    ASSERT(s.ok && s.clips.size() == 3, "SPI-06 three clips");
    if (s.clips.size() == 3) {
        ASSERT(std::strcmp(s.clips[0].name, "a_very_long_cli") == 0, "SPI-06 capped at 15 chars");
        ASSERT(std::strcmp(s.clips[1].name, "caf_ _") == 0, "SPI-06 non-ASCII code point and control → '_'");
        ASSERT(std::strcmp(s.clips[2].name, "clip2") == 0, "SPI-06 empty name → clipN");
        ASSERT(r.clips.size() == 3 && r.clips[0].name == "a_very_long_cli", "SPI-06 summary carries written names");
        ASSERT(r.clips.size() == 3 && r.clips[0].tag == "a_very_long_clip_name_indeed" && r.clips[1].tag == "caf\xC3\xA9 \x01" &&
               r.clips[2].tag.empty() && r.clips[2].fromTag, "SPI-06 summary carries each clip's source tag, as named in the file");
    }
}

static void test07_indexedOutOfRange() {
    AseBuilder a(2, 1, 8);
    a.add(0, AseBuilder::layer("body"));
    a.add(0, AseBuilder::cel(0, 0, 0, 2, 1, {3, 20}));
    const auto r = run(a.build());
    ASSERT(r.status == SpriteImportStatus::Unsupported, "SPI-07 opaque index 20 rejected");
    ASSERT(contains(r.error, "20"), "SPI-07 error names the index");
    // The source's transparent index may itself be > 14.
    AseBuilder b(2, 1, 8);
    b.transparent = 200;
    b.add(0, AseBuilder::layer("body"));
    b.add(0, AseBuilder::cel(0, 0, 0, 2, 1, {3, 200}));
    const Sheet s = readSheet(run(b.build()));
    ASSERT(s.ok && s.pixl == (Bytes{3, 15}), "SPI-07 transparent index 200 → 15");
}

static void test08_paletteChunks() {
    AseBuilder a = indexedFrames(1);
    a.add(0, AseBuilder::oldPalette(2, {{{10, 20, 30}}, {{40, 50, 60}}}));
    auto r = run(a.build());
    ASSERT(r.ok(), "SPI-08 old palette chunk accepted");
    ASSERT(r.sourcePalette.size() == 4 && r.sourcePalette[2] == (std::array<uint8_t, 4>{10, 20, 30, 255})
           && r.sourcePalette[3] == (std::array<uint8_t, 4>{40, 50, 60, 255}), "SPI-08 old palette read at its skip offset");
    AseBuilder b = indexedFrames(1);
    b.add(0, AseBuilder::oldPalette(0, {{{1, 1, 1}}}));
    b.add(0, AseBuilder::newPalette(1, {{{9, 8, 7, 128}}}));
    r = run(b.build());
    ASSERT(r.ok() && r.sourcePalette.size() == 2 && r.sourcePalette[1] == (std::array<uint8_t, 4>{9, 8, 7, 128}), "SPI-08 new palette chunk read, alpha kept");
    // A truncated palette chunk is malformed, not ignored.
    AseBuilder c = indexedFrames(1);
    Bytes bad = AseBuilder::oldPalette(0, {{{1, 2, 3}}, {{4, 5, 6}}});
    bad.resize(bad.size() - 2);
    bad[0] = static_cast<uint8_t>(bad.size());
    c.add(0, bad);
    ASSERT(run(c.build()).status == SpriteImportStatus::Malformed, "SPI-08 truncated old palette rejected");
}

static void test09_rgbaMatch() {
    AseBuilder a(2, 2, 32);
    a.add(0, AseBuilder::layer("body"));
    a.add(0, AseBuilder::cel(0, 0, 0, 2, 2, {
        30, 30, 30, 255,   0, 0, 0, 0,
        140, 140, 140, 255, 77, 1, 2, 0,   // fully transparent pixels can be any colour
    }));
    const auto r = run(a.build(), greyOpts());
    ASSERT(r.ok(), "SPI-09 RGBA on the palette imports");
    const Sheet s = readSheet(r);
    ASSERT(s.ok && s.pixl == (Bytes{3, 15, 14, 15}), "SPI-09 RGBA → palette slots, alpha 0 → 15");
}

static void test10_rgbaMismatch() {
    AseBuilder a(3, 2, 32);
    a.add(0, AseBuilder::layer("body"));
    a.add(0, AseBuilder::cel(0, 0, 0, 3, 2, {
        31, 30, 30, 255,  31, 30, 30, 255,  31, 30, 30, 255,  // off palette ×3, nearest slot 3
        20, 20, 20, 128,  50, 50, 50, 255,  200, 0, 0, 255,   // partial alpha ×1, ok, off ×1
    }));
    const auto r = run(a.build(), greyOpts());
    ASSERT(r.status == SpriteImportStatus::PaletteMismatch, "SPI-10 mismatch status");
    ASSERT(r.colourIssues.size() == 3, "SPI-10 one issue per offending value");
    if (r.colourIssues.size() == 3) {
        const auto& top = r.colourIssues[0];
        ASSERT(top.r == 31 && top.pixels == 3 && top.nearestSlot == 3 && !top.partialAlpha(), "SPI-10 most pixels first, nearest slot 3");
        bool sawAlpha = false, sawRed = false;
        for (const auto& i : r.colourIssues) {
            if (i.a == 128) sawAlpha = i.partialAlpha() && i.pixels == 1 && i.nearestSlot == 2;
            if (i.r == 200) sawRed = i.pixels == 1 && i.nearestSlot < PALETTE_MAX_ENTRIES;
        }
        ASSERT(sawAlpha, "SPI-10 partial alpha reported with its value and count");
        ASSERT(sawRed, "SPI-10 off-palette red reported");
    }
    ASSERT(!r.error.empty() && r.njn.empty(), "SPI-10 message, no output");
}

static void test11_rgbaCounting() {
    AseBuilder a(2, 1, 32, 3);
    a.add(0, AseBuilder::layer("body"));
    // Cel hangs off the right edge: only its first pixel is in canvas.
    a.add(0, AseBuilder::cel(0, 1, 0, 2, 1, {7, 7, 7, 255, 9, 9, 9, 255}));
    a.add(1, AseBuilder::linked(0, 1, 0, 0));
    a.add(2, AseBuilder::linked(0, 1, 0, 0));
    const auto r = run(a.build(), greyOpts());
    ASSERT(r.status == SpriteImportStatus::PaletteMismatch && r.colourIssues.size() == 1, "SPI-11 off-canvas colour not reported");
    ASSERT(r.colourIssues.size() == 1 && r.colourIssues[0].r == 7 && r.colourIssues[0].pixels == 1, "SPI-11 linked cel counted once");
}

static AseBuilder twoLayer() {
    // 4x4, 3 frames; layer 0 "base" static (linked), layer 1 "eye" moves, empty in frame 2.
    AseBuilder a(4, 4, 8, 3);
    a.add(0, AseBuilder::layer("base"));
    a.add(0, AseBuilder::layer("eye"));
    a.add(0, AseBuilder::cel(0, 0, 2, 4, 2, {1, 1, 1, 1, 2, 2, 2, 2}));
    a.add(0, AseBuilder::cel(1, 0, 0, 3, 2, {0, 5, 0, 0, 6, 0}));  // crops to 1x2 at (1,0)
    a.add(1, AseBuilder::linked(0, 0, 2, 0));
    a.add(1, AseBuilder::cel(1, 2, 1, 1, 2, {5, 6}));                // same image, moved
    a.add(2, AseBuilder::linked(0, 0, 2, 0));
    a.add(2, AseBuilder::cel(1, 0, 0, 2, 2, {0, 0, 0, 0}));          // empty → invisible
    return a;
}

static void test12_layered() {
    const auto r = run(twoLayer().build());
    ASSERT(r.ok() && r.kind == SpriteKind::Layered, "SPI-12 two visible layers → layered");
    NjnLayered L;
    ASSERT(readLayered(r.njn, L), "SPI-12 njn2DecodeLayered accepts the output");
    ASSERT(L.canvasW == 4 && L.canvasH == 4 && L.durations.size() == 3, "SPI-12 canvas and frames");
    ASSERT(L.parts.size() == 2 && std::strcmp(L.parts[0].name, "base") == 0 && std::strcmp(L.parts[1].name, "eye") == 0, "SPI-12 parts in painter order");
    ASSERT(L.images.size() == 2, "SPI-12 linked + identical images deduplicated");
    if (L.images.size() == 2 && L.refs.size() == 6) {
        ASSERT(L.images[1].w == 1 && L.images[1].h == 2 && L.images[1].pixels == (Bytes{5, 6}), "SPI-12 cel cropped to its opaque bounds");
        ASSERT(L.refs[1].imageIndex == 1 && L.refs[1].offsetX == 1 && L.refs[1].offsetY == 0, "SPI-12 crop offset");
        ASSERT(L.refs[3].imageIndex == 1 && L.refs[3].offsetX == 2 && L.refs[3].offsetY == 1, "SPI-12 reused image at a moved offset");
        ASSERT(L.refs[4].imageIndex == 0 && L.refs[4].offsetY == 2, "SPI-12 linked base cel");
        ASSERT(L.refs[5].imageIndex == NJN2_LAYERED_INVISIBLE, "SPI-12 empty cel → invisible");
    }
    ASSERT(L.pivotX == 2 && L.pivotY == 3, "SPI-12 bottom-centre pivot (w/2, h-1)");
    ASSERT(L.clips.size() == 1 && std::strcmp(L.clips[0].name, "default") == 0, "SPI-12 default clip");
    ASSERT(r.parts.size() == 2 && r.images == 2, "SPI-12 summary");
}

static void test13_kindOverride() {
    const auto sheet = run(twoLayer().build(), [] { SpriteImportOptions o; o.kind = SpriteKind::Sheet; return o; }());
    ASSERT(sheet.ok() && sheet.kind == SpriteKind::Sheet, "SPI-13 --sheet on two layers");
    const Sheet s = readSheet(sheet);
    // Frame 0 flattened: eye over base.
    const Bytes f0 = {15, 5, 15, 15,  15, 6, 15, 15,  1, 1, 1, 1,  2, 2, 2, 2};
    ASSERT(s.ok && s.pixl.size() == 48 && Bytes(s.pixl.begin(), s.pixl.begin() + 16) == f0, "SPI-13 layers flattened bottom to top");

    const auto lay = run(indexedFrames(2).build(), [] { SpriteImportOptions o; o.kind = SpriteKind::Layered; return o; }());
    NjnLayered L;
    ASSERT(lay.ok() && lay.kind == SpriteKind::Layered && readLayered(lay.njn, L) && L.parts.size() == 1, "SPI-13 --layered on one layer");
}

static void test14_flatZOrder() {
    // Layer 0 has z=+1 and layer 1 z=0: both order 1, so the tie goes by z and
    // layer 0 (z=1) paints last.
    AseBuilder a(1, 1, 8);
    a.add(0, AseBuilder::layer("a"));
    a.add(0, AseBuilder::layer("b"));
    a.add(0, AseBuilder::cel(0, 0, 0, 1, 1, {3}, false, 255, 1));
    a.add(0, AseBuilder::cel(1, 0, 0, 1, 1, {4}, false, 255, 0));
    SpriteImportOptions o; o.kind = SpriteKind::Sheet;
    Sheet s = readSheet(run(a.build(), o));
    ASSERT(s.ok && s.pixl == (Bytes{3}), "SPI-14 equal order: higher z paints on top");
    AseBuilder b(1, 1, 8);
    b.add(0, AseBuilder::layer("a"));
    b.add(0, AseBuilder::layer("b"));
    b.add(0, AseBuilder::cel(0, 0, 0, 1, 1, {3}, false, 255, 2));
    b.add(0, AseBuilder::cel(1, 0, 0, 1, 1, {4}));
    s = readSheet(run(b.build(), o));
    ASSERT(s.ok && s.pixl == (Bytes{3}), "SPI-14 z lifts a lower layer above");
}

static void test15_layeredRejects() {
    AseBuilder a = twoLayer();
    a.frames[1].chunks[1] = AseBuilder::cel(1, 2, 1, 1, 2, {5, 6}, false, 255, 1);
    auto r = run(a.build());
    ASSERT(r.status == SpriteImportStatus::Unsupported && contains(r.error, "z-index"), "SPI-15 nonzero z rejected");
    AseBuilder b = twoLayer();
    b.frames[1].chunks[1] = AseBuilder::cel(1, 2, 1, 1, 2, {5, 6}, false, 100);
    r = run(b.build());
    ASSERT(r.status == SpriteImportStatus::Unsupported && contains(r.error, "opacity"), "SPI-15 cel opacity rejected");
    AseBuilder c = twoLayer();
    c.frames[0].chunks[1] = AseBuilder::layer("eye", true, 0, 0, 0, 128);
    r = run(c.build());
    ASSERT(r.status == SpriteImportStatus::Unsupported && contains(r.error, "opacity"), "SPI-15 layer opacity rejected");
    c.flags = 0;  // header says layer opacity is not valid → treated as 255
    ASSERT(run(c.build()).ok(), "SPI-15 layer opacity ignored without the header flag");
}

static void test16_hiddenLayers() {
    AseBuilder a = indexedFrames(1);
    a.add(0, AseBuilder::layer("ref", false));
    a.add(0, AseBuilder::layer("map", false, 2));
    a.add(0, AseBuilder::cel(1, 0, 0, 1, 1, {9}));
    a.add(0, AseBuilder::tilemapCel(2));
    const auto r = run(a.build());
    ASSERT(r.ok() && r.kind == SpriteKind::Sheet, "SPI-16 hidden layers don't count toward layered");
    ASSERT((r.hiddenLayers == std::vector<std::string>{"ref", "map"}), "SPI-16 hidden layers reported");
    const Sheet s = readSheet(r);
    ASSERT(s.ok && s.pixl == (Bytes{1, 15, 15, 15}), "SPI-16 hidden pixels not drawn");
}

static void test17_unsupportedLayers() {
    AseBuilder a = indexedFrames(1);
    a.add(0, AseBuilder::layer("map", true, 2));
    a.add(0, AseBuilder::tilemapCel(1));
    auto r = run(a.build());
    ASSERT(r.status == SpriteImportStatus::Unsupported && contains(r.error, "ilemap"), "SPI-17 visible tilemap layer rejected");
    AseBuilder b = indexedFrames(1);
    b.add(0, AseBuilder::layer("grp", true, 1));
    r = run(b.build());
    ASSERT(r.status == SpriteImportStatus::Unsupported && contains(r.error, "roup"), "SPI-17 visible group layer rejected");
    AseBuilder c = indexedFrames(1);
    c.frames[0].chunks[0] = AseBuilder::layer("body", true, 0, 0, 1);
    r = run(c.build());
    ASSERT(r.status == SpriteImportStatus::Unsupported && contains(r.error, "blend"), "SPI-17 non-normal blend rejected");
    AseBuilder d(1, 1, 16);
    d.add(0, AseBuilder::layer("body"));
    ASSERT(run(d.build()).status == SpriteImportStatus::Unsupported, "SPI-17 grayscale rejected");
}

static void test18_compressed() {
    AseBuilder a(3, 1, 8);
    a.add(0, AseBuilder::layer("body"));
    a.add(0, AseBuilder::cel(0, 0, 0, 3, 1, {1, 2, 3}, true));
    const Sheet s = readSheet(run(a.build()));
    ASSERT(s.ok && s.pixl == (Bytes{1, 2, 3}), "SPI-18 compressed cel inflates");
    AseBuilder b(3, 1, 8);
    b.add(0, AseBuilder::layer("body"));
    Bytes c = AseBuilder::celHeader(0, 0, 0, 2, 255, 0);
    put16(c, 3); put16(c, 1);
    const Bytes z = zlibStored({1, 2});   // two bytes for a 3-pixel cel
    c.insert(c.end(), z.begin(), z.end());
    b.add(0, AseBuilder::chunk(0x2005, c));
    ASSERT(run(b.build()).status == SpriteImportStatus::Malformed, "SPI-18 short stream rejected");
    AseBuilder d(3, 1, 8);
    d.add(0, AseBuilder::layer("body"));
    Bytes e = AseBuilder::celHeader(0, 0, 0, 2, 255, 0);
    put16(e, 3); put16(e, 1);
    e.insert(e.end(), {0x78, 0x01, 0xFF, 0xFF, 0xFF});
    d.add(0, AseBuilder::chunk(0x2005, e));
    ASSERT(run(d.build()).status == SpriteImportStatus::Malformed, "SPI-18 garbage stream rejected");
}

static void test19_linked() {
    AseBuilder a = indexedFrames(2);
    a.frames[1].chunks[0] = AseBuilder::linked(0, 0, 0, 7);
    ASSERT(run(a.build()).status == SpriteImportStatus::Malformed, "SPI-19 link past the last frame");
    AseBuilder b(2, 2, 8, 2);
    b.add(0, AseBuilder::layer("body"));
    b.add(1, AseBuilder::linked(0, 0, 0, 0));
    ASSERT(run(b.build()).status == SpriteImportStatus::Malformed, "SPI-19 link to a frame with no cel");
    AseBuilder c(2, 2, 8, 2);
    c.add(0, AseBuilder::layer("body"));
    c.add(0, AseBuilder::linked(0, 0, 0, 1));
    c.add(1, AseBuilder::linked(0, 0, 0, 0));
    ASSERT(run(c.build()).status == SpriteImportStatus::Malformed, "SPI-19 link cycle");
}

static void test20_caps() {
    SpriteImportOptions o;
    o.limits.maxCanvasSide = 3;
    ASSERT(run(AseBuilder(4, 2, 8).build(), o).status == SpriteImportStatus::TooLarge, "SPI-20 canvas cap");
    o = {};
    o.limits.maxFrames = 2;
    ASSERT(run(indexedFrames(3).build(), o).status == SpriteImportStatus::TooLarge, "SPI-20 frame cap");
    o = {};
    o.limits.maxLayers = 1;
    ASSERT(run(twoLayer().build(), o).status == SpriteImportStatus::TooLarge, "SPI-20 layer cap");
    o = {};
    o.limits.maxCelSide = 2;
    ASSERT(run(twoLayer().build(), o).status == SpriteImportStatus::TooLarge, "SPI-20 cel side cap");
    o = {};
    o.limits.maxPixelBytes = 10;
    ASSERT(run(twoLayer().build(), o).status == SpriteImportStatus::TooLarge, "SPI-20 decoded pixel cap");
    o = {};
    o.limits.maxFileBytes = 64;
    ASSERT(run(twoLayer().build(), o).status == SpriteImportStatus::TooLarge, "SPI-20 file cap");
    // A sheet cell is u8 in META.
    AseBuilder wide(256, 1, 8);
    wide.add(0, AseBuilder::layer("body"));
    const auto r = run(wide.build());
    ASSERT(r.status == SpriteImportStatus::TooLarge && contains(r.error, "255"), "SPI-20 sheet canvas over 255");
    o = {};
    o.kind = SpriteKind::Layered;
    wide.add(0, AseBuilder::cel(0, 0, 0, 1, 1, {1}));
    ASSERT(run(wide.build(), o).ok(), "SPI-20 the same canvas imports layered");
}

static void test21_malformed() {
    const Bytes good = indexedFrames(2).build();
    ASSERT(run(good).ok(), "SPI-21 baseline ok");
    ASSERT(importSprite(nullptr, 0).status == SpriteImportStatus::Malformed, "SPI-21 null input");
    Bytes b = good; b[4] = 0;
    ASSERT(run(b).status == SpriteImportStatus::Malformed, "SPI-21 bad magic");
    b = good; b[6] = 0; b[7] = 0;
    ASSERT(run(b).status == SpriteImportStatus::Malformed, "SPI-21 zero frames");
    b = good; b[8] = 0; b[9] = 0;
    ASSERT(run(b).status == SpriteImportStatus::Malformed, "SPI-21 zero width");
    b = good; b[128 + 4] = 0;
    ASSERT(run(b).status == SpriteImportStatus::Malformed, "SPI-21 bad frame magic");
    b = good; b[128] = 0xFF; b[129] = 0xFF;
    ASSERT(run(b).status == SpriteImportStatus::Malformed, "SPI-21 frame size past the end");
    b = good; b[128 + 16] = 0xFF;
    ASSERT(run(b).status == SpriteImportStatus::Malformed, "SPI-21 chunk size past the frame");
    b = good; b.resize(b.size() - 1);
    ASSERT(run(b).status == SpriteImportStatus::Malformed, "SPI-21 truncated tail");
    AseBuilder a = indexedFrames(1);
    a.frames[0].chunks[1] = AseBuilder::cel(3, 0, 0, 2, 2, {1, 0, 0, 0});
    ASSERT(run(a.build()).status == SpriteImportStatus::Malformed, "SPI-21 cel on an unknown layer");
    AseBuilder d = indexedFrames(1);
    d.add(0, AseBuilder::cel(0, 0, 0, 2, 2, {1, 0, 0, 0}));
    ASSERT(run(d.build()).status == SpriteImportStatus::Malformed, "SPI-21 two cels on one layer in a frame");
    AseBuilder e(2, 2, 8);
    ASSERT(run(e.build()).status == SpriteImportStatus::Unsupported, "SPI-21 no visible layer");
}

static std::string testdata(const char* rel) { return std::string(ENJIN2_SOURCE_DIR) + "/" + rel; }

static void test22_tomoTune2() {
    const Bytes src = readFile(testdata("tools/testdata/tomo_tune2.aseprite"));
    ASSERT(!src.empty(), "SPI-22 fixture readable");
    Palette tomo;
    tomo.loadPreset("tomo");
    SpriteImportOptions o;
    std::copy(std::begin(tomo.colors), std::end(tomo.colors), o.palette.begin());
    const auto r = run(src, o);
    ASSERT(r.ok() && r.kind == SpriteKind::Layered, "SPI-22 tomo_tune2 imports layered");
    if (!r.ok()) { fprintf(stderr, "  error: %s\n", r.error.c_str()); return; }
    NjnLayered got, ref;
    ASSERT(readLayered(r.njn, got), "SPI-22 output decodes");
    const Bytes golden = readFile(testdata("tools/testdata/tomo_tune2.njn"));
    ASSERT(readLayered(golden, ref), "SPI-22 committed tomo_tune2.njn decodes");
    ASSERT(got.canvasW == 96 && got.canvasH == 110 && got.durations.size() == 23, "SPI-22 96x110, 23 frames");
    ASSERT(got.durations == ref.durations, "SPI-22 durations match");
    bool partsEq = got.parts.size() == ref.parts.size();
    for (size_t i = 0; partsEq && i < got.parts.size(); ++i) partsEq = std::strcmp(got.parts[i].name, ref.parts[i].name) == 0;
    ASSERT(partsEq, "SPI-22 parts match");
    bool imgEq = got.images.size() == ref.images.size();
    for (size_t i = 0; imgEq && i < got.images.size(); ++i) {
        imgEq = got.images[i].w == ref.images[i].w && got.images[i].h == ref.images[i].h && got.images[i].pixels == ref.images[i].pixels;
    }
    ASSERT(imgEq, "SPI-22 image pool matches");
    bool refEq = got.refs.size() == ref.refs.size();
    for (size_t i = 0; refEq && i < got.refs.size(); ++i) {
        refEq = got.refs[i].imageIndex == ref.refs[i].imageIndex && got.refs[i].offsetX == ref.refs[i].offsetX && got.refs[i].offsetY == ref.refs[i].offsetY;
    }
    ASSERT(refEq, "SPI-22 frame-part references match");
    ASSERT(got.pivotX == 48 && got.pivotY == 109, "SPI-22 new import gets the bottom-centre pivot");
}

static void test23_tomoTune2SystemPalette() {
    const auto r = run(readFile(testdata("tools/testdata/tomo_tune2.aseprite")));
    ASSERT(r.status == SpriteImportStatus::PaletteMismatch, "SPI-23 hue art vs the green ramp → mismatch");
    bool counted = !r.colourIssues.empty();
    for (size_t i = 0; i < r.colourIssues.size(); ++i) {
        counted = counted && r.colourIssues[i].pixels > 0 && r.colourIssues[i].a == 255
                  && (i == 0 || r.colourIssues[i - 1].pixels >= r.colourIssues[i].pixels);
    }
    ASSERT(counted, "SPI-23 issues counted, opaque, sorted by pixels");
}

/// Import must not crash; an ok result must re-read.
static bool survives(const Bytes& b, const SpriteImportOptions& o) {
    const auto r = importSprite(b.data(), b.size(), o);
    if (!r.ok()) return !r.error.empty();
    NjnV2Reader rd;
    if (!rd.open(r.njn.data(), r.njn.size())) return false;
    if (r.kind == SpriteKind::Layered) {
        NjnLayered L;
        return njn2DecodeLayered(rd, L);
    }
    return rd.find(NJN2_CHUNK_META) && rd.find(NJN2_CHUNK_PIXL);
}

static void test24_corpus() {
    std::vector<std::pair<std::string, Bytes>> corpus = {
        {"indexed", indexedFrames(3).build()},
        {"twoLayer", twoLayer().build()},
    };
    {
        AseBuilder a(3, 1, 8);
        a.add(0, AseBuilder::layer("body"));
        a.add(0, AseBuilder::oldPalette(0, {{{1, 2, 3}}}));
        a.add(0, AseBuilder::newPalette(0, {{{1, 2, 3, 4}}}));
        a.add(0, AseBuilder::cel(0, 0, 0, 3, 1, {1, 2, 3}, true));
        a.add(0, AseBuilder::tags({{0, 0, 2, 3, "t"}}));
        corpus.emplace_back("compressed", a.build());
    }
    for (const char* f : {"tools/testdata/tomo_tune2.aseprite", "tools/testdata/tomo_tune.aseprite",
                          "tests/testdata/sprite_import/tomo-intro.aseprite",
                          "tests/testdata/sprite_import/nowplaying.aseprite"}) {
        corpus.emplace_back(f, readFile(testdata(f)));
    }
    Palette tomo;
    tomo.loadPreset("tomo");
    SpriteImportOptions o;
    std::copy(std::begin(tomo.colors), std::end(tomo.colors), o.palette.begin());

    uint32_t seed = 0x2911u;
    auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
    size_t runs = 0;
    for (const auto& [name, file] : corpus) {
        ASSERT(!file.empty(), ("SPI-24 corpus file readable: " + name).c_str());
        bool ok = true;
        const size_t stride = file.size() > 4096 ? 7 : 1;
        for (size_t cut = 0; cut < file.size(); cut += stride) {
            ok = ok && survives(Bytes(file.begin(), file.begin() + static_cast<long>(cut)), o);
            ++runs;
        }
        for (int i = 0; i < 2000; ++i) {
            Bytes b = file;
            const int flips = 1 + static_cast<int>(rnd() % 4);
            for (int k = 0; k < flips; ++k) b[rnd() % b.size()] = static_cast<uint8_t>(rnd());
            ok = ok && survives(b, o);
            ++runs;
        }
        ASSERT(ok, ("SPI-24 truncation/corruption survives: " + name).c_str());
    }
    fprintf(stderr, "  SPI-24 %zu corpus runs\n", runs);
}

static void test25_clipFrameLimit() {
    AseBuilder a = indexedFrames(10);
    a.add(0, AseBuilder::tags({{0, 9, 0, 26, "long"}}));
    const auto r = run(a.build());
    ASSERT(r.status == SpriteImportStatus::TooLarge && contains(r.error, "long"), "SPI-25 260-frame clip → TooLarge, names the tag");
}

static void test26_sheetGridLimit() {
    AseBuilder a(1, 1, 8, 255 * 255 + 1);
    a.add(0, AseBuilder::layer("body"));
    a.add(0, AseBuilder::tags({{0, 0, 0, 0, "t"}}));
    SpriteImportOptions o;
    o.limits.maxFrames = 0xFFFF;
    const auto r = run(a.build(), o);
    ASSERT(r.status == SpriteImportStatus::TooLarge && contains(r.error, "grid"), "SPI-26 65026 frames overflow the sheet grid");
}

static int verifyLayered(const char* path) {
    NjnLayered L;
    const Bytes b = readFile(path);
    if (b.empty() || !readLayered(b, L)) {
        fprintf(stderr, "FAIL: %s does not decode as a layered .njn\n", path);
        return 1;
    }
    printf("%s: %ux%u, %zu frames, %zu parts, %zu images, %zu clips\n", path, L.canvasW, L.canvasH,
           L.durations.size(), L.parts.size(), L.images.size(), L.clips.size());
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 3 && std::strcmp(argv[1], "--verify-layered") == 0) return verifyLayered(argv[2]);

    test01_indexedSheetUntagged();
    test02_tagDirections();
    test03_tagRepeat();
    test04_zeroDuration();
    test05_tagClamp();
    test06_clipNames();
    test07_indexedOutOfRange();
    test08_paletteChunks();
    test09_rgbaMatch();
    test10_rgbaMismatch();
    test11_rgbaCounting();
    test12_layered();
    test13_kindOverride();
    test14_flatZOrder();
    test15_layeredRejects();
    test16_hiddenLayers();
    test17_unsupportedLayers();
    test18_compressed();
    test19_linked();
    test20_caps();
    test21_malformed();
    test22_tomoTune2();
    test23_tomoTune2SystemPalette();
    test24_corpus();
    test25_clipFrameLimit();
    test26_sheetGridLimit();

    printf("sprite_import_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
