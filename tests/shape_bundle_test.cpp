/**
 * @file shape_bundle_test.cpp
 * @brief Tests for the Shape pack format and the Shape bundle function (Tomodachi #330)
 *
 * Test IDs:
 *   SHP-01  Shape pack round trip: writer → index → one record by seek.
 *   SHP-02  A minimal Kit bundles: polygon Body, origin at the artboard centre, Q8.8.
 *   SHP-03  Path commands M/L/H/V/Z, absolute and relative, compact numbers, implicit lineto.
 *   SHP-04  Curves C/S/Q/T: S/T reflect, relative = absolute, flattening within tolerance.
 *   SHP-05  Arcs A: sweep and large-arc flags, radii scaled up, on the circle.
 *   SHP-06  Elements: circle, ellipse, rect (rx/ry), polyline, line.
 *   SHP-07  Transforms baked: translate, scale, rotate about a point, matrix, skewX, nesting.
 *   SHP-08  Paint: CSS classes, presentation attributes, style="", inheritance, fill-rule, none.
 *   SHP-09  Kit pull-apart: sockets + sides, anchors, poses, mirroring, _l/_r, draw order.
 *   SHP-10  Shape sidecar: Binding and sway defaults per slot, overrides.
 *   SHP-11  Id ledger: assign from 1, stable across runs, retire, never reuse, sorted output.
 *   SHP-12  Layer names: data-name over id, _xHH_ escapes, legacy _N_ suffix.
 *   SHP-13  Hard errors: no pack, ledger unchanged.
 *   SHP-14  The stand-in Star Kit (tools/testdata/shapes) bundles into the expected pieces.
 *   SHP-15  Truncation / mutation corpus over the Kit, sidecar, ledger and pack: never crashes.
 *   SHP-16  Preview (Tomodachi #340): a hard error drops its piece, or its whole source; the rest packs.
 *   SHP-17  Each source's fill colours come back, in first-use order, mapped or not.
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


/// The body's shape 0 ring 0 for @p markup (fails the test on a bundle error).
static std::vector<ShapePoint> bodyRing(const std::string& markup, const char* id, size_t shape = 0,
                                        ShapePiece* pieceOut = nullptr, const std::string& head = "",
                                        const std::string& sidecar = "") {
    const auto r = bundleOne(kitSvg(markup, "<g id=\"eyes\"><circle id=\"anchor\" cx=\"40\" cy=\"30\" r=\"1\"/></g>"
                                           "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"1\"/></g>", head),
                             sidecar);
    ShapePiece body;
    if (!r.ok() || !pieceOf(r, ShapeSlot::Body, 1, body)) {
        fprintf(stderr, "  %s: bundle failed\n", id);
        dumpIssues(r);
        return {};
    }
    if (pieceOut) *pieceOut = body;
    return ringPoints(body, shape);
}

static double dist(ShapePoint p, double x, double y) { return std::hypot(p.x / 256.0 - x, p.y / 256.0 - y); }

/// Max distance from the polyline @p pts to a point.
static double segDist(double px, double py, double ax, double ay, double bx, double by) {
    const double dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
    double t = l2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return std::hypot(px - (ax + t * dx), py - (ay + t * dy));
}

static double polyDist(const std::vector<ShapePoint>& pts, double x, double y, bool closed) {
    double best = 1e9;
    const size_t n = pts.size();
    for (size_t i = 0; i + (closed ? 0 : 1) < n; ++i) {
        const ShapePoint a = pts[i], b = pts[(i + 1) % n];
        best = std::fmin(best, segDist(x, y, a.x / 256.0, a.y / 256.0, b.x / 256.0, b.y / 256.0));
    }
    return best;
}

// Flattening budget: 0.125 px at 2× = 0.0625 px, plus Q8.8 rounding of both chord ends.
static constexpr double kTol = 0.0625 + 2.0 / 512.0;

static void test03_lineCommands() {
    const std::vector<ShapePoint> square = {q(-30, -30), q(30, -30), q(30, 30), q(-30, 30)};
    ASSERT(samePoints(bodyRing("<path d=\"M10,10 L70,10 V70 H10 Z\"/>", "SHP-03"), square), "SHP-03 absolute M L V H Z");
    ASSERT(samePoints(bodyRing("<path d=\"m10 10 l60 0 v60 h-60 z\"/>", "SHP-03"), square), "SHP-03 relative m l v h z");
    ASSERT(samePoints(bodyRing("<path d=\"M10 10h60v60H10z\"/>", "SHP-03"), square), "SHP-03 compact command letters");
    ASSERT(samePoints(bodyRing("<path d=\"M10 10 70 10 70 70 10 70z\"/>", "SHP-03"), square), "SHP-03 implicit lineto after M");
    ASSERT(samePoints(bodyRing("<path d=\"m10 10 60 0 0 60-60 0z\"/>", "SHP-03"), square),
           "SHP-03 implicit relative lineto after m, sign as separator");
    ASSERT(samePoints(bodyRing("<path d=\"M40.5.5L70 70 10 70z\"/>", "SHP-03"), {q(0.5, -39.5), q(30, 30), q(-30, 30)}),
           "SHP-03 '.5.5' is two numbers");
    ShapePiece p;
    bodyRing("<path d=\"M10 10 L70 10 L70 70\"/>", "SHP-03", 0, &p);
    ASSERT(p.rings.size() == 1 && !(p.rings[0].flags & SHAPE_RING_CLOSED), "SHP-03 a path without Z is an open ring");
    bodyRing("<path d=\"M10 10 L70 10 L70 70 Z M30 30 L50 30 L50 50 Z\"/>", "SHP-03", 0, &p);
    ASSERT(p.shapes.size() == 1 && p.shapes[0].ringCount == 2, "SHP-03 two subpaths are two rings of one Shape (holes)");
    const auto r = bodyRing("<path d=\"M10 10 L70 10 L70 70 Z l0 -20 l-20 0 z\"/>", "SHP-03", 0, &p);
    ASSERT(p.shapes.size() == 1 && p.shapes[0].ringCount == 2 && samePoints(ringPoints(p, 0, 1), {q(-30, -30), q(-30, -50), q(-50, -50)}),
           "SHP-03 after Z, a relative command starts from the subpath start");
}

static void test04_curves() {
    // S reflects the previous C's second control point; T the previous Q's control point.
    const auto s = bodyRing("<path d=\"M10 40 C10 20 30 20 40 40 S70 60 70 40 Z\"/>", "SHP-04");
    const auto c = bodyRing("<path d=\"M10 40 C10 20 30 20 40 40 C50 60 70 60 70 40 Z\"/>", "SHP-04");
    ASSERT(!s.empty() && samePoints(s, c), "SHP-04 S = C with the reflected control point");
    const auto t = bodyRing("<path d=\"M10 40 Q25 10 40 40 T70 40 Z\"/>", "SHP-04");
    const auto qq = bodyRing("<path d=\"M10 40 Q25 10 40 40 Q55 70 70 40 Z\"/>", "SHP-04");
    ASSERT(!t.empty() && samePoints(t, qq), "SHP-04 T = Q with the reflected control point");
    const auto sNoC = bodyRing("<path d=\"M10 40 S30 20 40 40 Z\"/>", "SHP-04");
    const auto sAsC = bodyRing("<path d=\"M10 40 C10 40 30 20 40 40 Z\"/>", "SHP-04");
    ASSERT(!sNoC.empty() && samePoints(sNoC, sAsC), "SHP-04 S without a previous C uses the current point");
    const auto rel = bodyRing("<path d=\"m10 40 c0-20 20-20 30 0 s30 20 30 0z\"/>", "SHP-04");
    ASSERT(samePoints(rel, c), "SHP-04 relative c/s = absolute C/S");
    const auto qrel = bodyRing("<path d=\"m10 40 q15-30 30 0 t30 0z\"/>", "SHP-04");
    ASSERT(samePoints(qrel, qq), "SHP-04 relative q/t = absolute Q/T");

    // Tolerance against the exact curve (Kit coordinates: subtract 40).
    auto cubic = [](double t, double p0, double p1, double p2, double p3) {
        const double u = 1 - t;
        return u * u * u * p0 + 3 * u * u * t * p1 + 3 * u * t * t * p2 + t * t * t * p3;
    };
    const auto big = bodyRing("<path d=\"M5 70 C5 -20 75 -20 75 70 Z\"/>", "SHP-04");
    double worst = 0, off = 0;
    for (int i = 0; i <= 2000; ++i) {
        const double tt = i / 2000.0;
        const double x = cubic(tt, -35, -35, 35, 35), y = cubic(tt, 30, -60, -60, 30);
        worst = std::fmax(worst, polyDist(big, x, y, true));
    }
    for (const auto& pt : big) {
        double best = 1e9;
        for (int i = 0; i <= 100000; ++i) {
            const double tt = i / 100000.0;
            best = std::fmin(best, dist(pt, cubic(tt, -35, -35, 35, 35), cubic(tt, 30, -60, -60, 30)));
        }
        off = std::fmax(off, best);
    }
    ASSERT(worst <= kTol, "SHP-04 every point of the cubic is within 0.0625 px of the flattened ring");
    ASSERT(off <= 1.5 / 256.0, "SHP-04 flattened points lie on the cubic (Q8.8 rounding only)");
    ASSERT(big.size() >= 20 && big.size() <= 120, "SHP-04 the cubic is not over- or under-sampled");
    if (worst > kTol || off > 1.5 / 256.0) fprintf(stderr, "    worst %.4f off %.4f n %zu\n", worst, off, big.size());
}

static void test05_arcs() {
    auto onCircle = [](const std::vector<ShapePoint>& pts, double r, int ySign) {
        if (pts.size() < 8) return false;
        for (const auto& p : pts) {
            const double d = dist(p, 0, 0);
            if (d > r + 2.0 / 256 || d < r - kTol) return false;
            if (ySign * p.y < -2) return false;
        }
        // The chord midpoints stay within tolerance of the circle too.
        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            const double mx = (pts[i].x + pts[i + 1].x) / 512.0, my = (pts[i].y + pts[i + 1].y) / 512.0;
            if (r - std::hypot(mx, my) > kTol) return false;
        }
        return true;
    };
    ShapePiece p;
    auto top = bodyRing("<path d=\"M10 40 A30 30 0 0 1 70 40\"/>", "SHP-05", 0, &p);
    ASSERT(onCircle(top, 30, -1), "SHP-05 sweep 1: the upper half circle (y down)");
    ASSERT(!top.empty() && top.front().x == q(-30, 0).x && top.back().x == q(30, 0).x && top.back().y == 0,
           "SHP-05 arc ends exactly at its end point");
    auto bottom = bodyRing("<path d=\"M10 40 A30 30 0 0 0 70 40\"/>", "SHP-05");
    ASSERT(onCircle(bottom, 30, 1), "SHP-05 sweep 0: the lower half circle");
    auto scaled = bodyRing("<path d=\"M10 40 A1 1 0 0 1 70 40\"/>", "SHP-05");
    ASSERT(samePoints(scaled, top), "SHP-05 radii too small are scaled up (SVG F.6.6)");
    auto rel = bodyRing("<path d=\"m10 40 a30 30 0 0 1 60 0\"/>", "SHP-05");
    ASSERT(samePoints(rel, top), "SHP-05 relative a = absolute A");
    auto flags = bodyRing("<path d=\"M10 40 a30 30 0 0160 0\"/>", "SHP-05");
    ASSERT(samePoints(flags, top), "SHP-05 compact flags '0160' parse as 0, 1, 60");
    // Large arc: three quarters of the circle from (40,10) to (70,40) the long way.
    auto large = bodyRing("<path d=\"M40 10 A30 30 0 1 0 70 40\"/>", "SHP-05");
    bool farSide = false;
    for (const auto& pt : large) farSide = farSide || (pt.x < -29 * 256 || pt.y > 29 * 256);
    ASSERT(onCircle(large, 30, 0) && farSide, "SHP-05 large-arc 1 takes the long way round");
    auto line = bodyRing("<path d=\"M10 40 A0 30 0 0 1 70 40\"/>", "SHP-05");
    ASSERT(samePoints(line, {q(-30, 0), q(30, 0)}), "SHP-05 a zero radius is a straight line");
}

static void test06_elements() {
    ShapePiece p;
    auto circle = bodyRing("<circle cx=\"40\" cy=\"40\" r=\"30\"/>", "SHP-06", 0, &p);
    bool ok = circle.size() >= 16;
    for (const auto& pt : circle) ok = ok && dist(pt, 0, 0) <= 30 + 2.0 / 256 && dist(pt, 0, 0) >= 30 - kTol;
    for (size_t i = 0; i < circle.size(); ++i) {
        const auto a = circle[i], b = circle[(i + 1) % circle.size()];
        ok = ok && 30 - std::hypot((a.x + b.x) / 512.0, (a.y + b.y) / 512.0) <= kTol;
    }
    ASSERT(ok && (p.rings[0].flags & SHAPE_RING_CLOSED), "SHP-06 circle: closed ring within tolerance of r");

    auto ellipse = bodyRing("<ellipse cx=\"40\" cy=\"40\" rx=\"30\" ry=\"15\"/>", "SHP-06");
    ok = ellipse.size() >= 16;
    for (const auto& pt : ellipse) {
        const double x = pt.x / 256.0, y = pt.y / 256.0, e = std::sqrt(x * x / 900 + y * y / 225);
        ok = ok && std::fabs(e - 1) < 0.004;
    }
    ASSERT(ok, "SHP-06 ellipse: points on x²/rx² + y²/ry² = 1");

    ASSERT(samePoints(bodyRing("<rect x=\"10\" y=\"20\" width=\"60\" height=\"40\"/>", "SHP-06"),
                      {q(-30, -20), q(30, -20), q(30, 20), q(-30, 20)}), "SHP-06 rect: four corners");
    auto rounded = bodyRing("<rect x=\"10\" y=\"20\" width=\"60\" height=\"40\" rx=\"8\"/>", "SHP-06");
    bool tangents = false, inside = !rounded.empty();
    for (const auto& pt : rounded) {
        tangents = tangents || (pt.x == q(-22, 0).x && pt.y == q(0, -20).y);
        inside = inside && std::abs(pt.x) <= 30 * 256 && std::abs(pt.y) <= 20 * 256;
        // Corner region: on the corner circle (centre ±22, ±12, r 8).
        const double x = std::fabs(pt.x / 256.0), y = std::fabs(pt.y / 256.0);
        if (x > 22.01 && y > 12.01) inside = inside && std::fabs(std::hypot(x - 22, y - 12) - 8) <= kTol;
    }
    ASSERT(tangents && inside, "SHP-06 rect rx (ry follows): straight edges meet quarter circles");
    auto clamped = bodyRing("<rect x=\"10\" y=\"20\" width=\"60\" height=\"40\" rx=\"50\" ry=\"50\"/>", "SHP-06");
    bool ellipseOk = !clamped.empty();
    for (const auto& pt : clamped) {
        const double x = pt.x / 256.0, y = pt.y / 256.0;
        ellipseOk = ellipseOk && std::fabs(std::sqrt(x * x / 900 + y * y / 400) - 1) < 0.005;
    }
    ASSERT(ellipseOk, "SHP-06 rect rx/ry clamp to half the size (an ellipse)");

    bodyRing("<polygon points=\"10,10 70,10 40,70\"/><polyline points=\"10 70 40 10 70 70\"/><line x1=\"10\" y1=\"40\" x2=\"70\" y2=\"40\"/>",
             "SHP-06", 0, &p);
    ASSERT(p.shapes.size() == 3, "SHP-06 polygon, polyline and line are one Shape each");
    if (p.shapes.size() == 3) {
        ASSERT(samePoints(ringPoints(p, 1), {q(-30, 30), q(0, -30), q(30, 30)}) && !(p.rings[1].flags & SHAPE_RING_CLOSED),
               "SHP-06 polyline: open ring");
        ASSERT(samePoints(ringPoints(p, 2), {q(-30, 0), q(30, 0)}) && !(p.rings[2].flags & SHAPE_RING_CLOSED),
               "SHP-06 line: open two-point ring");
    }
}

static void test07_transforms() {
    const std::string tri = "<polygon points=\"40,20 50,40 30,40\"/>";  // (0,-20) (10,0) (-10,0)
    ASSERT(samePoints(bodyRing("<g transform=\"translate(5 -3)\">" + tri + "</g>", "SHP-07"),
                      {q(5, -23), q(15, -3), q(-5, -3)}), "SHP-07 translate on a group");
    ASSERT(samePoints(bodyRing("<polygon transform=\"scale(0.5)\" points=\"40,20 60,40 20,40\"/>", "SHP-07"),
                      {q(-20, -30), q(-10, -20), q(-30, -20)}), "SHP-07 scale about the user origin");
    ASSERT(samePoints(bodyRing("<polygon transform=\"rotate(90 40 40)\" points=\"40,20 50,40 30,40\"/>", "SHP-07"),
                      {q(20, 0), q(0, 10), q(0, -10)}), "SHP-07 rotate(90 cx cy) about the artboard centre");
    ASSERT(samePoints(bodyRing("<polygon transform=\"matrix(1 0 0 1 2 4)\" points=\"40,20 50,40 30,40\"/>", "SHP-07"),
                      {q(2, -16), q(12, 4), q(-8, 4)}), "SHP-07 matrix()");
    ASSERT(samePoints(bodyRing("<polygon transform=\"skewX(45)\" points=\"0,10 10,10 10,20\"/>", "SHP-07"),
                      {q(-30, -30), q(-20, -30), q(-10, -20)}), "SHP-07 skewX(45)");
    ASSERT(samePoints(bodyRing("<g transform=\"translate(40 40)\"><g transform=\"scale(2)\"><polygon points=\"0,-10 5,0 -5,0\"/></g></g>",
                               "SHP-07"),
                      {q(0, -20), q(10, 0), q(-10, 0)}), "SHP-07 nested transforms compose outer ∘ inner");
    ASSERT(samePoints(bodyRing("<polygon transform=\"translate(40,40) scale(2,1)\" points=\"0,-10 5,0 -5,0\"/>", "SHP-07"),
                      {q(0, -10), q(10, 0), q(-10, 0)}), "SHP-07 a transform list applies right to left");
    // A circle under a non-uniform scale is still flattened in Kit space.
    auto e = bodyRing("<circle transform=\"translate(40 40) scale(3 1)\" r=\"10\"/>", "SHP-07");
    bool ok = e.size() >= 16;
    for (const auto& pt : e) ok = ok && std::fabs(std::sqrt(std::pow(pt.x / 256.0 / 30, 2) + std::pow(pt.y / 256.0 / 10, 2)) - 1) < 0.004;
    ASSERT(ok, "SHP-07 scaled circle: an ellipse in Kit coordinates");
}

static void test08_paint() {
    const std::string style =
        "<defs><style>.st0{fill:#b18cf9;}.st1{fill:#1C1C1C;fill-rule:evenodd}.st2, .st3 { fill: #b5dc6c }</style></defs>";
    const std::string sidecar = "{\"version\":1,\"roles\":{\"#b18cf9\":\"body\",\"#1c1c1c\":\"ink\",\"#b5dc6c\":\"accent\"}}";
    ShapePiece p;
    bodyRing("<rect class=\"st0\" width=\"10\" height=\"10\"/>"
             "<rect class=\"st1\" width=\"10\" height=\"10\"/>"
             "<rect class=\"st3\" width=\"10\" height=\"10\"/>"
             "<rect fill=\"#1c1c1c\" class=\"st0\" width=\"10\" height=\"10\"/>"
             "<rect class=\"st0\" style=\"fill:#1c1c1c\" width=\"10\" height=\"10\"/>"
             "<g fill=\"#b5dc6c\"><rect width=\"10\" height=\"10\"/></g>"
             "<rect fill=\"none\" width=\"10\" height=\"10\"/>"
             "<g fill-rule=\"evenodd\"><rect fill=\"#b18cf9\" width=\"10\" height=\"10\"/></g>",
             "SHP-08", 0, &p, style, sidecar);
    ASSERT(p.shapes.size() == 8, "SHP-08 eight shapes");
    if (p.shapes.size() != 8) return;
    ASSERT(p.shapes[0].role == ShapeRole::Body, "SHP-08 class fill → role from the sidecar");
    ASSERT(p.shapes[1].role == ShapeRole::Ink && (p.shapes[1].flags & SHAPE_FILL_EVENODD), "SHP-08 class fill-rule, hex case-insensitive");
    ASSERT(p.shapes[2].role == ShapeRole::Accent, "SHP-08 comma-separated selectors");
    ASSERT(p.shapes[3].role == ShapeRole::Body, "SHP-08 a class beats a presentation attribute");
    ASSERT(p.shapes[4].role == ShapeRole::Ink, "SHP-08 style=\"\" beats a class");
    ASSERT(p.shapes[5].role == ShapeRole::Accent, "SHP-08 fill inherits from a group");
    ASSERT(p.shapes[6].role == ShapeRole::None, "SHP-08 fill none → role none");
    ASSERT(p.shapes[7].role == ShapeRole::Body && (p.shapes[7].flags & SHAPE_FILL_EVENODD), "SHP-08 fill-rule inherits");
    for (const auto& f : p.shapes)
        ASSERT(f.dither == SHAPE_DITHER_SOLID && f.strokeWidth == 0 && f.clip == ShapeClip::None, "SHP-08 #332 paint fields at defaults");
    const auto r = bundleOne(kitSvg("<rect fill=\"#123456\" width=\"10\" height=\"10\"/>"));
    ASSERT(r.ok() && hasIssue(r, "unmapped-colour", ImportIssueSeverity::Lint), "SHP-08 a colour without a role is a lint");
}


// The pull-apart Kit: top behind the Body, a mirrored eye pair with two poses,
// a mouth with an implicit member, anchor-only cheeks, _l/_r arms.
static const char* kPullApartKit =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 80 80\">"
    "<g id=\"top\"><g id=\"antenna\"><circle id=\"anchor\" cx=\"40\" cy=\"20\" r=\"0.5\"/>"
    "<rect x=\"39\" y=\"10\" width=\"2\" height=\"10\"/></g></g>"
    "<g id=\"body\"><rect x=\"10\" y=\"20\" width=\"60\" height=\"50\"/></g>"
    "<g id=\"eyes\">"
    "<g id=\"open\"><g id=\"eye\"><rect x=\"24\" y=\"30\" width=\"2\" height=\"4\"/></g>"
    "<g id=\"eye1\" data-name=\"eye\"><path d=\"M56 30 L54 30 L54 34 L56 34 Z\"/></g></g>"
    "<g id=\"closed\"><g id=\"eye\"><rect x=\"23\" y=\"32\" width=\"4\" height=\"1\"/></g>"
    "<g id=\"eye1\" data-name=\"eye\"><rect x=\"53\" y=\"32\" width=\"4\" height=\"1\"/></g></g>"
    "</g>"
    "<g id=\"mouth\"><g id=\"neutral\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"0.5\"/>"
    "<rect x=\"36\" y=\"44\" width=\"8\" height=\"2\"/></g>"
    "<g id=\"happy\"><path d=\"M36 44 L44 44 L40 48 Z\"/></g></g>"
    "<g id=\"cheeks\"><circle id=\"anchor\" cx=\"25\" cy=\"40\" r=\"1\"/>"
    "<circle id=\"anchor1\" data-name=\"anchor\" cx=\"55\" cy=\"40\" r=\"1\"/></g>"
    "<g id=\"arms\"><g id=\"arm_l\"><circle id=\"anchor\" cx=\"15\" cy=\"45\" r=\"0.5\"/>"
    "<rect x=\"9\" y=\"44\" width=\"6\" height=\"2\"/></g>"
    "<g id=\"arm_r\"><circle id=\"anchor\" cx=\"65\" cy=\"45\" r=\"0.5\"/>"
    "<rect x=\"65\" y=\"44\" width=\"8\" height=\"2\"/></g></g>"
    "</svg>";

static bool hasPoint(const std::vector<ShapePoint>& v, ShapePoint p) {
    for (const auto& x : v)
        if (x.x == p.x && x.y == p.y) return true;
    return false;
}

static void test09_pullApart() {
    const auto r = bundleOne(kPullApartKit);
    ASSERT(r.ok(), "SHP-09 pull-apart Kit bundles");
    if (!r.ok()) {
        dumpIssues(r);
        return;
    }
    ShapePiece body, eyes, mouth, top, arms, none;
    ASSERT(firstPiece(r, ShapeSlot::Body, body) && firstPiece(r, ShapeSlot::Eyes, eyes) &&
           firstPiece(r, ShapeSlot::Mouth, mouth) && firstPiece(r, ShapeSlot::Top, top) &&
           firstPiece(r, ShapeSlot::Arms, arms), "SHP-09 one Body and four Features");
    ASSERT(!firstPiece(r, ShapeSlot::Cheeks, none), "SHP-09 anchor-only cheeks make no Feature");

    // Sockets, by slot then member, at the pose-0 anchors.
    struct Want { ShapeSlot slot; SocketSide side; double x, y; };
    const Want want[] = {
        {ShapeSlot::Eyes, SocketSide::Left, -15, -8},  {ShapeSlot::Eyes, SocketSide::Right, 15, -8},
        {ShapeSlot::Cheeks, SocketSide::Left, -15, 0}, {ShapeSlot::Cheeks, SocketSide::Right, 15, 0},
        {ShapeSlot::Mouth, SocketSide::Centre, 0, 5},  {ShapeSlot::Top, SocketSide::Centre, 0, -20},
        {ShapeSlot::Arms, SocketSide::Left, -25, 5},   {ShapeSlot::Arms, SocketSide::Right, 25, 5},
    };
    bool sockOk = body.sockets.size() == 8;
    for (size_t i = 0; sockOk && i < 8; ++i) {
        const ShapeSocket& so = body.sockets[i];
        const ShapePoint at = q(want[i].x, want[i].y);
        sockOk = so.slot == want[i].slot && so.side == want[i].side && so.at.x == at.x && so.at.y == at.y;
        if (!sockOk) fprintf(stderr, "    socket %zu: slot %d side %d at (%.2f,%.2f)\n", i, int(so.slot), int(so.side), so.at.x / 256.0, so.at.y / 256.0);
    }
    ASSERT(sockOk, "SHP-09 Body sockets: slot, side, position (anchor-only slots included)");

    // Eyes: two drawings of one eye → one Mirrored member; the left one is stored flipped.
    ASSERT(eyes.members.size() == 1 && eyes.members[0].side == MemberSide::Mirrored, "SHP-09 mirrored eye pair → one member");
    ASSERT(eyes.poses == std::vector<uint8_t>({0, 1}), "SHP-09 eyes poses open + closed");
    ASSERT(eyes.binding == ShapeBinding::Rigid && eyes.order == ShapeOrder::Front, "SHP-09 eyes: rigid, in front");
    if (eyes.members.size() == 1 && eyes.poses.size() == 2) {
        const ShapeSpan open = eyes.span(0, 0), closed = eyes.span(1, 0);
        ASSERT(open.shapeCount == 1 && closed.shapeCount == 1, "SHP-09 one Shape per pose");
        const auto o = ringPoints(eyes, open.firstShape), c = ringPoints(eyes, closed.firstShape);
        ASSERT(o.size() == 4 && hasPoint(o, q(-1, -2)) && hasPoint(o, q(1, 2)), "SHP-09 open eye relative to its bbox-centre anchor");
        ASSERT(c.size() == 4 && hasPoint(c, q(-2, 0)) && hasPoint(c, q(2, 1)),
               "SHP-09 closed eye relative to the open pose's anchor");
        ASSERT(eyes.members[0].home.x == q(-15, 0).x && eyes.members[0].home.y == q(0, -8).y, "SHP-09 member home = its anchor in the Kit");
    }

    // A member drawn left of centre is stored right-facing: flip a lopsided one.
    const auto rf = bundleOne(kitSvg("<rect x=\"10\" y=\"10\" width=\"60\" height=\"60\"/>",
        "<g id=\"eyes\"><g id=\"eye\"><circle id=\"anchor\" cx=\"20\" cy=\"30\" r=\"0.5\"/><rect x=\"20\" y=\"29\" width=\"5\" height=\"2\"/></g></g>"
        "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"1\"/></g>"));
    ShapePiece leftEye;
    ASSERT(firstPiece(rf, ShapeSlot::Eyes, leftEye) && leftEye.members.size() == 1, "SHP-09 lone left eye bundles");
    if (leftEye.members.size() == 1) {
        const auto pts = ringPoints(leftEye, 0);
        bool allLeft = !pts.empty();
        for (const auto& pt : pts) allLeft = allLeft && pt.x <= 0;
        ASSERT(allLeft && hasPoint(pts, q(-5, -1)), "SHP-09 a Mirrored member drawn on the left is stored flipped");
        ShapePiece b;
        firstPiece(rf, ShapeSlot::Body, b);
        ASSERT(b.sockets.size() == 2 && b.sockets[0].side == SocketSide::Left, "SHP-09 its socket stays on the left");
    }

    // Mouth: implicit member, anchor object, pose fallback anchor.
    ASSERT(mouth.members.size() == 1 && mouth.poses == std::vector<uint8_t>({0, 1}), "SHP-09 mouth: one member, neutral + happy");
    ASSERT(mouth.binding == ShapeBinding::Field, "SHP-09 mouth: field");
    if (mouth.poses.size() == 2) {
        ASSERT(samePoints(ringPoints(mouth, mouth.span(0, 0).firstShape), {q(-4, -1), q(4, -1), q(4, 1), q(-4, 1)}),
               "SHP-09 neutral relative to the anchor object, which is not drawn");
        ASSERT(samePoints(ringPoints(mouth, mouth.span(1, 0).firstShape), {q(-4, -1), q(4, -1), q(0, 3)}),
               "SHP-09 happy relative to neutral's anchor");
    }

    // Top: before the Body → behind; sway defaults.
    ASSERT(top.order == ShapeOrder::Behind, "SHP-09 a slot layer before body is drawn behind");
    ASSERT(top.binding == ShapeBinding::Sway && top.swayHz == 9830 && top.swayZeta == 1106, "SHP-09 top: sway 2.4 Hz ζ 0.27 (Q4.12)");

    // Arms: _l / _r keep their sides, nothing flipped.
    ASSERT(arms.members.size() == 2 && arms.members[0].side == MemberSide::Left && arms.members[1].side == MemberSide::Right,
           "SHP-09 _l / _r members");
    if (arms.members.size() == 2) {
        ASSERT(samePoints(ringPoints(arms, arms.span(0, 0).firstShape), {q(-6, -1), q(0, -1), q(0, 1), q(-6, 1)}),
               "SHP-09 the left arm is not flipped");
        ASSERT(samePoints(ringPoints(arms, arms.span(0, 1).firstShape), {q(0, -1), q(8, -1), q(8, 1), q(0, 1)}),
               "SHP-09 the right arm");
    }
    // The Body stays in Kit coordinates.
    ASSERT(samePoints(ringPoints(body, 0), {q(-30, -20), q(30, -20), q(30, 30), q(-30, 30)}), "SHP-09 Body relative to the Kit origin");
}

static void test10_sidecar() {
    const std::string sc = "{\"version\":1,\"features\":{\"top\":{\"binding\":\"field\"},"
                           "\"arms\":{\"sway\":{\"hz\":3,\"zeta\":0.5}},\"eyes\":{\"binding\":\"sway\"}}}";
    const auto r = bundleOne(kPullApartKit, sc);
    ShapePiece top, arms, eyes;
    ASSERT(r.ok() && firstPiece(r, ShapeSlot::Top, top) && firstPiece(r, ShapeSlot::Arms, arms) &&
           firstPiece(r, ShapeSlot::Eyes, eyes), "SHP-10 bundles with a sidecar");
    ASSERT(top.binding == ShapeBinding::Field && top.swayHz == 0 && top.swayZeta == 0, "SHP-10 binding override; no sway when not sway");
    ASSERT(arms.binding == ShapeBinding::Sway && arms.swayHz == 3 * 4096 && arms.swayZeta == 2048, "SHP-10 sway tuning override");
    ASSERT(eyes.binding == ShapeBinding::Sway && eyes.swayHz == 9830 && eyes.swayZeta == 1106,
           "SHP-10 eyes as sway take the default tuning");
    ASSERT(hasError(bundleOne(kPullApartKit, "{\"features\":{\"top\":{\"binding\":\"wobbly\"}}}"), "bad-sidecar"), "SHP-10 unknown binding");
    ASSERT(hasError(bundleOne(kPullApartKit, "{\"roles\":{\"#12345\":\"body\"}}"), "bad-sidecar"), "SHP-10 bad role key");
    ASSERT(hasError(bundleOne(kPullApartKit, "{\"roles\":{\"#123456\":\"skin\"}}"), "bad-sidecar"), "SHP-10 unknown role");
    ASSERT(hasError(bundleOne(kPullApartKit, "{\"features\":{\"top\":{\"sway\":{\"hz\":99,\"zeta\":0}}}}"), "bad-sidecar"), "SHP-10 sway out of range");
    ASSERT(hasError(bundleOne(kPullApartKit, "{\"roles\":"), "bad-sidecar"), "SHP-10 unparsable sidecar");
}

static std::vector<ShapePackEntry> indexOf(const ShapeBundleResult& r) {
    ShapePackMemory mem(r.pack.data(), r.pack.size());
    ShapePackIndex index;
    if (!index.open(mem)) return {};
    return index.pieces();
}

static uint16_t idOf(const ShapeBundleResult& r, ShapeSlot slot, const char* label) {
    for (const auto& e : indexOf(r))
        if (e.slot == slot && e.label == label) return e.id;
    return 0;
}

static void test11_ledger() {
    const std::string a = kPullApartKit;
    const std::string b = kitSvg("<rect x=\"20\" y=\"20\" width=\"40\" height=\"40\"/>",
                                 "<g id=\"eyes\"><rect x=\"38\" y=\"30\" width=\"4\" height=\"4\"/></g>"
                                 "<g id=\"mouth\"><rect x=\"36\" y=\"45\" width=\"8\" height=\"2\"/></g>");
    const ShapeSource sa{"kits/alpha.svg", bytesOf(a), {}}, sb{"kits/beta.svg", bytesOf(b), {}};
    const auto r1 = bundleShapes({sb, sa}, "");
    ASSERT(r1.ok(), "SHP-11 two sources bundle");
    ASSERT(idOf(r1, ShapeSlot::Body, "alpha") == 1 && idOf(r1, ShapeSlot::Body, "beta") == 2 &&
           idOf(r1, ShapeSlot::Eyes, "alpha") == 1 && idOf(r1, ShapeSlot::Eyes, "beta") == 2,
           "SHP-11 ids from 1 per slot, in source order, whatever the argument order");
    const auto r2 = bundleShapes({sa, sb}, r1.ledger);
    ASSERT(r2.ok() && r2.ledger == r1.ledger && r2.pack == r1.pack, "SHP-11 a second run changes nothing (same bytes)");

    // alpha's eyes vanish: retired; a new eyes piece never takes id 1 again.
    std::string a2 = a;
    const size_t e0 = a2.find("<g id=\"eyes\">"), e1 = a2.find("<g id=\"mouth\">");
    a2.erase(e0, e1 - e0);
    a2.insert(a2.find("<g id=\"mouth\">"), "<g id=\"eyes\"><circle id=\"anchor\" cx=\"40\" cy=\"30\" r=\"1\"/></g>");
    const auto r3 = bundleShapes({{"kits/alpha.svg", bytesOf(a2), {}}, sb}, r2.ledger);
    ASSERT(r3.ok() && idOf(r3, ShapeSlot::Eyes, "alpha") == 0 && idOf(r3, ShapeSlot::Eyes, "beta") == 2,
           "SHP-11 a vanished piece leaves the pack; the others keep their ids");
    ASSERT(r3.ledger.find("\"retired\": [\n    { \"source\": \"kits/alpha.svg\", \"slot\": \"eyes\", \"layer\": \"eyes\", \"id\": 1 }") !=
           std::string::npos, "SHP-11 its id is retired in the ledger");
    const auto r4 = bundleShapes({sa, sb}, r3.ledger);
    ASSERT(r4.ok() && idOf(r4, ShapeSlot::Eyes, "alpha") == 3, "SHP-11 the piece coming back is new: next id, never the retired one");
    ASSERT(idOf(r4, ShapeSlot::Body, "alpha") == 1, "SHP-11 untouched pieces keep their ids");

    // A hand-edited ledger with a gap: next skips ids still taken.
    const std::string hand = "{\"version\":1,\"next\":{\"eyes\":1},\"pieces\":[],"
                             "\"retired\":[{\"source\":\"x.svg\",\"slot\":\"eyes\",\"layer\":\"eyes\",\"id\":1}]}";
    const auto r5 = bundleShapes({sa}, hand);
    ASSERT(r5.ok() && idOf(r5, ShapeSlot::Eyes, "alpha") == 2, "SHP-11 next never hands out a retired id");
    ASSERT(r5.ledger.find("\"next\": { \"body\": 2, \"eyes\": 3,") != std::string::npos, "SHP-11 next advances past every id");

    const auto bad = bundleShapes({sa}, "{\"version\":1,\"pieces\":[{\"source\":\"a\",\"slot\":\"eyes\",\"layer\":\"eyes\",\"id\":1},"
                                        "{\"source\":\"b\",\"slot\":\"eyes\",\"layer\":\"eyes\",\"id\":1}]}");
    ASSERT(hasError(bad, "bad-ledger") && bad.pack.empty(), "SHP-11 an id used twice is a bad ledger");
    ASSERT(hasError(bundleShapes({sa}, "{\"version\":2}"), "bad-ledger"), "SHP-11 unknown ledger version");
    ASSERT(hasError(bundleShapes({sa}, "[1,2"), "bad-ledger"), "SHP-11 unparsable ledger");
    ASSERT(hasError(bundleShapes({sa}, "{\"version\":1,\"next\":{\"tail\":1}}"), "bad-ledger"), "SHP-11 unknown slot in next");
}

static void test12_names() {
    const auto r = bundleOne(
        "<svg viewBox=\"0 0 80 80\">"
        "<g id=\"body_1_\"><rect x=\"10\" y=\"10\" width=\"60\" height=\"60\"/></g>"
        "<g id=\"Layer_x5F_eyes\" data-name=\"eyes\"><g id=\"eye_x5F_l\"><rect x=\"20\" y=\"30\" width=\"2\" height=\"2\"/></g>"
        "<g id=\"eye_x5F_r\"><rect x=\"58\" y=\"30\" width=\"2\" height=\"2\"/></g></g>"
        "<g id=\"mouth\"><circle id=\"anchor_2_\" cx=\"40\" cy=\"45\" r=\"1\"/></g>"
        "</svg>");
    ShapePiece eyes;
    ASSERT(r.ok() && firstPiece(r, ShapeSlot::Eyes, eyes), "SHP-12 Kit with Illustrator names bundles");
    if (!r.ok()) dumpIssues(r);
    ASSERT(eyes.members.size() == 2 && eyes.members[0].side == MemberSide::Left && eyes.members[1].side == MemberSide::Right,
           "SHP-12 _x5F_ decodes to '_' (eye_l / eye_r), data-name beats id, body_1_ is body");
}

static void test13_hardErrors() {
    const std::string ledger = "{\"version\":1,\"next\":{\"body\":5}}";
    auto refused = [&](const std::string& svg, const char* code, const char* what) {
        const auto r = bundleOne(svg, "", ledger);
        const bool ok = hasError(r, code) && r.pack.empty() && r.ledger == ledger && !r.ok();
        if (!ok) {
            fprintf(stderr, "  %s: expected error %s\n", what, code);
            dumpIssues(r);
        }
        ASSERT(ok, what);
    };
    std::string wrong = kitSvg("<rect width=\"10\" height=\"10\"/>");
    wrong.replace(wrong.find("0 0 80 80"), 9, "0 0 64 64");
    refused(wrong, "wrong-artboard", "SHP-13 a 64x64 artboard");
    std::string noVb = kitSvg("<rect width=\"10\" height=\"10\"/>");
    noVb.replace(noVb.find("viewBox=\"0 0 80 80\""), 19, "width=\"80\"");
    refused(noVb, "wrong-artboard", "SHP-13 no viewBox");
    refused("<svg viewBox=\"0 0 80 80\"><g id=\"body\">", "unparsable", "SHP-13 truncated XML");
    refused("not xml at all", "unparsable", "SHP-13 not XML");
    refused("<html viewBox=\"0 0 80 80\"/>", "unparsable", "SHP-13 root is not <svg>");
    refused(kitSvg("<path d=\"M10 10 L20\"/>"), "unparsable", "SHP-13 bad path data");
    refused(kitSvg("<path d=\"M10 10 X20 20\"/>"), "unparsable", "SHP-13 unknown path command");
    refused(kitSvg("<rect transform=\"spin(3)\" width=\"10\" height=\"10\"/>"), "unparsable", "SHP-13 bad transform");
    for (const char* el : {"<text>hi</text>", "<image href=\"x.png\"/>", "<defs><linearGradient id=\"g\"/></defs>",
                           "<defs><radialGradient id=\"g\"/></defs>", "<defs><pattern id=\"p\"/></defs>",
                           "<defs><mask id=\"m\"/></defs>", "<defs><filter id=\"f\"/></defs>", "<symbol id=\"s\"/>",
                           "<use href=\"#a\"/>", "<foreignObject/>"}) {
        refused(kitSvg(std::string("<rect width=\"10\" height=\"10\"/>") + el), "forbidden-element",
                (std::string("SHP-13 forbidden ") + el).c_str());
    }
    {
        const auto r = bundleOne(kitSvg("<rect width=\"10\" height=\"10\"/>", "<g id=\"eyes\"><circle id=\"anchor\" cx=\"40\" cy=\"30\" r=\"1\"/></g>"
                                        "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"1\"/></g>",
                                        "<defs><clipPath id=\"clippath\"><use href=\"#x\"/><rect width=\"5\" height=\"5\"/></clipPath></defs>"));
        ASSERT(r.ok(), "SHP-13 <use> inside a clip path is allowed (legacy clip form)");
    }
    refused(kitSvg("<rect width=\"10\" height=\"10\"/>", "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"1\"/></g>"),
            "missing-socket", "SHP-13 a Body without an eyes socket");
    refused(kitSvg("<rect width=\"10\" height=\"10\"/>", "<g id=\"eyes\"><circle id=\"anchor\" cx=\"40\" cy=\"30\" r=\"1\"/></g>"),
            "missing-socket", "SHP-13 a Body without a mouth socket");
    refused(kitSvg("<rect width=\"10\" height=\"10\"/>",
                   "<g id=\"eyes\"><circle id=\"anchor\" cx=\"40\" cy=\"30\" r=\"1\"/></g>"
                   "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"1\"/></g>"
                   "<g id=\"eyes1\" data-name=\"eyes\"><rect width=\"2\" height=\"2\"/></g>"),
            "duplicate-piece", "SHP-13 a slot layer twice in one source");
    refused(kitSvg("<rect width=\"10\" height=\"10\"/>",
                   "<g id=\"eyes\"><g id=\"closed\"><rect width=\"2\" height=\"2\"/></g></g>"
                   "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"1\"/></g>"),
            "missing-pose", "SHP-13 eyes without the required open pose");
    {
        ShapeBundleOptions o;
        o.limits.maxSourceBytes = 64;
        const auto r = bundleShapes({{"kit.svg", bytesOf(kitSvg("<rect width=\"10\" height=\"10\"/>")), {}}}, ledger, o);
        ASSERT(hasError(r, "too-large") && r.pack.empty() && r.ledger == ledger, "SHP-13 source size cap");
    }
    {
        std::string deep;
        for (int i = 0; i < 100; ++i) deep += "<g>";
        for (int i = 0; i < 100; ++i) deep += "</g>";
        refused(kitSvg(deep), "too-large", "SHP-13 nesting depth cap");
    }
    {
        ShapeBundleOptions o;
        o.limits.maxPiecePoints = 8;
        const auto r = bundleShapes({{"kit.svg", bytesOf(kitSvg("<circle cx=\"40\" cy=\"40\" r=\"30\"/>")), {}}}, ledger, o);
        ASSERT(hasError(r, "too-large") && r.pack.empty(), "SHP-13 piece point cap");
    }
    // One bad source spoils the bundle: no pack, ledger unchanged.
    const auto r = bundleShapes({{"good.svg", bytesOf(kPullApartKit), {}}, {"bad.svg", bytesOf("<svg"), {}}}, ledger);
    ASSERT(r.pack.empty() && r.ledger == ledger && hasError(r, "unparsable"), "SHP-13 one bad source → no pack at all");
    bool tagged = false;
    for (const auto& i : r.issues) tagged = tagged || (i.source == "bad.svg" && i.severity == ImportIssueSeverity::Error);
    ASSERT(tagged, "SHP-13 the error names its source");
    // Lints carry the layer path.
    const auto lint = bundleOne(kitSvg("<rect width=\"10\" height=\"10\"/>", "<g id=\"eyes\"><circle id=\"anchor\" cx=\"40\" cy=\"30\" r=\"1\"/></g>"
                                       "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"1\"/></g><g id=\"tail\"/>"));
    bool path = false;
    for (const auto& i : lint.issues) path = path || (i.code == "unknown-layer" && i.layer == "tail");
    ASSERT(lint.ok() && path, "SHP-13 an unknown top-level layer is a lint with its layer path");
}

// ---------------------------------------------------------------------------
// SHP-16  Preview: an error drops its piece or its source, never the bundle
// ---------------------------------------------------------------------------

static ShapeBundleResult previewBundle(const std::vector<ShapeSource>& sources, const std::string& ledger = "") {
    ShapeBundleOptions o;
    o.preview = true;
    return bundleShapes(sources, ledger, o);
}

static void test16_preview() {
    const std::string anchors =
        "<g id=\"eyes\"><circle id=\"anchor\" cx=\"40\" cy=\"30\" r=\"1\"/></g>"
        "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"1\"/></g>";
    // A Feature without its required pose: that Feature goes, its sibling stays; the Body, which
    // lost its eyes socket with it, goes too (missing-socket).
    {
        const std::string svg = kitSvg("<rect width=\"10\" height=\"10\"/>",
            "<g id=\"eyes\"><g id=\"closed\"><rect width=\"2\" height=\"2\"/></g></g>"
            "<g id=\"mouth\"><rect x=\"38\" y=\"44\" width=\"4\" height=\"2\"/></g>");
        const auto r = previewBundle({{"kit.svg", bytesOf(svg), {}}});
        ShapePiece body, mouth, eyes;
        ASSERT(hasError(r, "missing-pose") && !r.ok(), "SHP-16 the error is still reported");
        ASSERT(!r.pack.empty() && firstPiece(r, ShapeSlot::Mouth, mouth) && !firstPiece(r, ShapeSlot::Eyes, eyes),
               "SHP-16 the broken Feature is dropped, its sibling packs");
        ASSERT(hasError(r, "missing-socket") && !firstPiece(r, ShapeSlot::Body, body), "SHP-16 a Body without eyes is dropped");
        if (r.pack.empty()) dumpIssues(r);
    }
    // A file-level error drops the whole source; another source still packs.
    {
        const auto r = previewBundle({{"good.svg", bytesOf(kPullApartKit), {}}, {"bad.svg", bytesOf("<svg"), {}}});
        ASSERT(hasError(r, "unparsable") && !r.pack.empty(), "SHP-16 an unparsable source doesn't stop the others");
        bool badPiece = false;
        for (const auto& e : indexOf(r)) badPiece = badPiece || e.label == "bad";
        ASSERT(!badPiece && !indexOf(r).empty(), "SHP-16 nothing of the bad source packs");
    }
    {
        const auto r = previewBundle({{"good.svg", bytesOf(kPullApartKit), bytesOf("{not json")}});
        ASSERT(hasError(r, "bad-sidecar") && r.pack.empty(), "SHP-16 a bad sidecar drops its source (here: all of it)");
        ASSERT(hasError(r, "unparsable"), "SHP-16 with nothing left, the empty bundle is still an error");
    }
    // A bad ledger: every piece is provisional (ids from 1), the bundle goes on.
    {
        const auto r = previewBundle({{"kit.svg", bytesOf(kitSvg("<rect width=\"10\" height=\"10\"/>", anchors)), {}}}, "{oops");
        ShapePiece body;
        ASSERT(hasError(r, "bad-ledger") && pieceOf(r, ShapeSlot::Body, 1, body), "SHP-16 a bad ledger is read as empty");
    }
    // The same source twice: the second is dropped.
    {
        const Bytes svg = bytesOf(kitSvg("<rect width=\"10\" height=\"10\"/>", anchors));
        const auto r = previewBundle({{"kit.svg", svg, {}}, {"kit.svg", svg, {}}});
        ASSERT(hasError(r, "duplicate-piece") && indexOf(r).size() == 1, "SHP-16 a source listed twice packs once");
    }
    // Without preview the same input still fails the whole bundle (the CLI's contract).
    {
        const auto r = bundleShapes({{"good.svg", bytesOf(kPullApartKit), {}}, {"bad.svg", bytesOf("<svg"), {}}}, "");
        ASSERT(r.pack.empty(), "SHP-16 the CLI's bundle still fails on any error");
    }
}

// ---------------------------------------------------------------------------
// SHP-17  Source colours
// ---------------------------------------------------------------------------

static void test17_colours() {
    const std::string svg = kitSvg(
        "<rect fill=\"#B18CF9\" width=\"10\" height=\"10\"/><rect fill=\"#1c1c1c\" width=\"5\" height=\"5\"/>",
        "<g id=\"eyes\"><circle fill=\"#1c1c1c\" cx=\"34\" cy=\"30\" r=\"2\"/></g>"
        "<g id=\"mouth\"><rect fill=\"#c8e86b\" x=\"38\" y=\"44\" width=\"4\" height=\"2\"/>"
        "<rect fill=\"none\" x=\"38\" y=\"44\" width=\"4\" height=\"2\"/></g>");
    const auto r = bundleShapes({{"kit.svg", bytesOf(svg), bytesOf("{\"roles\":{\"#b18cf9\":\"body\"}}")},
                                 {"other.svg", bytesOf(kitSvg("<rect width=\"10\" height=\"10\"/>")), {}}}, "");
    ASSERT(r.colours.size() == 2, "SHP-17 one colour list per source");
    if (r.colours.size() == 2) {
        ASSERT(r.colours[0].source == "kit.svg" && r.colours[1].source == "other.svg", "SHP-17 in source order");
        // Features are read before the Body: eyes, mouth, then body.
        ASSERT((r.colours[0].rgb == std::vector<uint32_t>{0x1c1c1c, 0xc8e86b, 0xb18cf9}),
               "SHP-17 distinct fills in first-use order, mapped or not; no fill isn't a colour");
        ASSERT((r.colours[1].rgb == std::vector<uint32_t>{0x000000}), "SHP-17 the default fill is black");
    }
}


static Bytes readFileBytes(const std::string& path);

static std::string kitPath(const char* name) { return std::string(ENJIN2_SOURCE_DIR) + "/tools/testdata/shapes/" + name; }

static void test14_standInKit() {
    const Bytes svg = readFileBytes(kitPath("star.svg"));
    const Bytes sidecar = readFileBytes(kitPath("star.svg.shape.json"));
    ASSERT(!svg.empty() && !sidecar.empty(), "SHP-14 stand-in Kit and sidecar present");
    const auto r = bundleShapes({{"star.svg", svg, sidecar}}, "");
    ASSERT(r.ok(), "SHP-14 the stand-in Kit bundles");
    for (const auto& i : r.issues)
        ASSERT(i.severity == ImportIssueSeverity::Lint && i.code == "unsupported-paint",
               "SHP-14 its only lint is the clip on the happy mouth (Tomodachi #332)");
    const auto idx = indexOf(r);
    ASSERT(idx.size() == 7, "SHP-14 Body + eyes, cheeks, mouth, top, arms, legs");
    for (const auto& e : idx) ASSERT(e.id == 1 && e.label == "star", "SHP-14 every piece is id 1, labelled star");
    ShapePiece body, eyes, mouth, top, arms, legs, cheeks;
    if (!(firstPiece(r, ShapeSlot::Body, body) && firstPiece(r, ShapeSlot::Eyes, eyes) && firstPiece(r, ShapeSlot::Mouth, mouth) &&
          firstPiece(r, ShapeSlot::Top, top) && firstPiece(r, ShapeSlot::Arms, arms) && firstPiece(r, ShapeSlot::Legs, legs) &&
          firstPiece(r, ShapeSlot::Cheeks, cheeks))) {
        ASSERT(false, "SHP-14 every piece reads");
        return;
    }
    ASSERT(body.sockets.size() == 10, "SHP-14 ten sockets: 2 eyes, 2 cheeks, mouth, top, 2 arms, 2 legs");
    ASSERT(eyes.poses == std::vector<uint8_t>({0, 1}) && eyes.members.size() == 1, "SHP-14 eyes: open + closed (hidden layer read), one drawing");
    ASSERT(mouth.poses == std::vector<uint8_t>({0, 1}) && mouth.members.size() == 1, "SHP-14 mouth: neutral + happy");
    ASSERT(mouth.span(1, 0).shapeCount == 2, "SHP-14 happy mouth: hole + tongue");
    ASSERT(top.order == ShapeOrder::Behind && top.binding == ShapeBinding::Sway && top.swayHz == 2 * 4096 &&
           top.swayZeta == static_cast<uint16_t>(std::lround(0.2 * 4096)), "SHP-14 top: behind, sidecar sway tuning");
    ASSERT(legs.order == ShapeOrder::Behind && legs.members.size() == 1, "SHP-14 legs: behind, one mirrored foot");
    ASSERT(arms.members.size() == 2 && arms.members[0].side == MemberSide::Left, "SHP-14 arms: arm_l / arm_r");
    ASSERT(cheeks.members.size() == 1 && cheeks.shapes.size() == 1 && cheeks.shapes[0].role == ShapeRole::Accent, "SHP-14 cheeks: accent");
    ASSERT(body.shapes.size() == 1 && body.shapes[0].role == ShapeRole::Body && body.points.size() == 16, "SHP-14 the star Body: 16 vertices");
}

/// Every result is either a pack that reads back in full, or an error with no pack.
static bool consistent(const ShapeBundleResult& r) {
    bool err = false;
    for (const auto& i : r.issues) err = err || i.severity == ImportIssueSeverity::Error;
    if (err) return r.pack.empty();
    if (r.pack.empty()) return false;
    ShapePackMemory mem(r.pack.data(), r.pack.size());
    ShapePackIndex index;
    if (!index.open(mem)) return false;
    for (const auto& e : index.pieces()) {
        ShapePiece p;
        if (!index.readPiece(mem, e, p)) return false;
    }
    return true;
}

static void test15_corpus() {
    const Bytes svg = readFileBytes(kitPath("star.svg"));
    const Bytes sidecar = readFileBytes(kitPath("star.svg.shape.json"));
    if (svg.empty()) return;
    const auto good = bundleShapes({{"star.svg", svg, sidecar}}, "");
    const std::string ledger = good.ledger;
    int bad = 0;
    // Every truncation of the SVG, the sidecar and the ledger.
    for (size_t n = 0; n < svg.size(); ++n)
        bad += !consistent(bundleShapes({{"star.svg", Bytes(svg.begin(), svg.begin() + n), sidecar}}, ledger));
    for (size_t n = 0; n < sidecar.size(); ++n)
        bad += !consistent(bundleShapes({{"star.svg", svg, Bytes(sidecar.begin(), sidecar.begin() + n)}}, ledger));
    for (size_t n = 0; n < ledger.size(); ++n)
        bad += !consistent(bundleShapes({{"star.svg", svg, sidecar}}, ledger.substr(0, n)));
    // Seeded mutations: overwrite bytes with characters that matter to the parsers.
    static const char kSpecial[] = "<>/=\"'&;#-.eE0123456789MmLlCcAaZz ,()[]{}:\\\x00\xff";
    uint32_t seed = 330;
    auto rnd = [&seed]() { return seed = seed * 1664525u + 1013904223u; };
    for (int i = 0; i < 3000; ++i) {
        Bytes m = svg;
        const int edits = 1 + static_cast<int>(rnd() % 4);
        for (int k = 0; k < edits; ++k) m[rnd() % m.size()] = static_cast<uint8_t>(kSpecial[rnd() % (sizeof kSpecial - 1)]);
        bad += !consistent(bundleShapes({{"star.svg", m, sidecar}}, ledger));
    }
    for (int i = 0; i < 500; ++i) {
        std::string m = ledger;
        m[rnd() % m.size()] = kSpecial[rnd() % (sizeof kSpecial - 1)];
        Bytes sc = sidecar;
        sc[rnd() % sc.size()] = static_cast<uint8_t>(kSpecial[rnd() % (sizeof kSpecial - 1)]);
        bad += !consistent(bundleShapes({{"star.svg", svg, sc}}, m));
    }
    ASSERT(bad == 0, "SHP-15 every truncated / mutated input gives a readable pack or an error, never a crash");
    if (bad) fprintf(stderr, "    %d inconsistent results\n", bad);

    // The pack reader on every truncation and on mutations of a real pack.
    const Bytes& pack = good.pack;
    int opened = 0;
    for (size_t n = 0; n < pack.size(); ++n) {
        ShapePackMemory mem(pack.data(), n);
        ShapePackIndex index;
        opened += index.open(mem);
    }
    ASSERT(opened == 0, "SHP-15 no truncated pack opens");
    for (int i = 0; i < 4000; ++i) {
        Bytes m = pack;
        m[rnd() % m.size()] = static_cast<uint8_t>(rnd());
        ShapePackMemory mem(m.data(), m.size());
        ShapePackIndex index;
        if (!index.open(mem)) continue;
        for (const auto& e : index.pieces()) {
            ShapePiece p;
            index.readPiece(mem, e, p);
        }
    }
    ASSERT(true, "SHP-15 mutated packs read without a crash");
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

static Bytes readFileBytes(const std::string& path) { return readFile(path); }

int main(int argc, char** argv) {
    if (argc >= 3 && std::strcmp(argv[1], "--verify-pack") == 0) return verifyPack(argc, argv);

    test01_packRoundTrip();
    test02_minimalKit();
    test03_lineCommands();
    test04_curves();
    test05_arcs();
    test06_elements();
    test07_transforms();
    test08_paint();
    test09_pullApart();
    test10_sidecar();
    test11_ledger();
    test12_names();
    test13_hardErrors();
    test14_standInKit();
    test15_corpus();
    test16_preview();
    test17_colours();

    printf("shape_bundle_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
