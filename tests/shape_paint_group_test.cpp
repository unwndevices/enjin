// Shape paint through the pack layer (Tomodachi #332): a record's dither,
// stroke and clip fields, drawn by a Shape group onto its retained layer.
//
// The pack is hand-built (ShapePackPieceIn) and every expected pixel is worked
// by hand from the coordinates below. The Body "box" is [-8, 8) × [-16, 0) in
// Kit px; at position (80, 100) it covers x 72..87, y 84..99.

#include <enjin2/graphics/asset_arena.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/shape_pack.hpp>
#include <enjin2/shape/shape_pack_group.hpp>

#include <cstdio>
#include <string>
#include <vector>

using namespace enjin2;
using namespace enjin2::shape;

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
    } while (0)

using Layer = Canvas4<160, 160>;

static int16_t q8(int px) { return static_cast<int16_t>(px * 256); }

// Append one ring of whole-px points; returns its index.
static uint16_t addRing(ShapePiece& p, std::vector<std::pair<int, int>> pts, bool closed) {
    ShapeRing r;
    r.firstPoint = static_cast<uint16_t>(p.points.size());
    r.pointCount = static_cast<uint16_t>(pts.size());
    r.flags = closed ? SHAPE_RING_CLOSED : 0;
    for (auto [x, y] : pts) p.points.push_back({q8(x), q8(y)});
    p.rings.push_back(r);
    return static_cast<uint16_t>(p.rings.size() - 1);
}

// Append a Shape of one ring; returns a reference to tweak its paint.
static ShapeFill& addShape(ShapePiece& p, std::vector<std::pair<int, int>> pts, bool closed, ShapeRole role) {
    ShapeFill f;
    f.role = role;
    f.firstRing = addRing(p, std::move(pts), closed);
    f.ringCount = 1;
    p.shapes.push_back(f);
    return p.shapes.back();
}

static std::vector<std::pair<int, int>> rect(int x0, int y0, int x1, int y1) {
    return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

// A one-pose, one-member piece; `draw` adds its Shapes, the span covers the
// first `spanShapes` of them (the rest are clip Shapes outside every span).
template <class F>
static ShapePiece piece(ShapeSlot slot, ShapeOrder order, MemberSide side, F draw, int spanShapes = -1) {
    ShapePiece p;
    p.slot = slot;
    p.order = order;
    p.binding = slot == ShapeSlot::Body ? ShapeBinding::Field : ShapeBinding::Rigid;
    p.poses = {0};
    p.members = {{side, {}}};
    draw(p);
    p.spans = {{0, static_cast<uint16_t>(spanShapes < 0 ? p.shapes.size() : size_t(spanShapes))}};
    return p;
}

static ShapeSocket sock(ShapeSlot slot, SocketSide side, int x, int y) { return {slot, side, {q8(x), q8(y)}}; }

// Body "box" with sockets: eyes left (-4, -12) + right (4, -12), cheeks (0, -8),
// mouth (0, -4), top (0, -16). `strokeWidth` > 0 gives it an ink inner stroke.
static ShapePiece boxBody(uint8_t strokeWidth = 0) {
    ShapePiece b = piece(ShapeSlot::Body, ShapeOrder::Front, MemberSide::Mirrored, [&](ShapePiece& p) {
        ShapeFill& f = addShape(p, rect(-8, -16, 8, 0), true, ShapeRole::Body);
        f.strokeWidth = strokeWidth;
        f.strokeRole = strokeWidth ? ShapeRole::Ink : ShapeRole::None;
    });
    b.sockets = {sock(ShapeSlot::Eyes, SocketSide::Left, -4, -12), sock(ShapeSlot::Eyes, SocketSide::Right, 4, -12),
                 sock(ShapeSlot::Cheeks, SocketSide::Centre, 0, -8), sock(ShapeSlot::Mouth, SocketSide::Centre, 0, -4),
                 sock(ShapeSlot::Top, SocketSide::Centre, 0, -16)};
    return b;
}

// Eyes "dot": 1×1 ink at the anchor, mirrored on the left socket.
static ShapePiece dotEyes() {
    return piece(ShapeSlot::Eyes, ShapeOrder::Front, MemberSide::Mirrored,
                 [](ShapePiece& p) { addShape(p, rect(0, 0, 1, 1), true, ShapeRole::Ink); });
}

// Mouth "dot": 1×1 ink at the anchor.
static ShapePiece dotMouth() {
    return piece(ShapeSlot::Mouth, ShapeOrder::Front, MemberSide::Mirrored,
                 [](ShapePiece& p) { addShape(p, rect(0, 0, 1, 1), true, ShapeRole::Ink); });
}

struct Fixture {
    std::vector<uint8_t> bytes;
    ShapePackMemory* src = nullptr;
    ShapePack pack;
    AssetArena arena;

    explicit Fixture(const std::vector<ShapePackPieceIn>& pieces) {
        const char* err = nullptr;
        if (!shapePackWrite(pieces, bytes, &err)) fprintf(stderr, "fixture: %s\n", err ? err : "?");
        src = new ShapePackMemory(bytes.data(), bytes.size());
        ShapeError e;
        if (!pack.open(*src, &e)) fprintf(stderr, "fixture open: %s\n", e.text);
        arena.allocateBacking(64 * 1024);
    }
    ~Fixture() { delete src; }
};

static ShapeDrawScratch* scratch() {
    static ShapeDrawScratch* s = new ShapeDrawScratch();
    return s;
}

// Body 9, accent 4, ink 0.
static ShapeLook look(std::initializer_list<std::pair<ShapeSlot, uint16_t>> extra = {}) {
    ShapeLook l;
    l.set(ShapeSlot::Body, 1);
    l.set(ShapeSlot::Eyes, 1);
    l.set(ShapeSlot::Mouth, 1);
    for (auto [s, id] : extra) l.set(s, id);
    l.colour[0] = 9;
    l.colour[1] = 4;
    l.colour[2] = 0;
    return l;
}

static std::string picture(const Layer& c, int x0, int y0, int w, int h) {
    std::string s;
    for (int y = y0; y < y0 + h; ++y) {
        for (int x = x0; x < x0 + w; ++x) {
            const uint8_t v = c.getPixel(static_cast<int16_t>(x), static_cast<int16_t>(y)).value;
            s += v == 15 ? '.' : "0123456789abcdef"[v];
        }
        if (y + 1 < y0 + h) s += '\n';
    }
    return s;
}

static bool expectPicture(const Layer& c, int x0, int y0, int w, int h, const char* want, const char* what) {
    const std::string got = picture(c, x0, y0, w, h);
    if (got == want) return true;
    fprintf(stderr, "  %s\n  want:\n%s\n  got:\n%s\n", what, want, got.c_str());
    return false;
}

static bool draw(Fixture& f, ShapePackGroup& pet, Layer& layer, const ShapeLook& l, int x, int y) {
    ShapeError e;
    if (!pet.built() && !pet.build(f.pack, l, f.arena, &e)) {
        fprintf(stderr, "  build: %s\n", e.text);
        return false;
    }
    pet.setPosition({Fixed::fromInt(x), Fixed::fromInt(y)});
    return pet.redraw(layer, *scratch());
}

// ---------------------------------------------------------------------------

// A cheek at 50 % (dither 2 quarters = Bayer 8) over the Body: the Body shows
// through where the screen's Bayer cell is 8 or more, and moving the pet one
// pixel does not move the pattern.
static void testDitheredCheek() {
    Fixture f({{ShapeSlot::Body, 1, "box", boxBody()},
               {ShapeSlot::Eyes, 1, "dot", dotEyes()},
               {ShapeSlot::Mouth, 1, "dot", dotMouth()},
               {ShapeSlot::Cheeks, 1, "half",
                piece(ShapeSlot::Cheeks, ShapeOrder::Front, MemberSide::Mirrored, [](ShapePiece& p) {
                    addShape(p, rect(-4, 0, 4, 2), true, ShapeRole::Accent).dither = 2;
                })}});
    static Layer layer;
    layer.clear(Pixel4(15));
    ShapePackGroup pet;
    ASSERT(draw(f, pet, layer, look({{ShapeSlot::Cheeks, 1}}), 80, 100), "draws");
    // Cheek at x 76..83, y 92..93. Bayer row 0 (y 92) {0 8 2 10}: even x < 8;
    // row 1 (y 93) {12 4 14 6}: odd x.
    ASSERT(expectPicture(layer, 75, 92, 10, 2,
                         "9494949499\n"
                         "9949494949",
                         "cheek at x 80"),
           "50 % cheek: the Body through every other pixel");
    ASSERT(draw(f, pet, layer, look({{ShapeSlot::Cheeks, 1}}), 81, 100), "moved");
    ASSERT(expectPicture(layer, 76, 92, 10, 2,
                         "9949494949\n"
                         "9494949499",
                         "cheek at x 81"),
           "moved one pixel right: the pattern stays on the screen");
}

// The Body's inner stroke is its outline ring in ink, over its own fill.
static void testInnerStrokeOnTheBody() {
    Fixture f({{ShapeSlot::Body, 1, "box", boxBody(1)},
               {ShapeSlot::Eyes, 1, "dot", dotEyes()},
               {ShapeSlot::Mouth, 1, "dot", dotMouth()}});
    static Layer layer;
    layer.clear(Pixel4(15));
    ShapePackGroup pet;
    ASSERT(draw(f, pet, layer, look(), 80, 100), "draws");
    ASSERT(expectPicture(layer, 71, 83, 6, 4,
                         "......\n"
                         ".00000\n"
                         ".09999\n"
                         ".09999",
                         "top-left corner"),
           "inner stroke: a 1 px ink ring inside the Body's edge");
    ASSERT(expectPicture(layer, 83, 97, 5, 4,
                         "99990\n"
                         "99990\n"
                         "00000\n"
                         ".....",
                         "bottom-right corner"),
           "inner stroke: the bottom-right corner");
}

// An open ring is a 1 px line; it reaches past the Body, and a move clears it.
static void testOpenStrokeLine() {
    Fixture f({{ShapeSlot::Body, 1, "box", boxBody()},
               {ShapeSlot::Eyes, 1, "dot", dotEyes()},
               {ShapeSlot::Mouth, 1, "line", piece(ShapeSlot::Mouth, ShapeOrder::Front, MemberSide::Mirrored,
                                                   [](ShapePiece& p) {
                                                       ShapeFill& s = addShape(p, {{-30, 0}, {30, 0}}, false, ShapeRole::None);
                                                       s.strokeWidth = 1;
                                                       s.strokeRole = ShapeRole::Ink;
                                                   })}});
    static Layer layer;
    layer.clear(Pixel4(15));
    ShapePackGroup pet;
    ASSERT(draw(f, pet, layer, look(), 80, 100), "draws");
    // Mouth anchor at screen (80, 96): the line runs x 50..110 on y 96.
    ASSERT(expectPicture(layer, 48, 95, 4, 3, "....\n..00\n....", "left end"), "the line starts at x 50");
    ASSERT(expectPicture(layer, 70, 96, 4, 1, "0000", "over the body"), "the line paints over the Body");
    ASSERT(expectPicture(layer, 109, 96, 3, 1, "00.", "right end"), "the line ends at x 110");
    ASSERT(draw(f, pet, layer, look(), 80, 130), "moved down");
    ASSERT(layer.getPixel(50, 96).value == 15 && layer.getPixel(110, 96).value == 15,
           "after a move the old line is cleared (it was in the footprint)");
    ASSERT(layer.getPixel(50, 126).value == 0, "and drawn at the new place");
}

// Teeth clipped in the Feature's own space: the clip Shape sits outside every
// span (never drawn) and is mirrored with its member.
static void testOwnClip() {
    auto teeth = [](ShapePiece& p) {
        addShape(p, rect(0, 0, 4, 2), true, ShapeRole::Ink);                // 0: lip
        ShapeFill& t = addShape(p, rect(-2, 1, 6, 6), true, ShapeRole::Accent);  // 1: teeth, clipped
        t.clip = ShapeClip::Own;
        t.clipShape = 2;
        addShape(p, rect(0, 0, 4, 3), true, ShapeRole::None);               // 2: the clip
    };
    Fixture f({{ShapeSlot::Body, 1, "box", boxBody()},
               {ShapeSlot::Eyes, 1, "teeth", piece(ShapeSlot::Eyes, ShapeOrder::Front, MemberSide::Mirrored, teeth, 2)},
               {ShapeSlot::Mouth, 1, "dot", dotMouth()}});
    static Layer layer;
    layer.clear(Pixel4(15));
    ShapePackGroup pet;
    ASSERT(draw(f, pet, layer, look(), 80, 100), "draws");
    // Right eye anchor (84, 88): lip x 84..87 y 88..89, teeth cut to x 84..87 y 89..90.
    // Left eye anchor (76, 88), mirrored: [72, 76), x 72..75.
    ASSERT(expectPicture(layer, 72, 87, 16, 5,
                         "9999999999999999\n"
                         "0000999999990000\n"
                         "4444999999994444\n"
                         "4444999999994444\n"
                         "9999999999999999",
                         "own clip"),
           "teeth only inside the clip Shape, mirrored with their member");
}

// A cap clipped to the live Body: drawn only where the Body is, wherever the
// group moves.
static void testLiveBodyClip() {
    Fixture f({{ShapeSlot::Body, 1, "box", boxBody()},
               {ShapeSlot::Eyes, 1, "dot", dotEyes()},
               {ShapeSlot::Mouth, 1, "dot", dotMouth()},
               {ShapeSlot::Top, 1, "cap", piece(ShapeSlot::Top, ShapeOrder::Front, MemberSide::Mirrored, [](ShapePiece& p) {
                    addShape(p, rect(-12, -4, 12, 6), true, ShapeRole::Accent).clip = ShapeClip::LiveBody;
                })}});
    static Layer layer;
    for (auto [x, y] : {std::pair<int, int>{80, 100}, {85, 103}}) {
        layer.clear(Pixel4(15));
        ShapePackGroup pet;
        ASSERT(draw(f, pet, layer, look({{ShapeSlot::Top, 1}}), x, y), "draws");
        // Cap at the top socket (x, y − 16): x−12..x+11, y−20..y−11; the Body x−8..x+7, y−16..y−1.
        bool ok = true;
        for (int py = y - 22; py < y + 2; ++py)
            for (int px = x - 14; px < x + 14; ++px) {
                const bool body = px >= x - 8 && px < x + 8 && py >= y - 16 && py < y;
                const bool cap = body && py < y - 10;
                const uint8_t v = layer.getPixel(int16_t(px), int16_t(py)).value;
                if (px == x && py == y - 4) continue;  // the mouth (the eyes are under the cap)
                ok = ok && v == (cap ? 4 : body ? 9 : 15);
            }
        ASSERT(ok, "the cap is cut to the Body where the Body is now");
    }
}

int main() {
    testDitheredCheek();
    testInnerStrokeOnTheBody();
    testOpenStrokeLine();
    testOwnClip();
    testLiveBodyClip();
    printf("shape_paint_group_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
