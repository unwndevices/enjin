// Pack layer + generic Shape group (Tomodachi #331, ADR-0016/0017): a Look read
// from a Shape pack into an AssetArena Look buffer, assembled into a Shape
// group (sockets, mirroring, draw order) that owns a retained layer.
//
// The pack is hand-built (ShapePackPieceIn) so every expected pixel comes from
// the coordinates below, worked by hand: rectangles on whole pixels fill
// exactly their pixels under the majority rule.

#include <enjin2/graphics/asset_arena.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/shape_pack.hpp>
#include <enjin2/shape/shape_pack_group.hpp>

#include <array>
#include <cstdio>
#include <cstring>
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

// ---------------------------------------------------------------------------
// Fixture pack
// ---------------------------------------------------------------------------

static int16_t q8(int px) { return static_cast<int16_t>(px * 256); }

// Append an axis-aligned rectangle [x0, x1) × [y0, y1) (whole px) as one Shape.
static void addRect(ShapePiece& p, int x0, int y0, int x1, int y1, ShapeRole role) {
    ShapeFill f;
    f.role = role;
    f.firstRing = static_cast<uint16_t>(p.rings.size());
    f.ringCount = 1;
    ShapeRing r;
    r.firstPoint = static_cast<uint16_t>(p.points.size());
    r.pointCount = 4;
    r.flags = SHAPE_RING_CLOSED;
    p.points.push_back({q8(x0), q8(y0)});
    p.points.push_back({q8(x1), q8(y0)});
    p.points.push_back({q8(x1), q8(y1)});
    p.points.push_back({q8(x0), q8(y1)});
    p.rings.push_back(r);
    p.shapes.push_back(f);
}

// A one-pose piece whose members each draw the rectangles given.
struct MemberRects {
    MemberSide side;
    std::vector<std::array<int, 4>> rects;
    ShapeRole role;
};

static ShapePiece makePiece(ShapeSlot slot, ShapeOrder order, std::vector<uint8_t> poses,
                            std::vector<std::vector<MemberRects>> perPose) {
    ShapePiece p;
    p.slot = slot;
    p.order = order;
    p.binding = slot == ShapeSlot::Body ? ShapeBinding::Field : ShapeBinding::Rigid;
    p.poses = poses;
    const size_t nMembers = perPose[0].size();
    for (size_t m = 0; m < nMembers; ++m) p.members.push_back({perPose[0][m].side, {}});
    for (size_t k = 0; k < poses.size(); ++k)
        for (size_t m = 0; m < nMembers; ++m) {
            ShapeSpan s;
            s.firstShape = static_cast<uint16_t>(p.shapes.size());
            for (const auto& r : perPose[k][m].rects) addRect(p, r[0], r[1], r[2], r[3], perPose[k][m].role);
            s.shapeCount = static_cast<uint16_t>(p.shapes.size() - s.firstShape);
            p.spans.push_back(s);
        }
    return p;
}

static ShapeSocket sock(ShapeSlot slot, SocketSide side, int x, int y) { return {slot, side, {q8(x), q8(y)}}; }

// Body 1 "box": [-8, 8) × [-16, 0) in Kit px, so its feet line is (0, 0).
// Sockets: eyes left (-4, -12) and right (4, -12), mouth (0, -4), arms left (-8, -8).
static ShapePiece boxBody() {
    ShapePiece b = makePiece(ShapeSlot::Body, ShapeOrder::Front, {0}, {{{MemberSide::Mirrored, {{-8, -16, 8, 0}}, ShapeRole::Body}}});
    b.sockets = {sock(ShapeSlot::Eyes, SocketSide::Left, -4, -12), sock(ShapeSlot::Eyes, SocketSide::Right, 4, -12),
                 sock(ShapeSlot::Mouth, SocketSide::Centre, 0, -4), sock(ShapeSlot::Arms, SocketSide::Left, -8, -8)};
    return b;
}

// Eyes 1 "dot": one Mirrored member, open = 2×2 at [0,2)², closed = 2×1 at [0,2)×[1,2).
static ShapePiece dotEyes() {
    return makePiece(ShapeSlot::Eyes, ShapeOrder::Front, {0, 1},
                     {{{MemberSide::Mirrored, {{0, 0, 2, 2}}, ShapeRole::Ink}},
                      {{MemberSide::Mirrored, {{0, 1, 2, 2}}, ShapeRole::Ink}}});
}

// Mouth 1 "line": [-2, 2) × [0, 1), ink.
static ShapePiece lineMouth() {
    return makePiece(ShapeSlot::Mouth, ShapeOrder::Front, {0}, {{{MemberSide::Mirrored, {{-2, 0, 2, 1}}, ShapeRole::Ink}}});
}

// A pack in memory that counts its reads.
class CountingSource : public ShapePackSource {
public:
    explicit CountingSource(const std::vector<uint8_t>& b) : mem_(b.data(), b.size()) {}
    uint32_t size() const override { return mem_.size(); }
    bool readAt(uint32_t offset, uint8_t* dst, uint32_t len) override {
        ++reads;
        return !failReads && mem_.readAt(offset, dst, len);
    }
    int reads = 0;
    bool failReads = false;

private:
    ShapePackMemory mem_;
};

struct Fixture {
    std::vector<uint8_t> bytes;
    CountingSource* src = nullptr;
    ShapePack pack;
    AssetArena arena;

    explicit Fixture(std::vector<ShapePackPieceIn> pieces) {
        const char* err = nullptr;
        if (!shapePackWrite(pieces, bytes, &err)) fprintf(stderr, "fixture: %s\n", err ? err : "?");
        src = new CountingSource(bytes);
        ShapeError e;
        if (!pack.open(*src, &e)) fprintf(stderr, "fixture open: %s\n", e.text);
        arena.allocateBacking(64 * 1024);
    }
    ~Fixture() { delete src; }
};

static std::vector<ShapePackPieceIn> basicPieces() {
    return {{ShapeSlot::Body, 1, "box", boxBody()},
            {ShapeSlot::Eyes, 1, "dot", dotEyes()},
            {ShapeSlot::Mouth, 1, "line", lineMouth()}};
}

static ShapeLook basicLook() {
    ShapeLook l;
    l.set(ShapeSlot::Body, 1);
    l.set(ShapeSlot::Eyes, 1);
    l.set(ShapeSlot::Mouth, 1);
    l.colour[0] = 9;  // body
    l.colour[1] = 4;  // accent
    l.colour[2] = 0;  // ink
    return l;
}

// The layer over a window as rows of hex palette indices ('.' = 15, transparent).
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

static ShapeDrawScratch* scratch() {
    static ShapeDrawScratch* s = new ShapeDrawScratch();
    return s;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void testBodyStandsOnItsFeetLine() {
    Fixture f(basicPieces());
    Layer layer;
    layer.clear(Pixel4(15));
    ShapePackGroup pet;
    ShapeError e;
    ASSERT(pet.build(f.pack, basicLook(), f.arena, &e), e.text);
    pet.setPosition({Fixed::fromInt(80), Fixed::fromInt(100)});
    ASSERT(pet.redraw(layer, *scratch()), "redraw");
    // The Body fills x 72..87, y 84..99; its bottom row sits just above y 100.
    ASSERT(layer.getPixel(72, 99).value == 9, "body bottom-left in body colour");
    ASSERT(layer.getPixel(87, 84).value == 9, "body top-right in body colour");
    ASSERT(layer.getPixel(71, 99).value == 15, "left of the body is transparent");
    ASSERT(layer.getPixel(80, 100).value == 15, "the feet line itself is below the body");
    ASSERT(layer.getPixel(88, 90).value == 15, "right of the body is transparent");
}

// The basic Look at (80, 100) over the Body's window x 72..87, y 86..97.
static const char* kBasicPicture =
    "9999999999999999\n"
    "9999999999999999\n"
    "9900999999990099\n"  // eyes: left (mirrored about x 76) at 74..75, right at 84..85
    "9900999999990099\n"
    "9999999999999999\n"
    "9999999999999999\n"
    "9999999999999999\n"
    "9999999999999999\n"
    "9999999999999999\n"
    "9999999999999999\n"
    "9999990000999999\n"  // mouth at the centre socket: 78..81
    "9999999999999999";

static Layer* drawBasic(ShapePackGroup& pet, Fixture& f) {
    static Layer layer;
    layer.clear(Pixel4(15));
    ShapeError e;
    ASSERT(pet.build(f.pack, basicLook(), f.arena, &e), e.text);
    pet.setPosition({Fixed::fromInt(80), Fixed::fromInt(100)});
    ASSERT(pet.redraw(layer, *scratch()), "redraw");
    return &layer;
}

static void testFeaturesSitOnTheirSockets() {
    Fixture f(basicPieces());
    ShapePackGroup pet;
    Layer& layer = *drawBasic(pet, f);
    ASSERT(expectPicture(layer, 72, 86, 16, 12, kBasicPicture, "basic Look"), "eyes mirrored on the left socket, mouth centred");
}

// A Feature with `_l` / `_r` members puts each on its side's socket, unmirrored.
static void testSideMembersGoOnTheirSide() {
    std::vector<ShapePackPieceIn> pieces = basicPieces();
    // Eyes 2: left member is 1 px wide at [0,1), right member 2 px at [0,2).
    pieces.push_back({ShapeSlot::Eyes, 2, "odd",
                      makePiece(ShapeSlot::Eyes, ShapeOrder::Front, {0},
                                {{{MemberSide::Left, {{0, 0, 1, 2}}, ShapeRole::Ink},
                                  {MemberSide::Right, {{0, 0, 2, 2}}, ShapeRole::Ink}}})});
    Fixture f(pieces);
    ShapePackGroup pet;
    Layer& layer = *drawBasic(pet, f);
    ShapeLook l = basicLook();
    l.set(ShapeSlot::Eyes, 2);
    ShapeError e;
    ASSERT(pet.setLook(l, &e), e.text);
    pet.redraw(layer, *scratch());
    // Left socket x 76: the 1-px left member at 76; right socket x 84: 84..85.
    ASSERT(expectPicture(layer, 72, 88, 16, 1, "9999099999990099", "side members"), "side members unmirrored");
}

// Arms 1 is drawn behind the Body, arms 2 in front; both [-2, 4) × [0, 2) in
// accent, mirrored on the Body's one (left) arms socket at screen (72, 92).
static std::vector<ShapePackPieceIn> armsPieces() {
    std::vector<ShapePackPieceIn> pieces = basicPieces();
    pieces.push_back({ShapeSlot::Arms, 1, "back",
                      makePiece(ShapeSlot::Arms, ShapeOrder::Behind, {0},
                                {{{MemberSide::Mirrored, {{-2, 0, 4, 2}}, ShapeRole::Accent}}})});
    pieces.push_back({ShapeSlot::Arms, 2, "front",
                      makePiece(ShapeSlot::Arms, ShapeOrder::Front, {0},
                                {{{MemberSide::Mirrored, {{-2, 0, 4, 2}}, ShapeRole::Accent}}})});
    return pieces;
}

static void testDrawOrderBehindAndInFront() {
    Fixture f(armsPieces());
    ShapePackGroup pet;
    Layer& layer = *drawBasic(pet, f);
    ShapeLook l = basicLook();
    l.set(ShapeSlot::Arms, 1);
    ShapeError e;
    ASSERT(pet.setLook(l, &e), e.text);
    pet.redraw(layer, *scratch());
    // Mirrored [-2, 4) is [-4, 2) about x 72: 68..73; the Body covers 72..73.
    ASSERT(expectPicture(layer, 68, 92, 8, 1, "44449999", "arms behind"), "behind: the Body paints over the arm");
    l.set(ShapeSlot::Arms, 2);
    ASSERT(pet.setLook(l, &e), e.text);
    pet.redraw(layer, *scratch());
    ASSERT(expectPicture(layer, 68, 92, 8, 1, "44444499", "arms in front"), "in front: the arm paints over the Body");
}

// Body 2 "capped": the box with a top socket at its crown (0, -16) and an arms
// socket on the left (-8, -8). Top 1 "cap" (in front) and top 2 "hood"
// (behind) are accent; arms 3 "fist" is ink, [-2, 4) × [0, 2) mirrored.
static std::vector<ShapePackPieceIn> cappedPieces() {
    std::vector<ShapePackPieceIn> pieces = basicPieces();
    ShapePiece b = boxBody();
    b.sockets.push_back(sock(ShapeSlot::Top, SocketSide::Centre, 0, -16));
    pieces.push_back({ShapeSlot::Body, 2, "capped", b});
    // The cap covers the whole Body: [-8, 8) × [0, 16) from the crown.
    pieces.push_back({ShapeSlot::Top, 1, "cap",
                      makePiece(ShapeSlot::Top, ShapeOrder::Front, {0},
                                {{{MemberSide::Mirrored, {{-8, 0, 8, 16}}, ShapeRole::Accent}}})});
    // The hood is wider than the Body: [-12, 12) × [0, 16) from the crown.
    pieces.push_back({ShapeSlot::Top, 2, "hood",
                      makePiece(ShapeSlot::Top, ShapeOrder::Behind, {0},
                                {{{MemberSide::Mirrored, {{-12, 0, 12, 16}}, ShapeRole::Accent}}})});
    pieces.push_back({ShapeSlot::Arms, 3, "fist",
                      makePiece(ShapeSlot::Arms, ShapeOrder::Front, {0},
                                {{{MemberSide::Mirrored, {{-2, 0, 4, 2}}, ShapeRole::Ink}}})});
    pieces.push_back({ShapeSlot::Arms, 4, "fist-back",
                      makePiece(ShapeSlot::Arms, ShapeOrder::Behind, {0},
                                {{{MemberSide::Mirrored, {{-2, 0, 4, 2}}, ShapeRole::Ink}}})});
    return pieces;
}

// #317 decision 6: in front, top paints first, then limbs, cheeks, mouth, and
// the eyes last. A cap drawn over the whole Body leaves the face on top.
static void testAnInFrontTopPaintsUnderTheFace() {
    Fixture f(cappedPieces());
    ShapePackGroup pet;
    Layer& layer = *drawBasic(pet, f);
    ShapeLook l = basicLook();
    l.set(ShapeSlot::Body, 2);
    l.set(ShapeSlot::Top, 1);
    ShapeError e;
    ASSERT(pet.setLook(l, &e), e.text);
    pet.redraw(layer, *scratch());
    std::string want = kBasicPicture;
    for (char& c : want)
        if (c == '9') c = '4';
    ASSERT(expectPicture(layer, 72, 86, 16, 12, want.c_str(), "cap under the face"), "eyes and mouth paint over the cap");
}

// #317 decision 6: behind the Body, limbs paint first and the top over them;
// in front, the limbs paint over the top.
static void testLimbsAndTopKeepTheirOrderOnEachSide() {
    Fixture f(cappedPieces());
    ShapePackGroup pet;
    Layer& layer = *drawBasic(pet, f);
    ShapeLook l = basicLook();
    l.set(ShapeSlot::Body, 2);
    l.set(ShapeSlot::Top, 2);
    l.set(ShapeSlot::Arms, 4);
    ShapeError e;
    ASSERT(pet.setLook(l, &e), e.text);
    pet.redraw(layer, *scratch());
    // The hood spans x 68..91; the arm 68..73 is under it, the Body over both.
    ASSERT(expectPicture(layer, 66, 92, 10, 1, "..44449999", "behind: hood over arm"), "behind: the top paints over the limbs");
    l.set(ShapeSlot::Top, 1);
    l.set(ShapeSlot::Arms, 3);
    ASSERT(pet.setLook(l, &e), e.text);
    pet.redraw(layer, *scratch());
    // The cap covers the Body at 72..87; the arm 68..73 paints over it.
    ASSERT(expectPicture(layer, 66, 92, 10, 1, "..00000044", "in front: arm over cap"), "in front: the limbs paint over the top");
}

static bool refuses(Fixture& f, const ShapeLook& l, const char* want) {
    ShapePackGroup pet;
    ShapeError e;
    const size_t before = f.arena.used();
    const bool ok = pet.build(f.pack, l, f.arena, &e);
    if (ok || std::strcmp(e.text, want) != 0) {
        fprintf(stderr, "  want refusal \"%s\", got %s \"%s\"\n", want, ok ? "success" : "refusal", e.text);
        return false;
    }
    return f.arena.used() == before && !pet.built();
}

static void testBadLooksAreRefusedNamingSlotAndId() {
    Fixture f(armsPieces());
    ShapeLook l = basicLook();
    l.clear(ShapeSlot::Mouth);
    ASSERT(refuses(f, l, "Look: no mouth (body, eyes and mouth are required)"), "missing mouth");
    l = basicLook();
    l.clear(ShapeSlot::Body);
    ASSERT(refuses(f, l, "Look: no body (body, eyes and mouth are required)"), "missing body");
    l = basicLook();
    l.clear(ShapeSlot::Eyes);
    ASSERT(refuses(f, l, "Look: no eyes (body, eyes and mouth are required)"), "missing eyes");
    l = basicLook();
    l.set(ShapeSlot::Mouth, 17);
    ASSERT(refuses(f, l, "Look: mouth 17 is not in the Shape pack (unknown or retired id)"), "unknown id");
    l = basicLook();
    l.set(ShapeSlot::Legs, 1);
    ASSERT(refuses(f, l, "Look: legs 1 is not in the Shape pack (unknown or retired id)"), "id in no slot");
    l = basicLook();
    l.colour[2] = 15;
    ASSERT(refuses(f, l, "Look: colours.ink 15 is outside the palette (0..14; 15 is transparent)"), "colour");
}

static void testASlotWithNoSocketIsRefused() {
    std::vector<ShapePackPieceIn> pieces = basicPieces();
    pieces.push_back({ShapeSlot::Top, 3, "stem",
                      makePiece(ShapeSlot::Top, ShapeOrder::Behind, {0},
                                {{{MemberSide::Mirrored, {{0, -4, 1, 0}}, ShapeRole::Body}}})});
    Fixture f(pieces);
    ShapeLook l = basicLook();
    l.set(ShapeSlot::Top, 3);
    ASSERT(refuses(f, l, "Look: top 3 has no socket on body 1"), "no top socket on the box");
}

static void testSetLookRereadsOnlyChangedSlotsInPlace() {
    Fixture f(armsPieces());
    ShapePackGroup pet;
    drawBasic(pet, f);
    const size_t used = f.arena.used();
    ShapeError e;
    ShapeLook l = basicLook();

    f.src->reads = 0;
    l.set(ShapeSlot::Arms, 1);
    ASSERT(pet.setLook(l, &e), e.text);
    ASSERT(f.src->reads == 1, "adding arms reads one record");

    f.src->reads = 0;
    l.set(ShapeSlot::Arms, 2);
    l.set(ShapeSlot::Eyes, 1);
    ASSERT(pet.setLook(l, &e), e.text);
    ASSERT(f.src->reads == 1, "swapping arms reads only the arms");

    f.src->reads = 0;
    l.clear(ShapeSlot::Arms);
    ASSERT(pet.setLook(l, &e), e.text);
    ASSERT(f.src->reads == 0, "emptying a slot reads nothing");

    for (int i = 0; i < 50; ++i) {
        l.set(ShapeSlot::Arms, uint16_t(1 + (i & 1)));
        pet.setLook(l, &e);
    }
    ASSERT(f.arena.used() == used, "rolling Looks never grows the arena");
}

static void testColoursOnlyChangeReadsNothing() {
    Fixture f(basicPieces());
    ShapePackGroup pet;
    Layer& layer = *drawBasic(pet, f);
    ASSERT(!pet.changed(), "clean after a redraw");
    f.src->reads = 0;
    ShapeLook l = basicLook();
    l.colour[0] = 3;
    l.colour[2] = 12;
    ShapeError e;
    ASSERT(pet.setLook(l, &e), e.text);
    ASSERT(f.src->reads == 0, "a colours-only change does no I/O");
    ASSERT(pet.changed(), "a colours-only change redraws");
    pet.flush(layer, *scratch());
    ASSERT(expectPicture(layer, 72, 88, 16, 1, "33cc33333333cc33", "recoloured"), "body and ink recoloured");
}

static void testPoseSwapsTheWholeFeatureWithoutIo() {
    Fixture f(basicPieces());
    ShapePackGroup pet;
    Layer& layer = *drawBasic(pet, f);
    ShapeError e;
    ASSERT(std::strcmp(pet.poseName(ShapeSlot::Eyes), "open") == 0, "a new Feature starts in its default pose");
    const char* names[8];
    ASSERT(pet.poseNames(ShapeSlot::Eyes, names, 8) == 2 && std::strcmp(names[1], "closed") == 0, "poses the eyes have");
    f.src->reads = 0;
    ASSERT(pet.setPose(ShapeSlot::Eyes, "closed", &e), e.text);
    ASSERT(f.src->reads == 0, "a pose swap does no I/O");
    pet.flush(layer, *scratch());
    ASSERT(expectPicture(layer, 72, 88, 16, 2, "9999999999999999\n9900999999990099", "closed eyes"), "closed eyes drawn");

    ASSERT(!pet.setPose(ShapeSlot::Eyes, "wink", &e), "unknown pose");
    ASSERT(std::strcmp(e.text, "pose: eyes has no pose \"wink\"") == 0, e.text);
    ASSERT(!pet.setPose(ShapeSlot::Eyes, "angry", &e), "pose the Feature lacks");
    ASSERT(std::strcmp(e.text, "pose: eyes 1 has no \"angry\" pose") == 0, e.text);
    ASSERT(!pet.setPose(ShapeSlot::Arms, "default", &e), "empty slot");
    ASSERT(std::strcmp(e.text, "pose: no arms in this Look") == 0, e.text);
}

static void testMovingClearsTheOldFootprint() {
    Fixture f(basicPieces());
    ShapePackGroup pet;
    Layer& layer = *drawBasic(pet, f);
    layer.clearDirty();
    // Body at x 72..87, y 84..99 covers tiles (4..5, 5..6). Move 40 px left.
    pet.setPosition({Fixed::fromInt(40), Fixed::fromInt(100)});
    ASSERT(pet.flush(layer, *scratch()), "a move redraws");
    ASSERT(layer.getPixel(80, 90).value == 15, "the old spot is transparent again");
    ASSERT(layer.getPixel(40, 90).value == 9, "the new spot is drawn");
    ASSERT(layer.isTileDirty(5, 5) && layer.isTileDirty(5, 6), "old tiles dirty");
    ASSERT(layer.isTileDirty(2, 5) && layer.isTileDirty(2, 6), "new tiles dirty (x 32..47)");
    ASSERT(!layer.isTileDirty(8, 0), "untouched tiles stay clean");
    layer.clearDirty();
    ASSERT(!pet.flush(layer, *scratch()) && !layer.hasDirty(), "no change, no redraw");
    pet.erase(layer);
    ASSERT(layer.getPixel(40, 90).value == 15 && layer.isTileDirty(2, 5), "erase clears and marks the footprint");
}

static void testAFailedReadRollsTheBuildBack() {
    Fixture f(basicPieces());
    f.src->failReads = true;
    ShapePackGroup pet;
    ShapeError e;
    const size_t before = f.arena.used();
    ASSERT(!pet.build(f.pack, basicLook(), f.arena, &e), "a read failure refuses the build");
    ASSERT(f.arena.used() == before && !pet.built(), "and rolls the arena back");
}

int main() {
    testSetLookRereadsOnlyChangedSlotsInPlace();
    testColoursOnlyChangeReadsNothing();
    testPoseSwapsTheWholeFeatureWithoutIo();
    testMovingClearsTheOldFootprint();
    testAFailedReadRollsTheBuildBack();
    testBodyStandsOnItsFeetLine();
    testFeaturesSitOnTheirSockets();
    testSideMembersGoOnTheirSide();
    testDrawOrderBehindAndInFront();
    testAnInFrontTopPaintsUnderTheFace();
    testLimbsAndTopKeepTheirOrderOnEachSide();
    testBadLooksAreRefusedNamingSlotAndId();
    testASlotWithNoSocketIsRefused();
    printf("shape_pack_group_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
