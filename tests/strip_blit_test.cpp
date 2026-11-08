// The column-scaled strip blit (Tomodachi #459, ADR-0026): a 4-bit image drawn
// as a strip whose left and right edges have their own heights around a shared
// centre line. The cover flow's Side covers and its Flip are this call.
#include <enjin2/graphics/blit.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <cstdio>
#include <cstring>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

// Rows of a canvas's top-left as text: '.' for 0, else the index's digit.
template <uint16_t W, uint16_t H>
static bool rowsAre(const Canvas4<W, H>& c, const char* const* rows, int n) {
    bool ok = true;
    for (int y = 0; y < n; ++y) {
        const int len = int(strlen(rows[y]));
        for (int x = 0; x < len; ++x) {
            const uint8_t v = c.getPixel(int16_t(x), int16_t(y)).value;
            const char want = rows[y][x];
            ok = ok && v == (want == '.' ? 0 : uint8_t(want - '0'));
        }
        if (!ok) {
            fprintf(stderr, "row %d differs:", y);
            for (int x = 0; x < len; ++x) fprintf(stderr, " %d", c.getPixel(int16_t(x), int16_t(y)).value);
            fprintf(stderr, "\n");
            return false;
        }
    }
    return true;
}

// Any source pattern with distinct neighbours.
template <uint16_t W, uint16_t H>
static void paint(Canvas4<W, H>& c) {
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) c.setPixel(int16_t(x), int16_t(y), Pixel4(uint8_t((x * 3 + y * 5) % 8)));
}

// w, hL and hR equal to the source's size: a plain blit at (x, y).
static void testPlainBlitIdentity() {
    Canvas4<12, 9> src;
    paint(src);
    Canvas4<32, 32> dst;
    dst.clear(Pixel4(ROLE_ACCENT4));
    stripBlit(dst, src, 5, 7, 12, 9, 9);
    bool same = true;
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            const bool in = x >= 5 && x < 17 && y >= 7 && y < 16;
            const uint8_t want = in ? src.getPixel(int16_t(x - 5), int16_t(y - 7)).value : ROLE_ACCENT4;
            same = same && dst.getPixel(int16_t(x), int16_t(y)).value == want;
        }
    ASSERT(same, "a strip the source's own size is a plain blit");
}

// A 2x2 source drawn 4 wide, the left edge 2 tall and the right 6, worked by
// hand: column heights 2, 3, 5, 6 (linear, rounded), each centred on the
// strip's centre line (3 rows down), source column x*2/4, source row j*2/hc.
static void testRecedingEdgeByHand() {
    Canvas4<2, 2> src;
    src.setPixel(0, 0, Pixel4(1));
    src.setPixel(1, 0, Pixel4(2));
    src.setPixel(0, 1, Pixel4(3));
    src.setPixel(1, 1, Pixel4(4));
    Canvas4<8, 8> dst;
    dst.clear(Pixel4(0));
    stripBlit(dst, src, 0, 0, 4, 2, 6);
    const char* const grows[] = {
        "..22.",
        ".122.",
        "1122.",
        "3344.",
        "..44.",
        "...4.",
        ".....",
    };
    ASSERT(rowsAre(dst, grows, 7), "hL < hR: the strip grows to the right");

    // The mirror: the right edge recedes.
    dst.clear(Pixel4(0));
    stripBlit(dst, src, 0, 0, 4, 6, 2);
    const char* const shrinks[] = {
        "11...",
        "112..",
        "1122.",
        "3344.",
        "33...",
        "3....",
        ".....",
    };
    ASSERT(rowsAre(dst, shrinks, 7), "hL > hR: the strip shrinks to the right");
}

// A Side cover's shape: a 72x72 source in 24 columns, the outer edge 74 tall
// and the inner 54. Each column is centred and as tall as the line between
// the edges says, and reads the source column under it.
static void testSideCoverShape() {
    Canvas4<72, 72> src;
    for (int y = 0; y < 72; ++y)
        for (int x = 0; x < 72; ++x) src.setPixel(int16_t(x), int16_t(y), Pixel4(uint8_t(1 + (x / 9) % 7)));
    Canvas4<40, 80> dst;
    dst.clear(Pixel4(0));
    stripBlit(dst, src, 3, 2, 24, 74, 54);
    bool ok = true;
    for (int i = 0; i < 24; ++i) {
        // 74 at the left, 54 at the right: 20 rows over 23 steps.
        const int hc = 74 - (20 * i * 2 + 23) / (2 * 23);
        const int top = 2 + (74 - hc) / 2;
        for (int y = 0; y < 80; ++y) {
            const uint8_t v = dst.getPixel(int16_t(3 + i), int16_t(y)).value;
            const bool in = y >= top && y < top + hc;
            ok = ok && (in ? v == uint8_t(1 + ((i * 72 / 24) / 9) % 7) : v == 0);
        }
    }
    ASSERT(ok, "a Side cover: column heights and source columns follow the edges");
}

// Clipping: drawn into a small canvas at any edge (or far past it), the strip
// leaves exactly the pixels a large canvas shows in that window.
static void testClipsAtEveryEdge() {
    Canvas4<10, 7> src;
    paint(src);
    const int offsets[][2] = {{-6, 3}, {9, 4}, {4, -5}, {3, 10}, {-8, -6}, {12, 11}, {-30000, 2}, {2, 30000}};
    bool ok = true;
    for (const auto& o : offsets) {
        Canvas4<16, 16> small;
        small.clear(Pixel4(ROLE_ACCENT4));
        stripBlit(small, src, int16_t(o[0]), int16_t(o[1]), 13, 5, 11);
        if (o[0] < -100 || o[1] > 100) {
            bool untouched = true;
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x) untouched = untouched && small.getPixel(int16_t(x), int16_t(y)).value == ROLE_ACCENT4;
            ok = ok && untouched;
            continue;
        }
        Canvas4<64, 64> big;
        big.clear(Pixel4(ROLE_ACCENT4));
        stripBlit(big, src, int16_t(o[0] + 24), int16_t(o[1] + 24), 13, 5, 11);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                ok = ok && small.getPixel(int16_t(x), int16_t(y)).value ==
                               big.getPixel(int16_t(x + 24), int16_t(y + 24)).value;
    }
    ASSERT(ok, "clipped at the left, right, top, bottom and corners like a window on a big canvas");
}

// The transparent index leaves what is under it; an empty strip draws nothing.
static void testTransparentAndEmpty() {
    Canvas4<4, 4> src;
    src.clear(Pixel4(PALETTE_TRANSPARENT));
    src.setPixel(1, 1, Pixel4(ROLE_TEXT));
    Canvas4<8, 8> dst;
    dst.clear(Pixel4(ROLE_SURFACE));
    stripBlit(dst, src, 0, 0, 4, 4, 4);
    int text = 0, surface = 0;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            const uint8_t v = dst.getPixel(int16_t(x), int16_t(y)).value;
            text += v == ROLE_TEXT;
            surface += v == ROLE_SURFACE;
        }
    ASSERT(text == 1 && surface == 63, "transparent source pixels leave the canvas as it was");

    dst.clear(Pixel4(ROLE_SURFACE));
    stripBlit(dst, src, 0, 0, 0, 4, 4);
    stripBlit(dst, src, 0, 0, 4, 0, 0);
    stripBlit(dst, src, 0, 0, -3, 4, 4);
    bool empty = true;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) empty = empty && dst.getPixel(int16_t(x), int16_t(y)).value == ROLE_SURFACE;
    ASSERT(empty, "zero or negative width or heights draw nothing");
}

int main() {
    testPlainBlitIdentity();
    testRecedingEdgeByHand();
    testSideCoverShape();
    testClipsAtEveryEdge();
    testTransparentAndEmpty();
    printf("strip_blit_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
