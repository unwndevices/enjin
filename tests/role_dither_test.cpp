// Oklab and the ordered dither into the Palette roles (Tomodachi #459,
// ADR-0026): RGB565 covers become 4-bit role images, the same bytes on every
// target.
#include <enjin2/graphics/blit_rgb565.hpp>
#include <enjin2/graphics/oklab.hpp>
#include <enjin2/graphics/role_dither.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

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

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// The float conversion against Ottosson's published values (the Oklab post's
// table, sRGB primaries and white).
static void testOklabReference() {
    const Oklab white = srgbToOklab(255, 255, 255);
    ASSERT(near(white.L, 1.0f, 1e-3f) && near(white.a, 0.0f, 1e-3f) && near(white.b, 0.0f, 1e-3f),
           "white is L=1, a=b=0");
    const Oklab red = srgbToOklab(255, 0, 0);
    ASSERT(near(red.L, 0.627955f, 1e-3f) && near(red.a, 0.224863f, 1e-3f) &&
               near(red.b, 0.125846f, 1e-3f),
           "sRGB red matches the reference");
    const Oklab blue = srgbToOklab(0, 0, 255);
    ASSERT(near(blue.L, 0.452014f, 1e-3f) && near(blue.a, -0.032457f, 1e-3f) &&
               near(blue.b, -0.311528f, 1e-3f),
           "sRGB blue matches the reference");
    const Oklab black = srgbToOklab(0, 0, 0);
    ASSERT(black.L == 0.0f && black.a == 0.0f && black.b == 0.0f, "black is the origin");
}

// The integer conversion (what the dither runs) follows the float one closely
// over the whole cube.
static void testOklabFixedFollowsFloat() {
    float worst = 0.0f;
    for (int r = 0; r < 256; r += 3)
        for (int g = 0; g < 256; g += 3)
            for (int b = 0; b < 256; b += 3) {
                const Oklab f = srgbToOklab(uint8_t(r), uint8_t(g), uint8_t(b));
                const OklabQ q = srgbToOklabQ(uint8_t(r), uint8_t(g), uint8_t(b));
                const float s = 1.0f / float(OKLAB_Q_ONE);
                worst = std::fmax(worst, std::fabs(q.L * s - f.L));
                worst = std::fmax(worst, std::fabs(q.a * s - f.a));
                worst = std::fmax(worst, std::fabs(q.b * s - f.b));
            }
    printf("oklab: worst integer/float difference %.5f\n", worst);
    ASSERT(worst < 0.001f, "the integer Oklab is within 0.001 of the float one");
    const OklabQ white = srgbToOklabQ(255, 255, 255);
    ASSERT(std::abs(white.L - OKLAB_Q_ONE) <= 40 && std::abs(white.a) <= 40 && std::abs(white.b) <= 40,
           "integer white is L=1, a=b=0");
}

// --- the dither ---------------------------------------------------------------

static Palette paletteOf(const uint32_t (&hex)[ROLE_COUNT]) {
    Palette p;
    for (uint8_t i = 0; i < ROLE_COUNT; ++i)
        p.setColor(i, uint8_t(hex[i] >> 16), uint8_t(hex[i] >> 8), uint8_t(hex[i]));
    return p;
}

// Two presets from assets-src/palettes/: one dark, one light.
static Palette nyx8Dark() {
    return paletteOf({0x08141e, 0x20394f, 0x816271, 0xf6d6bd, 0x997577, 0x4e495f, 0x0f2a3f, 0xc3a38a});
}
static Palette paper8Light() {
    return paletteOf({0xf6e79c, 0xd1a67e, 0xa8605d, 0x1f244b, 0x3c6b64, 0x60ae7b, 0xb6cf8e, 0x654053});
}

static uint8_t nibble(const std::vector<uint8_t>& packed, size_t rowBytes, int x, int y) {
    const uint8_t byte = packed[size_t(y) * rowBytes + size_t(x) / 2];
    return (x & 1) ? uint8_t(byte >> 4) : uint8_t(byte & 0x0F);
}

// Every RGB565 value, under both palettes, lands on a role (0-7).
static void testOnlyRoles() {
    std::vector<uint16_t> all(256 * 256);
    for (size_t i = 0; i < all.size(); ++i) all[i] = uint16_t(i);
    for (const Palette& pal : {nyx8Dark(), paper8Light()}) {
        RoleDither rd;
        rd.build(pal);
        std::vector<uint8_t> out(128 * 256);
        rd.dither(all.data(), 256, 256, 256, out.data(), 128);
        bool ok = true;
        for (uint8_t byte : out) ok = ok && (byte & 0x0F) < ROLE_COUNT && (byte >> 4) < ROLE_COUNT;
        ASSERT(ok, "every RGB565 value dithers to a role, never 8-15");
    }
}

// With no spread it is the plain nearest role: a flat patch of a role's own
// colour comes back as that role, every pixel.
static void testNoSpreadIsNearestRole() {
    for (const Palette& pal : {nyx8Dark(), paper8Light()}) {
        RoleDither rd;
        rd.build(pal, 0);
        bool ok = true;
        for (uint8_t role = 0; role < ROLE_COUNT; ++role) {
            const RGB c = pal.getColor(role);
            std::vector<uint16_t> patch(8 * 8, packRgb565(c.r, c.g, c.b));
            std::vector<uint8_t> out(4 * 8);
            rd.dither(patch.data(), 8, 8, 8, out.data(), 4);
            for (uint8_t byte : out) ok = ok && byte == uint8_t(role | role << 4);
            ok = ok && rd.nearestRole(c.r, c.g, c.b) == role;
        }
        ASSERT(ok, "spread 0: a role's own colour is that role");
    }
}

// A flat colour halfway between two roles dithers into a mix of both, in the
// 4x4 Bayer pattern (it repeats every 4 pixels both ways).
static void testFlatColourMixesInBayer() {
    const Palette pal = paletteOf({0x000000, 0xffffff, 0x000000, 0x000000, 0x000000, 0x000000,
                                   0x000000, 0x000000});
    RoleDither rd;
    rd.build(pal);
    std::vector<uint16_t> grey(16 * 16, packRgb565(0x76, 0x76, 0x78));
    std::vector<uint8_t> out(8 * 16);
    rd.dither(grey.data(), 16, 16, 16, out.data(), 8);
    int white = 0;
    bool periodic = true;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            const uint8_t v = nibble(out, 8, x, y);
            white += v == ROLE_SURFACE;
            periodic = periodic && v == nibble(out, 8, x & 3, y & 3);
        }
    ASSERT(white > 0 && white < 256, "a mid grey between black and white mixes both");
    ASSERT(periodic, "the mix repeats every 4 pixels");
}

// An odd width packs its last pixel in a low nibble and writes the pad 0.
static void testOddWidth() {
    const Palette pal = paper8Light();
    RoleDither rd;
    rd.build(pal, 0);
    const RGB t = pal.getColor(ROLE_TEXT);
    std::vector<uint16_t> img(5 * 2, packRgb565(t.r, t.g, t.b));
    std::vector<uint8_t> out(3 * 2, 0xAA);
    rd.dither(img.data(), 5, 2, 5, out.data(), 3);
    ASSERT(out[2] == ROLE_TEXT && out[5] == ROLE_TEXT, "an odd row ends in a low nibble, pad 0");
    ASSERT(out[0] == uint8_t(ROLE_TEXT * 0x11), "the even pairs hold both nibbles");
}

// The Canvas4 form writes the same nibbles at the canvas's top-left, keeps the
// pixels around them, and marks their tiles dirty.
static void testIntoCanvas() {
    const Palette pal = nyx8Dark();
    RoleDither rd;
    rd.build(pal);
    std::vector<uint16_t> img(21 * 19);
    for (size_t i = 0; i < img.size(); ++i) img[i] = uint16_t(i * 2654435761u >> 16);
    std::vector<uint8_t> raw(11 * 19);
    rd.dither(img.data(), 21, 19, 21, raw.data(), 11);

    Canvas4<32, 32> c;
    c.clear(Pixel4(ROLE_ACCENT4));
    c.clearDirty();
    rd.dither(img.data(), 21, 19, c);
    bool same = true;
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            const uint8_t want = (x < 21 && y < 19) ? nibble(raw, 11, x, y) : ROLE_ACCENT4;
            same = same && c.getPixel(int16_t(x), int16_t(y)).value == want;
        }
    ASSERT(same, "the canvas holds the image's roles and keeps its other pixels");
    ASSERT(c.isTileDirty(0, 0) && c.isTileDirty(1, 1), "the touched tiles are dirty");

    Canvas4<8, 8> small;
    rd.dither(img.data(), 21, 19, small);
    bool clipped = true;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            clipped = clipped && small.getPixel(int16_t(x), int16_t(y)).value == nibble(raw, 11, x, y);
    ASSERT(clipped, "a smaller canvas takes the image's top-left, clipped");
}

// --- goldens ------------------------------------------------------------------
//
// The bytes are pinned: a host and the device must give these. Rewrite them
// with ENJIN2_ROLE_DITHER_GOLDEN=1 after a deliberate change (and look at them).

static std::string dataPath(const char* name) {
    return std::string(ENJIN2_ROLE_DITHER_DATA_DIR) + "/" + name;
}

static std::vector<uint8_t> readFile(const std::string& path) {
    std::vector<uint8_t> bytes;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return bytes;
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) bytes.insert(bytes.end(), buf, buf + n);
    fclose(f);
    return bytes;
}

// A 72x72 sweep: hue across, brightness down (white at the top, black at the
// bottom), and a grey ramp in the last 8 rows.
static std::vector<uint16_t> gradient72() {
    std::vector<uint16_t> img(72 * 72);
    for (int y = 0; y < 72; ++y)
        for (int x = 0; x < 72; ++x) {
            int r, g, b;
            if (y >= 64) {
                r = g = b = x * 255 / 71;
            } else {
                const int h = x * 6 * 256 / 72;  // hue, 256 a sixth
                const int f = h & 255;
                int c[3];
                switch (h >> 8) {
                case 0: c[0] = 255; c[1] = f; c[2] = 0; break;
                case 1: c[0] = 255 - f; c[1] = 255; c[2] = 0; break;
                case 2: c[0] = 0; c[1] = 255; c[2] = f; break;
                case 3: c[0] = 0; c[1] = 255 - f; c[2] = 255; break;
                case 4: c[0] = f; c[1] = 0; c[2] = 255; break;
                default: c[0] = 255; c[1] = 0; c[2] = 255 - f; break;
                }
                // Rows 0-31 fade from white into the hue, 32-63 from it to black.
                for (int& v : c)
                    v = y < 32 ? 255 - (255 - v) * y / 31 : v * (63 - y) / 31;
                r = c[0];
                g = c[1];
                b = c[2];
            }
            img[size_t(y) * 72 + size_t(x)] = packRgb565(uint8_t(r), uint8_t(g), uint8_t(b));
        }
    return img;
}

static void checkGolden(const char* name, const std::vector<uint16_t>& img, const Palette& pal) {
    RoleDither rd;
    rd.build(pal);
    std::vector<uint8_t> out(36 * 72);
    rd.dither(img.data(), 72, 72, 72, out.data(), 36);
    const std::string path = dataPath(name);
    if (getenv("ENJIN2_ROLE_DITHER_GOLDEN")) {
        FILE* f = fopen(path.c_str(), "wb");
        if (f) {
            fwrite(out.data(), 1, out.size(), f);
            fclose(f);
        }
        printf("role_dither: rewrote %s\n", path.c_str());
    }
    const std::vector<uint8_t> want = readFile(path);
    int diff = 0;
    for (size_t i = 0; i < out.size(); ++i) diff += i >= want.size() || want[i] != out[i];
    if (diff) fprintf(stderr, "%s: %d of %zu bytes differ\n", name, diff, out.size());
    ASSERT(want.size() == out.size() && diff == 0, name);
}

static void testGoldens() {
    const std::vector<uint8_t> photoBytes = readFile(dataPath("pearl-72.rgb565"));
    ASSERT(photoBytes.size() == 72 * 72 * 2, "the photo fixture is 72x72 RGB565");
    std::vector<uint16_t> photo(72 * 72);
    for (size_t i = 0; i < photo.size() && 2 * i + 1 < photoBytes.size(); ++i)
        photo[i] = uint16_t(photoBytes[2 * i] | photoBytes[2 * i + 1] << 8);
    const std::vector<uint16_t> gradient = gradient72();
    checkGolden("pearl-nyx8-dark.bin", photo, nyx8Dark());
    checkGolden("pearl-paper-8-light.bin", photo, paper8Light());
    checkGolden("gradient-nyx8-dark.bin", gradient, nyx8Dark());
    checkGolden("gradient-paper-8-light.bin", gradient, paper8Light());
}

int main() {
    testOklabReference();
    testOklabFixedFollowsFloat();
    testOnlyRoles();
    testNoSpreadIsNearestRole();
    testFlatColourMixesInBayer();
    testOddWidth();
    testIntoCanvas();
    testGoldens();
    printf("role_dither_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
