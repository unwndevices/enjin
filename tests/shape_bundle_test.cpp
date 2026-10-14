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
 *   SHP-18  Dither (Tomodachi #332): opacity / fill-opacity → quarters, off-level lint, group opacity error.
 *   SHP-19  Stroke: inner width on closed shapes, a line on open paths, rounding lint, inheritance.
 *   SHP-20  Holes: even-odd and opposite-winding compound paths keep their rings and rule.
 *   SHP-21  Clip: own space (clippath-N and legacy <use>), live Body, near-Body lint, mirrored dedup.
 *   SHP-22  Feature sheets (Tomodachi #333): slot → Feature → pose → members, any artboard, by anchors.
 *   SHP-23  Pose fallback: a missing optional pose shows the required one, with a lint.
 *   SHP-24  Lint pixel-tie: a filled edge on a pixel centre (Kit coordinates; a sheet's from the anchor).
 *   SHP-25  Lint sway-without-anchor: a sway member swinging about its bbox centre.
 *   SHP-26  Lint too-many-colours: more than 4 fill + stroke colours in one source.
 *   SHP-27  Lint retired-id: a ledger piece no source has any more (not one an error dropped).
 *   SHP-28  The stand-in Feature sheet (tools/testdata/shapes/sheet.svg) joins the pool with ledger ids.
 *   SHP-29  Non-finite geometry (an overflowing transform) is a hard error, not a cast.
 *   SHP-30  Numbers parse the same under a decimal-comma C locale (skipped when none is installed).
 *
 * `shape_bundle_test --verify-pack <a.njn> [<b.njn>]` checks a CLI output
 * (index readable, every record decodes; with two files, identical indices).
 */

#include <enjin2/graphics/shape_pack.hpp>
#include <enjin2/import/shape_bundle.hpp>

#include <clocale>
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
    // An entry over the record cap is refused before any buffer is sized for it.
    ShapePackEntry huge = index.pieces()[0];
    huge.size = 0xFFFFFFFFu;
    err = nullptr;
    ASSERT(!index.readPiece(mem, huge, got, &err) && err && std::strcmp(err, "shape pack: record too large") == 0,
           "SHP-01 an entry over the record cap is refused as too large");

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
    ASSERT(eyes.poses == std::vector<uint8_t>({0, 1, 2, 3}), "SHP-09 eyes poses open + closed (+ fallbacks, SHP-23)");
    ASSERT(eyes.binding == ShapeBinding::Rigid && eyes.order == ShapeOrder::Front, "SHP-09 eyes: rigid, in front");
    if (eyes.members.size() == 1 && eyes.poses.size() >= 2) {
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
    ASSERT(mouth.members.size() == 1 && mouth.poses == std::vector<uint8_t>({0, 1, 2, 3, 4}),
           "SHP-09 mouth: one member, neutral + happy (+ fallbacks, SHP-23)");
    ASSERT(mouth.binding == ShapeBinding::Field, "SHP-09 mouth: field");
    if (mouth.poses.size() >= 2) {
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

// ---------------------------------------------------------------------------
// SHP-18  Dither: opacity → quarters of coverage (Tomodachi #332)
// ---------------------------------------------------------------------------

static void test18_dither() {
    ShapePiece p;
    bodyRing("<rect opacity=\".5\" width=\"10\" height=\"10\"/>"
             "<rect fill-opacity=\"0.75\" width=\"10\" height=\"10\"/>"
             "<rect class=\"q\" width=\"10\" height=\"10\"/>"
             "<rect opacity=\"1\" width=\"10\" height=\"10\"/>"
             "<rect opacity=\".5\" fill-opacity=\".5\" width=\"10\" height=\"10\"/>"
             "<g fill-opacity=\".5\"><rect width=\"10\" height=\"10\"/></g>"
             "<rect style=\"opacity:0.6\" width=\"10\" height=\"10\"/>",
             "SHP-18", 0, &p, "<defs><style>.q{opacity:.25}</style></defs>");
    ASSERT(p.shapes.size() == 7, "SHP-18 seven shapes");
    if (p.shapes.size() != 7) return;
    ASSERT(p.shapes[0].dither == 2, "SHP-18 opacity 50 % → 2 quarters");
    ASSERT(p.shapes[1].dither == 3, "SHP-18 fill-opacity 75 % → 3");
    ASSERT(p.shapes[2].dither == 1, "SHP-18 class opacity 25 % → 1");
    ASSERT(p.shapes[3].dither == SHAPE_DITHER_SOLID, "SHP-18 opacity 1 → solid");
    ASSERT(p.shapes[4].dither == 1, "SHP-18 opacity × fill-opacity: 50 % × 50 % → 25 %");
    ASSERT(p.shapes[5].dither == 2, "SHP-18 fill-opacity inherits from a group");
    ASSERT(p.shapes[6].dither == 2, "SHP-18 off-level 60 % snaps to 50 %");

    const auto off = bundleOne(kitSvg("<rect opacity=\".6\" width=\"10\" height=\"10\"/>"));
    ASSERT(off.ok() && hasIssue(off, "off-level-opacity", ImportIssueSeverity::Lint), "SHP-18 off-level opacity is a lint");
    const auto on = bundleOne(kitSvg("<rect opacity=\".75\" width=\"10\" height=\"10\"/>"));
    ASSERT(on.ok() && !hasIssue(on, "off-level-opacity", ImportIssueSeverity::Lint), "SHP-18 an on-level opacity is not");
    const auto group = bundleOne(kitSvg("<g opacity=\".5\"><rect width=\"10\" height=\"10\"/></g>"));
    ASSERT(hasError(group, "group-opacity") && group.pack.empty(), "SHP-18 opacity on a group is an error");
    for (const auto& r : {off, on})
        ASSERT(!hasIssue(r, "unsupported-paint", ImportIssueSeverity::Lint), "SHP-18 opacity is converted, not linted");
}

// ---------------------------------------------------------------------------
// SHP-19  Stroke: inner width on closed shapes, a line on open paths
// ---------------------------------------------------------------------------

static void test19_stroke() {
    const std::string sidecar = "{\"version\":1,\"roles\":{\"#b18cf9\":\"body\",\"#1c1c1c\":\"ink\",\"#b5dc6c\":\"accent\"}}";
    ShapePiece p;
    bodyRing("<rect fill=\"#b18cf9\" stroke=\"#1c1c1c\" stroke-width=\"2\" width=\"10\" height=\"10\"/>"
             "<rect stroke=\"#b5dc6c\" width=\"10\" height=\"10\"/>"
             "<rect stroke=\"#1c1c1c\" stroke-width=\"1.5\" width=\"10\" height=\"10\"/>"
             "<g transform=\"scale(2)\"><rect stroke=\"#1c1c1c\" width=\"5\" height=\"5\"/></g>"
             "<g stroke=\"#1c1c1c\" stroke-width=\"3\"><rect width=\"10\" height=\"10\"/></g>"
             "<path fill=\"none\" stroke=\"#1c1c1c\" d=\"M10 10 L20 10 L20 14\"/>"
             "<rect stroke=\"none\" width=\"10\" height=\"10\"/>"
             "<rect fill=\"none\" stroke=\"#b5dc6c\" width=\"10\" height=\"10\"/>",
             "SHP-19", 0, &p, "", sidecar);
    ASSERT(p.shapes.size() == 8, "SHP-19 eight shapes");
    if (p.shapes.size() != 8) return;
    ASSERT(p.shapes[0].role == ShapeRole::Body && p.shapes[0].strokeWidth == 2 && p.shapes[0].strokeRole == ShapeRole::Ink,
           "SHP-19 stroke-width is the inner width; stroke colour → its role");
    ASSERT(p.shapes[1].strokeWidth == 1 && p.shapes[1].strokeRole == ShapeRole::Accent, "SHP-19 default stroke-width 1");
    ASSERT(p.shapes[2].strokeWidth == 2, "SHP-19 a non-integer width rounds to whole px");
    ASSERT(p.shapes[3].strokeWidth == 2, "SHP-19 the width scales with the transform");
    ASSERT(p.shapes[4].strokeWidth == 3 && p.shapes[4].strokeRole == ShapeRole::Ink, "SHP-19 stroke and width inherit");
    ASSERT(p.shapes[5].role == ShapeRole::None && p.shapes[5].strokeWidth == 1 && p.shapes[5].strokeRole == ShapeRole::Ink &&
               p.shapes[5].ringCount == 1 && !(p.rings[p.shapes[5].firstRing].flags & SHAPE_RING_CLOSED),
           "SHP-19 an open path: no fill, an open ring, a 1 px line");
    ASSERT(p.shapes[6].strokeWidth == 0 && p.shapes[6].strokeRole == ShapeRole::None, "SHP-19 stroke none");
    ASSERT(p.shapes[7].role == ShapeRole::None && p.shapes[7].strokeRole == ShapeRole::Accent, "SHP-19 stroke without fill");

    const auto frac = bundleOne(kitSvg("<rect stroke=\"#000\" stroke-width=\"1.5\" width=\"10\" height=\"10\"/>"));
    ASSERT(frac.ok() && hasIssue(frac, "fractional-stroke", ImportIssueSeverity::Lint), "SHP-19 non-integer width is a lint");
    const auto whole = bundleOne(kitSvg("<rect stroke=\"#000\" stroke-width=\"2\" width=\"10\" height=\"10\"/>"));
    ASSERT(whole.ok() && !hasIssue(whole, "fractional-stroke", ImportIssueSeverity::Lint) &&
               !hasIssue(whole, "unsupported-paint", ImportIssueSeverity::Lint),
           "SHP-19 a whole width is not, and strokes are converted");
    ShapePiece thin;
    const auto hair = bundleOne(kitSvg("<rect stroke=\"#000\" stroke-width=\".25\" width=\"10\" height=\"10\"/>"));
    ASSERT(hair.ok() && pieceOf(hair, ShapeSlot::Body, 1, thin) && thin.shapes[0].strokeWidth == 0 &&
               thin.shapes[0].strokeRole == ShapeRole::None && hasIssue(hair, "fractional-stroke", ImportIssueSeverity::Lint),
           "SHP-19 a width that rounds to 0 strokes nothing (lint)");
    ASSERT((bundleShapes({{"kit.svg", bytesOf(kitSvg("<rect fill=\"#b18cf9\" stroke=\"#123456\" width=\"10\" height=\"10\"/>")), {}}}, "")
                .colours[0].rgb == std::vector<uint32_t>{0xb18cf9, 0x123456}),
           "SHP-19 stroke colours are source colours too (the Studio asks their role)");
}

// ---------------------------------------------------------------------------
// SHP-20  Holes: compound paths reach the fill rule
// ---------------------------------------------------------------------------

static double signedArea(const std::vector<ShapePoint>& v) {
    double a = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        const ShapePoint p = v[i], q2 = v[(i + 1) % v.size()];
        a += double(p.x) * q2.y - double(q2.x) * p.y;
    }
    return a / 2;
}

static void test20_holes() {
    ShapePiece p;
    bodyRing("<path fill-rule=\"evenodd\" d=\"M0 0H20V20H0Z M5 5H15V15H5Z\"/>"
             "<path d=\"M0 0H20V20H0Z M5 5V15H15V5Z\"/>",
             "SHP-20", 0, &p);
    ASSERT(p.shapes.size() == 2, "SHP-20 two compound paths, one Shape each");
    if (p.shapes.size() != 2) return;
    ASSERT(p.shapes[0].ringCount == 2 && (p.shapes[0].flags & SHAPE_FILL_EVENODD), "SHP-20 even-odd: two rings, even-odd rule");
    ASSERT(p.shapes[1].ringCount == 2 && !(p.shapes[1].flags & SHAPE_FILL_EVENODD), "SHP-20 non-zero: two rings");
    const double outer = signedArea(ringPoints(p, 1, 0)), inner = signedArea(ringPoints(p, 1, 1));
    ASSERT(outer * inner < 0, "SHP-20 non-zero: the hole keeps its opposite winding");
}

// ---------------------------------------------------------------------------
// SHP-21  Clip: own space, the live Body, the legacy <use> form
// ---------------------------------------------------------------------------

/// The shapes no span of @p p draws.
static std::vector<size_t> unspanned(const ShapePiece& p) {
    std::vector<bool> drawn(p.shapes.size());
    for (const auto& s : p.spans)
        for (uint16_t i = 0; i < s.shapeCount; ++i) drawn[s.firstShape + i] = true;
    std::vector<size_t> out;
    for (size_t i = 0; i < drawn.size(); ++i)
        if (!drawn[i]) out.push_back(i);
    return out;
}

static void test21_clip() {
    const std::string body = "<path d=\"M10 20H70V70H10Z\"/>";
    const std::string eyes = "<g id=\"eyes\"><circle id=\"anchor\" cx=\"40\" cy=\"30\" r=\"1\"/></g>";
    // Teeth clipped in the mouth's own space (Illustrator 29.x: <clipPath id="clippath-N"> holding a copy).
    {
        const std::string svg = kitSvg(body, eyes +
            "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"0.5\"/>"
            "<rect x=\"36\" y=\"44\" width=\"8\" height=\"4\"/>"
            "<g clip-path=\"url(#clippath-1)\"><rect x=\"34\" y=\"46\" width=\"12\" height=\"6\"/>"
            "<rect x=\"30\" y=\"40\" width=\"2\" height=\"2\"/></g></g>",
            "<defs><clipPath id=\"clippath-1\"><rect x=\"36\" y=\"44\" width=\"8\" height=\"4\"/></clipPath></defs>");
        const auto r = bundleOne(svg);
        ShapePiece m;
        ASSERT(r.ok() && firstPiece(r, ShapeSlot::Mouth, m), "SHP-21 own clip bundles");
        if (!r.ok()) dumpIssues(r);
        ASSERT(!hasIssue(r, "unsupported-paint", ImportIssueSeverity::Lint), "SHP-21 the clip is converted, not linted");
        const auto extra = unspanned(m);
        ASSERT(m.span(0, 0).shapeCount == 3 && extra.size() == 1, "SHP-21 the clip is one more Shape, outside every span");
        if (m.span(0, 0).shapeCount == 3 && extra.size() == 1) {
            const ShapeFill& lip = m.shapes[m.span(0, 0).firstShape];
            const ShapeFill& teeth = m.shapes[m.span(0, 0).firstShape + 1];
            const ShapeFill& stray = m.shapes[m.span(0, 0).firstShape + 2];
            ASSERT(lip.clip == ShapeClip::None, "SHP-21 outside the clip group: unclipped");
            ASSERT(teeth.clip == ShapeClip::Own && teeth.clipShape == extra[0] && stray.clip == ShapeClip::Own &&
                       stray.clipShape == extra[0],
                   "SHP-21 both shapes of the clip group share the one clip Shape");
            ASSERT(m.shapes[extra[0]].role == ShapeRole::None, "SHP-21 the clip Shape paints nothing");
            ASSERT(samePoints(ringPoints(m, extra[0]), {q(-4, -1), q(4, -1), q(4, 3), q(-4, 3)}),
                   "SHP-21 the clip is relative to the member's anchor");
        }
    }
    // The legacy form: <clipPath><use xlink:href="#path"/></clipPath>, the path in <defs>.
    {
        const std::string svg = kitSvg(body, eyes +
            "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"0.5\"/>"
            "<g style=\"clip-path:url(#SVGID_2_)\"><rect x=\"34\" y=\"46\" width=\"12\" height=\"6\"/></g></g>",
            "<defs><rect id=\"SVGID_1_\" x=\"36\" y=\"44\" width=\"8\" height=\"4\"/></defs>"
            "<clipPath id=\"SVGID_2_\"><use xlink:href=\"#SVGID_1_\" style=\"overflow:visible\"/></clipPath>");
        const auto r = bundleOne(svg);
        ShapePiece m;
        ASSERT(r.ok() && firstPiece(r, ShapeSlot::Mouth, m), "SHP-21 legacy <use> clip bundles");
        if (!r.ok()) dumpIssues(r);
        const auto extra = unspanned(m);
        ASSERT(extra.size() == 1 && m.shapes[0].clip == ShapeClip::Own &&
                   samePoints(ringPoints(m, extra[0]), {q(-4, -1), q(4, -1), q(4, 3), q(-4, 3)}),
               "SHP-21 legacy <use> clip: the referenced geometry");
    }
    // A clip that matches the Body binds to the live Body; one that almost does is a lint.
    {
        auto cap = [&](const std::string& clipD) {
            return bundleOne(kitSvg(body, eyes +
                "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"1\"/></g>"
                "<g id=\"top\"><circle id=\"anchor\" cx=\"40\" cy=\"20\" r=\"0.5\"/>"
                "<g class=\"c\"><rect x=\"0\" y=\"10\" width=\"80\" height=\"20\"/></g></g>",
                "<defs><style>.c{clip-path:url(#clippath)}</style>"
                "<clipPath id=\"clippath\"><path d=\"" + clipD + "\"/></clipPath></defs>"));
        };
        const auto live = cap("M10 20H70V70H10Z");
        ShapePiece top;
        ASSERT(live.ok() && firstPiece(live, ShapeSlot::Top, top), "SHP-21 body clip bundles");
        ASSERT(top.shapes.size() == 1 && top.shapes[0].clip == ShapeClip::LiveBody, "SHP-21 matches the Body → the live Body, no clip Shape");
        ASSERT(!hasIssue(live, "near-body-clip", ImportIssueSeverity::Lint), "SHP-21 an exact match is not a lint");
        const auto near = cap("M10 21H70V70H10Z");
        ASSERT(near.ok() && firstPiece(near, ShapeSlot::Top, top), "SHP-21 near-body clip bundles");
        ASSERT(hasIssue(near, "near-body-clip", ImportIssueSeverity::Lint), "SHP-21 a clip that almost matches the Body is a lint");
        ASSERT(top.shapes.size() == 2 && top.shapes[0].clip == ShapeClip::Own, "SHP-21 ... and clips in its own space");
        const auto far = cap("M30 10H50V30H30Z");
        ASSERT(far.ok() && firstPiece(far, ShapeSlot::Top, top) && top.shapes[0].clip == ShapeClip::Own &&
                   !hasIssue(far, "near-body-clip", ImportIssueSeverity::Lint),
               "SHP-21 an unrelated clip: own space, no lint");
        const auto missing = bundleOne(kitSvg(body, eyes +
            "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"1\"/>"
            "<rect clip-path=\"url(#nope)\" x=\"38\" y=\"44\" width=\"4\" height=\"2\"/></g>"));
        ShapePiece mm;
        ASSERT(missing.ok() && hasIssue(missing, "missing-clip", ImportIssueSeverity::Lint) &&
                   firstPiece(missing, ShapeSlot::Mouth, mm) && mm.shapes[0].clip == ShapeClip::None,
               "SHP-21 a clip-path to nothing: a lint, unclipped");
    }
    // A mirrored pair with the same clipped drawing keeps one member, and its clip.
    {
        const std::string svg = kitSvg(body, eyes +
            "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"1\"/></g>"
            "<g id=\"cheeks\">"
            "<g id=\"c1\" data-name=\"cheek\"><g clip-path=\"url(#k1)\"><rect x=\"20\" y=\"38\" width=\"6\" height=\"4\"/></g></g>"
            "<g id=\"c2\" data-name=\"cheek\"><g clip-path=\"url(#k2)\"><rect x=\"54\" y=\"38\" width=\"6\" height=\"4\"/></g></g></g>",
            "<defs><clipPath id=\"k1\"><rect x=\"22\" y=\"38\" width=\"4\" height=\"4\"/></clipPath>"
            "<clipPath id=\"k2\"><rect x=\"54\" y=\"38\" width=\"4\" height=\"4\"/></clipPath></defs>");
        const auto r = bundleOne(svg);
        ShapePiece c;
        ASSERT(r.ok() && firstPiece(r, ShapeSlot::Cheeks, c), "SHP-21 clipped cheeks bundle");
        if (!r.ok()) dumpIssues(r);
        ASSERT(c.members.size() == 1 && c.shapes.size() == 2 && c.shapes[c.span(0, 0).firstShape].clip == ShapeClip::Own &&
                   unspanned(c).size() == 1 && c.shapes[c.span(0, 0).firstShape].clipShape == unspanned(c)[0],
               "SHP-21 one mirrored member kept, its clip Shape with it");
    }
}

// ---------------------------------------------------------------------------
// SHP-22  Feature sheets (Tomodachi #333)
// ---------------------------------------------------------------------------

/// A Feature sheet: no Body, any artboard size.
static std::string sheetSvg(const std::string& layers, const char* viewBox = "0 0 200 100") {
    return std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                       "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" viewBox=\"") +
           viewBox + "\">" + layers + "</svg>";
}

static const char* const kSheetLayers =
    "<g id=\"mouth\">"
    "<g id=\"grin\">"
    "<g id=\"neutral\"><circle id=\"anchor\" cx=\"20\" cy=\"20\" r=\".5\"/><rect x=\"16\" y=\"19\" width=\"8\" height=\"2\"/></g>"
    "<g id=\"happy\"><rect x=\"16\" y=\"18\" width=\"8\" height=\"4\"/></g>"
    "</g>"
    "<g id=\"pout\"><circle id=\"anchor\" cx=\"60\" cy=\"20\" r=\".5\"/><rect x=\"58\" y=\"19\" width=\"4\" height=\"2\"/></g>"
    "</g>"
    "<g id=\"eyes\"><g id=\"dots\">"
    "<g id=\"eye\"><rect x=\"100\" y=\"10\" width=\"2\" height=\"2\"/></g>"
    "<g id=\"eye1\" data-name=\"eye\"><rect x=\"110\" y=\"10\" width=\"2\" height=\"2\"/></g>"
    "</g></g>";

static ShapeBundleResult bundleSheet(const std::string& layers, const std::string& sidecar = "") {
    return bundleShapes({{"sheet.svg", bytesOf(sheetSvg(layers)), bytesOf(sidecar)}}, "");
}

static void test22_featureSheet() {
    const auto r = bundleSheet(kSheetLayers);
    ASSERT(r.ok(), "SHP-22 a Feature sheet (200x100, no Body) bundles");
    if (!r.ok()) dumpIssues(r);
    const auto idx = indexOf(r);
    ASSERT(idx.size() == 3, "SHP-22 one piece per Feature: two mouths, one pair of eyes");
    ShapePiece grin, pout, dots;
    if (!(pieceOf(r, ShapeSlot::Mouth, 1, grin) && pieceOf(r, ShapeSlot::Mouth, 2, pout) && pieceOf(r, ShapeSlot::Eyes, 1, dots))) {
        ASSERT(false, "SHP-22 ids per slot, Features in name order (grin 1, pout 2)");
        return;
    }
    ASSERT(idOf(r, ShapeSlot::Mouth, "grin") == 1 && idOf(r, ShapeSlot::Mouth, "pout") == 2 && idOf(r, ShapeSlot::Eyes, "dots") == 1,
           "SHP-22 a sheet piece is labelled by its Feature name");
    ASSERT(r.ledger.find("{ \"source\": \"sheet.svg\", \"slot\": \"mouth\", \"layer\": \"mouth/grin\", \"id\": 1 }") != std::string::npos &&
               r.ledger.find("\"layer\": \"mouth/pout\", \"id\": 2") != std::string::npos &&
               r.ledger.find("\"layer\": \"eyes/dots\", \"id\": 1") != std::string::npos,
           "SHP-22 ledger key: slot layer / Feature name");
    ASSERT(samePoints(ringPoints(grin, grin.span(0, 0).firstShape), {q(-4, -1), q(4, -1), q(4, 1), q(-4, 1)}),
           "SHP-22 rings relative to the member's anchor");
    ASSERT(grin.poses.size() >= 2 && grin.poses[0] == 0 && grin.poses[1] == 1, "SHP-22 a sheet Feature's pose layers");
    ASSERT(samePoints(ringPoints(pout, pout.span(0, 0).firstShape), {q(-2, -1), q(2, -1), q(2, 1), q(-2, 1)}),
           "SHP-22 a Feature without pose layers is its required pose");
    ASSERT(grin.sockets.empty() && pout.sockets.empty() && dots.sockets.empty(), "SHP-22 a sheet makes no sockets");
    ASSERT(grin.members.size() == 1 && grin.members[0].home.x == 0 && grin.members[0].home.y == 0,
           "SHP-22 a single member's home is its Feature's centre");
    ASSERT(dots.members.size() == 1 && dots.members[0].side == MemberSide::Mirrored, "SHP-22 two same eyes: one mirrored member");
    ASSERT(dots.members.size() == 1 && dots.members[0].home.x == q(-5, 0).x && dots.members[0].home.y == 0,
           "SHP-22 home is relative to the Feature's centre");
    ASSERT(grin.order == ShapeOrder::Front && dots.binding == ShapeBinding::Rigid, "SHP-22 sheet Features: in front, slot Bindings");

    // Mixed with a Kit: the sheet's pieces join the pool after the Kit's.
    const auto mixed = bundleShapes({{"sheet.svg", bytesOf(sheetSvg(kSheetLayers)), {}}, {"kit.svg", bytesOf(kPullApartKit), {}}}, "");
    ASSERT(mixed.ok() && idOf(mixed, ShapeSlot::Mouth, "kit") == 1 && idOf(mixed, ShapeSlot::Mouth, "grin") == 2 &&
               idOf(mixed, ShapeSlot::Mouth, "pout") == 3,
           "SHP-22 a Kit and a sheet share each slot's ids");
    if (!mixed.ok()) dumpIssues(mixed);

    // The sidecar keys a sheet Feature by its ledger layer; `order` puts it behind.
    const auto tuned = bundleSheet(kSheetLayers, "{\"version\":1,\"features\":{\"mouth/pout\":{\"binding\":\"rigid\",\"order\":\"behind\"}}}");
    ShapePiece pout2;
    ASSERT(tuned.ok() && pieceOf(tuned, ShapeSlot::Mouth, 2, pout2) && pout2.binding == ShapeBinding::Rigid &&
               pout2.order == ShapeOrder::Behind,
           "SHP-22 sidecar Binding and order for a sheet Feature");
    ASSERT(hasError(bundleSheet(kSheetLayers, "{\"version\":1,\"features\":{\"mouth/pout\":{\"order\":\"under\"}}}"), "bad-sidecar"),
           "SHP-22 order is behind or front");

    // The sheet's own structure.
    const auto dup = bundleSheet("<g id=\"mouth\"><g id=\"grin\"><rect width=\"2\" height=\"2\"/></g>"
                                 "<g id=\"grin1\" data-name=\"grin\"><rect width=\"4\" height=\"2\"/></g></g>");
    ASSERT(hasError(dup, "duplicate-piece") && dup.pack.empty(), "SHP-22 two Features of one name in a slot: duplicate-piece");
    const auto stray = bundleSheet("<g id=\"mouth\"><rect width=\"2\" height=\"2\"/><g id=\"grin\"><rect width=\"4\" height=\"2\"/></g></g>");
    ASSERT(stray.ok() && hasIssue(stray, "unknown-layer", ImportIssueSeverity::Lint) && indexOf(stray).size() == 1,
           "SHP-22 a shape outside any Feature is ignored with a lint");
    const auto missing = bundleSheet("<g id=\"mouth\"><g id=\"grin\"><g id=\"happy\"><rect width=\"4\" height=\"2\"/></g></g></g>");
    ASSERT(hasError(missing, "missing-pose"), "SHP-22 a sheet Feature still needs its required pose");
    for (const auto& i : missing.issues)
        if (i.code == "missing-pose") ASSERT(i.layer == "mouth/grin", "SHP-22 issues name the Feature's layer path");
    std::string wide = kitSvg("<rect width=\"10\" height=\"10\"/>");
    wide.replace(wide.find("0 0 80 80"), 9, "0 0 90 80");
    ASSERT(hasError(bundleOne(wide), "wrong-artboard"), "SHP-22 a Kit is still 80x80");
    ASSERT(hasError(bundleShapes({{"sheet.svg", bytesOf("<svg xmlns=\"http://www.w3.org/2000/svg\"><g id=\"mouth\"/></svg>"), {}}}, ""),
                    "wrong-artboard"),
           "SHP-22 a sheet still needs a viewBox");
}

// ---------------------------------------------------------------------------
// SHP-23  Pose fallback (Tomodachi #333)
// ---------------------------------------------------------------------------

static size_t issueCount(const ShapeBundleResult& r, const char* code) {
    size_t n = 0;
    for (const auto& i : r.issues) n += i.code == code;
    return n;
}

static const ImportIssue* issueOf(const ShapeBundleResult& r, const char* code) {
    for (const auto& i : r.issues)
        if (i.code == code) return &i;
    return nullptr;
}

static void test23_poseFallback() {
    // The pull-apart Kit's mouth draws neutral + happy; its eyes open + closed.
    const auto r = bundleOne(kPullApartKit);
    ShapePiece mouth, eyes, top;
    if (!(r.ok() && firstPiece(r, ShapeSlot::Mouth, mouth) && firstPiece(r, ShapeSlot::Eyes, eyes) &&
          firstPiece(r, ShapeSlot::Top, top))) {
        ASSERT(false, "SHP-23 the pull-apart Kit bundles");
        return;
    }
    ASSERT(mouth.poses == std::vector<uint8_t>({0, 1, 2, 3, 4}), "SHP-23 the mouth holds every pose of its slot");
    ASSERT(eyes.poses == std::vector<uint8_t>({0, 1, 2, 3}), "SHP-23 the eyes hold every pose of theirs");
    ASSERT(top.poses == std::vector<uint8_t>({0}), "SHP-23 a one-pose slot is unchanged");
    bool same = mouth.spans.size() == 5;
    for (size_t pose = 2; same && pose < 5; ++pose)
        same = mouth.span(pose, 0).firstShape == mouth.span(0, 0).firstShape && mouth.span(pose, 0).shapeCount == mouth.span(0, 0).shapeCount;
    ASSERT(same, "SHP-23 a missing optional pose draws the required pose's Shapes (no copy)");
    ASSERT(mouth.span(1, 0).firstShape != mouth.span(0, 0).firstShape, "SHP-23 a drawn pose keeps its own drawing");
    ASSERT(issueCount(r, "missing-optional-pose") == 2, "SHP-23 one lint per Feature missing optional poses");
    const ImportIssue* lint = nullptr;
    for (const auto& i : r.issues)
        if (i.code == "missing-optional-pose" && i.layer == "mouth") lint = &i;
    ASSERT(lint && lint->severity == ImportIssueSeverity::Lint && lint->message.find("sad, open, surprised") != std::string::npos &&
               lint->message.find("neutral") != std::string::npos,
           "SHP-23 the lint names the missing poses and the one they show");

    // Every pose drawn: no lint.
    const auto full = bundleOne(kitSvg("<rect x=\"10\" y=\"10\" width=\"60\" height=\"60\"/>",
        "<g id=\"eyes\"><circle id=\"anchor\" cx=\"40\" cy=\"30\" r=\"1\"/></g>"
        "<g id=\"mouth\"><g id=\"neutral\"><rect x=\"38\" y=\"44\" width=\"4\" height=\"2\"/></g>"
        "<g id=\"happy\"><rect x=\"38\" y=\"44\" width=\"4\" height=\"3\"/></g><g id=\"sad\"><rect x=\"38\" y=\"45\" width=\"4\" height=\"2\"/></g>"
        "<g id=\"open\"><rect x=\"38\" y=\"43\" width=\"4\" height=\"4\"/></g><g id=\"surprised\"><rect x=\"39\" y=\"43\" width=\"2\" height=\"4\"/></g></g>"));
    ASSERT(full.ok() && issueCount(full, "missing-optional-pose") == 0, "SHP-23 a Feature with every pose drawn: no lint");
    if (!full.ok() || issueCount(full, "missing-optional-pose")) dumpIssues(full);
}

// ---------------------------------------------------------------------------
// SHP-24  Lint: an edge that ties on a pixel centre (Tomodachi #333)
// ---------------------------------------------------------------------------

static void test24_pixelTie() {
    const auto tie = bundleOne(kitSvg("<circle cx=\"40\" cy=\"40\" r=\"4.5\"/>"));
    ASSERT(tie.ok() && issueCount(tie, "pixel-tie") == 1, "SHP-24 rx 4.5 at an integer centre: one pixel-tie lint per Shape");
    const ImportIssue* i = issueOf(tie, "pixel-tie");
    ASSERT(i && i->severity == ImportIssueSeverity::Lint && i->layer == "body", "SHP-24 a lint, on the shape's layer");
    ASSERT(issueCount(bundleOne(kitSvg("<circle cx=\"40\" cy=\"40\" r=\"4\"/>")), "pixel-tie") == 0, "SHP-24 r 4: no tie");
    ASSERT(issueCount(bundleOne(kitSvg("<circle cx=\"40.5\" cy=\"40.5\" r=\"4.5\"/>")), "pixel-tie") == 0,
           "SHP-24 r 4.5 at a pixel centre: edges on pixel lines, no tie");
    ASSERT(issueCount(bundleOne(kitSvg("<rect x=\"10.5\" y=\"10\" width=\"20\" height=\"20\"/>")), "pixel-tie") == 1,
           "SHP-24 a straight edge on a pixel centre ties");
    ASSERT(issueCount(bundleOne(kitSvg("<rect x=\"10\" y=\"10\" width=\"20\" height=\"20\"/>"
                                       "<path d=\"M10.5 50.5 L30.5 50.5\" fill=\"none\" stroke=\"#000\"/>")), "pixel-tie") == 0,
           "SHP-24 an open line is not a filled edge");
    // A Kit Feature is measured where its Kit draws it, not from a bbox-centre anchor on a half pixel.
    ASSERT(issueCount(bundleOne(kitSvg("<rect x=\"10\" y=\"10\" width=\"60\" height=\"60\"/>",
                                       "<g id=\"eyes\"><rect x=\"34\" y=\"25\" width=\"2\" height=\"3\"/></g>"
                                       "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"1\"/></g>")), "pixel-tie") == 0,
           "SHP-24 a Kit Feature on whole pixels doesn't tie");
    // A sheet Feature is measured from its member's anchor.
    ASSERT(issueCount(bundleSheet("<g id=\"mouth\"><g id=\"m\"><circle id=\"anchor\" cx=\"20\" cy=\"20\" r=\".5\"/>"
                                  "<rect x=\"16.5\" y=\"19\" width=\"7\" height=\"2\"/></g></g>"), "pixel-tie") == 1,
           "SHP-24 sheet: an edge half a pixel off its anchor's grid ties");
    ASSERT(issueCount(bundleSheet("<g id=\"mouth\"><g id=\"m\"><circle id=\"anchor\" cx=\"20.5\" cy=\"20\" r=\".5\"/>"
                                  "<rect x=\"16.5\" y=\"19\" width=\"8\" height=\"2\"/></g></g>"), "pixel-tie") == 0,
           "SHP-24 sheet: on its anchor's grid, no tie");
}

// ---------------------------------------------------------------------------
// SHP-25  Lint: a sway member without an anchor (Tomodachi #333)
// ---------------------------------------------------------------------------

static void test25_swayAnchor() {
    const std::string body = "<rect x=\"10\" y=\"20\" width=\"60\" height=\"50\"/>";
    const std::string face = "<g id=\"eyes\"><circle id=\"anchor\" cx=\"40\" cy=\"30\" r=\"1\"/></g>"
                             "<g id=\"mouth\"><circle id=\"anchor\" cx=\"40\" cy=\"45\" r=\"1\"/></g>";
    // Top sways by default: an antenna without an anchor swings about its bbox centre.
    const auto loose = bundleOne(kitSvg(body, face + "<g id=\"top\"><g id=\"antenna\"><rect x=\"39\" y=\"8\" width=\"2\" height=\"12\"/></g></g>"));
    ASSERT(loose.ok() && issueCount(loose, "sway-without-anchor") == 1, "SHP-25 a sway member without an anchor: one lint");
    const ImportIssue* i = issueOf(loose, "sway-without-anchor");
    ASSERT(i && i->severity == ImportIssueSeverity::Lint && i->layer == "top/antenna", "SHP-25 on the member's layer");
    const auto anchored = bundleOne(kitSvg(body, face + "<g id=\"top\"><g id=\"antenna\"><circle id=\"anchor\" cx=\"40\" cy=\"20\" r=\".5\"/>"
                                                        "<rect x=\"39\" y=\"8\" width=\"2\" height=\"12\"/></g></g>"));
    ASSERT(anchored.ok() && issueCount(anchored, "sway-without-anchor") == 0, "SHP-25 with an anchor: no lint");
    const auto field = bundleOne(kitSvg(body, face + "<g id=\"top\"><g id=\"cap\"><rect x=\"30\" y=\"14\" width=\"20\" height=\"8\"/></g></g>"),
                                 "{\"version\":1,\"features\":{\"top\":{\"binding\":\"field\"}}}");
    ASSERT(field.ok() && issueCount(field, "sway-without-anchor") == 0, "SHP-25 a top bound to the field needs no anchor");
    const auto cheeks = bundleOne(kitSvg(body, face + "<g id=\"cheeks\"><rect x=\"20\" y=\"38\" width=\"4\" height=\"2\"/></g>"),
                                  "{\"version\":1,\"features\":{\"cheeks\":{\"binding\":\"sway\"}}}");
    ASSERT(cheeks.ok() && issueCount(cheeks, "sway-without-anchor") == 1, "SHP-25 sway by sidecar override counts too");
}

// ---------------------------------------------------------------------------
// SHP-26  Lint: more than 4 colours in a source (Tomodachi #333)
// ---------------------------------------------------------------------------

static void test26_colourCount() {
    auto kit = [](int colours, bool stroke) {
        std::string body;
        static const char* const hex[] = {"#111111", "#222222", "#333333", "#444444", "#555555"};
        for (int c = 0; c < colours; ++c)
            body += "<rect x=\"" + std::to_string(10 + c * 10) + "\" y=\"10\" width=\"10\" height=\"60\" fill=\"" + hex[c] + "\"/>";
        if (stroke) body += "<path d=\"M10 75 L70 75\" fill=\"none\" stroke=\"#666666\"/>";
        return kitSvg(body);
    };
    const auto four = bundleOne(kit(4, false));
    ASSERT(four.ok() && issueCount(four, "too-many-colours") == 0, "SHP-26 four colours: no lint");
    const auto five = bundleOne(kit(5, false));
    ASSERT(five.ok() && issueCount(five, "too-many-colours") == 1, "SHP-26 five colours: one lint for the source");
    const ImportIssue* i = issueOf(five, "too-many-colours");
    ASSERT(i && i->severity == ImportIssueSeverity::Lint && i->source == "kit.svg" && i->layer.empty() &&
               i->message.find('5') != std::string::npos,
           "SHP-26 a lint on the file, saying how many");
    ASSERT(issueCount(bundleOne(kit(4, true)), "too-many-colours") == 1, "SHP-26 stroke colours count");
    ASSERT(issueCount(bundleSheet(std::string("<g id=\"mouth\"><g id=\"a\"><rect width=\"2\" height=\"2\" fill=\"#111111\"/></g>"
                                              "<g id=\"b\"><rect width=\"2\" height=\"2\" fill=\"#222222\"/></g>"
                                              "<g id=\"c\"><rect width=\"2\" height=\"2\" fill=\"#333333\"/></g>"
                                              "<g id=\"d\"><rect width=\"2\" height=\"2\" fill=\"#444444\"/></g>"
                                              "<g id=\"e\"><rect width=\"2\" height=\"2\" fill=\"#555555\"/></g></g>")),
                      "too-many-colours") == 1,
           "SHP-26 a Feature sheet counts its colours too");
}

// ---------------------------------------------------------------------------
// SHP-27  Lint: an id retired since the last ledger (Tomodachi #333)
// ---------------------------------------------------------------------------

static void test27_retiredId() {
    const ShapeSource kit{"kits/alpha.svg", bytesOf(kPullApartKit), {}};
    const auto r1 = bundleShapes({kit}, "");
    ASSERT(r1.ok() && issueCount(r1, "retired-id") == 0, "SHP-27 a new ledger retires nothing");
    std::string noEyes = kPullApartKit;
    const size_t e0 = noEyes.find("<g id=\"eyes\">"), e1 = noEyes.find("<g id=\"mouth\">");
    noEyes.replace(e0, e1 - e0, "<g id=\"eyes\"><circle id=\"anchor\" cx=\"40\" cy=\"30\" r=\"1\"/></g>");
    const auto r2 = bundleShapes({{"kits/alpha.svg", bytesOf(noEyes), {}}}, r1.ledger);
    ASSERT(r2.ok() && issueCount(r2, "retired-id") == 1, "SHP-27 a vanished piece: one retired-id lint");
    const ImportIssue* i = issueOf(r2, "retired-id");
    ASSERT(i && i->severity == ImportIssueSeverity::Lint && i->source == "kits/alpha.svg" && i->layer == "eyes" &&
               i->message.find("eyes 1") != std::string::npos,
           "SHP-27 the lint names the slot + id, on the piece's source and layer");
    ASSERT(issueCount(bundleShapes({{"kits/alpha.svg", bytesOf(noEyes), {}}}, r2.ledger), "retired-id") == 0,
           "SHP-27 once retired, the next run is quiet");
    const auto gone = bundleShapes({{"other.svg", bytesOf(kitSvg("<rect x=\"10\" y=\"10\" width=\"60\" height=\"60\"/>")), {}}}, r1.ledger);
    ASSERT(gone.ok() && issueCount(gone, "retired-id") == 5, "SHP-27 a source taken out retires each of its pieces");

    // Preview: a piece dropped by an error is broken, not gone, so it isn't reported retired.
    ShapeBundleOptions preview;
    preview.preview = true;
    std::string broken = kPullApartKit;
    broken.replace(broken.find("<g id=\"open\">"), 13, "<g id=\"angry\">");
    const auto pr = bundleShapes({{"kits/alpha.svg", bytesOf(broken), {}}}, r1.ledger, preview);
    ASSERT(hasError(pr, "missing-pose") && !pr.pack.empty() && issueCount(pr, "retired-id") == 0,
           "SHP-27 preview: a piece an error drops is not reported retired");
}

static Bytes readFileBytes(const std::string& path);

/// Exactly these issue codes, in this order (dumps the issues when not).
static bool onlyIssues(const ShapeBundleResult& r, const std::vector<std::string>& codes) {
    bool same = r.issues.size() == codes.size();
    for (size_t i = 0; same && i < codes.size(); ++i) same = r.issues[i].code == codes[i];
    if (!same) dumpIssues(r);
    return same;
}

static std::string kitPath(const char* name) { return std::string(ENJIN2_SOURCE_DIR) + "/tools/testdata/shapes/" + name; }

static void test14_standInKit() {
    const Bytes svg = readFileBytes(kitPath("star.svg"));
    const Bytes sidecar = readFileBytes(kitPath("star.svg.shape.json"));
    ASSERT(!svg.empty() && !sidecar.empty(), "SHP-14 stand-in Kit and sidecar present");
    const auto r = bundleShapes({{"star.svg", svg, sidecar}}, "");
    ASSERT(r.ok(), "SHP-14 the stand-in Kit bundles");
    // Its paint is all converted (Tomodachi #332); what is left are the authoring lints (#333).
    // The tongue (ry 1.5) and the surprised mouth (r 3.5) sit on pixel centres; sad, happy and angry eyes aren't drawn.
    ASSERT(onlyIssues(r, {"pixel-tie", "pixel-tie", "missing-optional-pose", "missing-optional-pose"}),
           "SHP-14 lints: two pixel ties and the poses the Star doesn't draw");
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
    ASSERT(eyes.poses == std::vector<uint8_t>({0, 1, 2, 3}) && eyes.members.size() == 1 &&
               eyes.span(1, 0).firstShape != eyes.span(0, 0).firstShape && eyes.span(2, 0).firstShape == eyes.span(0, 0).firstShape,
           "SHP-14 eyes: open + closed (hidden layer read), one drawing; happy, angry show open");
    ASSERT(mouth.poses == std::vector<uint8_t>({0, 1, 2, 3, 4}) && mouth.members.size() == 1 &&
               mouth.span(2, 0).firstShape == mouth.span(0, 0).firstShape,
           "SHP-14 mouth: neutral, happy, open, surprised; sad shows neutral");
    if (mouth.poses.size() == 5) {
        const ShapeFill& line = mouth.shapes[mouth.span(0, 0).firstShape];
        ASSERT(mouth.span(0, 0).shapeCount == 1 && line.role == ShapeRole::None && line.strokeWidth == 1 &&
                   line.strokeRole == ShapeRole::Ink && !(mouth.rings[line.firstRing].flags & SHAPE_RING_CLOSED),
               "SHP-14 neutral mouth: a 1 px ink curve (open path)");
        const ShapeSpan happy = mouth.span(1, 0);
        ASSERT(happy.shapeCount == 2 && mouth.shapes[happy.firstShape + 1].clip == ShapeClip::Own &&
                   mouth.shapes[mouth.shapes[happy.firstShape + 1].clipShape].role == ShapeRole::None,
               "SHP-14 happy mouth: the tongue clipped inside the mouth (own space)");
        const ShapeFill& open = mouth.shapes[mouth.span(3, 0).firstShape];
        ASSERT(open.ringCount == 2 && (open.flags & SHAPE_FILL_EVENODD), "SHP-14 open mouth: a ring with a hole");
        const ShapeFill& wow = mouth.shapes[mouth.span(4, 0).firstShape];
        ASSERT(wow.role == ShapeRole::Accent && wow.strokeWidth == 1 && wow.strokeRole == ShapeRole::Ink,
               "SHP-14 surprised mouth: accent with a 1 px ink inner stroke");
    }
    ASSERT(top.order == ShapeOrder::Behind && top.binding == ShapeBinding::Sway && top.swayHz == 2 * 4096 &&
           top.swayZeta == static_cast<uint16_t>(std::lround(0.2 * 4096)), "SHP-14 top: behind, sidecar sway tuning");
    ASSERT(legs.order == ShapeOrder::Behind && legs.members.size() == 1, "SHP-14 legs: behind, one mirrored foot");
    ASSERT(arms.members.size() == 2 && arms.members[0].side == MemberSide::Left, "SHP-14 arms: arm_l / arm_r");
    ASSERT(cheeks.members.size() == 1 && cheeks.shapes.size() == 1 && cheeks.shapes[0].role == ShapeRole::Accent &&
               cheeks.shapes[0].dither == 2,
           "SHP-14 cheeks: accent, 50 % dithered");
    ASSERT(top.shapes.size() == 2 && top.shapes[0].role == ShapeRole::None && top.shapes[0].strokeWidth == 1 &&
               top.shapes[0].strokeRole == ShapeRole::Body,
           "SHP-14 antenna: a stroked stem (open path) + the bulb");

    // The Chestnut Kit: a cap clipped to its Body (legacy <use> form) binds to the live Body.
    const auto rc = bundleShapes({{"chestnut.svg", readFileBytes(kitPath("chestnut.svg")),
                                   readFileBytes(kitPath("chestnut.svg.shape.json"))}}, "");
    ShapePiece cap;
    ASSERT(rc.ok() && firstPiece(rc, ShapeSlot::Top, cap), "SHP-14 the Chestnut Kit bundles");
    ASSERT(onlyIssues(rc, {"missing-optional-pose", "missing-optional-pose"}), "SHP-14 Chestnut lints: only its undrawn poses");
    ASSERT(cap.binding == ShapeBinding::Field && cap.shapes.size() == 1 && cap.shapes[0].clip == ShapeClip::LiveBody,
           "SHP-14 Chestnut cap: field-bound, clipped to the live Body");
    ASSERT(body.shapes.size() == 1 && body.shapes[0].role == ShapeRole::Body && body.points.size() == 16, "SHP-14 the star Body: 16 vertices");
}

// ---------------------------------------------------------------------------
// SHP-28  The stand-in Feature sheet joins the pool (Tomodachi #333)
// ---------------------------------------------------------------------------

static void test28_standInSheet() {
    const std::string ledger = [] {
        const Bytes b = readFileBytes(kitPath("ledger.json"));
        return std::string(b.begin(), b.end());
    }();
    std::vector<ShapeSource> sources;
    for (const char* name : {"star.svg", "chestnut.svg", "sheet.svg"})
        sources.push_back({name, readFileBytes(kitPath(name)), readFileBytes(kitPath((std::string(name) + ".shape.json").c_str()))});
    ASSERT(!sources[2].svg.empty() && !sources[2].sidecar.empty() && !ledger.empty(), "SHP-28 stand-in sheet, sidecar and ledger present");
    const auto r = bundleShapes(sources, ledger);
    ASSERT(r.ok() && r.ledger == ledger, "SHP-28 the Kits and the sheet bundle with the committed ledger, unchanged");
    if (!r.ok()) dumpIssues(r);
    struct Want { ShapeSlot slot; uint16_t id; const char* label; };
    const Want want[] = {{ShapeSlot::Mouth, 3, "fang"}, {ShapeSlot::Mouth, 4, "grin"}, {ShapeSlot::Mouth, 5, "pout"},
                         {ShapeSlot::Top, 3, "bow"},    {ShapeSlot::Top, 4, "sprout"}, {ShapeSlot::Top, 5, "tuft"}};
    for (const Want& w : want) {
        char m[80];
        std::snprintf(m, sizeof m, "SHP-28 %s %u \"%s\" is in the pool", shapeSlotName(w.slot), w.id, w.label);
        ASSERT(idOf(r, w.slot, w.label) == w.id, m);
    }
    ShapePiece grin, fang, sprout, bow, tuft;
    if (!(pieceOf(r, ShapeSlot::Mouth, 4, grin) && pieceOf(r, ShapeSlot::Mouth, 3, fang) && pieceOf(r, ShapeSlot::Top, 4, sprout) &&
          pieceOf(r, ShapeSlot::Top, 3, bow) && pieceOf(r, ShapeSlot::Top, 5, tuft))) {
        ASSERT(false, "SHP-28 the sheet's pieces read");
        return;
    }
    bool own = grin.poses.size() == 5;
    for (size_t pose = 1; own && pose < 5; ++pose) own = grin.span(pose, 0).firstShape != grin.span(0, 0).firstShape;
    ASSERT(own, "SHP-28 grin draws every mouth pose");
    ASSERT(fang.poses.size() == 5 && fang.span(3, 0).firstShape != fang.span(0, 0).firstShape &&
               fang.span(1, 0).firstShape == fang.span(0, 0).firstShape,
           "SHP-28 fang: neutral + open, the rest fall back");
    ASSERT(sprout.order == ShapeOrder::Behind && sprout.binding == ShapeBinding::Sway && bow.order == ShapeOrder::Front &&
               bow.binding == ShapeBinding::Field && tuft.binding == ShapeBinding::Sway,
           "SHP-28 tops: a sprout swaying behind, a field-bound bow, a tuft");
    size_t sheetIssues = 0;
    for (const auto& i : r.issues)
        if (i.source == "sheet.svg") {
            ++sheetIssues;
            ASSERT(i.code == "missing-optional-pose" && (i.layer == "mouth/fang" || i.layer == "mouth/pout"),
                   "SHP-28 the sheet's only lints: fang and pout fall back to neutral");
        }
    ASSERT(sheetIssues == 2, "SHP-28 two sheet lints");
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
    // The stand-in Feature sheet (Tomodachi #333): every truncation, and mutations, beside the Star.
    const Bytes sheet = readFileBytes(kitPath("sheet.svg"));
    const Bytes sheetCar = readFileBytes(kitPath("sheet.svg.shape.json"));
    for (size_t n = 0; n < sheet.size(); ++n)
        bad += !consistent(bundleShapes({{"star.svg", svg, sidecar}, {"sheet.svg", Bytes(sheet.begin(), sheet.begin() + n), sheetCar}}, ledger));
    for (int i = 0; i < 1500 && !sheet.empty(); ++i) {
        Bytes m = sheet;
        const int edits = 1 + static_cast<int>(rnd() % 4);
        for (int k = 0; k < edits; ++k) m[rnd() % m.size()] = static_cast<uint8_t>(kSpecial[rnd() % (sizeof kSpecial - 1)]);
        bad += !consistent(bundleShapes({{"sheet.svg", m, sheetCar}}, ""));
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

// ---------------------------------------------------------------------------
// SHP-29  Non-finite geometry is a hard error, never a cast
// ---------------------------------------------------------------------------

static void test29_nonFinite() {
    // Two transforms that overflow a double: inf, and inf - inf = NaN, before Q8.8.
    for (const char* body : {"<g transform=\"scale(1e300)\"><rect transform=\"scale(1e300)\" width=\"10\" height=\"10\"/></g>",
                             "<g transform=\"scale(1e300)\"><path transform=\"matrix(1e10 0 -1e10 1 0 0)\" "
                             "d=\"M10 10 L70 10 L70 70 Z\"/></g>"}) {
        const auto r = bundleOne(kitSvg(body));
        ASSERT(!r.ok() && r.pack.empty() && hasError(r, "non-finite"), "SHP-29 a non-finite coordinate is a hard error");
        if (r.ok()) dumpIssues(r);
    }
}

// ---------------------------------------------------------------------------
// SHP-30  Number parsing ignores the C locale
// ---------------------------------------------------------------------------

static void test30_locale() {
    const Bytes svg = readFileBytes(kitPath("star.svg"));
    const Bytes sidecar = readFileBytes(kitPath("star.svg.shape.json"));
    const auto c = bundleShapes({{"star.svg", svg, sidecar}}, "");
    const auto half = bundleOne(kitSvg("<path d=\"M10.5 10 L70 10 L70 70 Z\"/>"));
    const char* comma = nullptr;
    for (const char* name : {"de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "it_IT.UTF-8"})
        if (std::setlocale(LC_NUMERIC, name)) {
            comma = name;
            break;
        }
    if (!comma) {
        printf("SHP-30 skipped: no decimal-comma locale installed\n");
        return;
    }
    const auto de = bundleShapes({{"star.svg", svg, sidecar}}, "");
    const auto halfDe = bundleOne(kitSvg("<path d=\"M10.5 10 L70 10 L70 70 Z\"/>"));
    std::setlocale(LC_NUMERIC, "C");
    ASSERT(c.ok() && de.ok() && c.pack == de.pack && c.ledger == de.ledger,
           "SHP-30 the Star bundles to the same bytes under a decimal-comma locale");
    ASSERT(half.ok() && halfDe.ok() && half.pack == halfDe.pack, "SHP-30 '10.5' is 10.5 under a decimal-comma locale");
}

int main(int argc, char** argv) {
    if (argc >= 3 && std::strcmp(argv[1], "--verify-pack") == 0) return verifyPack(argc, argv);

    static const struct { const char* id; void (*run)(); } tests[] = {
        {"SHP-01", test01_packRoundTrip},
        {"SHP-02", test02_minimalKit},
        {"SHP-03", test03_lineCommands},
        {"SHP-04", test04_curves},
        {"SHP-05", test05_arcs},
        {"SHP-06", test06_elements},
        {"SHP-07", test07_transforms},
        {"SHP-08", test08_paint},
        {"SHP-09", test09_pullApart},
        {"SHP-10", test10_sidecar},
        {"SHP-11", test11_ledger},
        {"SHP-12", test12_names},
        {"SHP-13", test13_hardErrors},
        {"SHP-14", test14_standInKit},
        {"SHP-15", test15_corpus},
        {"SHP-16", test16_preview},
        {"SHP-17", test17_colours},
        {"SHP-18", test18_dither},
        {"SHP-19", test19_stroke},
        {"SHP-20", test20_holes},
        {"SHP-21", test21_clip},
        {"SHP-22", test22_featureSheet},
        {"SHP-23", test23_poseFallback},
        {"SHP-24", test24_pixelTie},
        {"SHP-25", test25_swayAnchor},
        {"SHP-26", test26_colourCount},
        {"SHP-27", test27_retiredId},
        {"SHP-28", test28_standInSheet},
        {"SHP-29", test29_nonFinite},
        {"SHP-30", test30_locale},
    };
    // `--only SHP-nn`: one group, so each lint and the Feature sheets are a ctest of their own.
    const char* only = argc >= 3 && std::strcmp(argv[1], "--only") == 0 ? argv[2] : nullptr;
    bool ran = false;
    for (const auto& t : tests)
        if (!only || std::strcmp(only, t.id) == 0) {
            t.run();
            ran = true;
        }
    if (!ran) {
        fprintf(stderr, "no test group %s\n", only);
        return 1;
    }

    printf("shape_bundle_test%s%s: %d passed, %d failed\n", only ? " " : "", only ? only : "", passes, failures);
    return failures == 0 ? 0 : 1;
}
