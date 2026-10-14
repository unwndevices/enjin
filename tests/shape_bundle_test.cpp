/**
 * @file shape_bundle_test.cpp
 * @brief Tests for the Shape pack format and the Shape bundle function (Tomodachi #330)
 *
 * Test IDs:
 *   SHP-01  Shape pack round trip: writer → index → one record by seek.
 *   SHP-02  A minimal Kit bundles: polygon Body, origin at the artboard centre, Q8.8.
 *
 * `shape_bundle_test --verify-pack <a.njn> [<b.njn>]` checks a CLI output
 * (index readable, every record decodes; with two files, identical indices).
 */

#include <enjin2/graphics/shape_pack.hpp>
#include <enjin2/import/shape_bundle.hpp>

#include <cmath>
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
// SHP-01  Pack round trip
// ---------------------------------------------------------------------------

static ShapePiece sampleFeature() {
    ShapePiece p;
    p.slot = ShapeSlot::Eyes;
    p.binding = ShapeBinding::Rigid;
    p.order = ShapeOrder::Front;
    p.swayHz = 0;
    p.swayZeta = 0;
    p.poses = {0, 1};  // open, closed
    p.members = {{MemberSide::Mirrored, {int16_t(-6 * 256), int16_t(-13 * 256)}}};
    // pose 0 member 0 → shape 0; pose 1 member 0 → shape 1
    p.spans = {{0, 1}, {1, 1}};
    ShapeFill a;
    a.role = ShapeRole::Ink;
    a.firstRing = 0;
    a.ringCount = 1;
    ShapeFill b = a;
    b.flags = SHAPE_FILL_EVENODD;
    b.firstRing = 1;
    p.shapes = {a, b};
    p.rings = {{0, 4, SHAPE_RING_CLOSED}, {4, 3, 0}};
    p.points = {{-256, -384}, {256, -384}, {256, 384}, {-256, 384},
                {-256, 0}, {0, 128}, {256, 0}};
    return p;
}

static ShapePiece sampleBody() {
    ShapePiece p;
    p.slot = ShapeSlot::Body;
    p.poses = {0};
    p.members = {{MemberSide::Mirrored, {0, 0}}};
    p.spans = {{0, 1}};
    ShapeFill s;
    s.firstRing = 0;
    s.ringCount = 1;
    p.shapes = {s};
    p.rings = {{0, 3, SHAPE_RING_CLOSED}};
    p.points = {{0, -5120}, {5120, 5120}, {-5120, 5120}};
    p.sockets = {{ShapeSlot::Eyes, SocketSide::Left, {-1280, -3328}},
                 {ShapeSlot::Eyes, SocketSide::Right, {1280, -3328}},
                 {ShapeSlot::Mouth, SocketSide::Centre, {0, 0}}};
    return p;
}

static bool samePiece(const ShapePiece& a, const ShapePiece& b) {
    auto pt = [](ShapePoint x, ShapePoint y) { return x.x == y.x && x.y == y.y; };
    if (a.slot != b.slot || a.binding != b.binding || a.order != b.order || a.swayHz != b.swayHz ||
        a.swayZeta != b.swayZeta || a.poses != b.poses)
        return false;
    if (a.members.size() != b.members.size() || a.spans.size() != b.spans.size() ||
        a.shapes.size() != b.shapes.size() || a.rings.size() != b.rings.size() ||
        a.points.size() != b.points.size() || a.sockets.size() != b.sockets.size())
        return false;
    for (size_t i = 0; i < a.members.size(); ++i)
        if (a.members[i].side != b.members[i].side || !pt(a.members[i].home, b.members[i].home)) return false;
    for (size_t i = 0; i < a.spans.size(); ++i)
        if (a.spans[i].firstShape != b.spans[i].firstShape || a.spans[i].shapeCount != b.spans[i].shapeCount) return false;
    for (size_t i = 0; i < a.shapes.size(); ++i) {
        const ShapeFill& x = a.shapes[i];
        const ShapeFill& y = b.shapes[i];
        if (x.role != y.role || x.flags != y.flags || x.dither != y.dither || x.strokeWidth != y.strokeWidth ||
            x.strokeRole != y.strokeRole || x.clip != y.clip || x.clipShape != y.clipShape ||
            x.firstRing != y.firstRing || x.ringCount != y.ringCount)
            return false;
    }
    for (size_t i = 0; i < a.rings.size(); ++i)
        if (a.rings[i].firstPoint != b.rings[i].firstPoint || a.rings[i].pointCount != b.rings[i].pointCount ||
            a.rings[i].flags != b.rings[i].flags)
            return false;
    for (size_t i = 0; i < a.points.size(); ++i)
        if (!pt(a.points[i], b.points[i])) return false;
    for (size_t i = 0; i < a.sockets.size(); ++i)
        if (a.sockets[i].slot != b.sockets[i].slot || a.sockets[i].side != b.sockets[i].side ||
            !pt(a.sockets[i].at, b.sockets[i].at))
            return false;
    return true;
}

static void test01_packRoundTrip() {
    std::vector<ShapePackPieceIn> in = {
        {ShapeSlot::Body, 1, "star", sampleBody()},
        {ShapeSlot::Eyes, 7, "star", sampleFeature()},
    };
    Bytes pack;
    const char* err = nullptr;
    ASSERT(shapePackWrite(in, pack, &err), "SHP-01 writer accepts two valid pieces");

    ShapePackMemory mem(pack.data(), pack.size());
    ShapePackIndex index;
    ASSERT(index.open(mem, &err), "SHP-01 index opens");
    ASSERT(index.pieces().size() == 2, "SHP-01 index lists both pieces");
    if (index.pieces().size() != 2) return;
    ASSERT(index.pieces()[0].slot == ShapeSlot::Body && index.pieces()[0].id == 1 &&
           index.pieces()[0].label == "star", "SHP-01 index entry: slot, id, label");
    const ShapePackEntry* eyes = index.find(ShapeSlot::Eyes, 7);
    ASSERT(eyes != nullptr, "SHP-01 find by slot + id");
    ASSERT(index.find(ShapeSlot::Eyes, 1) == nullptr, "SHP-01 unknown id not found");

    ShapePiece got;
    ASSERT(eyes && index.readPiece(mem, *eyes, got, &err), "SHP-01 eyes record reads by seek");
    ASSERT(samePiece(got, sampleFeature()), "SHP-01 eyes record round-trips");
    ShapePiece body;
    ASSERT(index.readPiece(mem, index.pieces()[0], body, &err), "SHP-01 body record reads");
    ASSERT(samePiece(body, sampleBody()), "SHP-01 body record round-trips (sockets)");

    // The record bytes are exactly the index's offset/size window in the file.
    Bytes rec(pack.begin() + eyes->offset, pack.begin() + eyes->offset + eyes->size);
    ShapePiece fromBytes;
    ASSERT(shapePackDecodePiece(rec.data(), rec.size(), fromBytes, &err) && samePiece(fromBytes, sampleFeature()),
           "SHP-01 decode from the raw record window");
}


// ---------------------------------------------------------------------------
// Bundle helpers
// ---------------------------------------------------------------------------

static Bytes bytesOf(const std::string& s) { return Bytes(s.begin(), s.end()); }

/// An 80×80 Kit: @p body is the body layer's markup, plus eyes and mouth
/// anchors (so the Body has the sockets it needs) unless @p rest replaces them.
static std::string kitSvg(const std::string& body, const std::string& rest =
    "<g id=\"eyes\"><circle id=\"anchor\" cx=\"40\" cy=\"30\" r=\"1\"/></g>"
    "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"1\"/></g>",
    const std::string& head = "") {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" viewBox=\"0 0 80 80\">" + head +
           "<g id=\"body\">" + body + "</g>" + rest + "</svg>";
}

static ShapeBundleResult bundleOne(const std::string& svg, const std::string& sidecar = "",
                                   const std::string& ledger = "") {
    return bundleShapes({{"kit.svg", bytesOf(svg), bytesOf(sidecar)}}, ledger);
}

static bool hasIssue(const ShapeBundleResult& r, const char* code, ImportIssueSeverity sev) {
    for (const auto& i : r.issues)
        if (i.code == code && i.severity == sev) return true;
    return false;
}

static bool hasError(const ShapeBundleResult& r, const char* code) {
    return hasIssue(r, code, ImportIssueSeverity::Error);
}

static void dumpIssues(const ShapeBundleResult& r) {
    for (const auto& i : r.issues)
        fprintf(stderr, "    %s %s: %s [%s %s]\n", importIssueSeverityName(i.severity), i.code.c_str(),
                i.message.c_str(), i.source.c_str(), i.layer.c_str());
}

/// Read piece (slot, id) from a bundled pack.
static bool pieceOf(const ShapeBundleResult& r, ShapeSlot slot, uint16_t id, ShapePiece& out) {
    ShapePackMemory mem(r.pack.data(), r.pack.size());
    ShapePackIndex index;
    if (!index.open(mem)) return false;
    const ShapePackEntry* e = index.find(slot, id);
    return e && index.readPiece(mem, *e, out);
}

/// The first piece of @p slot in a bundled pack.
static bool firstPiece(const ShapeBundleResult& r, ShapeSlot slot, ShapePiece& out) {
    ShapePackMemory mem(r.pack.data(), r.pack.size());
    ShapePackIndex index;
    if (!index.open(mem)) return false;
    for (const auto& e : index.pieces())
        if (e.slot == slot) return index.readPiece(mem, e, out);
    return false;
}

static ShapePoint q(double x, double y) {
    return {static_cast<int16_t>(std::lround(x * 256)), static_cast<int16_t>(std::lround(y * 256))};
}

/// Points of ring @p ring of shape @p shape, as a vector.
static std::vector<ShapePoint> ringPoints(const ShapePiece& p, size_t shape, size_t ring = 0) {
    std::vector<ShapePoint> out;
    if (shape >= p.shapes.size() || ring >= p.shapes[shape].ringCount) return out;
    const ShapeRing& r = p.rings[p.shapes[shape].firstRing + ring];
    for (uint16_t i = 0; i < r.pointCount; ++i) out.push_back(p.points[r.firstPoint + i]);
    return out;
}

static bool samePoints(const std::vector<ShapePoint>& a, const std::vector<ShapePoint>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].x != b[i].x || a[i].y != b[i].y) return false;
    return true;
}

static void printPoints(const char* what, const std::vector<ShapePoint>& v) {
    fprintf(stderr, "    %s:", what);
    for (const auto& p : v) fprintf(stderr, " (%.3f,%.3f)", p.x / 256.0, p.y / 256.0);
    fprintf(stderr, "\n");
}

// ---------------------------------------------------------------------------
// SHP-02  Minimal Kit
// ---------------------------------------------------------------------------

static void test02_minimalKit() {
    const auto r = bundleOne(kitSvg("<polygon points=\"40,10 70,70 10,70\"/>"));
    ASSERT(r.ok(), "SHP-02 minimal Kit bundles");
    if (!r.ok()) dumpIssues(r);
    ShapePiece body;
    ASSERT(pieceOf(r, ShapeSlot::Body, 1, body), "SHP-02 body is piece 1");
    ASSERT(body.shapes.size() == 1 && body.rings.size() == 1, "SHP-02 one shape, one ring");
    const auto pts = ringPoints(body, 0);
    ASSERT(samePoints(pts, {q(0, -30), q(30, 30), q(-30, 30)}), "SHP-02 polygon relative to the artboard centre");
    ASSERT(body.rings.size() == 1 && (body.rings[0].flags & SHAPE_RING_CLOSED), "SHP-02 polygon ring is closed");
    ASSERT(body.sockets.size() == 2, "SHP-02 eyes + mouth sockets");
    if (body.sockets.size() == 2) {
        ASSERT(body.sockets[0].slot == ShapeSlot::Eyes && body.sockets[0].side == SocketSide::Centre &&
               body.sockets[0].at.x == 0 && body.sockets[0].at.y == q(0, -10).y,
               "SHP-02 eyes socket at its anchor");
        ASSERT(body.sockets[1].slot == ShapeSlot::Mouth && body.sockets[1].at.y == q(0, 5).y,
               "SHP-02 mouth socket at its anchor");
    }
    ShapePiece none;
    ASSERT(!firstPiece(r, ShapeSlot::Eyes, none), "SHP-02 anchor-only slots make no Feature");
}

// ---------------------------------------------------------------------------

static Bytes readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

static int verifyPack(int argc, char** argv) {
    std::vector<std::vector<ShapePackEntry>> indices;
    for (int i = 2; i < argc; ++i) {
        const Bytes pack = readFile(argv[i]);
        ShapePackMemory mem(pack.data(), pack.size());
        ShapePackIndex index;
        const char* err = "";
        if (!index.open(mem, &err)) {
            fprintf(stderr, "%s: %s\n", argv[i], err);
            return 1;
        }
        for (const auto& e : index.pieces()) {
            ShapePiece p;
            if (!index.readPiece(mem, e, p, &err)) {
                fprintf(stderr, "%s: %s %u: %s\n", argv[i], shapeSlotName(e.slot), e.id, err);
                return 1;
            }
            printf("%s %u \"%s\" (%u bytes)\n", shapeSlotName(e.slot), e.id, e.label.c_str(), e.size);
        }
        indices.push_back(index.pieces());
    }
    for (size_t i = 1; i < indices.size(); ++i) {
        bool same = indices[i].size() == indices[0].size();
        for (size_t k = 0; same && k < indices[0].size(); ++k)
            same = indices[i][k].slot == indices[0][k].slot && indices[i][k].id == indices[0][k].id &&
                   indices[i][k].label == indices[0][k].label;
        if (!same) {
            fprintf(stderr, "pack indices differ\n");
            return 1;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 3 && std::strcmp(argv[1], "--verify-pack") == 0) return verifyPack(argc, argv);

    test01_packRoundTrip();
    test02_minimalKit();

    printf("shape_bundle_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
