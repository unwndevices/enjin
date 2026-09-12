/**
 * @file njn2_layered_test.cpp
 * @brief Tests for the .njn v2 layered sprite chunk contract (issue #93)
 *
 * Test IDs:
 *   LAY-01  Writer emits the exact golden byte stream (byte-identical with the
 *           Python mirror in enjin_assets/emit.py).
 *   LAY-02  Round-trip: write → read → decode → identical canvas, images, parts,
 *           frame references, offsets, invisibility, durations, clips.
 *   LAY-03  Re-serialising a decoded asset reproduces the original bytes.
 *   LAY-04  Unknown chunks are skipped; a layered asset still decodes.
 *   LAY-05  Missing required chunk rejected.
 *   LAY-06  Duplicate required chunk rejected.
 *   LAY-07  Unsupported layered schema version rejected.
 *   LAY-08  Zero canvas extent rejected.
 *   LAY-09  Zero frame/part/image count rejected.
 *   LAY-10  Truncated LHDR rejected.
 *   LAY-11  Truncated LIMG pixels rejected.
 *   LAY-12  Zero-size part image rejected.
 *   LAY-13  LIMG trailing bytes rejected.
 *   LAY-14  LPRT size mismatch rejected.
 *   LAY-15  LREF size mismatch under overflowing counts rejected (no OOB read).
 *   LAY-16  Invalid part-image reference rejected.
 *   LAY-17  LDUR size mismatch rejected.
 *   LAY-18  CLIP frame index out of range rejected.
 *   LAY-19  Duplicate CLIP rejected.
 *   LAY-20  Malformed directory entry rejected by the container reader.
 */

#include <enjin2/graphics/njn2.hpp>
#include <cstdio>
#include <cstring>
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

/// Decode a hex string into bytes.
static std::vector<uint8_t> fromHex(const char* hex) {
    std::vector<uint8_t> out;
    const size_t n = std::strlen(hex);
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    for (size_t i = 0; i + 1 < n; i += 2) {
        out.push_back(static_cast<uint8_t>((nib(hex[i]) << 4) | nib(hex[i + 1])));
    }
    return out;
}

/// The canonical layered fixture: 4x4 canvas, 2 frames, 2 parts, 2 images,
/// with an invisible reference, a reused image at a moving offset, and a
/// default clip.  Shared by the Python mirror's golden test.
static NjnLayered makeFixture() {
    NjnLayered l;
    l.schemaVersion = NJN2_LAYERED_SCHEMA_VERSION;
    l.canvasW = 4;
    l.canvasH = 4;

    NjnPartImage img0;
    img0.w = 2;
    img0.h = 2;
    img0.pixels = {0, 1, 2, 3};
    l.images.push_back(std::move(img0));

    NjnPartImage img1;
    img1.w = 1;
    img1.h = 1;
    img1.pixels = {5};
    l.images.push_back(std::move(img1));

    NjnPart body;
    std::memset(body.name, 0, sizeof(body.name));
    std::memcpy(body.name, "body", 4);
    l.parts.push_back(body);

    NjnPart eye;
    std::memset(eye.name, 0, sizeof(eye.name));
    std::memcpy(eye.name, "eye", 3);
    l.parts.push_back(eye);

    // Frame-major refs: frame0 = {body@img0 (0,0), eye@img1 (1,-2)},
    //                    frame1 = {body@img0 (-1,0), eye invisible}.
    l.refs.push_back({0, 0, 0});
    l.refs.push_back({1, 1, -2});
    l.refs.push_back({0, -1, 0});
    l.refs.push_back({NJN2_LAYERED_INVISIBLE, 0, 0});

    l.durations = {100, 150};

    NjnClip clip;
    std::memset(clip.name, 0, sizeof(clip.name));
    std::memcpy(clip.name, "default", 7);
    clip.loopMode = NjnLoopMode::Loop;
    clip.frames.push_back({0, 100, 0});
    clip.frames.push_back({1, 150, 0});
    l.clips.push_back(std::move(clip));

    return l;
}

static std::vector<uint8_t> writeLayered(const NjnLayered& l) {
    NjnV2Writer w;
    njn2WriteLayered(w, l);
    std::vector<uint8_t> out;
    w.finalise(out);
    return out;
}

/// Low-level writer with structural toggles for negative tests.
static std::vector<uint8_t> writeVariant(const NjnLayered& l,
                                         bool dupLhdr, bool omitLdur, bool dupClip) {
    const uint16_t nf = static_cast<uint16_t>(l.durations.size());
    const uint16_t np = static_cast<uint16_t>(l.parts.size());
    const uint16_t ni = static_cast<uint16_t>(l.images.size());

    NjnV2Writer w;

    auto lhdr = [&]() {
        w.beginChunk(NJN2_CHUNK_LHDR);
        w.writeU8(l.schemaVersion);
        w.writeU16LE(l.canvasW);
        w.writeU16LE(l.canvasH);
        w.writeU16LE(nf);
        w.writeU16LE(np);
        w.writeU16LE(ni);
        w.endChunk();
    };
    lhdr();
    if (dupLhdr) lhdr();

    w.beginChunk(NJN2_CHUNK_LIMG);
    for (const auto& img : l.images) {
        w.writeU16LE(img.w);
        w.writeU16LE(img.h);
        w.writeBytes(img.pixels.data(), img.pixels.size());
    }
    w.endChunk();

    w.beginChunk(NJN2_CHUNK_LPRT);
    for (const auto& p : l.parts) {
        for (int k = 0; k < 16; ++k) w.writeU8(k < 15 ? static_cast<uint8_t>(p.name[k]) : 0u);
    }
    w.endChunk();

    w.beginChunk(NJN2_CHUNK_LREF);
    for (const auto& r : l.refs) {
        w.writeU16LE(r.imageIndex);
        w.writeU16LE(static_cast<uint16_t>(r.offsetX));
        w.writeU16LE(static_cast<uint16_t>(r.offsetY));
    }
    w.endChunk();

    if (!omitLdur) {
        w.beginChunk(NJN2_CHUNK_LDUR);
        for (uint16_t d : l.durations) w.writeU16LE(d);
        w.endChunk();
    }

    if (!l.clips.empty()) {
        njn2WriteClip(w, l.clips.data(), static_cast<uint8_t>(l.clips.size()));
        if (dupClip) njn2WriteClip(w, l.clips.data(), static_cast<uint8_t>(l.clips.size()));
    }

    std::vector<uint8_t> out;
    w.finalise(out);
    return out;
}

/// Overwrite a directory entry's size field (entry 0 = first chunk in the file).
static void setChunkSize(std::vector<uint8_t>& buf, int entryIndex, uint32_t newSize) {
    uint8_t* e = buf.data() + NJN2_FILE_HEADER_SIZE + entryIndex * NJN2_DIR_ENTRY_SIZE + 8;
    e[0] = static_cast<uint8_t>(newSize & 0xFF);
    e[1] = static_cast<uint8_t>((newSize >> 8) & 0xFF);
    e[2] = static_cast<uint8_t>((newSize >> 16) & 0xFF);
    e[3] = static_cast<uint8_t>((newSize >> 24) & 0xFF);
}

/// Read a directory entry's data offset (entry 0 = first chunk in the file).
static uint32_t chunkDataOffset(const std::vector<uint8_t>& buf, int entryIndex) {
    const uint8_t* e = buf.data() + NJN2_FILE_HEADER_SIZE + entryIndex * NJN2_DIR_ENTRY_SIZE + 4;
    return static_cast<uint32_t>(e[0])
         | (static_cast<uint32_t>(e[1]) << 8)
         | (static_cast<uint32_t>(e[2]) << 16)
         | (static_cast<uint32_t>(e[3]) << 24);
}

// ---------------------------------------------------------------------------
// LAY-01: golden bytes (byte-identical with the Python mirror)
// ---------------------------------------------------------------------------
static void test_LAY_01_golden_bytes() {
    printf("--- LAY-01: Golden byte stream ---\n");
    const std::vector<uint8_t> golden = fromHex(
        "4e4a020006000000c50000004c484452540000000b0000004c494d475f000000"
        "0d0000004c5052546c000000200000004c5245468c000000180000004c445552"
        "a400000004000000434c4950a80000001d000000010400040002000200020002"
        "000200000102030100010005626f647900000000000000000000000065796500"
        "00000000000000000000000000000000000001000100feff0000ffff0000ffff"
        "00000000640096000164656661756c7400000000000000000001020000640000"
        "0100960000");
    const std::vector<uint8_t> written = writeLayered(makeFixture());
    ASSERT(written.size() == golden.size(), "LAY-01: size matches");
    ASSERT(written == golden, "LAY-01: bytes identical to Python mirror");
}

// ---------------------------------------------------------------------------
// LAY-02: round-trip decode
// ---------------------------------------------------------------------------
static void test_LAY_02_roundtrip() {
    printf("--- LAY-02: Round-trip ---\n");
    const NjnLayered src = makeFixture();
    const std::vector<uint8_t> buf = writeLayered(src);

    NjnV2Reader r;
    ASSERT(r.open(buf.data(), buf.size()), "LAY-02: open succeeds");

    NjnLayered out;
    const char* err = nullptr;
    ASSERT(njn2DecodeLayered(r, out, &err), "LAY-02: decode succeeds");

    ASSERT(out.schemaVersion == NJN2_LAYERED_SCHEMA_VERSION, "LAY-02: schema version");
    ASSERT(out.canvasW == 4 && out.canvasH == 4, "LAY-02: canvas extent");

    ASSERT(out.images.size() == 2, "LAY-02: two images");
    ASSERT(out.images[0].w == 2 && out.images[0].h == 2, "LAY-02: image0 dims");
    ASSERT(out.images[0].pixels == (std::vector<uint8_t>{0, 1, 2, 3}), "LAY-02: image0 pixels");
    ASSERT(out.images[1].w == 1 && out.images[1].h == 1, "LAY-02: image1 dims");
    ASSERT(out.images[1].pixels == (std::vector<uint8_t>{5}), "LAY-02: image1 pixels");

    ASSERT(out.parts.size() == 2, "LAY-02: two parts");
    ASSERT(std::strncmp(out.parts[0].name, "body", 4) == 0, "LAY-02: part0 name");
    ASSERT(std::strncmp(out.parts[1].name, "eye", 3) == 0, "LAY-02: part1 name");

    ASSERT(out.refs.size() == 4, "LAY-02: four refs");
    ASSERT(out.refs[0].imageIndex == 0 && out.refs[0].offsetX == 0 && out.refs[0].offsetY == 0,
           "LAY-02: ref[0]");
    ASSERT(out.refs[1].imageIndex == 1 && out.refs[1].offsetX == 1 && out.refs[1].offsetY == -2,
           "LAY-02: ref[1] negative offset");
    ASSERT(out.refs[2].imageIndex == 0 && out.refs[2].offsetX == -1 && out.refs[2].offsetY == 0,
           "LAY-02: ref[2] moving negative offset");
    ASSERT(out.refs[3].imageIndex == NJN2_LAYERED_INVISIBLE, "LAY-02: ref[3] invisible");

    ASSERT(out.durations == (std::vector<uint16_t>{100, 150}), "LAY-02: durations");

    ASSERT(out.clips.size() == 1, "LAY-02: one clip");
    ASSERT(std::strncmp(out.clips[0].name, "default", 7) == 0, "LAY-02: clip name");
    ASSERT(out.clips[0].loopMode == NjnLoopMode::Loop, "LAY-02: clip loop mode");
    ASSERT(out.clips[0].frames.size() == 2, "LAY-02: clip frames");
    ASSERT(out.clips[0].frames[1].durationMs == 150, "LAY-02: clip frame duration");
}

// ---------------------------------------------------------------------------
// LAY-03: re-serialise a decoded asset
// ---------------------------------------------------------------------------
static void test_LAY_03_reserialise() {
    printf("--- LAY-03: Re-serialise ---\n");
    const std::vector<uint8_t> buf = writeLayered(makeFixture());

    NjnV2Reader r;
    r.open(buf.data(), buf.size());
    NjnLayered out;
    njn2DecodeLayered(r, out);

    const std::vector<uint8_t> again = writeLayered(out);
    ASSERT(buf == again, "LAY-03: re-serialised bytes identical");
}

// ---------------------------------------------------------------------------
// LAY-04: unknown chunk skipped
// ---------------------------------------------------------------------------
static void test_LAY_04_unknown_chunk() {
    printf("--- LAY-04: Unknown chunk skip ---\n");
    NjnV2Writer w;
    njn2WriteLayered(w, makeFixture());
    w.beginChunk(njnTag('Z', 'Z', 'Z', 'Z'));
    w.writeU8(0xDE);
    w.writeU8(0xAD);
    w.endChunk();
    std::vector<uint8_t> buf;
    w.finalise(buf);

    NjnV2Reader r;
    ASSERT(r.open(buf.data(), buf.size()), "LAY-04: open with unknown chunk");
    NjnLayered out;
    ASSERT(njn2DecodeLayered(r, out), "LAY-04: decode ignores unknown chunk");
    ASSERT(out.canvasW == 4, "LAY-04: canvas still decodes");
}

// ---------------------------------------------------------------------------
// LAY-05: missing required chunk
// ---------------------------------------------------------------------------
static void test_LAY_05_missing_required() {
    printf("--- LAY-05: Missing required chunk ---\n");
    std::vector<uint8_t> buf = writeVariant(makeFixture(), false, true, false); // omit LDUR
    NjnV2Reader r;
    ASSERT(r.open(buf.data(), buf.size()), "LAY-05: open succeeds");
    NjnLayered out;
    const char* err = nullptr;
    ASSERT(!njn2DecodeLayered(r, out, &err), "LAY-05: missing chunk rejected");
    ASSERT(err != nullptr && std::strstr(err, "missing") != nullptr, "LAY-05: reason");
}

// ---------------------------------------------------------------------------
// LAY-06: duplicate required chunk
// ---------------------------------------------------------------------------
static void test_LAY_06_duplicate_required() {
    printf("--- LAY-06: Duplicate required chunk ---\n");
    std::vector<uint8_t> buf = writeVariant(makeFixture(), true, false, false); // two LHDR
    NjnV2Reader r;
    ASSERT(r.open(buf.data(), buf.size()), "LAY-06: open succeeds");
    NjnLayered out;
    const char* err = nullptr;
    ASSERT(!njn2DecodeLayered(r, out, &err), "LAY-06: duplicate chunk rejected");
    ASSERT(err != nullptr && std::strstr(err, "duplicate") != nullptr, "LAY-06: reason");
}

// ---------------------------------------------------------------------------
// LAY-07: unsupported schema version
// ---------------------------------------------------------------------------
static void test_LAY_07_schema_version() {
    printf("--- LAY-07: Unsupported schema version ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    buf[chunkDataOffset(buf, 0)] = 2; // schemaVersion field
    NjnV2Reader r;
    r.open(buf.data(), buf.size());
    NjnLayered out;
    const char* err = nullptr;
    ASSERT(!njn2DecodeLayered(r, out, &err), "LAY-07: schema 2 rejected");
    ASSERT(err != nullptr && std::strstr(err, "schema") != nullptr, "LAY-07: reason");
}

// ---------------------------------------------------------------------------
// LAY-08: zero canvas extent
// ---------------------------------------------------------------------------
static void test_LAY_08_zero_canvas() {
    printf("--- LAY-08: Zero canvas extent ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    uint32_t off = chunkDataOffset(buf, 0);
    buf[off + 1] = 0; // canvasW low byte → 0
    NjnV2Reader r;
    r.open(buf.data(), buf.size());
    NjnLayered out;
    ASSERT(!njn2DecodeLayered(r, out), "LAY-08: zero canvas rejected");
}

// ---------------------------------------------------------------------------
// LAY-09: zero frame/part count
// ---------------------------------------------------------------------------
static void test_LAY_09_zero_counts() {
    printf("--- LAY-09: Zero frame/part/image count ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    uint32_t off = chunkDataOffset(buf, 0);
    buf[off + 5] = 0; // numFrames low byte → 0
    NjnV2Reader r;
    r.open(buf.data(), buf.size());
    NjnLayered out;
    ASSERT(!njn2DecodeLayered(r, out), "LAY-09: zero frame count rejected");

    // numImages = 0 (with valid frames/parts) must also be rejected.
    std::vector<uint8_t> buf2 = writeLayered(makeFixture());
    uint32_t off2 = chunkDataOffset(buf2, 0);
    buf2[off2 + 9] = 0; // numImages low byte → 0
    NjnV2Reader r2;
    r2.open(buf2.data(), buf2.size());
    NjnLayered out2;
    const char* err = nullptr;
    ASSERT(!njn2DecodeLayered(r2, out2, &err), "LAY-09: zero image count rejected");
    ASSERT(err != nullptr && std::strstr(err, "image pool") != nullptr, "LAY-09: reason");
}

// ---------------------------------------------------------------------------
// LAY-10: truncated LHDR
// ---------------------------------------------------------------------------
static void test_LAY_10_truncated_lhdr() {
    printf("--- LAY-10: Truncated LHDR ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    setChunkSize(buf, 0, 10); // LHDR declared 10 bytes instead of 11
    NjnV2Reader r;
    r.open(buf.data(), buf.size());
    NjnLayered out;
    const char* err = nullptr;
    ASSERT(!njn2DecodeLayered(r, out, &err), "LAY-10: truncated LHDR rejected");
    ASSERT(err != nullptr && std::strstr(err, "LHDR") != nullptr, "LAY-10: reason");
}

// ---------------------------------------------------------------------------
// LAY-11: truncated LIMG pixels
// ---------------------------------------------------------------------------
static void test_LAY_11_truncated_limg() {
    printf("--- LAY-11: Truncated LIMG pixels ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    setChunkSize(buf, 1, 6); // LIMG declared 6 bytes (img0 header + 2 px) instead of 13
    NjnV2Reader r;
    r.open(buf.data(), buf.size());
    NjnLayered out;
    ASSERT(!njn2DecodeLayered(r, out), "LAY-11: truncated LIMG rejected");
}

// ---------------------------------------------------------------------------
// LAY-12: zero-size part image
// ---------------------------------------------------------------------------
static void test_LAY_12_zero_image() {
    printf("--- LAY-12: Zero-size part image ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    uint32_t off = chunkDataOffset(buf, 1); // LIMG data
    buf[off] = 0; // img0 w low byte → 0
    NjnV2Reader r;
    r.open(buf.data(), buf.size());
    NjnLayered out;
    ASSERT(!njn2DecodeLayered(r, out), "LAY-12: zero-size image rejected");
}

// ---------------------------------------------------------------------------
// LAY-13: LIMG trailing bytes
// ---------------------------------------------------------------------------
static void test_LAY_13_limg_trailing() {
    printf("--- LAY-13: LIMG trailing bytes ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    setChunkSize(buf, 1, 14); // LIMG declared 14 bytes instead of 13
    NjnV2Reader r;
    r.open(buf.data(), buf.size());
    NjnLayered out;
    ASSERT(!njn2DecodeLayered(r, out), "LAY-13: LIMG trailing bytes rejected");
}

// ---------------------------------------------------------------------------
// LAY-14: LPRT size mismatch
// ---------------------------------------------------------------------------
static void test_LAY_14_lprt_size() {
    printf("--- LAY-14: LPRT size mismatch ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    setChunkSize(buf, 2, 16); // LPRT declared 16 instead of 32
    NjnV2Reader r;
    r.open(buf.data(), buf.size());
    NjnLayered out;
    ASSERT(!njn2DecodeLayered(r, out), "LAY-14: LPRT size mismatch rejected");
}

// ---------------------------------------------------------------------------
// LAY-15: LREF size mismatch under overflowing counts
// ---------------------------------------------------------------------------
static void test_LAY_15_lref_overflow() {
    printf("--- LAY-15: LREF size mismatch under overflowing counts ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    uint32_t off = chunkDataOffset(buf, 0);
    // Set numFrames to 0xFFFF (numParts stays 2): numFrames*numParts*6 would
    // overflow a 32-bit multiply, but the decoder computes the expected LREF
    // size in 64-bit and must reject via a size mismatch rather than read OOB.
    buf[off + 5] = 0xFF; buf[off + 6] = 0xFF;
    NjnV2Reader r;
    r.open(buf.data(), buf.size());
    NjnLayered out;
    const char* err = nullptr;
    ASSERT(!njn2DecodeLayered(r, out, &err), "LAY-15: overflowing ref count rejected");
    ASSERT(err != nullptr && std::strstr(err, "LREF") != nullptr, "LAY-15: reason");
}

// ---------------------------------------------------------------------------
// LAY-16: invalid part-image reference
// ---------------------------------------------------------------------------
static void test_LAY_16_invalid_ref() {
    printf("--- LAY-16: Invalid part-image reference ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    uint32_t off = chunkDataOffset(buf, 3); // LREF data
    buf[off] = 0x05; // first ref imageIndex → 5 (>= numImages 2, not invisible)
    NjnV2Reader r;
    r.open(buf.data(), buf.size());
    NjnLayered out;
    const char* err = nullptr;
    ASSERT(!njn2DecodeLayered(r, out, &err), "LAY-16: invalid reference rejected");
    ASSERT(err != nullptr && std::strstr(err, "reference") != nullptr, "LAY-16: reason");
}

// ---------------------------------------------------------------------------
// LAY-17: LDUR size mismatch
// ---------------------------------------------------------------------------
static void test_LAY_17_ldur_size() {
    printf("--- LAY-17: LDUR size mismatch ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    setChunkSize(buf, 4, 2); // LDUR declared 2 instead of 4
    NjnV2Reader r;
    r.open(buf.data(), buf.size());
    NjnLayered out;
    ASSERT(!njn2DecodeLayered(r, out), "LAY-17: LDUR size mismatch rejected");
}

// ---------------------------------------------------------------------------
// LAY-18: CLIP frame index out of range
// ---------------------------------------------------------------------------
static void test_LAY_18_clip_frame_oob() {
    printf("--- LAY-18: CLIP frame index out of range ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    uint32_t off = chunkDataOffset(buf, 5); // CLIP data
    // numClips(1) + name(16) + loop(1) + numFrames(1) = 19 → first frameIndex
    buf[off + 19] = 0x0A; // frameIndex 10 (>= numFrames 2)
    NjnV2Reader r;
    r.open(buf.data(), buf.size());
    NjnLayered out;
    const char* err = nullptr;
    ASSERT(!njn2DecodeLayered(r, out, &err), "LAY-18: OOB clip frame rejected");
    ASSERT(err != nullptr && std::strstr(err, "frame index") != nullptr, "LAY-18: reason");
}

// ---------------------------------------------------------------------------
// LAY-19: duplicate CLIP
// ---------------------------------------------------------------------------
static void test_LAY_19_duplicate_clip() {
    printf("--- LAY-19: Duplicate CLIP ---\n");
    std::vector<uint8_t> buf = writeVariant(makeFixture(), false, false, true);
    NjnV2Reader r;
    r.open(buf.data(), buf.size());
    NjnLayered out;
    const char* err = nullptr;
    ASSERT(!njn2DecodeLayered(r, out, &err), "LAY-19: duplicate CLIP rejected");
    ASSERT(err != nullptr && std::strstr(err, "duplicate") != nullptr, "LAY-19: reason");
}

// ---------------------------------------------------------------------------
// LAY-20: malformed directory entry
// ---------------------------------------------------------------------------
static void test_LAY_20_bad_directory() {
    printf("--- LAY-20: Malformed directory entry ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    // Corrupt the first directory entry's offset beyond the file.
    uint8_t* e = buf.data() + NJN2_FILE_HEADER_SIZE + 4;
    e[0] = 0xFF; e[1] = 0xFF; e[2] = 0xFF; e[3] = 0x0F;
    NjnV2Reader r;
    ASSERT(!r.open(buf.data(), buf.size()), "LAY-20: OOB directory entry rejected");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    printf("=== njn2_layered_test: .njn v2 layered sprite contract ===\n");

    test_LAY_01_golden_bytes();
    test_LAY_02_roundtrip();
    test_LAY_03_reserialise();
    test_LAY_04_unknown_chunk();
    test_LAY_05_missing_required();
    test_LAY_06_duplicate_required();
    test_LAY_07_schema_version();
    test_LAY_08_zero_canvas();
    test_LAY_09_zero_counts();
    test_LAY_10_truncated_lhdr();
    test_LAY_11_truncated_limg();
    test_LAY_12_zero_image();
    test_LAY_13_limg_trailing();
    test_LAY_14_lprt_size();
    test_LAY_15_lref_overflow();
    test_LAY_16_invalid_ref();
    test_LAY_17_ldur_size();
    test_LAY_18_clip_frame_oob();
    test_LAY_19_duplicate_clip();
    test_LAY_20_bad_directory();

    printf("=== Results: %d passed, %d failed ===\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
