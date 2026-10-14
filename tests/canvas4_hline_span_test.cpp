// Canvas4::drawHLine nibble-aware spans (Tomodachi #318): every start/width
// parity — including spans clipped at either edge, on even and odd canvas
// widths — paints exactly the pixels and dirty tiles the per-pixel setPixel
// reference does, and leaves the neighbouring nibbles untouched.

#include <enjin2/graphics/canvas.hpp>
#include <cstdio>
#include <cstring>

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

// A busy background so a span that over- or under-writes a neighbouring
// nibble shows up as a mismatch.
template <uint16_t W, uint16_t H>
static void paintBackground(Canvas4<W, H>& c)
{
    for (int16_t y = 0; y < H; ++y)
        for (int16_t x = 0; x < W; ++x)
            c.setPixel(x, y, Pixel4(static_cast<uint8_t>((x * 7 + y * 3 + 1) % 16)));
    c.clearDirty();
}

// Per-pixel reference: what drawHLine is specified to do.
template <uint16_t W, uint16_t H>
static void referenceHLine(Canvas4<W, H>& c, int16_t x, int16_t y, int16_t w, Pixel4 color)
{
    for (int16_t i = 0; i < w; ++i)
        c.setPixel(static_cast<int16_t>(x + i), y, color);
}

template <uint16_t W, uint16_t H>
static bool samePixelsAndDirty(const Canvas4<W, H>& a, const Canvas4<W, H>& b)
{
    for (int16_t y = 0; y < H; ++y)
        for (int16_t x = 0; x < W; ++x)
            if (a.getPixel(x, y).value != b.getPixel(x, y).value)
                return false;
    return std::memcmp(a.dirtyBitmap(), b.dirtyBitmap(),
                       Canvas4<W, H>::dirtyBitmapBytes()) == 0;
}

// Sweep start x over both parities (off the left edge, interior, crossing
// and past the right edge) and width over both parities, for two colours
// whose high and low nibbles differ from the background pattern.
template <uint16_t W, uint16_t H>
static size_t sweepMismatches()
{
    static Canvas4<W, H> fast, ref;
    size_t bad = 0;
    const Pixel4 colours[] = {Pixel4(0), Pixel4(13)};
    for (Pixel4 colour : colours)
    {
        for (int16_t x = -5; x <= static_cast<int16_t>(W) + 2; ++x)
        {
            for (int16_t w = -1; w <= 21; ++w)
            {
                const int16_t y = static_cast<int16_t>((x + w + 64) % H);
                paintBackground(fast);
                paintBackground(ref);
                fast.drawHLine(x, y, w, colour);
                referenceHLine(ref, x, y, w, colour);
                if (!samePixelsAndDirty(fast, ref))
                {
                    if (bad < 8)
                        fprintf(stderr, "  mismatch W=%u x=%d w=%d colour=%u\n",
                                static_cast<unsigned>(W), x, w, colour.value);
                    bad++;
                }
            }
        }
    }
    return bad;
}

static void test_every_parity_matches_reference()
{
    printf("--- drawHLine parity sweep vs per-pixel reference ---\n");
    ASSERT((sweepMismatches<48, 32>() == 0),
           "even-width canvas: every start/width parity matches setPixel");
    ASSERT((sweepMismatches<47, 32>() == 0),
           "odd-width canvas: every start/width parity matches setPixel");
}

static void test_full_row_spans()
{
    printf("--- full-row spans ---\n");
    static Canvas4<47, 20> fast, ref;
    paintBackground(fast);
    paintBackground(ref);
    fast.drawHLine(-3, 4, 60, Pixel4(9));
    referenceHLine(ref, -3, 4, 60, Pixel4(9));
    fast.fillRect(1, 8, 45, 5, Pixel4(6));
    for (int16_t r = 0; r < 5; ++r)
        referenceHLine(ref, 1, static_cast<int16_t>(8 + r), 45, Pixel4(6));
    ASSERT(samePixelsAndDirty(fast, ref),
           "overhanging row span and odd-start/odd-width fillRect match setPixel");
}

// The span is written straight into the packed buffer (head nibble, memset
// middle, tail nibble) — never through the virtual per-pixel setPixel, which
// was half of the S3 span-paint time for a filled vector Shape (#315).
class CountingCanvas : public Canvas4<48, 16>
{
public:
    int setPixelCalls = 0;
    void setPixel(int16_t x, int16_t y, Pixel4 color) override
    {
        ++setPixelCalls;
        Canvas4<48, 16>::setPixel(x, y, color);
    }
};

static void test_spans_skip_per_pixel_dispatch()
{
    printf("--- spans skip per-pixel setPixel ---\n");
    static CountingCanvas c;
    int calls = 0;
    for (int16_t x = 0; x <= 3; ++x)
        for (int16_t w = 1; w <= 9; ++w)
        {
            c.setPixelCalls = 0;
            c.drawHLine(x, 3, w, Pixel4(5));
            calls += c.setPixelCalls;
        }
    ASSERT(calls == 0, "odd-start / odd-width spans make no setPixel calls");
}

int main()
{
    printf("=== canvas4_hline_span_test ===\n\n");

    test_every_parity_matches_reference();
    test_full_row_spans();
    test_spans_skip_per_pixel_dispatch();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
