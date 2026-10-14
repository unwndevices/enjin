/**
 * @file shape_bundle_test.cpp
 * @brief Tests for the Shape pack format and the Shape bundle function (Tomodachi #330)
 *
 * Test IDs:
 *   SHP-01  Shape pack round trip: writer → index → one record by seek.
 *
 * `shape_bundle_test --verify-pack <a.njn> [<b.njn>]` checks a CLI output
 * (index readable, every record decodes; with two files, identical indices).
 */

#include <enjin2/graphics/shape_pack.hpp>

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

    printf("shape_bundle_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
