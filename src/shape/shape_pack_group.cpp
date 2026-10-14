// Pack layer (ADR-0016/0017, Tomodachi #331). See shape_pack_group.hpp.

#include "enjin2/shape/shape_pack_group.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <new>

#include "enjin2/core/math.hpp"

namespace enjin2 {
namespace shape {

namespace {

void setError(ShapeError* err, const char* fmt, ...)
{
    if (!err)
        return;
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(err->text, sizeof err->text, fmt, ap);
    va_end(ap);
}

constexpr size_t align8(size_t v) { return (v + 7) & ~size_t(7); }

constexpr const char* kRoleNames[3] = {"body", "accent", "ink"};

static_assert(uint8_t(Binding::Field) == uint8_t(ShapeBinding::Field) &&
                  uint8_t(Binding::Rigid) == uint8_t(ShapeBinding::Rigid) &&
                  uint8_t(Binding::Sway) == uint8_t(ShapeBinding::Sway),
              "shape::Binding mirrors the pack's ShapeBinding");

bool isFeature(ShapeSlot s) { return s != ShapeSlot::Body; }

} // namespace

// ---------------------------------------------------------------------------
// ShapePack
// ---------------------------------------------------------------------------

bool ShapePack::open(ShapePackSource& src, ShapeError* err)
{
    index_ = ShapePackIndex{};
    bodies_.clear();
    std::memset(maxRecord_, 0, sizeof maxRecord_);
    src_ = nullptr;
    const char* why = nullptr;
    if (!index_.open(src, &why))
    {
        setError(err, "%s", why ? why : "shape pack: cannot read the index");
        return false;
    }
    std::vector<uint8_t> rec;
    for (const ShapePackEntry& e : index_.pieces())
    {
        uint32_t& m = maxRecord_[uint8_t(e.slot)];
        if (e.size > m)
            m = e.size;
        if (e.slot != ShapeSlot::Body)
            continue;
        rec.resize(e.size);
        ShapeRecordView v;
        if (!src.readAt(e.offset, rec.data(), e.size) || !v.parse(rec.data(), rec.size(), &why) ||
            v.slot() != ShapeSlot::Body)
        {
            setError(err, "shape pack: body %u \"%s\": %s", unsigned(e.id), e.label.c_str(),
                     why ? why : "cannot read the record");
            index_ = ShapePackIndex{};
            bodies_.clear();
            return false;
        }
        BodyInfo b{e.id, {}};
        for (uint8_t i = 0; i < v.socketCount(); ++i)
        {
            uint8_t& n = b.sockets[uint8_t(v.socket(i).slot)];
            if (n < 255)
                ++n;
        }
        bodies_.push_back(b);
    }
    src_ = &src;
    return true;
}

const uint8_t* ShapePack::bodySockets(uint16_t id) const
{
    for (const BodyInfo& b : bodies_)
        if (b.id == id)
            return b.sockets;
    return nullptr;
}

size_t ShapePack::regionBytes(uint32_t recordSize)
{
    if (recordSize == 0)
        return 0;
    // The raw record, then its points as FixedVec2 (8 bytes for 4 on the wire;
    // the points can't be more than the record past its 20-byte header).
    const size_t maxPoints = recordSize > 20 ? (recordSize - 20) / 4 : 0;
    return align8(recordSize) + align8(maxPoints * sizeof(FixedVec2));
}

size_t ShapePack::lookBufferBytes() const
{
    size_t total = 0;
    for (uint8_t s = 0; s < SHAPE_SLOT_COUNT; ++s)
        total += slotRegionBytes(ShapeSlot(s));
    return total;
}

namespace {
int pickMember(const ShapeRecordView& v, SocketSide side, bool& mirror);
} // namespace

// ---------------------------------------------------------------------------
// ShapePackGroup
// ---------------------------------------------------------------------------

void ShapePackGroup::clear()
{
    uint8_t* const buffer = buffer_;
    const size_t bytes = bufferBytes_;
    ShapeHitStore* const hits = hits_;
    reset();
    buffer_ = buffer;
    bufferBytes_ = bytes;
    hits_ = hits;
}

void ShapePackGroup::reset()
{
    // In place: a whole-group temporary (the Lattice and every slot) would
    // sit on the caller's stack, the Lua task's on the device.
    this->~ShapePackGroup();
    new (this) ShapePackGroup();
}

bool ShapePackGroup::build(ShapePack& pack, const ShapeLook& look, AssetArena& arena, ShapeError* err)
{
    clear();
    if (!pack.source())
    {
        setError(err, "shape pack: not open");
        return false;
    }
    const size_t total = pack.lookBufferBytes();
    const AssetArena::Mark mark = arena.mark();
    uint8_t* const keptBuffer = buffer_;
    const size_t keptBytes = bufferBytes_;
    ShapeHitStore* const keptHits = hits_;
    // All or nothing: undo what this build carved, keep what it reused.
    auto fail = [&] {
        arena.rollback(mark);
        reset();
        buffer_ = keptBuffer;
        bufferBytes_ = keptBytes;
        hits_ = keptHits;
        return false;
    };
    if (total > bufferBytes_)
    {
        uint8_t* buf = arena.allocate(total, 8);
        if (!buf)
        {
            setError(err, "shape pack: the asset arena has no room for a %u-byte Look buffer", unsigned(total));
            return fail();
        }
        buffer_ = buf;
        bufferBytes_ = total;
    }
    size_t off = 0;
    for (uint8_t s = 0; s < SHAPE_SLOT_COUNT; ++s)
    {
        slots_[s].capacity = pack.slotRegionBytes(ShapeSlot(s));
        slots_[s].region = total ? buffer_ + off : nullptr;
        off += slots_[s].capacity;
    }
    // The last drawn masks, for hit() and grab() (#336).
    if (!hits_)
    {
        uint8_t* hits = arena.allocate(sizeof(ShapeHitStore), 8);
        if (!hits)
        {
            setError(err, "shape pack: the asset arena has no room for the pet's hit masks (%u bytes)",
                     unsigned(sizeof(ShapeHitStore)));
            return fail();
        }
        hits_ = reinterpret_cast<ShapeHitStore*>(hits);
    }
    group_.setHitStore(new (hits_) ShapeHitStore());
    pack_ = &pack;
    if (!setLook(look, err))
        return fail();
    return true;
}

bool ShapePackGroup::validate(const ShapeLook& look, ShapeError* err) const
{
    static const ShapeSlot kRequired[] = {ShapeSlot::Body, ShapeSlot::Eyes, ShapeSlot::Mouth};
    for (ShapeSlot s : kRequired)
        if (!look.has(s))
        {
            setError(err, "Look: no %s (body, eyes and mouth are required)", shapeSlotName(s));
            return false;
        }
    for (uint8_t i = 0; i < SHAPE_SLOT_COUNT; ++i)
    {
        const ShapeSlot s = ShapeSlot(i);
        if (look.has(s) && !pack_->index().find(s, look.get(s)))
        {
            setError(err, "Look: %s %u is not in the Shape pack (unknown or retired id)", shapeSlotName(s),
                     unsigned(look.get(s)));
            return false;
        }
    }
    const uint8_t* sockets = pack_->bodySockets(look.get(ShapeSlot::Body));
    for (uint8_t i = 1; i < SHAPE_SLOT_COUNT; ++i)
    {
        const ShapeSlot s = ShapeSlot(i);
        if (look.has(s) && (!sockets || sockets[i] == 0))
        {
            setError(err, "Look: %s %u has no socket on body %u", shapeSlotName(s), unsigned(look.get(s)),
                     unsigned(look.get(ShapeSlot::Body)));
            return false;
        }
    }
    for (uint8_t r = 0; r < 3; ++r)
        if (look.colour[r] > 14)
        {
            setError(err, "Look: colours.%s %u is outside the palette (0..14; 15 is transparent)", kRoleNames[r],
                     unsigned(look.colour[r]));
            return false;
        }
    return true;
}

bool ShapePackGroup::readSlot(ShapeSlot s, uint16_t id, ShapeError* err)
{
    Slot& slot = slots_[uint8_t(s)];
    slot.loaded = false;
    const ShapePackEntry* e = pack_->index().find(s, id);
    if (!e || ShapePack::regionBytes(e->size) > slot.capacity)
    {
        setError(err, "Look: %s %u is not in the Shape pack (unknown or retired id)", shapeSlotName(s), unsigned(id));
        return false;
    }
    const char* why = nullptr;
    if (!pack_->source()->readAt(e->offset, slot.region, e->size) || !slot.view.parse(slot.region, e->size, &why) ||
        slot.view.slot() != s)
    {
        setError(err, "Look: %s %u \"%s\": %s", shapeSlotName(s), unsigned(id), e->label.c_str(),
                 why ? why : "cannot read the record");
        slot.view = ShapeRecordView{};
        return false;
    }
    for (uint16_t i = 0; i < slot.view.shapeCount(); ++i)
    {
        const ShapeFill f = slot.view.shape(i);
        uint32_t points = 0;
        for (uint16_t r = 0; r < f.ringCount; ++r)
            points += slot.view.ring(f.firstRing + r).pointCount;
        if (f.ringCount > ShapeDrawScratch::kMaxRings || points > ShapeDrawScratch::kMaxPoints)
        {
            setError(err, "Look: %s %u \"%s\": a Shape has more than %u rings or %u points", shapeSlotName(s),
                     unsigned(id), e->label.c_str(), unsigned(ShapeDrawScratch::kMaxRings),
                     unsigned(ShapeDrawScratch::kMaxPoints));
            slot.view = ShapeRecordView{};
            return false;
        }
    }
    auto* pts = reinterpret_cast<FixedVec2*>(slot.region + align8(e->size));
    for (uint16_t i = 0; i < slot.view.pointCount(); ++i)
    {
        const ShapePoint p = slot.view.point(i);
        pts[i] = {Fixed::fromQ8_8(p.x), Fixed::fromQ8_8(p.y)};
    }
    slot.points = pts;
    // A new Feature starts in its slot's default pose (the first of the list).
    const int def = slot.view.findPose(0);
    slot.pose = uint8_t(def < 0 ? 0 : def);
    slot.loaded = true;
    return true;
}

bool ShapePackGroup::setLook(const ShapeLook& look, ShapeError* err)
{
    if (!pack_)
    {
        setError(err, "shape group: not built");
        return false;
    }
    if (!validate(look, err))
        return false;
    const ShapeLook old = look_;
    const bool fresh = !slots_[uint8_t(ShapeSlot::Body)].loaded;
    bool pieces = false;
    for (uint8_t i = 0; i < SHAPE_SLOT_COUNT; ++i)
    {
        const ShapeSlot s = ShapeSlot(i);
        if (!fresh && look.samePiece(old, s))
            continue;
        pieces = true;
        if (!look.has(s))
        {
            slots_[i].loaded = false;
            continue;
        }
        if (!readSlot(s, look.get(s), err))
        {
            // Put back what was overwritten (the same reads that loaded it).
            for (uint8_t k = 0; k <= i; ++k)
            {
                const ShapeSlot ks = ShapeSlot(k);
                if (fresh || !look.samePiece(old, ks))
                {
                    if (!fresh && old.has(ks))
                        readSlot(ks, old.get(ks), nullptr);
                    else
                        slots_[k].loaded = false;
                }
            }
            return false;
        }
    }
    look_ = look;
    Remap r = Remap::identity();
    for (uint8_t k = 0; k < 3; ++k)
        r.lut[k] = look.colour[k];
    group_.setRemap(r);
    if (pieces)
    {
        refreshBody();
        refreshLattice();
        if (breathing_)
            breathing_ = startBreath();
        group_.markChanged();
    }
    return true;
}

void ShapePackGroup::refreshBody()
{
    // The feet line stays where it is (and a drag keeps going) across a new Body.
    const FixedVec2 at = position();
    const bool carrying = group_.lattice().carrying();
    const Slot& b = slots_[uint8_t(ShapeSlot::Body)];
    feet_ = {};
    bodyMin_ = bodyMax_ = {};
    if (!b.loaded)
        return;
    bool any = false;
    Fixed minX, maxX, minY, maxY;
    const ShapeSpan span = b.view.span(b.pose, 0);
    for (uint16_t k = 0; k < span.shapeCount; ++k)
    {
        const ShapeFill f = b.view.shape(span.firstShape + k);
        for (uint16_t r = 0; r < f.ringCount; ++r)
        {
            const ShapeRing ring = b.view.ring(f.firstRing + r);
            if (!(ring.flags & SHAPE_RING_CLOSED))
                continue;
            for (uint16_t i = 0; i < ring.pointCount; ++i)
            {
                const FixedVec2 p = b.points[ring.firstPoint + i];
                if (!any)
                {
                    minX = maxX = p.x;
                    minY = maxY = p.y;
                    any = true;
                    continue;
                }
                if (p.x < minX) minX = p.x;
                if (p.x > maxX) maxX = p.x;
                if (p.y > maxY) maxY = p.y;
                if (p.y < minY) minY = p.y;
            }
        }
    }
    if (any)
    {
        feet_ = {Fixed::fromRaw(int32_t((int64_t(minX.raw) + maxX.raw) / 2)), maxY};
        bodyMin_ = {minX, minY};
        bodyMax_ = {maxX, maxY};
    }
    group_.setOrigin(at - feet_);
    if (carrying)
        group_.moveTo(carryFeet_ - feet_);
}

namespace {

// round(n / d), half away from zero (d > 0).
int64_t roundDiv(int64_t n, int64_t d) { return n >= 0 ? (n + d / 2) / d : -((-n + d / 2) / d); }

} // namespace

bool ShapePackGroup::startBreath()
{
    const Fixed w = bodyMax_.x - bodyMin_.x, h = bodyMax_.y - bodyMin_.y;
    if (w.raw <= 0 || h.raw <= 0)
        return false;
    const bool centre = breath_.pivot == BreathPivot::Centre;
    const FixedVec2 pivot = centre ? FixedVec2{feet_.x, Fixed::fromRaw(int32_t((int64_t(bodyMin_.y.raw) + bodyMax_.y.raw) / 2))}
                                   : feet_;
    DriveStep steps[SteppedDrive::kMaxSteps];
    for (uint8_t i = 0; i < breath_.steps; ++i)
    {
        const uint16_t turn = uint16_t((uint32_t(i) << 16) / breath_.steps);
        const int32_t k = int32_t(roundDiv(int64_t(breath_.amplitude) * math::TrigLUT::sin16(turn), 32767));
        const int32_t dh = centre ? 2 * k : k;  // the box's height change, px
        // The sides move in by whole pixels each, as the area asks.
        const int32_t dw = -2 * int32_t(roundDiv(int64_t(w.raw) * dh, 2 * int64_t(h.raw)));
        // About the feet the top is a whole box height from the pivot; about
        // the centre top and feet are half of it each.
        steps[i].grow = {Fixed::fromInt(dw), Fixed::fromInt(dh)};
    }
    return group_.startDrive(steps, breath_.steps, breath_.periodMicros, pivot, {w, h});
}

bool ShapePackGroup::breathe(const Breath& b, ShapeError* err)
{
    if (!pack_)
    {
        setError(err, "breathe: the pet is not built");
        return false;
    }
    if (b.steps < SteppedDrive::kMinSteps || b.steps > SteppedDrive::kMaxSteps)
    {
        setError(err, "breathe: steps %u is outside %u..%u", unsigned(b.steps), unsigned(SteppedDrive::kMinSteps),
                 unsigned(SteppedDrive::kMaxSteps));
        return false;
    }
    if (b.periodMicros == 0)
    {
        setError(err, "breathe: the period must be longer than 0 s");
        return false;
    }
    if (b.amplitude > 16)
    {
        setError(err, "breathe: amplitude %u is past 16 px", unsigned(b.amplitude));
        return false;
    }
    breath_ = b;
    breathing_ = startBreath();
    if (!breathing_)
        setError(err, "breathe: the Body has no closed outline to breathe");
    return breathing_;
}

void ShapePackGroup::stopBreathing()
{
    breathing_ = false;
    group_.stopDrive();
}

void ShapePackGroup::refreshLattice()
{
    const Slot& body = slots_[uint8_t(ShapeSlot::Body)];
    if (!body.loaded)
        return;
    bool any = false;
    FixedVec2 lo{}, hi{};
    // Every point of `slot`'s record, member `m` (all poses), placed at `at`.
    auto add = [&](const Slot& slot, uint8_t m, FixedVec2 at, bool mirror) {
        for (uint8_t pose = 0; pose < slot.view.poseCount(); ++pose)
        {
            const ShapeSpan span = slot.view.span(pose, m);
            for (uint16_t k = 0; k < span.shapeCount; ++k)
            {
                const ShapeFill f = slot.view.shape(span.firstShape + k);
                for (uint16_t r = 0; r < f.ringCount; ++r)
                {
                    const ShapeRing ring = slot.view.ring(f.firstRing + r);
                    for (uint16_t i = 0; i < ring.pointCount; ++i)
                    {
                        const FixedVec2 p = slot.points[ring.firstPoint + i];
                        const FixedVec2 q = {mirror ? at.x - p.x : at.x + p.x, at.y + p.y};
                        if (!any)
                        {
                            lo = hi = q;
                            any = true;
                            continue;
                        }
                        if (q.x < lo.x) lo.x = q.x;
                        if (q.y < lo.y) lo.y = q.y;
                        if (q.x > hi.x) hi.x = q.x;
                        if (q.y > hi.y) hi.y = q.y;
                    }
                }
            }
        }
    };
    add(body, 0, {}, false);
    for (uint8_t i = 1; i < SHAPE_SLOT_COUNT; ++i)
    {
        const Slot& slot = slots_[i];
        if (!slot.loaded)
            continue;
        for (uint8_t k = 0; k < body.view.socketCount(); ++k)
        {
            const ShapeSocket sk = body.view.socket(k);
            if (uint8_t(sk.slot) != i)
                continue;
            bool mirror = false;
            const int m = pickMember(slot.view, sk.side, mirror);
            add(slot, uint8_t(m), {Fixed::fromQ8_8(sk.at.x), Fixed::fromQ8_8(sk.at.y)}, mirror);
        }
    }
    Lattice& lat = group_.lattice();
    if (!any)
    {
        lat = Lattice{};
        return;
    }
    const FixedVec2 margin = {Fixed::fromInt(3), Fixed::fromInt(3)};
    lo = lo - margin;
    hi = hi + margin;
    if (!lat.configured() || lat.boxMin() != lo || lat.boxMax() != hi)
        lat.configure(lo, hi);

    // A sway arm per socket a sway Feature sits on: from the socket to the
    // centre of the member's points (every pose, mirrored as drawn).
    lat.clearSway();
    for (uint8_t i = 1; i < SHAPE_SLOT_COUNT; ++i)
    {
        const Slot& slot = slots_[i];
        if (!slot.loaded || slot.view.binding() != ShapeBinding::Sway)
            continue;
        SwaySpring spring;
        if (slot.view.swayHz() != 0)
            spring.hz = Fixed::fromRaw(int32_t(slot.view.swayHz()) << 4);  // Q4.12 → Q16.16
        if (slot.view.swayHz() != 0 || slot.view.swayZeta() != 0)
            spring.damping = Fixed::fromRaw(int32_t(slot.view.swayZeta()) << 4);
        for (uint8_t k = 0; k < body.view.socketCount(); ++k)
        {
            const ShapeSocket sk = body.view.socket(k);
            if (uint8_t(sk.slot) != i)
                continue;
            bool mirror = false;
            const int m = pickMember(slot.view, sk.side, mirror);
            int64_t sx = 0, sy = 0, count = 0;
            for (uint8_t pose = 0; pose < slot.view.poseCount(); ++pose)
            {
                const ShapeSpan span = slot.view.span(pose, uint8_t(m));
                for (uint16_t s = 0; s < span.shapeCount; ++s)
                {
                    const ShapeFill f = slot.view.shape(span.firstShape + s);
                    for (uint16_t r = 0; r < f.ringCount; ++r)
                    {
                        const ShapeRing ring = slot.view.ring(f.firstRing + r);
                        for (uint16_t q = 0; q < ring.pointCount; ++q)
                        {
                            const FixedVec2 p = slot.points[ring.firstPoint + q];
                            sx += mirror ? -p.x.raw : p.x.raw;
                            sy += p.y.raw;
                            ++count;
                        }
                    }
                }
            }
            const FixedVec2 arm = count ? FixedVec2{Fixed::fromRaw(int32_t(sx / count)), Fixed::fromRaw(int32_t(sy / count))}
                                        : FixedVec2{};
            lat.addSway({Fixed::fromQ8_8(sk.at.x), Fixed::fromQ8_8(sk.at.y)}, arm, spring);
        }
    }
}

void ShapePackGroup::setPosition(FixedVec2 feet)
{
    carryFeet_ = feet;
    group_.setOrigin(feet - feet_);
}

void ShapePackGroup::moveTo(FixedVec2 feet)
{
    carryFeet_ = feet;
    group_.moveTo(feet - feet_);
}

namespace {

// A drawn Shape's hit tag: its slot and its socket's place among the slot's.
constexpr uint16_t hitTag(ShapeSlot s, uint8_t member) { return uint16_t(uint8_t(s) << 8 | member); }

} // namespace

bool ShapePackGroup::hit(int x, int y, ShapeSlot& slot, uint8_t& member) const
{
    const int32_t tag = group_.hit(x, y);
    if (tag < 0)
        return false;
    slot = ShapeSlot(uint8_t(tag >> 8));
    member = uint8_t(tag & 0xFF);
    return true;
}

int ShapePackGroup::grab(FixedVec2 at)
{
    ShapeSlot s;
    uint8_t member;
    if (!pack_ || !hit(at.x.floorToInt(), at.y.floorToInt(), s, member))
        return -1;
    const Slot& slot = slots_[uint8_t(s)];
    if (s != ShapeSlot::Body && slot.loaded && slot.view.binding() == ShapeBinding::Sway)
    {
        // That member's socket, and the arm hung on it.
        const Slot& body = slots_[uint8_t(ShapeSlot::Body)];
        uint8_t place = 0;
        for (uint8_t k = 0; k < body.view.socketCount(); ++k)
        {
            const ShapeSocket sk = body.view.socket(k);
            if (sk.slot != s)
                continue;
            if (place++ != member)
                continue;
            const int arm = group_.lattice().swayIndexAt({Fixed::fromQ8_8(sk.at.x), Fixed::fromQ8_8(sk.at.y)});
            if (arm >= 0)
                return group_.grabSway(uint8_t(arm));
            break;
        }
    }
    return group_.grabNode(at);
}

bool ShapePackGroup::setPose(ShapeSlot s, const char* name, ShapeError* err)
{
    Slot& slot = slots_[uint8_t(s)];
    if (uint8_t(s) >= SHAPE_SLOT_COUNT || !slot.loaded)
    {
        setError(err, "pose: no %s in this Look", shapeSlotName(s));
        return false;
    }
    int slotPose = -1;
    for (uint8_t i = 0; i < shapePoseCount(s); ++i)
        if (name && std::strcmp(shapePoseName(s, i), name) == 0)
            slotPose = i;
    if (slotPose < 0)
    {
        setError(err, "pose: %s has no pose \"%s\"", shapeSlotName(s), name ? name : "");
        return false;
    }
    const int rec = slot.view.findPose(uint8_t(slotPose));
    if (rec < 0)
    {
        setError(err, "pose: %s %u has no \"%s\" pose", shapeSlotName(s), unsigned(look_.get(s)), name);
        return false;
    }
    if (slot.pose != rec)
    {
        slot.pose = uint8_t(rec);
        group_.markChanged();
        group_.lattice().wake();
    }
    return true;
}

const char* ShapePackGroup::poseName(ShapeSlot s) const
{
    const Slot& slot = slots_[uint8_t(s)];
    return slot.loaded ? shapePoseName(s, slot.view.pose(slot.pose)) : nullptr;
}

uint8_t ShapePackGroup::poseNames(ShapeSlot s, const char** out, uint8_t max) const
{
    const Slot& slot = slots_[uint8_t(s)];
    if (!slot.loaded)
        return 0;
    uint8_t n = 0;
    for (uint8_t i = 0; i < slot.view.poseCount() && n < max; ++i)
        out[n++] = shapePoseName(s, slot.view.pose(i));
    return n;
}

// ---------------------------------------------------------------------------
// Assembly
// ---------------------------------------------------------------------------

namespace {

// The member a socket gets and whether it is mirrored (see the header).
int pickMember(const ShapeRecordView& v, SocketSide side, bool& mirror)
{
    int single = -1, left = -1, right = -1;
    for (uint8_t m = 0; m < v.memberCount(); ++m)
    {
        const MemberSide ms = v.member(m).side;
        int& slot = ms == MemberSide::Left ? left : ms == MemberSide::Right ? right : single;
        if (slot < 0)
            slot = m;
    }
    mirror = false;
    if (side == SocketSide::Left)
    {
        if (left >= 0)
            return left;
        mirror = true;
        return single >= 0 ? single : right >= 0 ? right : 0;
    }
    if (side == SocketSide::Right)
        return right >= 0 ? right : single >= 0 ? single : 0;
    return single >= 0 ? single : right >= 0 ? right : 0;
}

} // namespace

bool ShapePackGroup::forEach(ShapeDrawScratch& scratch, ShapeEmit emit, void* ctx) const
{
    const Slot& body = slots_[uint8_t(ShapeSlot::Body)];
    if (!body.loaded)
        return true;
    bool ok = true;

    // A record Shape's rest rings into `rings` (room checked at readSlot).
    auto restShape = [](const Slot& slot, const ShapeFill& f, Ring* rings, Shape& out) {
        for (uint16_t r = 0; r < f.ringCount; ++r)
        {
            const ShapeRing ring = slot.view.ring(f.firstRing + r);
            rings[r] = Ring{slot.points + ring.firstPoint, ring.pointCount, (ring.flags & SHAPE_RING_CLOSED) != 0};
        }
        out = Shape{};
        out.rings = rings;
        out.ringCount = f.ringCount;
        out.paint.fill = ColourRole(uint8_t(f.role));
        out.paint.rule = (f.flags & SHAPE_FILL_EVENODD) ? FillRule::EvenOdd : FillRule::NonZero;
        out.paint.dither = uint8_t(f.dither * 4);  // quarters of coverage → Bayer level
        out.paint.strokeWidth = f.strokeWidth;
        out.paint.stroke = ColourRole(uint8_t(f.strokeRole));
    };

    // The live Body clip: the Body's Shapes as drawn, in group coordinates.
    uint16_t liveCount = 0;
    bool liveOk = true;
    {
        const ShapeSpan span = body.view.span(body.pose, 0);
        uint16_t rings = 0;
        for (uint16_t k = 0; k < span.shapeCount && liveOk; ++k)
        {
            const ShapeFill f = body.view.shape(span.firstShape + k);
            if (liveCount == ShapeDrawScratch::kMaxClipShapes || rings + f.ringCount > ShapeDrawScratch::kMaxRings)
            {
                liveOk = false;
                break;
            }
            restShape(body, f, scratch.liveRestRings + rings, scratch.liveRest[liveCount++]);
            rings = uint16_t(rings + f.ringCount);
        }
    }

    // Emit one member's Shapes in its current pose.
    auto emitMember = [&](const Slot& slot, uint8_t member, FixedVec2 at, bool mirror, uint16_t tag) {
        const ShapeSpan span = slot.view.span(slot.pose, member);
        for (uint16_t k = 0; k < span.shapeCount; ++k)
        {
            const ShapeFill f = slot.view.shape(span.firstShape + k);
            Shape shape;
            restShape(slot, f, scratch.restRings, shape);
            shape.tag = tag;
            if (f.clip == ShapeClip::Own)
            {
                restShape(slot, slot.view.shape(f.clipShape), scratch.clipRestRings, scratch.clipRest);
                shape.clip = &scratch.clipRest;
                shape.clipCount = 1;
                shape.clipSpace = ClipSpace::Item;
            }
            else if (f.clip == ShapeClip::LiveBody)
            {
                if (!liveOk)
                {
                    ok = false;
                    continue;
                }
                if (liveCount == 0)
                    continue;  // a Body that draws nothing clips everything away
                shape.clip = scratch.liveRest;
                shape.clipCount = liveCount;
                shape.clipSpace = ClipSpace::Group;
            }
            // A Body is field; a Feature rides by its baked Binding.
            const Binding binding = slot.view.slot() == ShapeSlot::Body ? Binding::Field
                                                                        : Binding(uint8_t(slot.view.binding()));
            if (!emit(ctx, shape, at, mirror, binding))
                ok = false;
        }
    };
    // Paint order of the Feature slots on each side of the Body (#317
    // decision 6). Behind: limbs, then top. In front: top, limbs, cheeks,
    // mouth, eyes (eyes always on top). Limbs are arms, then legs.
    static constexpr ShapeSlot kBehind[] = {ShapeSlot::Arms,   ShapeSlot::Legs,  ShapeSlot::Top,
                                            ShapeSlot::Cheeks, ShapeSlot::Mouth, ShapeSlot::Eyes};
    static constexpr ShapeSlot kFront[] = {ShapeSlot::Top,    ShapeSlot::Arms,  ShapeSlot::Legs,
                                           ShapeSlot::Cheeks, ShapeSlot::Mouth, ShapeSlot::Eyes};
    static_assert(sizeof(kBehind) == SHAPE_SLOT_COUNT - 1 && sizeof(kFront) == SHAPE_SLOT_COUNT - 1,
                  "every Feature slot paints once per side");
    auto emitFeatures = [&](ShapeOrder order) {
        for (const ShapeSlot ps : order == ShapeOrder::Behind ? kBehind : kFront)
        {
            const uint8_t i = uint8_t(ps);
            const Slot& slot = slots_[i];
            if (!slot.loaded || slot.view.order() != order)
                continue;
            uint8_t place = 0;
            for (uint8_t k = 0; k < body.view.socketCount(); ++k)
            {
                const ShapeSocket sk = body.view.socket(k);
                if (uint8_t(sk.slot) != i)
                    continue;
                bool mirror = false;
                const int m = pickMember(slot.view, sk.side, mirror);
                emitMember(slot, uint8_t(m), {Fixed::fromQ8_8(sk.at.x), Fixed::fromQ8_8(sk.at.y)}, mirror,
                           hitTag(ps, place++));
            }
        }
    };

    emitFeatures(ShapeOrder::Behind);
    emitMember(body, 0, {}, false, hitTag(ShapeSlot::Body, 0));
    emitFeatures(ShapeOrder::Front);
    return ok;
}

} // namespace shape
} // namespace enjin2
