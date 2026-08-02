// Odd-width Canvas4 (unwn #155 / #165): rows are stored at a padded byte
// stride ROW_BYTES = (WIDTH + 1) / 2, so Canvas4<127,127> is legal and the
// old flat-packing failure modes — row aliasing ((126,0) vs (0,1) sharing a
// nibble) and the one-byte overflow at (126,126) — cannot occur. Even widths
// keep the exact old layout.

#include <enjin2/graphics/canvas.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

// ============================================================
// a. Buffer geometry
// ============================================================
static void test_buffer_geometry()
{
    printf("--- Buffer geometry ---\n");

    Canvas4<127, 127> odd;
    ASSERT(odd.getBufferSize() == 64u * 127u,
           "Canvas4<127,127> buffer is ROW_BYTES(64) * HEIGHT(127) = 8128");

    Canvas4<128, 128> even;
    ASSERT(even.getBufferSize() == (128u * 128u) / 2u,
           "Canvas4<128,128> keeps the old flat size 8192");
}

// ============================================================
// b. No row aliasing on odd width
// ============================================================
static void test_no_row_aliasing()
{
    printf("--- Row aliasing ---\n");

    Canvas4<127, 127> c;
    c.setPixel(126, 0, Pixel4(15));
    ASSERT(c.getPixel(0, 1).value == 0,
           "(126,0) and (0,1) occupy distinct nibbles");

    c.setPixel(0, 1, Pixel4(7));
    ASSERT(c.getPixel(126, 0).value == 15,
           "writing (0,1) leaves (126,0) intact");
    ASSERT(c.getPixel(0, 1).value == 7,
           "(0,1) readback");

    c.setPixel(126, 126, Pixel4(9));
    ASSERT(c.getPixel(126, 126).value == 9,
           "last pixel (126,126) is addressable in-buffer");
}

// ============================================================
// c. Every pixel independent (full-canvas pattern round-trip)
// ============================================================
static void test_full_roundtrip()
{
    printf("--- Full-canvas round-trip ---\n");

    Canvas4<127, 127> c;
    for (int16_t y = 0; y < 127; ++y)
        for (int16_t x = 0; x < 127; ++x)
            c.setPixel(x, y, Pixel4(static_cast<uint8_t>((x + y * 3) % 16)));

    size_t bad = 0;
    for (int16_t y = 0; y < 127; ++y)
        for (int16_t x = 0; x < 127; ++x)
            if (c.getPixel(x, y).value != (x + y * 3) % 16)
                bad++;
    ASSERT(bad == 0, "every pixel reads back what was written");
}

// ============================================================
// d. Fast paths match the setPixel reference on odd width
// ============================================================
static void test_fast_paths()
{
    printf("--- drawHLine / setPixelBatch fast paths ---\n");

    Canvas4<127, 127> fast, ref;

    // Even x + even width takes the memset fast path; must land on the same
    // pixels as the per-pixel fallback.
    fast.drawHLine(4, 5, 100, Pixel4(11));
    for (int16_t i = 0; i < 100; ++i)
        ref.setPixel(4 + i, 5, Pixel4(11));

    // Odd-width line clipped at the right edge (slow path) for contrast.
    fast.drawHLine(0, 6, 127, Pixel4(3));
    for (int16_t i = 0; i < 127; ++i)
        ref.setPixel(i, 6, Pixel4(3));

    Pixel4 batch[64];
    for (int16_t i = 0; i < 64; ++i)
        batch[i] = Pixel4(static_cast<uint8_t>(i % 16));
    fast.setPixelBatch(2, 7, batch, 64);
    for (int16_t i = 0; i < 64; ++i)
        ref.setPixel(2 + i, 7, batch[i]);

    size_t bad = 0;
    for (int16_t y = 0; y < 127; ++y)
        for (int16_t x = 0; x < 127; ++x)
            if (fast.getPixel(x, y).value != ref.getPixel(x, y).value)
                bad++;
    ASSERT(bad == 0, "fast paths draw exactly the reference pixels");
}

// ============================================================
// e. Even-width layout unchanged (byte-level)
// ============================================================
static void test_even_layout_unchanged()
{
    printf("--- Even-width byte layout ---\n");

    Canvas4<128, 128> c;
    c.setPixel(0, 0, Pixel4(0xA));
    c.setPixel(1, 0, Pixel4(0xB));
    c.setPixel(0, 1, Pixel4(0xC));

    const PackedPixel4* buf = c.getBuffer();
    ASSERT(buf[0].getLow().value == 0xA && buf[0].getHigh().value == 0xB,
           "byte 0 packs pixels (0,0) low / (1,0) high as before");
    ASSERT(buf[64].getLow().value == 0xC,
           "row 1 still starts at byte 64 for width 128");
}

int main()
{
    printf("=== canvas4_odd_width_test ===\n\n");

    test_buffer_geometry();
    test_no_row_aliasing();
    test_full_roundtrip();
    test_fast_paths();
    test_even_layout_unchanged();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
