/**
 * @file shape_pack.hpp
 * @brief Shape pack: the pet's Bodies and Features in one `.njn` v2 (ADR-0017, Tomodachi #330)
 *
 * A Shape pack holds every Body and Feature one applet can use, built offline
 * by enjin2::bundleShapes() (`import/shape_bundle.hpp`).  This header is the
 * format: the reader the device uses (index once, then one record by seek)
 * and the writer the bundle function uses.  Header-only, no allocation beyond
 * std::vector, no exceptions, no aborts: every input is untrusted.
 *
 * ## Chunks (a `.njn` v2 container, see njn2.hpp)
 *
 * | ID     | Content                                                        |
 * |--------|----------------------------------------------------------------|
 * | `SHDR` | u8 schema (= 1), u8 reserved, u16 numPieces                    |
 * | `SIDX` | piece index: numPieces entries, sorted by (slot, id)           |
 * | `SREC` | the records, one per piece, back to back                       |
 *
 * SIDX entry: u8 slot, u8 labelLen, u16 id, u32 offset (from the start of the
 * SREC data), u32 size, then labelLen label bytes (ASCII, ≤ 31, no NUL).
 *
 * ## Record (one Body or Feature)
 *
 * ```
 * u8  slot, u8 binding, u8 order, u8 flags (0)
 * u16 swayHz, u16 swayZeta          Q4.12 (Hz, damping ratio ζ)
 * u8  numPoses, u8 numMembers, u8 numSockets, u8 reserved
 * u16 numShapes, u16 numRings, u16 numPoints, u16 reserved
 * poses   [numPoses]               u8 pose (index into the slot's pose list)
 * members [numMembers]             u8 side, s16 homeX, s16 homeY
 * spans   [numPoses × numMembers]  u16 firstShape, u16 shapeCount  (pose-major)
 * sockets [numSockets]             u8 slot, u8 side, s16 x, s16 y  (Body only)
 * shapes  [numShapes]              u8 role, u8 flags, u8 dither, u8 strokeWidth,
 *                                  u8 strokeRole, u8 clip, u16 clipShape,
 *                                  u16 firstRing, u16 ringCount
 * rings   [numRings]               u16 firstPoint, u16 pointCount, u8 flags
 * points  [numPoints]              s16 x, s16 y
 * ```
 *
 * All coordinates are **Q8.8** logical pixels, y down.  A Feature member's
 * rings are relative to that member's anchor; a Body's to the Kit origin (the
 * artboard centre).  A member's `home` is where its anchor sat in its source
 * (Kit coordinates), for previews.  A socket is a member's anchor on the Body.
 *
 * A pose's member spans name the Shapes drawn for that member in that pose,
 * in paint order.  MemberSide::Mirrored is the single right-facing member,
 * mirrored on left sockets; Left/Right members go on sockets of their side.
 *
 * Paint fields reserved for later tickets (Tomodachi #332): `dither` (quarters
 * of coverage, 4 = solid), `strokeWidth` + `strokeRole` (inner / open-path
 * stroke), `clip` + `clipShape`.  The bundle writes 4 / 0 / None for now.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "njn2.hpp"

namespace enjin2 {

static constexpr NjnChunkTag NJN2_CHUNK_SHDR = {{'S', 'H', 'D', 'R'}};
static constexpr NjnChunkTag NJN2_CHUNK_SIDX = {{'S', 'I', 'D', 'X'}};
static constexpr NjnChunkTag NJN2_CHUNK_SREC = {{'S', 'R', 'E', 'C'}};

/// The only Shape pack schema this reader understands.
static constexpr uint8_t SHAPE_PACK_SCHEMA_VERSION = 1;

static constexpr uint16_t SHAPE_PACK_MAX_PIECES = 4096;
static constexpr uint8_t  SHAPE_PACK_MAX_LABEL  = 31;
static constexpr uint32_t SHAPE_PACK_MAX_RECORD = 1u << 20;

/// The fixed slots.  Body is a slot of its own in the pack.
enum class ShapeSlot : uint8_t { Body, Eyes, Cheeks, Mouth, Top, Arms, Legs };
static constexpr uint8_t SHAPE_SLOT_COUNT = 7;

/// How a Feature rides the Lattice (a Body is Field).
enum class ShapeBinding : uint8_t { Field, Rigid, Sway };
/// Behind or in front of the Body (a Body is Front).
enum class ShapeOrder : uint8_t { Front, Behind };
/// Colour role of a Shape's fill or stroke.
enum class ShapeRole : uint8_t { Body, Accent, Ink, None };
/// Clip of a Shape (Tomodachi #332).
enum class ShapeClip : uint8_t { None, Own, LiveBody };
/// A socket's side of the Body's centre line, on screen.
enum class SocketSide : uint8_t { Centre, Left, Right };
/// Which sockets a member goes on.
enum class MemberSide : uint8_t { Mirrored, Left, Right };

static constexpr uint8_t SHAPE_FILL_EVENODD = 0x01;  ///< ShapeFill::flags: even-odd fill rule.
static constexpr uint8_t SHAPE_RING_CLOSED  = 0x01;  ///< ShapeRing::flags: closed ring.
static constexpr uint8_t SHAPE_DITHER_SOLID = 4;

struct ShapePoint { int16_t x = 0, y = 0; };  ///< Q8.8.

struct ShapeMember {
    MemberSide side = MemberSide::Mirrored;
    ShapePoint home;
};

struct ShapeSpan { uint16_t firstShape = 0, shapeCount = 0; };

struct ShapeSocket {
    ShapeSlot  slot = ShapeSlot::Eyes;
    SocketSide side = SocketSide::Centre;
    ShapePoint at;
};

/// One Shape: its paint and its rings.
struct ShapeFill {
    ShapeRole role = ShapeRole::Body;
    uint8_t   flags = 0;
    uint8_t   dither = SHAPE_DITHER_SOLID;
    uint8_t   strokeWidth = 0;
    ShapeRole strokeRole = ShapeRole::None;
    ShapeClip clip = ShapeClip::None;
    uint16_t  clipShape = 0;
    uint16_t  firstRing = 0, ringCount = 0;
};

struct ShapeRing { uint16_t firstPoint = 0, pointCount = 0; uint8_t flags = 0; };

/// One Body or Feature, decoded.
struct ShapePiece {
    ShapeSlot    slot = ShapeSlot::Body;
    ShapeBinding binding = ShapeBinding::Field;
    ShapeOrder   order = ShapeOrder::Front;
    uint16_t     swayHz = 0, swayZeta = 0;  ///< Q4.12.
    std::vector<uint8_t>     poses;
    std::vector<ShapeMember> members;
    std::vector<ShapeSpan>   spans;  ///< poses.size() × members.size(), pose-major.
    std::vector<ShapeSocket> sockets;
    std::vector<ShapeFill>   shapes;
    std::vector<ShapeRing>   rings;
    std::vector<ShapePoint>  points;

    const ShapeSpan& span(size_t pose, size_t member) const { return spans[pose * members.size() + member]; }
};

/// One piece index entry.  `offset` is from the start of the file.
struct ShapePackEntry {
    ShapeSlot   slot = ShapeSlot::Body;
    uint16_t    id = 0;
    std::string label;
    uint32_t    offset = 0, size = 0;
};

// ---------------------------------------------------------------------------
// Names
// ---------------------------------------------------------------------------

inline const char* shapeSlotName(ShapeSlot s) {
    static const char* const names[] = {"body", "eyes", "cheeks", "mouth", "top", "arms", "legs"};
    const auto i = static_cast<uint8_t>(s);
    return i < SHAPE_SLOT_COUNT ? names[i] : "?";
}

/// Number of poses in a slot's fixed list (pose 0 is the required one).
inline uint8_t shapePoseCount(ShapeSlot s) {
    switch (s) {
        case ShapeSlot::Eyes: return 4;
        case ShapeSlot::Mouth: return 5;
        default: return static_cast<uint8_t>(s) < SHAPE_SLOT_COUNT ? 1 : 0;
    }
}

/// Name of pose @p pose of slot @p s, or nullptr past the list.
inline const char* shapePoseName(ShapeSlot s, uint8_t pose) {
    static const char* const eyes[] = {"open", "closed", "happy", "angry"};
    static const char* const mouth[] = {"neutral", "happy", "sad", "open", "surprised"};
    if (pose >= shapePoseCount(s)) return nullptr;
    if (s == ShapeSlot::Eyes) return eyes[pose];
    if (s == ShapeSlot::Mouth) return mouth[pose];
    return "default";
}

// ---------------------------------------------------------------------------
// Record codec
// ---------------------------------------------------------------------------

namespace shape_pack_detail {

inline uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
inline uint32_t rd32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
inline void put8(std::vector<uint8_t>& b, uint32_t v) { b.push_back(static_cast<uint8_t>(v)); }
inline void put16(std::vector<uint8_t>& b, uint32_t v) { put8(b, v); put8(b, v >> 8); }
inline void put32(std::vector<uint8_t>& b, uint32_t v) { put16(b, v); put16(b, v >> 16); }
inline void putPt(std::vector<uint8_t>& b, ShapePoint p) {
    put16(b, static_cast<uint16_t>(p.x));
    put16(b, static_cast<uint16_t>(p.y));
}
inline ShapePoint rdPt(const uint8_t* p) {
    return {static_cast<int16_t>(rd16(p)), static_cast<int16_t>(rd16(p + 2))};
}

inline bool fail(const char** err, const char* m) {
    if (err) *err = m;
    return false;
}

/// Structural checks shared by the writer and the decoder.
inline bool validate(const ShapePiece& p, const char** err) {
    if (static_cast<uint8_t>(p.slot) >= SHAPE_SLOT_COUNT) return fail(err, "shape record: bad slot");
    if (static_cast<uint8_t>(p.binding) > 2) return fail(err, "shape record: bad binding");
    if (static_cast<uint8_t>(p.order) > 1) return fail(err, "shape record: bad order");
    if (p.poses.empty() || p.poses.size() > 255) return fail(err, "shape record: bad pose count");
    if (p.members.empty() || p.members.size() > 255) return fail(err, "shape record: bad member count");
    if (p.sockets.size() > 255) return fail(err, "shape record: too many sockets");
    if (p.shapes.size() > 0xFFFF || p.rings.size() > 0xFFFF || p.points.size() > 0xFFFF)
        return fail(err, "shape record: too many shapes, rings or points");
    if (p.spans.size() != p.poses.size() * p.members.size()) return fail(err, "shape record: span table size");
    if (p.slot != ShapeSlot::Body && !p.sockets.empty()) return fail(err, "shape record: sockets on a Feature");
    const uint8_t nPoses = shapePoseCount(p.slot);
    for (size_t i = 0; i < p.poses.size(); ++i) {
        if (p.poses[i] >= nPoses) return fail(err, "shape record: pose outside the slot's list");
        for (size_t k = 0; k < i; ++k)
            if (p.poses[k] == p.poses[i]) return fail(err, "shape record: duplicate pose");
    }
    for (const auto& m : p.members)
        if (static_cast<uint8_t>(m.side) > 2) return fail(err, "shape record: bad member side");
    for (const auto& s : p.spans)
        if (static_cast<uint32_t>(s.firstShape) + s.shapeCount > p.shapes.size())
            return fail(err, "shape record: span past the shapes");
    for (const auto& s : p.sockets)
        if (s.slot == ShapeSlot::Body || static_cast<uint8_t>(s.slot) >= SHAPE_SLOT_COUNT ||
            static_cast<uint8_t>(s.side) > 2)
            return fail(err, "shape record: bad socket");
    for (const auto& f : p.shapes) {
        if (static_cast<uint8_t>(f.role) > 3 || static_cast<uint8_t>(f.strokeRole) > 3 ||
            static_cast<uint8_t>(f.clip) > 2 || f.dither < 1 || f.dither > SHAPE_DITHER_SOLID)
            return fail(err, "shape record: bad paint");
        if (f.clip == ShapeClip::Own && f.clipShape >= p.shapes.size())
            return fail(err, "shape record: clip past the shapes");
        if (static_cast<uint32_t>(f.firstRing) + f.ringCount > p.rings.size())
            return fail(err, "shape record: shape past the rings");
    }
    for (const auto& r : p.rings) {
        if (static_cast<uint32_t>(r.firstPoint) + r.pointCount > p.points.size())
            return fail(err, "shape record: ring past the points");
        if (r.pointCount < ((r.flags & SHAPE_RING_CLOSED) ? 3 : 2))
            return fail(err, "shape record: degenerate ring");
    }
    return true;
}

static constexpr uint32_t kRecordHeader = 20;

}  // namespace shape_pack_detail

/// Encode one record.  Fails (no output) on a structurally invalid piece.
inline bool shapePackEncodePiece(const ShapePiece& p, std::vector<uint8_t>& out, const char** err = nullptr) {
    using namespace shape_pack_detail;
    if (!validate(p, err)) return false;
    out.clear();
    put8(out, static_cast<uint8_t>(p.slot));
    put8(out, static_cast<uint8_t>(p.binding));
    put8(out, static_cast<uint8_t>(p.order));
    put8(out, 0);
    put16(out, p.swayHz);
    put16(out, p.swayZeta);
    put8(out, static_cast<uint32_t>(p.poses.size()));
    put8(out, static_cast<uint32_t>(p.members.size()));
    put8(out, static_cast<uint32_t>(p.sockets.size()));
    put8(out, 0);
    put16(out, static_cast<uint32_t>(p.shapes.size()));
    put16(out, static_cast<uint32_t>(p.rings.size()));
    put16(out, static_cast<uint32_t>(p.points.size()));
    put16(out, 0);
    for (uint8_t pose : p.poses) put8(out, pose);
    for (const auto& m : p.members) {
        put8(out, static_cast<uint8_t>(m.side));
        putPt(out, m.home);
    }
    for (const auto& s : p.spans) {
        put16(out, s.firstShape);
        put16(out, s.shapeCount);
    }
    for (const auto& s : p.sockets) {
        put8(out, static_cast<uint8_t>(s.slot));
        put8(out, static_cast<uint8_t>(s.side));
        putPt(out, s.at);
    }
    for (const auto& f : p.shapes) {
        put8(out, static_cast<uint8_t>(f.role));
        put8(out, f.flags);
        put8(out, f.dither);
        put8(out, f.strokeWidth);
        put8(out, static_cast<uint8_t>(f.strokeRole));
        put8(out, static_cast<uint8_t>(f.clip));
        put16(out, f.clipShape);
        put16(out, f.firstRing);
        put16(out, f.ringCount);
    }
    for (const auto& r : p.rings) {
        put16(out, r.firstPoint);
        put16(out, r.pointCount);
        put8(out, r.flags);
    }
    for (const auto& pt : p.points) putPt(out, pt);
    return true;
}

/**
 * @brief A validated, read-only view over one record's bytes: no allocation,
 *        fields decoded on access.  The device keeps a Look's records as raw
 *        bytes in its AssetArena and reads them through this.
 */
class ShapeRecordView {
public:
    /// Parse and validate @p size bytes at @p data (kept, not copied).
    bool parse(const uint8_t* data, size_t size, const char** err = nullptr) {
        using namespace shape_pack_detail;
        *this = ShapeRecordView{};
        if (!data || size < kRecordHeader) return fail(err, "shape record: truncated header");
        const uint8_t* h = data;
        const uint32_t nPoses = h[8], nMembers = h[9], nSockets = h[10];
        const uint32_t nShapes = rd16(h + 12), nRings = rd16(h + 14), nPoints = rd16(h + 16);
        const uint64_t want = uint64_t(kRecordHeader) + nPoses + nMembers * 5u + uint64_t(nPoses) * nMembers * 4u +
                              nSockets * 6u + nShapes * 12u + nRings * 5u + nPoints * 4u;
        if (want != size) return fail(err, "shape record: size does not match its counts");
        ShapeRecordView v;
        v.m_data = data;
        v.m_size = static_cast<uint32_t>(size);
        v.m_poses = static_cast<uint8_t>(nPoses);
        v.m_members = static_cast<uint8_t>(nMembers);
        v.m_sockets = static_cast<uint8_t>(nSockets);
        v.m_shapes = static_cast<uint16_t>(nShapes);
        v.m_rings = static_cast<uint16_t>(nRings);
        v.m_points = static_cast<uint16_t>(nPoints);
        uint32_t o = kRecordHeader;
        v.m_posesOff = o;   o += nPoses;
        v.m_membersOff = o; o += nMembers * 5u;
        v.m_spansOff = o;   o += nPoses * nMembers * 4u;
        v.m_socketsOff = o; o += nSockets * 6u;
        v.m_shapesOff = o;  o += nShapes * 12u;
        v.m_ringsOff = o;   o += nRings * 5u;
        v.m_pointsOff = o;
        if (!v.validate(err)) return false;
        *this = v;
        return true;
    }

    bool valid() const { return m_data != nullptr; }
    const uint8_t* data() const { return m_data; }
    uint32_t size() const { return m_size; }

    ShapeSlot slot() const { return static_cast<ShapeSlot>(m_data[0]); }
    ShapeBinding binding() const { return static_cast<ShapeBinding>(m_data[1]); }
    ShapeOrder order() const { return static_cast<ShapeOrder>(m_data[2]); }
    uint16_t swayHz() const { return shape_pack_detail::rd16(m_data + 4); }
    uint16_t swayZeta() const { return shape_pack_detail::rd16(m_data + 6); }

    uint8_t poseCount() const { return m_poses; }
    uint8_t memberCount() const { return m_members; }
    uint8_t socketCount() const { return m_sockets; }
    uint16_t shapeCount() const { return m_shapes; }
    uint16_t ringCount() const { return m_rings; }
    uint16_t pointCount() const { return m_points; }

    /// Slot pose (an index into the slot's fixed list) of the record's pose @p i.
    uint8_t pose(size_t i) const { return m_data[m_posesOff + i]; }
    /// The record's pose number holding slot pose @p slotPose, or -1 when it lacks it.
    int findPose(uint8_t slotPose) const {
        for (uint8_t i = 0; i < m_poses; ++i)
            if (pose(i) == slotPose) return i;
        return -1;
    }
    ShapeMember member(size_t i) const {
        const uint8_t* q = m_data + m_membersOff + i * 5;
        return {static_cast<MemberSide>(q[0]), shape_pack_detail::rdPt(q + 1)};
    }
    ShapeSpan span(size_t pose, size_t member) const {
        const uint8_t* q = m_data + m_spansOff + (pose * m_members + member) * 4;
        return {shape_pack_detail::rd16(q), shape_pack_detail::rd16(q + 2)};
    }
    ShapeSocket socket(size_t i) const {
        const uint8_t* q = m_data + m_socketsOff + i * 6;
        return {static_cast<ShapeSlot>(q[0]), static_cast<SocketSide>(q[1]), shape_pack_detail::rdPt(q + 2)};
    }
    ShapeFill shape(size_t i) const {
        using shape_pack_detail::rd16;
        const uint8_t* q = m_data + m_shapesOff + i * 12;
        ShapeFill f;
        f.role = static_cast<ShapeRole>(q[0]);
        f.flags = q[1];
        f.dither = q[2];
        f.strokeWidth = q[3];
        f.strokeRole = static_cast<ShapeRole>(q[4]);
        f.clip = static_cast<ShapeClip>(q[5]);
        f.clipShape = rd16(q + 6);
        f.firstRing = rd16(q + 8);
        f.ringCount = rd16(q + 10);
        return f;
    }
    ShapeRing ring(size_t i) const {
        const uint8_t* q = m_data + m_ringsOff + i * 5;
        return {shape_pack_detail::rd16(q), shape_pack_detail::rd16(q + 2), q[4]};
    }
    ShapePoint point(size_t i) const { return shape_pack_detail::rdPt(m_data + m_pointsOff + i * 4); }

private:
    // The same rules as shape_pack_detail::validate(), over the bytes.
    bool validate(const char** err) const {
        using shape_pack_detail::fail;
        if (static_cast<uint8_t>(slot()) >= SHAPE_SLOT_COUNT) return fail(err, "shape record: bad slot");
        if (static_cast<uint8_t>(binding()) > 2) return fail(err, "shape record: bad binding");
        if (static_cast<uint8_t>(order()) > 1) return fail(err, "shape record: bad order");
        if (m_poses == 0) return fail(err, "shape record: bad pose count");
        if (m_members == 0) return fail(err, "shape record: bad member count");
        if (slot() != ShapeSlot::Body && m_sockets != 0) return fail(err, "shape record: sockets on a Feature");
        const uint8_t nPoses = shapePoseCount(slot());
        for (uint8_t i = 0; i < m_poses; ++i) {
            if (pose(i) >= nPoses) return fail(err, "shape record: pose outside the slot's list");
            for (uint8_t k = 0; k < i; ++k)
                if (pose(k) == pose(i)) return fail(err, "shape record: duplicate pose");
        }
        for (uint8_t i = 0; i < m_members; ++i)
            if (static_cast<uint8_t>(member(i).side) > 2) return fail(err, "shape record: bad member side");
        for (uint8_t p = 0; p < m_poses; ++p)
            for (uint8_t m = 0; m < m_members; ++m) {
                const ShapeSpan s = span(p, m);
                if (static_cast<uint32_t>(s.firstShape) + s.shapeCount > m_shapes)
                    return fail(err, "shape record: span past the shapes");
            }
        for (uint8_t i = 0; i < m_sockets; ++i) {
            const ShapeSocket s = socket(i);
            if (s.slot == ShapeSlot::Body || static_cast<uint8_t>(s.slot) >= SHAPE_SLOT_COUNT ||
                static_cast<uint8_t>(s.side) > 2)
                return fail(err, "shape record: bad socket");
        }
        for (uint16_t i = 0; i < m_shapes; ++i) {
            const ShapeFill f = shape(i);
            if (static_cast<uint8_t>(f.role) > 3 || static_cast<uint8_t>(f.strokeRole) > 3 ||
                static_cast<uint8_t>(f.clip) > 2 || f.dither < 1 || f.dither > SHAPE_DITHER_SOLID)
                return fail(err, "shape record: bad paint");
            if (f.clip == ShapeClip::Own && f.clipShape >= m_shapes)
                return fail(err, "shape record: clip past the shapes");
            if (static_cast<uint32_t>(f.firstRing) + f.ringCount > m_rings)
                return fail(err, "shape record: shape past the rings");
        }
        for (uint16_t i = 0; i < m_rings; ++i) {
            const ShapeRing r = ring(i);
            if (static_cast<uint32_t>(r.firstPoint) + r.pointCount > m_points)
                return fail(err, "shape record: ring past the points");
            if (r.pointCount < ((r.flags & SHAPE_RING_CLOSED) ? 3 : 2))
                return fail(err, "shape record: degenerate ring");
        }
        return true;
    }

    const uint8_t* m_data = nullptr;
    uint32_t m_size = 0;
    uint8_t m_poses = 0, m_members = 0, m_sockets = 0;
    uint16_t m_shapes = 0, m_rings = 0, m_points = 0;
    uint32_t m_posesOff = 0, m_membersOff = 0, m_spansOff = 0, m_socketsOff = 0;
    uint32_t m_shapesOff = 0, m_ringsOff = 0, m_pointsOff = 0;
};

/// Decode and validate one record (the bytes of one index entry's window).
inline bool shapePackDecodePiece(const uint8_t* data, size_t size, ShapePiece& out, const char** err = nullptr) {
    out = ShapePiece{};
    ShapeRecordView v;
    if (!v.parse(data, size, err)) return false;
    out.slot = v.slot();
    out.binding = v.binding();
    out.order = v.order();
    out.swayHz = v.swayHz();
    out.swayZeta = v.swayZeta();
    for (uint8_t i = 0; i < v.poseCount(); ++i) out.poses.push_back(v.pose(i));
    for (uint8_t i = 0; i < v.memberCount(); ++i) out.members.push_back(v.member(i));
    for (uint8_t p = 0; p < v.poseCount(); ++p)
        for (uint8_t m = 0; m < v.memberCount(); ++m) out.spans.push_back(v.span(p, m));
    for (uint8_t i = 0; i < v.socketCount(); ++i) out.sockets.push_back(v.socket(i));
    for (uint16_t i = 0; i < v.shapeCount(); ++i) out.shapes.push_back(v.shape(i));
    for (uint16_t i = 0; i < v.ringCount(); ++i) out.rings.push_back(v.ring(i));
    for (uint16_t i = 0; i < v.pointCount(); ++i) out.points.push_back(v.point(i));
    return true;
}

// ---------------------------------------------------------------------------
// Pack writer
// ---------------------------------------------------------------------------

/// One piece going into a pack.
struct ShapePackPieceIn {
    ShapeSlot   slot = ShapeSlot::Body;
    uint16_t    id = 0;
    std::string label;
    ShapePiece  piece;
};

/**
 * @brief Write a Shape pack.  Pieces are indexed sorted by (slot, id).
 * @return false (and @p out empty) on a duplicate (slot, id), a slot that
 *         differs from its record, a bad label or an invalid record.
 */
inline bool shapePackWrite(const std::vector<ShapePackPieceIn>& pieces, std::vector<uint8_t>& out,
                           const char** err = nullptr) {
    using namespace shape_pack_detail;
    out.clear();
    if (pieces.size() > SHAPE_PACK_MAX_PIECES) return fail(err, "shape pack: too many pieces");
    std::vector<size_t> order(pieces.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    // Insertion sort keeps this header free of <algorithm>; packs are small.
    for (size_t i = 1; i < order.size(); ++i) {
        const size_t v = order[i];
        size_t k = i;
        auto less = [&](size_t a, size_t b) {
            return pieces[a].slot != pieces[b].slot ? pieces[a].slot < pieces[b].slot : pieces[a].id < pieces[b].id;
        };
        while (k > 0 && less(v, order[k - 1])) {
            order[k] = order[k - 1];
            --k;
        }
        order[k] = v;
    }
    std::vector<uint8_t> idx, rec, one;
    for (size_t n = 0; n < order.size(); ++n) {
        const ShapePackPieceIn& p = pieces[order[n]];
        if (n > 0 && pieces[order[n - 1]].slot == p.slot && pieces[order[n - 1]].id == p.id)
            return fail(err, "shape pack: duplicate slot + id");
        if (p.piece.slot != p.slot) return fail(err, "shape pack: index slot differs from the record");
        if (p.label.size() > SHAPE_PACK_MAX_LABEL) return fail(err, "shape pack: label too long");
        for (char c : p.label)
            if (c < 0x20 || c > 0x7E) return fail(err, "shape pack: label not printable ASCII");
        if (!shapePackEncodePiece(p.piece, one, err)) return false;
        if (one.size() > SHAPE_PACK_MAX_RECORD) return fail(err, "shape pack: record too large");
        put8(idx, static_cast<uint8_t>(p.slot));
        put8(idx, static_cast<uint32_t>(p.label.size()));
        put16(idx, p.id);
        put32(idx, static_cast<uint32_t>(rec.size()));
        put32(idx, static_cast<uint32_t>(one.size()));
        idx.insert(idx.end(), p.label.begin(), p.label.end());
        rec.insert(rec.end(), one.begin(), one.end());
    }
    NjnV2Writer w;
    w.beginChunk(NJN2_CHUNK_SHDR);
    w.writeU8(SHAPE_PACK_SCHEMA_VERSION);
    w.writeU8(0);
    w.writeU16LE(static_cast<uint16_t>(pieces.size()));
    w.endChunk();
    w.beginChunk(NJN2_CHUNK_SIDX);
    w.writeBytes(idx.data(), idx.size());
    w.endChunk();
    w.beginChunk(NJN2_CHUNK_SREC);
    w.writeBytes(rec.data(), rec.size());
    w.endChunk();
    w.finalise(out);
    return true;
}

// ---------------------------------------------------------------------------
// Pack reader
// ---------------------------------------------------------------------------

/// Random-access bytes: a file on the device, a buffer on the host.
class ShapePackSource {
public:
    virtual ~ShapePackSource() = default;
    virtual uint32_t size() const = 0;
    /// Copy @p len bytes at @p offset into @p dst; false past the end or on I/O error.
    virtual bool readAt(uint32_t offset, uint8_t* dst, uint32_t len) = 0;
};

/// A Shape pack already in memory.
class ShapePackMemory : public ShapePackSource {
public:
    ShapePackMemory(const uint8_t* data, size_t size) : m_data(data), m_size(size > 0xFFFFFFFFu ? 0 : size) {}
    uint32_t size() const override { return static_cast<uint32_t>(m_size); }
    bool readAt(uint32_t offset, uint8_t* dst, uint32_t len) override {
        if (!m_data || static_cast<uint64_t>(offset) + len > m_size) return false;
        if (len) std::memcpy(dst, m_data + offset, len);
        return true;
    }

private:
    const uint8_t* m_data;
    size_t m_size;
};

/**
 * @brief The piece index of a Shape pack: read once, then one record per seek.
 *
 * open() reads only the file header, the chunk directory, `SHDR` and `SIDX`.
 * readPiece() reads one record's window and decodes it.
 */
class ShapePackIndex {
public:
    bool open(ShapePackSource& src, const char** err = nullptr) {
        using namespace shape_pack_detail;
        m_pieces.clear();
        auto bad = [&](const char* m) {
            m_pieces.clear();
            return fail(err, m);
        };
        uint8_t hdr[NJN2_FILE_HEADER_SIZE];
        if (src.size() < NJN2_FILE_HEADER_SIZE || !src.readAt(0, hdr, sizeof hdr))
            return bad("shape pack: truncated header");
        if (hdr[0] != NJN2_MAGIC_0 || hdr[1] != NJN2_MAGIC_1 || hdr[2] != NJN2_VERSION)
            return bad("shape pack: not a .njn v2");
        const uint32_t numChunks = rd32(hdr + 4);
        if (rd32(hdr + 8) != src.size()) return bad("shape pack: size does not match its header");
        if (numChunks > 64) return bad("shape pack: too many chunks");
        std::vector<uint8_t> dir(numChunks * NJN2_DIR_ENTRY_SIZE);
        if (!src.readAt(NJN2_FILE_HEADER_SIZE, dir.data(), static_cast<uint32_t>(dir.size())))
            return bad("shape pack: truncated chunk directory");
        struct Win { uint32_t off = 0, size = 0; int n = 0; } shdr, sidx, srec;
        for (uint32_t i = 0; i < numChunks; ++i) {
            const uint8_t* e = dir.data() + i * NJN2_DIR_ENTRY_SIZE;
            const uint32_t off = rd32(e + 4), size = rd32(e + 8);
            if (off < NJN2_FILE_HEADER_SIZE || static_cast<uint64_t>(off) + size > src.size())
                return bad("shape pack: chunk outside the file");
            const NjnChunkTag tag = {{e[0], e[1], e[2], e[3]}};
            Win* w = tag == NJN2_CHUNK_SHDR ? &shdr : tag == NJN2_CHUNK_SIDX ? &sidx : tag == NJN2_CHUNK_SREC ? &srec : nullptr;
            if (w) *w = {off, size, w->n + 1};
        }
        if (shdr.n != 1 || sidx.n != 1 || srec.n != 1) return bad("shape pack: needs one SHDR, SIDX and SREC");
        uint8_t h[4];
        if (shdr.size != 4 || !src.readAt(shdr.off, h, 4)) return bad("shape pack: bad SHDR");
        if (h[0] != SHAPE_PACK_SCHEMA_VERSION) return bad("shape pack: unsupported schema");
        const uint32_t count = rd16(h + 2);
        if (count > SHAPE_PACK_MAX_PIECES) return bad("shape pack: too many pieces");
        if (sidx.size > count * (12u + SHAPE_PACK_MAX_LABEL)) return bad("shape pack: SIDX too large");
        std::vector<uint8_t> idx(sidx.size);
        if (!src.readAt(sidx.off, idx.data(), sidx.size)) return bad("shape pack: truncated SIDX");
        size_t pos = 0;
        m_pieces.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (pos + 12 > idx.size()) return bad("shape pack: truncated SIDX entry");
            const uint8_t* e = idx.data() + pos;
            ShapePackEntry ent;
            ent.slot = static_cast<ShapeSlot>(e[0]);
            const uint8_t labelLen = e[1];
            ent.id = rd16(e + 2);
            const uint32_t off = rd32(e + 4);
            ent.size = rd32(e + 8);
            pos += 12;
            if (static_cast<uint8_t>(ent.slot) >= SHAPE_SLOT_COUNT) return bad("shape pack: bad slot in SIDX");
            if (labelLen > SHAPE_PACK_MAX_LABEL || pos + labelLen > idx.size()) return bad("shape pack: bad label");
            ent.label.assign(reinterpret_cast<const char*>(idx.data() + pos), labelLen);
            pos += labelLen;
            if (static_cast<uint64_t>(off) + ent.size > srec.size || ent.size > SHAPE_PACK_MAX_RECORD)
                return bad("shape pack: record outside SREC");
            ent.offset = srec.off + off;
            if (!m_pieces.empty()) {
                const ShapePackEntry& prev = m_pieces.back();
                if (prev.slot > ent.slot || (prev.slot == ent.slot && prev.id >= ent.id))
                    return bad("shape pack: SIDX not sorted by slot + id");
            }
            m_pieces.push_back(std::move(ent));
        }
        if (pos != idx.size()) return bad("shape pack: trailing SIDX bytes");
        return true;
    }

    const std::vector<ShapePackEntry>& pieces() const { return m_pieces; }

    const ShapePackEntry* find(ShapeSlot slot, uint16_t id) const {
        for (const auto& e : m_pieces)
            if (e.slot == slot && e.id == id) return &e;
        return nullptr;
    }

    /// Read @p e's record from @p src and decode it into @p out.
    bool readPiece(ShapePackSource& src, const ShapePackEntry& e, ShapePiece& out, const char** err = nullptr) const {
        using namespace shape_pack_detail;
        std::vector<uint8_t> rec(e.size);
        if (e.size > SHAPE_PACK_MAX_RECORD || !src.readAt(e.offset, rec.data(), e.size))
            return fail(err, "shape pack: cannot read the record");
        if (!shapePackDecodePiece(rec.data(), rec.size(), out, err)) return false;
        if (out.slot != e.slot) {
            out = ShapePiece{};
            return fail(err, "shape pack: record slot differs from its index entry");
        }
        return true;
    }

private:
    std::vector<ShapePackEntry> m_pieces;
};

}  // namespace enjin2
