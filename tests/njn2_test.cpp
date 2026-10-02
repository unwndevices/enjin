/**
 * @file njn2_test.cpp
 * @brief Tests for the .njn v2 typed-chunk container — reader + writer (issue #76)
 *
 * Test IDs:
 *   NJN2-01  File header: magic, version, numChunks, fileSize written correctly
 *   NJN2-02  Directory: each entry has correct tag, offset, size
 *   NJN2-03  Round-trip: write → read → identical bytes
 *   NJN2-04  Unknown chunk: reader skips without error; find() returns nullptr
 *   NJN2-05  META chunk: encode/decode geometry (cellW, cellH, cols, rows)
 *   NJN2-06  PIXL chunk: raw pixel bytes survive the round-trip byte-for-byte
 *   NJN2-07  ATTR chunk: encode/decode per-tile attributes (flags + kind)
 *   NJN2-08  PALB chunk: encode/decode palette bank table (numBanks × 16 RGB565)
 *   NJN2-09  CLIP chunk: writer emits tag; reader locates it (content opaque)
 *   NJN2-10  Reject bad magic
 *   NJN2-11  Reject wrong version (v1 file)
 *   NJN2-12  Reject truncated file (header too short)
 *   NJN2-13  Reject directory entry pointing beyond fileSize
 *   NJN2-14  Empty file (zero chunks) is valid
 *   NJN2-15  Multiple META+PIXL+ATTR+PALB in one file; all found; ordering preserved
 *
 * Chunk index + njn2ReplaceChunk (Tomodachi #296):
 *   NJN2-16  chunks(): directory order, id, offset, size, data = file + offset
 *   NJN2-17  Replace CLIP on a sheet; META/PIXL byte-for-byte, order kept
 *   NJN2-18  Replace CLIP on a layered file; still decodes, other chunks intact
 *   NJN2-19  Add LPIV where absent: appended last, pivot decodes
 *   NJN2-20  An unknown chunk survives a replace byte-for-byte, in place
 *   NJN2-21  Truncated / malformed input returns an error, never a file
 *   NJN2-22  A duplicated target id is an error (ambiguous replace)
 *   NJN2-23  Bad new-chunk arguments are errors; empty new chunk is allowed
 *   NJN2-24  Header reserved byte is preserved
 */

#include <enjin2/graphics/njn2.hpp>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace enjin2;

static int passes   = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", msg); \
            ++failures; \
        } else { \
            ++passes; \
        } \
    } while (0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Build a minimal v2 file with one META chunk and return the byte buffer.
static std::vector<uint8_t> makeMetaFile(uint8_t cw=8, uint8_t ch=8,
                                          uint8_t cols=2, uint8_t rows=3) {
    NjnV2Writer w;
    njn2WriteMeta(w, cw, ch, cols, rows);
    std::vector<uint8_t> out;
    w.finalise(out);
    return out;
}

/// Build a full PIXL file (META + PIXL chunks).
static std::vector<uint8_t> makePixlFile(uint8_t cw=4, uint8_t ch=4,
                                           uint8_t cols=1, uint8_t rows=2) {
    NjnV2Writer w;
    njn2WriteMeta(w, cw, ch, cols, rows);

    const uint32_t sz = static_cast<uint32_t>(cw) * ch * cols * rows;
    std::vector<uint8_t> px(sz);
    for (uint32_t i = 0; i < sz; ++i) px[i] = static_cast<uint8_t>(i & 0x0F);
    njn2WritePixl(w, px.data(), sz);

    std::vector<uint8_t> out;
    w.finalise(out);
    return out;
}

// ---------------------------------------------------------------------------
// NJN2-01: File header fields
// ---------------------------------------------------------------------------
static void test_NJN2_01_file_header() {
    printf("--- NJN2-01: File header fields ---\n");
    auto buf = makeMetaFile();

    // Must be at least 12 (header) + 12 (1 dir entry) + 4 (META data)
    ASSERT(buf.size() >= 28, "NJN2-01: buffer too small");
    ASSERT(buf[0] == 'N',            "NJN2-01: magic[0]");
    ASSERT(buf[1] == 'J',            "NJN2-01: magic[1]");
    ASSERT(buf[2] == 2,              "NJN2-01: version == 2");
    ASSERT(buf[3] == 0,              "NJN2-01: reserved == 0");

    // numChunks (u32 LE at offset 4) == 1
    uint32_t numChunks = static_cast<uint32_t>(buf[4])
                       | (static_cast<uint32_t>(buf[5]) << 8)
                       | (static_cast<uint32_t>(buf[6]) << 16)
                       | (static_cast<uint32_t>(buf[7]) << 24);
    ASSERT(numChunks == 1, "NJN2-01: numChunks == 1");

    // fileSize (u32 LE at offset 8) == buf.size()
    uint32_t fileSize = static_cast<uint32_t>(buf[8])
                      | (static_cast<uint32_t>(buf[9]) << 8)
                      | (static_cast<uint32_t>(buf[10]) << 16)
                      | (static_cast<uint32_t>(buf[11]) << 24);
    ASSERT(fileSize == static_cast<uint32_t>(buf.size()), "NJN2-01: fileSize == buf.size()");
}

// ---------------------------------------------------------------------------
// NJN2-02: Directory entry fields
// ---------------------------------------------------------------------------
static void test_NJN2_02_directory_entry() {
    printf("--- NJN2-02: Directory entry ---\n");
    auto buf = makeMetaFile(8, 8, 2, 3);

    // Directory starts at offset 12
    const uint8_t* entry = buf.data() + 12;
    ASSERT(entry[0] == 'M', "NJN2-02: tag[0] == 'M'");
    ASSERT(entry[1] == 'E', "NJN2-02: tag[1] == 'E'");
    ASSERT(entry[2] == 'T', "NJN2-02: tag[2] == 'T'");
    ASSERT(entry[3] == 'A', "NJN2-02: tag[3] == 'A'");

    // Offset (u32 LE at entry+4): 12 header + 12 dir = 24
    uint32_t offset = static_cast<uint32_t>(entry[4])
                    | (static_cast<uint32_t>(entry[5]) << 8)
                    | (static_cast<uint32_t>(entry[6]) << 16)
                    | (static_cast<uint32_t>(entry[7]) << 24);
    ASSERT(offset == 24, "NJN2-02: META data offset == 24");

    // Size (u32 LE at entry+8): META chunk is 4 bytes
    uint32_t sz = static_cast<uint32_t>(entry[8])
                | (static_cast<uint32_t>(entry[9]) << 8)
                | (static_cast<uint32_t>(entry[10]) << 16)
                | (static_cast<uint32_t>(entry[11]) << 24);
    ASSERT(sz == 4, "NJN2-02: META size == 4");
}

// ---------------------------------------------------------------------------
// NJN2-03: Round-trip write → read → identical bytes
// ---------------------------------------------------------------------------
static void test_NJN2_03_roundtrip() {
    printf("--- NJN2-03: Round-trip ---\n");

    // Build a file with META + PIXL
    const uint8_t cw=6, ch=6, cols=1, rows=1;
    const uint32_t pixelSz = static_cast<uint32_t>(cw) * ch * cols * rows;
    uint8_t pixels[36];
    for (uint32_t i = 0; i < pixelSz; ++i) pixels[i] = static_cast<uint8_t>((i * 3) & 0x0F);

    NjnV2Writer w;
    njn2WriteMeta(w, cw, ch, cols, rows);
    njn2WritePixl(w, pixels, pixelSz);

    std::vector<uint8_t> written;
    w.finalise(written);

    // Read it back
    NjnV2Reader r;
    ASSERT(r.open(written.data(), written.size()), "NJN2-03: open succeeds");
    ASSERT(r.chunkCount() == 2, "NJN2-03: two chunks");

    // Decode META
    uint8_t rcw=0, rch=0, rcols=0, rrows=0;
    ASSERT(njn2DecodeMeta(r.find(NJN2_CHUNK_META), rcw, rch, rcols, rrows),
           "NJN2-03: decode META");
    ASSERT(rcw == cw && rch == ch && rcols == cols && rrows == rows,
           "NJN2-03: META geometry matches");

    // Decode PIXL — byte-for-byte match
    const NjnV2Chunk* pixlChunk = r.find(NJN2_CHUNK_PIXL);
    ASSERT(pixlChunk != nullptr, "NJN2-03: PIXL chunk present");
    ASSERT(pixlChunk->size == pixelSz, "NJN2-03: PIXL size matches");
    ASSERT(std::memcmp(pixlChunk->data, pixels, pixelSz) == 0,
           "NJN2-03: PIXL data byte-for-byte identical");

    // Re-serialise using helpers (exercises the helper round-trip)
    NjnV2Writer w2;
    njn2WriteMeta(w2, rcw, rch, rcols, rrows);
    njn2WritePixl(w2, pixlChunk->data, pixlChunk->size);

    std::vector<uint8_t> written2;
    w2.finalise(written2);

    ASSERT(written == written2, "NJN2-03: re-serialised bytes are identical");
}

// ---------------------------------------------------------------------------
// NJN2-04: Unknown chunk skip
// ---------------------------------------------------------------------------
static void test_NJN2_04_unknown_chunk_skip() {
    printf("--- NJN2-04: Unknown chunk skip ---\n");

    // Build a file with a META chunk plus a made-up 'ZZZZ' tag
    NjnV2Writer w;
    njn2WriteMeta(w, 4, 4, 1, 1);

    // Write a custom unknown chunk manually
    w.beginChunk(njnTag('Z','Z','Z','Z'));
    w.writeU8(0xDE); w.writeU8(0xAD);
    w.endChunk();

    std::vector<uint8_t> buf;
    w.finalise(buf);

    NjnV2Reader r;
    ASSERT(r.open(buf.data(), buf.size()), "NJN2-04: open succeeds with unknown chunk");
    ASSERT(r.chunkCount() == 2, "NJN2-04: two chunks in directory");
    ASSERT(r.find(NJN2_CHUNK_META) != nullptr, "NJN2-04: META still found");
    ASSERT(r.find(njnTag('Z','Z','Z','Z')) != nullptr, "NJN2-04: ZZZZ located by tag");
    ASSERT(r.find(NJN2_CHUNK_PIXL) == nullptr, "NJN2-04: PIXL not found (not written)");
}

// ---------------------------------------------------------------------------
// NJN2-05: META encode/decode
// ---------------------------------------------------------------------------
static void test_NJN2_05_meta_chunk() {
    printf("--- NJN2-05: META chunk ---\n");

    auto buf = makeMetaFile(12, 16, 3, 4);
    NjnV2Reader r;
    ASSERT(r.open(buf.data(), buf.size()), "NJN2-05: open");

    uint8_t cw=0, ch=0, cols=0, rows=0;
    ASSERT(njn2DecodeMeta(r.find(NJN2_CHUNK_META), cw, ch, cols, rows),
           "NJN2-05: decode META");
    ASSERT(cw == 12,  "NJN2-05: cellW");
    ASSERT(ch == 16,  "NJN2-05: cellH");
    ASSERT(cols == 3, "NJN2-05: cols");
    ASSERT(rows == 4, "NJN2-05: rows");
}

// ---------------------------------------------------------------------------
// NJN2-06: PIXL chunk byte-for-byte
// ---------------------------------------------------------------------------
static void test_NJN2_06_pixl_chunk() {
    printf("--- NJN2-06: PIXL chunk ---\n");

    auto buf = makePixlFile(4, 4, 1, 2);
    NjnV2Reader r;
    ASSERT(r.open(buf.data(), buf.size()), "NJN2-06: open");

    const NjnV2Chunk* c = r.find(NJN2_CHUNK_PIXL);
    ASSERT(c != nullptr, "NJN2-06: PIXL present");
    // 4*4*1*2 = 32 bytes
    ASSERT(c->size == 32, "NJN2-06: PIXL size == 32");

    // Pixel i == (i & 0x0F)
    bool pixOk = true;
    for (uint32_t i = 0; i < 32 && pixOk; ++i) {
        if (c->data[i] != static_cast<uint8_t>(i & 0x0F)) pixOk = false;
    }
    ASSERT(pixOk, "NJN2-06: PIXL data matches written pattern");
}

// ---------------------------------------------------------------------------
// NJN2-07: ATTR chunk encode/decode
// ---------------------------------------------------------------------------
static void test_NJN2_07_attr_chunk() {
    printf("--- NJN2-07: ATTR chunk ---\n");

    // 4 tile attributes
    NjnTileAttr attrs[4] = {
        {0b00000001, 0},   // SOLID
        {0b00000010, 7},   // ONEWAY, kind=7
        {0b00000000, 42},  // passable, kind=42
        {0b00000101, 255}, // SOLID|DIR=1, kind=255
    };

    NjnV2Writer w;
    njn2WriteAttr(w, attrs, 4);
    std::vector<uint8_t> buf;
    w.finalise(buf);

    NjnV2Reader r;
    ASSERT(r.open(buf.data(), buf.size()), "NJN2-07: open");

    std::vector<NjnTileAttr> decoded;
    ASSERT(njn2DecodeAttr(r.find(NJN2_CHUNK_ATTR), decoded), "NJN2-07: decode ATTR");
    ASSERT(decoded.size() == 4, "NJN2-07: 4 attrs decoded");
    ASSERT(decoded[0].flags == 0b00000001 && decoded[0].kind == 0,   "NJN2-07: attr[0]");
    ASSERT(decoded[1].flags == 0b00000010 && decoded[1].kind == 7,   "NJN2-07: attr[1]");
    ASSERT(decoded[2].flags == 0b00000000 && decoded[2].kind == 42,  "NJN2-07: attr[2]");
    ASSERT(decoded[3].flags == 0b00000101 && decoded[3].kind == 255, "NJN2-07: attr[3]");
}

// ---------------------------------------------------------------------------
// NJN2-08: PALB chunk encode/decode
// ---------------------------------------------------------------------------
static void test_NJN2_08_palb_chunk() {
    printf("--- NJN2-08: PALB chunk ---\n");

    // 2 banks × 16 entries each
    uint16_t banks[2 * 16];
    for (int i = 0; i < 32; ++i) banks[i] = static_cast<uint16_t>(i * 100 + 7);

    NjnV2Writer w;
    njn2WritePalb(w, banks, 2);
    std::vector<uint8_t> buf;
    w.finalise(buf);

    NjnV2Reader r;
    ASSERT(r.open(buf.data(), buf.size()), "NJN2-08: open");

    std::vector<uint16_t> decoded;
    uint8_t nb = 0;
    ASSERT(njn2DecodePalb(r.find(NJN2_CHUNK_PALB), decoded, nb), "NJN2-08: decode PALB");
    ASSERT(nb == 2, "NJN2-08: numBanks == 2");
    ASSERT(decoded.size() == 32, "NJN2-08: 32 entries decoded");

    bool ok = true;
    for (int i = 0; i < 32 && ok; ++i) {
        if (decoded[static_cast<size_t>(i)] != static_cast<uint16_t>(i * 100 + 7)) ok = false;
    }
    ASSERT(ok, "NJN2-08: PALB RGB565 values match");
}

// ---------------------------------------------------------------------------
// NJN2-09: CLIP chunk — write via helper, decode via helper
// ---------------------------------------------------------------------------
static void test_NJN2_09_clip_chunk() {
    printf("--- NJN2-09: CLIP chunk ---\n");

    // Build two clips using the typed helper
    NjnClip clips[2];
    // Clip 0: "run", loop, 2 frames
    std::memset(clips[0].name, 0, 16);
    std::memcpy(clips[0].name, "run", 3);
    clips[0].loopMode = NjnLoopMode::Loop;
    clips[0].frames.push_back({0, 100, 0});
    clips[0].frames.push_back({1, 150, 5});

    // Clip 1: "idle", once, 1 frame
    std::memset(clips[1].name, 0, 16);
    std::memcpy(clips[1].name, "idle", 4);
    clips[1].loopMode = NjnLoopMode::Once;
    clips[1].frames.push_back({3, 200, 0});

    NjnV2Writer w;
    njn2WriteClip(w, clips, 2);

    std::vector<uint8_t> buf;
    w.finalise(buf);

    NjnV2Reader r;
    ASSERT(r.open(buf.data(), buf.size()), "NJN2-09: open");

    const NjnV2Chunk* c = r.find(NJN2_CHUNK_CLIP);
    ASSERT(c != nullptr, "NJN2-09: CLIP chunk present");

    // Decode via typed helper
    std::vector<NjnClip> decoded;
    ASSERT(njn2DecodeClip(c, decoded), "NJN2-09: decode succeeds");
    ASSERT(decoded.size() == 2, "NJN2-09: 2 clips decoded");

    // Clip 0 fields
    ASSERT(std::strncmp(decoded[0].name, "run", 3) == 0, "NJN2-09: clip[0] name == 'run'");
    ASSERT(decoded[0].loopMode == NjnLoopMode::Loop,     "NJN2-09: clip[0] loopMode");
    ASSERT(decoded[0].frames.size() == 2,                "NJN2-09: clip[0] 2 frames");
    ASSERT(decoded[0].frames[1].durationMs == 150,       "NJN2-09: clip[0] frame[1].durationMs");
    ASSERT(decoded[0].frames[1].eventId    == 5,         "NJN2-09: clip[0] frame[1].eventId");

    // Clip 1 fields
    ASSERT(std::strncmp(decoded[1].name, "idle", 4) == 0, "NJN2-09: clip[1] name == 'idle'");
    ASSERT(decoded[1].loopMode == NjnLoopMode::Once,      "NJN2-09: clip[1] loopMode");
    ASSERT(decoded[1].frames.size() == 1,                 "NJN2-09: clip[1] 1 frame");
    ASSERT(decoded[1].frames[0].frameIndex == 3,          "NJN2-09: clip[1] frame[0].frameIndex");
}

// ---------------------------------------------------------------------------
// NJN2-10: Reject bad magic
// ---------------------------------------------------------------------------
static void test_NJN2_10_reject_bad_magic() {
    printf("--- NJN2-10: Reject bad magic ---\n");

    auto buf = makeMetaFile();
    buf[0] = 'X'; // corrupt magic

    NjnV2Reader r;
    ASSERT(!r.open(buf.data(), buf.size()), "NJN2-10: bad magic rejected");
}

// ---------------------------------------------------------------------------
// NJN2-11: Reject wrong version (v1 file)
// ---------------------------------------------------------------------------
static void test_NJN2_11_reject_wrong_version() {
    printf("--- NJN2-11: Reject version 1 ---\n");

    auto buf = makeMetaFile();
    buf[2] = 1; // version 1

    NjnV2Reader r;
    ASSERT(!r.open(buf.data(), buf.size()), "NJN2-11: version 1 rejected");
}

// ---------------------------------------------------------------------------
// NJN2-12: Reject truncated file header
// ---------------------------------------------------------------------------
static void test_NJN2_12_reject_truncated() {
    printf("--- NJN2-12: Reject truncated header ---\n");

    NjnV2Reader r;
    const uint8_t tiny[4] = { 'N', 'J', 2, 0 };
    ASSERT(!r.open(tiny, 4), "NJN2-12: 4-byte file rejected");
    ASSERT(!r.open(nullptr, 0), "NJN2-12: null data rejected");
}

// ---------------------------------------------------------------------------
// NJN2-13: Reject directory entry pointing beyond fileSize
// ---------------------------------------------------------------------------
static void test_NJN2_13_reject_oob_entry() {
    printf("--- NJN2-13: Reject out-of-bounds directory entry ---\n");

    auto buf = makeMetaFile();
    // Corrupt the chunk offset in the directory entry (at buf[12+4..15]) to a huge value
    buf[16] = 0xFF;
    buf[17] = 0xFF;
    buf[18] = 0xFF;
    buf[19] = 0x0F; // offset = 0x0FFFFFFF

    NjnV2Reader r;
    ASSERT(!r.open(buf.data(), buf.size()), "NJN2-13: OOB chunk offset rejected");
}

// ---------------------------------------------------------------------------
// NJN2-14: Empty file (zero chunks) is valid
// ---------------------------------------------------------------------------
static void test_NJN2_14_empty_file() {
    printf("--- NJN2-14: Empty file (zero chunks) ---\n");

    NjnV2Writer w;
    std::vector<uint8_t> buf;
    w.finalise(buf);

    // Must be exactly 12 bytes (header only, no directory, no data)
    ASSERT(buf.size() == 12, "NJN2-14: empty file is 12 bytes");

    NjnV2Reader r;
    ASSERT(r.open(buf.data(), buf.size()), "NJN2-14: open succeeds");
    ASSERT(r.chunkCount() == 0, "NJN2-14: zero chunks");
    ASSERT(r.find(NJN2_CHUNK_META) == nullptr, "NJN2-14: find(META) == nullptr");
}

// ---------------------------------------------------------------------------
// NJN2-15: Full multi-chunk file
// ---------------------------------------------------------------------------
static void test_NJN2_15_full_file() {
    printf("--- NJN2-15: Full file (META+PIXL+ATTR+PALB) ---\n");

    const uint8_t cw=8, ch=8, cols=2, rows=2;
    const uint32_t pixSz = static_cast<uint32_t>(cw) * ch * cols * rows;

    std::vector<uint8_t> px(pixSz, 0x05);

    NjnTileAttr attrs[4] = {{1,0},{2,3},{0,9},{3,255}};

    uint16_t banks[16];
    for (int i = 0; i < 16; ++i) banks[i] = static_cast<uint16_t>(i * 1000);

    NjnV2Writer w;
    njn2WriteMeta(w, cw, ch, cols, rows);
    njn2WritePixl(w, px.data(), pixSz);
    njn2WriteAttr(w, attrs, 4);
    njn2WritePalb(w, banks, 1);

    std::vector<uint8_t> buf;
    w.finalise(buf);

    NjnV2Reader r;
    ASSERT(r.open(buf.data(), buf.size()), "NJN2-15: open");
    ASSERT(r.chunkCount() == 4, "NJN2-15: 4 chunks");

    uint8_t rcw=0, rch=0, rcols=0, rrows=0;
    ASSERT(njn2DecodeMeta(r.find(NJN2_CHUNK_META), rcw, rch, rcols, rrows),
           "NJN2-15: META decoded");
    ASSERT(rcw==cw && rch==ch && rcols==cols && rrows==rows, "NJN2-15: META values");

    const NjnV2Chunk* pc = r.find(NJN2_CHUNK_PIXL);
    ASSERT(pc && pc->size == pixSz, "NJN2-15: PIXL size");
    bool pixOk = true;
    for (uint32_t i = 0; i < pixSz && pixOk; ++i)
        if (pc->data[i] != 0x05) pixOk = false;
    ASSERT(pixOk, "NJN2-15: PIXL data");

    std::vector<NjnTileAttr> decoded;
    ASSERT(njn2DecodeAttr(r.find(NJN2_CHUNK_ATTR), decoded), "NJN2-15: ATTR decoded");
    ASSERT(decoded.size() == 4, "NJN2-15: 4 attrs");
    ASSERT(decoded[3].flags == 3 && decoded[3].kind == 255, "NJN2-15: attr[3]");

    std::vector<uint16_t> palbDecoded;
    uint8_t nb = 0;
    ASSERT(njn2DecodePalb(r.find(NJN2_CHUNK_PALB), palbDecoded, nb), "NJN2-15: PALB decoded");
    ASSERT(nb == 1, "NJN2-15: numBanks==1");
    ASSERT(palbDecoded[7] == 7000, "NJN2-15: bank[0][7] == 7000");
}

// ---------------------------------------------------------------------------
// Helpers for the chunk-index / njn2ReplaceChunk tests (Tomodachi #296)
// ---------------------------------------------------------------------------

/// Copy a chunk's bytes out of an opened reader (empty if absent).
static std::vector<uint8_t> chunkBytes(const NjnV2Reader& r, const NjnChunkTag& tag) {
    const NjnV2Chunk* c = r.find(tag);
    if (!c) return {};
    return std::vector<uint8_t>(c->data, c->data + c->size);
}

/// Serialise clips into a bare CLIP chunk payload (no file header).
static std::vector<uint8_t> clipPayload(const std::vector<NjnClip>& clips) {
    NjnV2Writer w;
    njn2WriteClip(w, clips.data(), static_cast<uint8_t>(clips.size()));
    std::vector<uint8_t> file;
    w.finalise(file);
    NjnV2Reader r;
    if (!r.open(file.data(), file.size())) return {};
    return chunkBytes(r, NJN2_CHUNK_CLIP);
}

static NjnClip makeClip(const char* name, NjnLoopMode mode,
                        std::initializer_list<uint16_t> frames) {
    NjnClip c{};
    std::memset(c.name, 0, sizeof(c.name));
    std::strncpy(c.name, name, 15);
    c.loopMode = mode;
    for (uint16_t f : frames) c.frames.push_back({f, 100, 0});
    return c;
}

/// Sheet: META + PIXL + CLIP("idle").
static std::vector<uint8_t> makeSheetWithClip() {
    NjnV2Writer w;
    njn2WriteMeta(w, 4, 4, 2, 1);
    std::vector<uint8_t> px(32);
    for (size_t i = 0; i < px.size(); ++i) px[i] = static_cast<uint8_t>(i & 0x0F);
    njn2WritePixl(w, px.data(), static_cast<uint32_t>(px.size()));
    std::vector<NjnClip> clips = { makeClip("idle", NjnLoopMode::Loop, {0, 1}) };
    njn2WriteClip(w, clips.data(), 1);
    std::vector<uint8_t> out;
    w.finalise(out);
    return out;
}

/// Layered: 2 parts, 2 frames, 2 images, CLIP("walk"), pivot (0,0) → no LPIV.
static NjnLayered makeLayeredAsset() {
    NjnLayered a;
    a.canvasW = 8;
    a.canvasH = 8;
    NjnPartImage img0; img0.w = 2; img0.h = 2; img0.pixels = {1, 2, 3, 4};
    NjnPartImage img1; img1.w = 1; img1.h = 3; img1.pixels = {5, 6, 7};
    a.images = { img0, img1 };
    NjnPart body{}; std::strncpy(body.name, "body", 15);
    NjnPart head{}; std::strncpy(head.name, "head", 15);
    a.parts = { body, head };
    a.refs = { {0, 0, 0}, {1, 2, 1},
               {0, 1, 0}, {NJN2_LAYERED_INVISIBLE, 0, 0} };
    a.durations = { 100, 150 };
    a.clips = { makeClip("walk", NjnLoopMode::Loop, {0, 1}) };
    return a;
}

static std::vector<uint8_t> makeLayeredFile() {
    NjnV2Writer w;
    njn2WriteLayered(w, makeLayeredAsset());
    std::vector<uint8_t> out;
    w.finalise(out);
    return out;
}

/// Directory ids in order.
static std::vector<NjnChunkTag> chunkIds(const NjnV2Reader& r) {
    std::vector<NjnChunkTag> ids;
    for (const NjnV2Chunk& c : r.chunks()) ids.push_back(c.id);
    return ids;
}

// ---------------------------------------------------------------------------
// NJN2-16: chunk-index accessor
// ---------------------------------------------------------------------------
static void test_NJN2_16_chunk_index() {
    printf("--- NJN2-16: chunk index (id, offset, size) ---\n");

    NjnV2Writer w;
    njn2WriteMeta(w, 4, 4, 1, 1);
    w.beginChunk(njnTag('Z','Z','Z','Z'));
    w.writeU8(0xDE); w.writeU8(0xAD); w.writeU8(0xBE);
    w.endChunk();
    w.beginChunk(NJN2_CHUNK_PIXL);
    w.endChunk(); // empty chunk
    std::vector<uint8_t> buf;
    w.finalise(buf);

    NjnV2Reader r;
    ASSERT(r.open(buf.data(), buf.size()), "NJN2-16: open");
    const auto& cs = r.chunks();
    ASSERT(cs.size() == 3, "NJN2-16: three entries");
    ASSERT(cs.size() == r.chunkCount(), "NJN2-16: chunks().size() == chunkCount()");
    if (cs.size() != 3) return;

    ASSERT(cs[0].id == NJN2_CHUNK_META, "NJN2-16: [0] META");
    ASSERT(cs[1].id == njnTag('Z','Z','Z','Z'), "NJN2-16: [1] ZZZZ (unknown) listed");
    ASSERT(cs[2].id == NJN2_CHUNK_PIXL, "NJN2-16: [2] PIXL");

    // Offsets/sizes match the raw directory entries.
    bool dirOk = true;
    for (uint32_t i = 0; i < 3; ++i) {
        const uint8_t* e = buf.data() + NJN2_FILE_HEADER_SIZE + i * NJN2_DIR_ENTRY_SIZE;
        const uint32_t off = e[4] | (e[5] << 8) | (e[6] << 16) | (uint32_t(e[7]) << 24);
        const uint32_t sz  = e[8] | (e[9] << 8) | (e[10] << 16) | (uint32_t(e[11]) << 24);
        if (cs[i].offset != off || cs[i].size != sz) dirOk = false;
        if (cs[i].data != buf.data() + off) dirOk = false;
    }
    ASSERT(dirOk, "NJN2-16: offset/size/data match the directory");
    ASSERT(cs[0].offset == NJN2_FILE_HEADER_SIZE + 3 * NJN2_DIR_ENTRY_SIZE,
           "NJN2-16: first chunk sits right after the directory");
    ASSERT(cs[1].size == 3 && cs[1].data[0] == 0xDE && cs[1].data[2] == 0xBE,
           "NJN2-16: unknown chunk bytes reachable");
    ASSERT(cs[2].size == 0, "NJN2-16: empty chunk has size 0");

    // A failed open leaves an empty index.
    std::vector<uint8_t> bad = buf;
    bad[0] = 'X';
    ASSERT(!r.open(bad.data(), bad.size()), "NJN2-16: bad magic rejected");
    ASSERT(r.chunks().empty(), "NJN2-16: failed open clears the index");
}

// ---------------------------------------------------------------------------
// NJN2-17: replace CLIP on a sheet
// ---------------------------------------------------------------------------
static void test_NJN2_17_replace_clip_sheet() {
    printf("--- NJN2-17: replace CLIP on a sheet ---\n");

    const auto src = makeSheetWithClip();
    NjnV2Reader before;
    ASSERT(before.open(src.data(), src.size()), "NJN2-17: source opens");

    const std::vector<NjnClip> newClips = {
        makeClip("run",  NjnLoopMode::PingPong, {1, 0, 1}),
        makeClip("stop", NjnLoopMode::Once,     {0}),
    };
    const auto payload = clipPayload(newClips);
    ASSERT(!payload.empty(), "NJN2-17: payload built");

    NjnEditResult res = njn2ReplaceChunk(src.data(), src.size(), NJN2_CHUNK_CLIP,
                                         payload.data(), payload.size());
    ASSERT(res.ok(), "NJN2-17: replace ok");
    ASSERT(res.error.empty(), "NJN2-17: no error string");
    if (!res.ok()) { fprintf(stderr, "  error: %s\n", res.error.c_str()); return; }

    NjnV2Reader after;
    ASSERT(after.open(res.njn.data(), res.njn.size()), "NJN2-17: result opens");
    ASSERT(after.chunkCount() == 3, "NJN2-17: chunk count unchanged");
    ASSERT(chunkIds(after) == chunkIds(before), "NJN2-17: chunk order unchanged");
    ASSERT(chunkBytes(after, NJN2_CHUNK_CLIP) == payload, "NJN2-17: CLIP is the new bytes");
    ASSERT(chunkBytes(after, NJN2_CHUNK_META) == chunkBytes(before, NJN2_CHUNK_META),
           "NJN2-17: META byte-for-byte");
    ASSERT(chunkBytes(after, NJN2_CHUNK_PIXL) == chunkBytes(before, NJN2_CHUNK_PIXL),
           "NJN2-17: PIXL byte-for-byte");

    std::vector<NjnClip> decoded;
    ASSERT(njn2DecodeClip(after.find(NJN2_CHUNK_CLIP), decoded), "NJN2-17: CLIP decodes");
    ASSERT(decoded.size() == 2 && std::strcmp(decoded[0].name, "run") == 0 &&
           decoded[0].loopMode == NjnLoopMode::PingPong &&
           decoded[0].frames.size() == 3 && std::strcmp(decoded[1].name, "stop") == 0,
           "NJN2-17: decoded clips are the new ones");

    // The input buffer is untouched.
    ASSERT(src == makeSheetWithClip(), "NJN2-17: input not modified");
}

// ---------------------------------------------------------------------------
// NJN2-18: replace CLIP on a layered file
// ---------------------------------------------------------------------------
static void test_NJN2_18_replace_clip_layered() {
    printf("--- NJN2-18: replace CLIP on a layered file ---\n");

    const auto src = makeLayeredFile();
    NjnV2Reader before;
    ASSERT(before.open(src.data(), src.size()), "NJN2-18: source opens");

    const auto payload = clipPayload({ makeClip("hop", NjnLoopMode::Once, {1, 1, 0}) });
    NjnEditResult res = njn2ReplaceChunk(src.data(), src.size(), NJN2_CHUNK_CLIP,
                                         payload.data(), payload.size());
    ASSERT(res.ok(), "NJN2-18: replace ok");
    if (!res.ok()) { fprintf(stderr, "  error: %s\n", res.error.c_str()); return; }

    NjnV2Reader after;
    ASSERT(after.open(res.njn.data(), res.njn.size()), "NJN2-18: result opens");
    ASSERT(chunkIds(after) == chunkIds(before), "NJN2-18: chunk order unchanged");
    bool othersOk = true;
    for (const NjnV2Chunk& c : before.chunks()) {
        if (c.id == NJN2_CHUNK_CLIP) continue;
        if (chunkBytes(after, c.id) != chunkBytes(before, c.id)) othersOk = false;
    }
    ASSERT(othersOk, "NJN2-18: every non-CLIP chunk byte-for-byte");

    NjnLayered decoded;
    const char* err = nullptr;
    ASSERT(njn2DecodeLayered(after, decoded, &err), "NJN2-18: layered still decodes");
    ASSERT(decoded.clips.size() == 1 && std::strcmp(decoded.clips[0].name, "hop") == 0 &&
           decoded.clips[0].loopMode == NjnLoopMode::Once &&
           decoded.clips[0].frames.size() == 3,
           "NJN2-18: decoded CLIP is the new one");
    ASSERT(decoded.parts.size() == 2 && decoded.images.size() == 2 &&
           decoded.durations.size() == 2 && decoded.durations[1] == 150,
           "NJN2-18: layered body intact");
}

// ---------------------------------------------------------------------------
// NJN2-19: add LPIV where absent
// ---------------------------------------------------------------------------
static void test_NJN2_19_add_lpiv() {
    printf("--- NJN2-19: add LPIV where absent ---\n");

    const auto src = makeLayeredFile();
    NjnV2Reader before;
    ASSERT(before.open(src.data(), src.size()), "NJN2-19: source opens");
    ASSERT(before.find(NJN2_CHUNK_LPIV) == nullptr, "NJN2-19: source has no LPIV");

    // pivot (3, -2): s16 LE
    const uint8_t piv[4] = { 0x03, 0x00, 0xFE, 0xFF };
    NjnEditResult res = njn2ReplaceChunk(src.data(), src.size(), NJN2_CHUNK_LPIV, piv, 4);
    ASSERT(res.ok(), "NJN2-19: add ok");
    if (!res.ok()) { fprintf(stderr, "  error: %s\n", res.error.c_str()); return; }

    NjnV2Reader after;
    ASSERT(after.open(res.njn.data(), res.njn.size()), "NJN2-19: result opens");
    ASSERT(after.chunkCount() == before.chunkCount() + 1, "NJN2-19: one more chunk");
    ASSERT(after.chunks().back().id == NJN2_CHUNK_LPIV, "NJN2-19: LPIV appended last");
    std::vector<NjnChunkTag> idsAfter = chunkIds(after);
    idsAfter.pop_back();
    ASSERT(idsAfter == chunkIds(before), "NJN2-19: existing order unchanged");
    bool othersOk = true;
    for (const NjnV2Chunk& c : before.chunks()) {
        if (chunkBytes(after, c.id) != chunkBytes(before, c.id)) othersOk = false;
    }
    ASSERT(othersOk, "NJN2-19: existing chunks byte-for-byte");

    NjnLayered decoded;
    ASSERT(njn2DecodeLayered(after, decoded), "NJN2-19: layered decodes");
    ASSERT(decoded.pivotX == 3 && decoded.pivotY == -2, "NJN2-19: pivot (3,-2)");

    // Replacing it again changes it in place (no duplicate).
    const uint8_t piv2[4] = { 0x05, 0x00, 0x06, 0x00 };
    NjnEditResult res2 = njn2ReplaceChunk(res.njn.data(), res.njn.size(),
                                          NJN2_CHUNK_LPIV, piv2, 4);
    ASSERT(res2.ok(), "NJN2-19: second replace ok");
    NjnV2Reader again;
    ASSERT(again.open(res2.njn.data(), res2.njn.size()) &&
           again.count(NJN2_CHUNK_LPIV) == 1 &&
           again.chunkCount() == after.chunkCount(),
           "NJN2-19: second replace keeps a single LPIV");
    NjnLayered decoded2;
    ASSERT(njn2DecodeLayered(again, decoded2) &&
           decoded2.pivotX == 5 && decoded2.pivotY == 6, "NJN2-19: pivot (5,6)");
}

// ---------------------------------------------------------------------------
// NJN2-20: unknown chunk survives a replace
// ---------------------------------------------------------------------------
static void test_NJN2_20_unknown_survives() {
    printf("--- NJN2-20: unknown chunk survives a replace ---\n");

    NjnV2Writer w;
    njn2WriteMeta(w, 4, 4, 1, 1);
    w.beginChunk(njnTag('Z','Z','Z','Z'));
    for (int i = 0; i < 37; ++i) w.writeU8(static_cast<uint8_t>(i * 7 + 1));
    w.endChunk();
    std::vector<NjnClip> clips = { makeClip("a", NjnLoopMode::Loop, {0}) };
    njn2WriteClip(w, clips.data(), 1);
    w.beginChunk(njnTag('x','y','z','\0'));
    w.writeU8(0x42);
    w.endChunk();
    std::vector<uint8_t> src;
    w.finalise(src);

    NjnV2Reader before;
    ASSERT(before.open(src.data(), src.size()), "NJN2-20: source opens");

    const auto payload = clipPayload({ makeClip("b", NjnLoopMode::Once, {0, 0}) });
    NjnEditResult res = njn2ReplaceChunk(src.data(), src.size(), NJN2_CHUNK_CLIP,
                                         payload.data(), payload.size());
    ASSERT(res.ok(), "NJN2-20: replace ok");
    if (!res.ok()) { fprintf(stderr, "  error: %s\n", res.error.c_str()); return; }

    NjnV2Reader after;
    ASSERT(after.open(res.njn.data(), res.njn.size()), "NJN2-20: result opens");
    ASSERT(chunkIds(after) == chunkIds(before), "NJN2-20: order kept, unknowns in place");
    ASSERT(chunkBytes(after, njnTag('Z','Z','Z','Z')) ==
           chunkBytes(before, njnTag('Z','Z','Z','Z')), "NJN2-20: ZZZZ byte-for-byte");
    ASSERT(chunkBytes(after, njnTag('Z','Z','Z','Z')).size() == 37, "NJN2-20: ZZZZ size kept");
    ASSERT(chunkBytes(after, njnTag('x','y','z','\0')) ==
           std::vector<uint8_t>{0x42}, "NJN2-20: trailing unknown byte-for-byte");
    ASSERT(chunkBytes(after, NJN2_CHUNK_CLIP) == payload, "NJN2-20: CLIP replaced");

    // Replacing an unknown id works the same way.
    const uint8_t z[2] = { 9, 9 };
    NjnEditResult resZ = njn2ReplaceChunk(src.data(), src.size(),
                                          njnTag('Z','Z','Z','Z'), z, 2);
    NjnV2Reader afterZ;
    ASSERT(resZ.ok() && afterZ.open(resZ.njn.data(), resZ.njn.size()) &&
           chunkBytes(afterZ, njnTag('Z','Z','Z','Z')) == std::vector<uint8_t>({9, 9}) &&
           chunkIds(afterZ) == chunkIds(before),
           "NJN2-20: an unknown id can itself be replaced");
}

// ---------------------------------------------------------------------------
// NJN2-21: truncated / malformed input is an error
// ---------------------------------------------------------------------------
static void test_NJN2_21_truncated_input() {
    printf("--- NJN2-21: truncated input returns an error ---\n");

    const auto src = makeSheetWithClip();
    const auto payload = clipPayload({ makeClip("x", NjnLoopMode::Loop, {0}) });

    bool allErr = true;
    bool allEmpty = true;
    bool allMsg = true;
    for (size_t n = 0; n < src.size(); ++n) {
        NjnEditResult res = njn2ReplaceChunk(src.data(), n, NJN2_CHUNK_CLIP,
                                             payload.data(), payload.size());
        if (res.ok()) allErr = false;
        if (!res.njn.empty()) allEmpty = false;
        if (res.error.empty()) allMsg = false;
    }
    ASSERT(allErr, "NJN2-21: every truncation is an error");
    ASSERT(allEmpty, "NJN2-21: no file returned on error");
    ASSERT(allMsg, "NJN2-21: every error carries a message");

    NjnEditResult nul = njn2ReplaceChunk(nullptr, 0, NJN2_CHUNK_CLIP,
                                         payload.data(), payload.size());
    ASSERT(!nul.ok() && !nul.error.empty(), "NJN2-21: null input is an error");

    std::vector<uint8_t> v1 = src;
    v1[2] = 1;
    NjnEditResult ver = njn2ReplaceChunk(v1.data(), v1.size(), NJN2_CHUNK_CLIP,
                                         payload.data(), payload.size());
    ASSERT(!ver.ok() && !ver.error.empty(), "NJN2-21: wrong version is an error");

    std::vector<uint8_t> oob = src;
    oob[16] = 0xFF; oob[17] = 0xFF; oob[18] = 0xFF; oob[19] = 0x0F;
    NjnEditResult o = njn2ReplaceChunk(oob.data(), oob.size(), NJN2_CHUNK_CLIP,
                                       payload.data(), payload.size());
    ASSERT(!o.ok() && !o.error.empty(), "NJN2-21: out-of-bounds directory is an error");

    // numChunks * 12 + 12 wraps uint32: must not walk entries past the buffer.
    std::vector<uint8_t> wrap(64, 0);
    wrap[0] = 'N'; wrap[1] = 'J'; wrap[2] = NJN2_VERSION;
    const uint32_t hugeCount = UINT32_MAX / NJN2_DIR_ENTRY_SIZE; // dirBytes + 12 wraps to 8
    for (int i = 0; i < 4; ++i) wrap[4 + i] = static_cast<uint8_t>(hugeCount >> (8 * i));
    wrap[8] = 64;
    // Entries that fit are valid (offset 12, size 0), so only the wrap stops it.
    for (size_t e = NJN2_FILE_HEADER_SIZE; e + NJN2_DIR_ENTRY_SIZE <= wrap.size();
         e += NJN2_DIR_ENTRY_SIZE) {
        wrap[e + 4] = static_cast<uint8_t>(NJN2_FILE_HEADER_SIZE);
    }
    NjnV2Reader wr;
    ASSERT(!wr.open(wrap.data(), wrap.size()), "NJN2-21: wrapping chunk count rejected");
    NjnEditResult wres = njn2ReplaceChunk(wrap.data(), wrap.size(), NJN2_CHUNK_CLIP,
                                          payload.data(), payload.size());
    ASSERT(!wres.ok() && !wres.error.empty(), "NJN2-21: wrapping chunk count is an error");
}

// ---------------------------------------------------------------------------
// NJN2-22: duplicate target id is an error
// ---------------------------------------------------------------------------
static void test_NJN2_22_duplicate_target() {
    printf("--- NJN2-22: duplicated target id is an error ---\n");

    NjnV2Writer w;
    std::vector<NjnClip> clips = { makeClip("a", NjnLoopMode::Loop, {0}) };
    njn2WriteClip(w, clips.data(), 1);
    njn2WriteClip(w, clips.data(), 1);
    njn2WriteMeta(w, 4, 4, 1, 1);
    std::vector<uint8_t> src;
    w.finalise(src);

    const uint8_t one[1] = { 0 };
    NjnEditResult res = njn2ReplaceChunk(src.data(), src.size(), NJN2_CHUNK_CLIP, one, 1);
    ASSERT(!res.ok() && res.njn.empty() && !res.error.empty(),
           "NJN2-22: duplicate CLIP refused");

    // A duplicate of some *other* id is not this call's business.
    NjnEditResult meta = njn2ReplaceChunk(src.data(), src.size(), NJN2_CHUNK_META, one, 1);
    ASSERT(meta.ok(), "NJN2-22: replacing a non-duplicated id still works");
    NjnV2Reader r;
    ASSERT(meta.ok() && r.open(meta.njn.data(), meta.njn.size()) &&
           r.count(NJN2_CHUNK_CLIP) == 2, "NJN2-22: both CLIP copies kept");
}

// ---------------------------------------------------------------------------
// NJN2-23: new-chunk argument validation
// ---------------------------------------------------------------------------
static void test_NJN2_23_new_chunk_args() {
    printf("--- NJN2-23: new-chunk argument validation ---\n");

    const auto src = makeSheetWithClip();

    NjnEditResult nul = njn2ReplaceChunk(src.data(), src.size(), NJN2_CHUNK_CLIP, nullptr, 4);
    ASSERT(!nul.ok() && nul.njn.empty() && !nul.error.empty(),
           "NJN2-23: null bytes with non-zero size is an error");

    NjnEditResult empty = njn2ReplaceChunk(src.data(), src.size(), NJN2_CHUNK_CLIP, nullptr, 0);
    ASSERT(empty.ok(), "NJN2-23: an empty new chunk is allowed");
    NjnV2Reader r;
    ASSERT(empty.ok() && r.open(empty.njn.data(), empty.njn.size()) &&
           r.find(NJN2_CHUNK_CLIP) != nullptr && r.find(NJN2_CHUNK_CLIP)->size == 0,
           "NJN2-23: empty CLIP present with size 0");
}

// ---------------------------------------------------------------------------
// NJN2-24: header reserved byte preserved
// ---------------------------------------------------------------------------
static void test_NJN2_24_header_preserved() {
    printf("--- NJN2-24: header reserved byte preserved ---\n");

    auto src = makeSheetWithClip();
    src[3] = 0x5A; // reserved; the reader ignores it
    const uint8_t one[1] = { 0 };
    NjnEditResult res = njn2ReplaceChunk(src.data(), src.size(), NJN2_CHUNK_CLIP, one, 1);
    ASSERT(res.ok(), "NJN2-24: replace ok");
    ASSERT(res.ok() && res.njn.size() >= 4 && res.njn[0] == 'N' && res.njn[1] == 'J' &&
           res.njn[2] == NJN2_VERSION && res.njn[3] == 0x5A,
           "NJN2-24: magic, version, reserved byte kept");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    printf("=== njn2_test: .njn v2 typed-chunk container ===\n");

    test_NJN2_01_file_header();
    test_NJN2_02_directory_entry();
    test_NJN2_03_roundtrip();
    test_NJN2_04_unknown_chunk_skip();
    test_NJN2_05_meta_chunk();
    test_NJN2_06_pixl_chunk();
    test_NJN2_07_attr_chunk();
    test_NJN2_08_palb_chunk();
    test_NJN2_09_clip_chunk();
    test_NJN2_10_reject_bad_magic();
    test_NJN2_11_reject_wrong_version();
    test_NJN2_12_reject_truncated();
    test_NJN2_13_reject_oob_entry();
    test_NJN2_14_empty_file();
    test_NJN2_15_full_file();
    test_NJN2_16_chunk_index();
    test_NJN2_17_replace_clip_sheet();
    test_NJN2_18_replace_clip_layered();
    test_NJN2_19_add_lpiv();
    test_NJN2_20_unknown_survives();
    test_NJN2_21_truncated_input();
    test_NJN2_22_duplicate_target();
    test_NJN2_23_new_chunk_args();
    test_NJN2_24_header_preserved();

    printf("=== Results: %d passed, %d failed ===\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
