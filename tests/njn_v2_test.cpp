// .njn v2 codec unit test (unwn #204): the content-addressed asset store's
// on-device format. v2 extends the existing 8-byte header with 4-bit
// nibble-packed pixels (two per byte, matching Canvas4's even→low / odd→high
// convention) and reserves palette index 15 as transparent.
//
// The seam under test is pure codec: pack indices → bytes → unpack indices,
// with no allocation and no engine deps. Round-trip must be byte-exact for a
// static 1x1-grid bitmap and a multi-cell sheet, must preserve index 15, and
// must survive an odd pixel count (final high nibble is padding).
#include <enjin2/graphics/sprite_asset.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { printf("PASS: %s\n", msg); passes++; }                         \
    } while (0)

// Nibble order is the contract with Canvas4: the even (low-index) pixel lands in
// the low nibble, the odd pixel in the high nibble. {1, 2} must pack to 0x21.
static void test_nibble_order_matches_canvas4() {
    const uint8_t idx[2] = {0x1, 0x2};
    uint8_t packed[1] = {0};
    size_t n = njnPackNibbles(idx, 2, packed);
    ASSERT(n == 1, "njn v2: two pixels pack into one byte");
    ASSERT(packed[0] == 0x21, "njn v2: even->low, odd->high nibble (Canvas4 order)");
}

// Every index 0..15 must round-trip; the packed size halves the pixel count.
static void test_pack_unpack_roundtrip_all_indices() {
    uint8_t idx[16];
    for (uint8_t i = 0; i < 16; ++i) idx[i] = i;
    uint8_t packed[8] = {0};
    size_t n = njnPackNibbles(idx, 16, packed);
    ASSERT(n == 8, "njn v2: 16 pixels pack into 8 bytes");

    uint8_t out[16] = {0};
    njnUnpackNibbles(packed, 16, out);
    ASSERT(std::memcmp(idx, out, 16) == 0, "njn v2: all indices 0..15 round-trip");
}

// An odd pixel count leaves a padding high nibble in the last byte; unpack must
// ignore it and recover exactly the pixels that were packed.
static void test_odd_pixel_count() {
    const uint8_t idx[3] = {0xA, 0xB, 0xC};
    uint8_t packed[2] = {0};
    size_t n = njnPackNibbles(idx, 3, packed);
    ASSERT(n == 2, "njn v2: 3 pixels need 2 bytes (one padding nibble)");
    ASSERT((packed[1] & 0xF0) == 0, "njn v2: trailing padding nibble is zero");

    uint8_t out[3] = {0};
    njnUnpackNibbles(packed, 3, out);
    ASSERT(std::memcmp(idx, out, 3) == 0, "njn v2: odd pixel count round-trips");
}

// Index 15 is the reserved transparent marker; the codec is agnostic to it (it
// round-trips like any value) but exposes the palette convention as a predicate.
static void test_transparency_index() {
    ASSERT(njnIsTransparent(15), "njn v2: index 15 is transparent");
    ASSERT(!njnIsTransparent(0), "njn v2: index 0 is opaque (black)");
    ASSERT(!njnIsTransparent(14), "njn v2: index 14 is opaque (brightest paintable)");

    const uint8_t idx[4] = {15, 0, 15, 7};
    uint8_t packed[2] = {0};
    njnPackNibbles(idx, 4, packed);
    uint8_t out[4] = {0};
    njnUnpackNibbles(packed, 4, out);
    ASSERT(std::memcmp(idx, out, 4) == 0, "njn v2: transparent pixels survive round-trip");
}

// A single static bitmap is a 1x1-grid .njn: full file encode → header parse →
// unpack recovers the pixels.
static void test_static_bitmap_file_roundtrip() {
    // 4x2 static bitmap (cols=rows=1), 8 pixels.
    uint8_t pixels[8] = {0, 1, 2, 3, 15, 14, 8, 15};
    uint8_t file[8 + 4] = {0}; // header + 4 packed bytes
    size_t fileSize = njnEncodeV2(4, 2, 1, 1, pixels, 8, file, sizeof(file));
    ASSERT(fileSize == 8 + 4, "njn v2: 8-pixel static file is header + 4 bytes");

    NjnHeader h{};
    ASSERT(parseNjnHeader(file, fileSize, h), "njn v2: static file header parses");
    ASSERT(h.version == 2, "njn v2: version field is 2");
    ASSERT(h.cellW == 4 && h.cellH == 2 && h.cols == 1 && h.rows == 1,
           "njn v2: static header dims round-trip");
    ASSERT(njnPixelCount(h) == 8, "njn v2: pixel count from header");

    uint8_t out[8] = {0};
    njnUnpackNibbles(file + sizeof(NjnHeader), 8, out);
    ASSERT(std::memcmp(pixels, out, 8) == 0, "njn v2: static bitmap pixels round-trip");
}

// A sprite sheet uses cols/rows > 1; the pixel plane is cellW*cellH*cols*rows.
static void test_sheet_file_roundtrip() {
    // 2x2 cells, 2x2 grid → 16 pixels.
    uint8_t pixels[16];
    for (uint8_t i = 0; i < 16; ++i) pixels[i] = static_cast<uint8_t>(i);
    uint8_t file[8 + 8] = {0};
    size_t fileSize = njnEncodeV2(2, 2, 2, 2, pixels, 16, file, sizeof(file));
    ASSERT(fileSize == 8 + 8, "njn v2: 16-pixel sheet is header + 8 bytes");

    NjnHeader h{};
    ASSERT(parseNjnHeader(file, fileSize, h), "njn v2: sheet header parses");
    ASSERT(h.cols == 2 && h.rows == 2, "njn v2: sheet grid dims round-trip");
    ASSERT(njnPixelCount(h) == 16, "njn v2: sheet pixel count");

    uint8_t out[16] = {0};
    njnUnpackNibbles(file + sizeof(NjnHeader), 16, out);
    ASSERT(std::memcmp(pixels, out, 16) == 0, "njn v2: sheet pixels round-trip");
}

// Header validation is version-aware: a v2 buffer too short for its packed
// pixels is rejected; a valid v1 byte-per-pixel buffer still parses.
static void test_header_validation() {
    // v2 with dims claiming 8 pixels (4 packed bytes) but only 2 bytes present.
    uint8_t truncated[8 + 2] = {'N', 'J', 2, 4, 2, 1, 1, 0, 0, 0};
    NjnHeader h{};
    ASSERT(!parseNjnHeader(truncated, sizeof(truncated), h),
           "njn v2: buffer too short for packed pixels is rejected");

    // v1 byte-per-pixel still parses (backward compatibility).
    uint8_t v1[8 + 4] = {'N', 'J', 1, 2, 2, 1, 1, 0, 0, 1, 2, 3};
    NjnHeader h1{};
    ASSERT(parseNjnHeader(v1, sizeof(v1), h1), "njn v1: byte-per-pixel still parses");
    ASSERT(h1.version == 1, "njn v1: version preserved");

    // Bad magic rejected.
    uint8_t bad[8] = {'X', 'J', 2, 1, 1, 1, 1, 0};
    NjnHeader hb{};
    ASSERT(!parseNjnHeader(bad, sizeof(bad), hb), "njn: bad magic rejected");
}

int main() {
    test_nibble_order_matches_canvas4();
    test_pack_unpack_roundtrip_all_indices();
    test_odd_pixel_count();
    test_transparency_index();
    test_static_bitmap_file_roundtrip();
    test_sheet_file_roundtrip();
    test_header_validation();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
