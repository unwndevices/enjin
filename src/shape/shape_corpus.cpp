/**
 * @file shape_corpus.cpp
 * @brief The Shape bit-identity corpus (ADR-0016, Tomodachi #337)
 */
#include "enjin2/shape/shape_corpus.hpp"

#include <cstdio>

namespace enjin2 {
namespace shape {

namespace {

// The script's frame boundaries: each phase starts at its frame.
constexpr uint16_t kRest = 1;
constexpr uint16_t kKick = 11;
constexpr uint16_t kDrag = 81;
constexpr uint16_t kDragEnd = 110;  // last frame that moves the carry target
constexpr uint16_t kPull = 171;
constexpr uint16_t kRelease = 191;
constexpr uint16_t kBreathe = 271;
static_assert(ShapeCorpus::kFrames == 321, "the script ends at frame 320");

constexpr int kFeetX = 80;
constexpr int kFeetY = 140;

FixedVec2 px(int x, int y) { return {Fixed::fromInt(x), Fixed::fromInt(y)}; }

ShapeCorpusPhase phaseOf(uint16_t f)
{
    if (f < kRest) return ShapeCorpusPhase::Draw;
    if (f < kKick) return ShapeCorpusPhase::Rest;
    if (f < kDrag) return ShapeCorpusPhase::Kick;
    if (f < kPull) return ShapeCorpusPhase::Drag;
    if (f < kBreathe) return ShapeCorpusPhase::Pull;
    return ShapeCorpusPhase::Breathe;
}

}  // namespace

uint32_t shapeCrc32(uint32_t crc, const void* data, size_t size)
{
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t c = ~crc;
    for (size_t i = 0; i < size; ++i) {
        c ^= p[i];
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

uint32_t shapeLayerCrc(const ShapeCorpusLayer& layer)
{
    uint32_t c = 0;
    uint8_t row[160];
    for (int16_t y = 0; y < 160; ++y) {
        for (int16_t x = 0; x < 160; ++x) row[x] = layer.getPixel(x, y).value;
        c = shapeCrc32(c, row, sizeof row);
    }
    return c;
}

const char* shapeCorpusPhaseName(ShapeCorpusPhase p)
{
    switch (p) {
        case ShapeCorpusPhase::Draw: return "draw";
        case ShapeCorpusPhase::Rest: return "rest";
        case ShapeCorpusPhase::Kick: return "kick";
        case ShapeCorpusPhase::Drag: return "drag";
        case ShapeCorpusPhase::Pull: return "pull";
        case ShapeCorpusPhase::Breathe: return "breathe";
    }
    return "?";
}

size_t formatShapeCorpusFrame(const ShapeCorpusFrame& f, char* out, size_t n)
{
    if (!out || n == 0) return 0;
    const int len = std::snprintf(out, n, "%u %s %08X %d %d", unsigned(f.index), shapeCorpusPhaseName(f.phase),
                                  unsigned(f.crc), f.redrew ? 1 : 0, f.asleep ? 1 : 0);
    if (len < 0 || size_t(len) >= n) {
        out[0] = '\0';
        return 0;
    }
    return size_t(len);
}

bool ShapeCorpus::begin(ShapePack& pack, AssetArena& arena, ShapeError* err)
{
    ShapeLook look;
    for (uint8_t k = 0; k < SHAPE_SLOT_COUNT; ++k) look.set(ShapeSlot(k), 1);
    look.colour[0] = 9;
    look.colour[1] = 4;
    look.colour[2] = 0;
    begun_ = pet_.build(pack, look, arena, err);
    if (!begun_) return false;
    pet_.setPosition(px(kFeetX, kFeetY));
    grab_ = -1;
    next_ = 0;
    failed_ = false;
    return true;
}

bool ShapeCorpus::step(ShapeCorpusLayer& layer, ShapeDrawScratch& scratch, ShapeCorpusFrame& out)
{
    if (!begun_ || failed_ || next_ >= kFrames) return false;
    const uint16_t f = next_;
    const ShapeClock clock = scratch.clock;
    out.simTicks = out.rasterTicks = out.paintTicks = 0;

    if (f == 0) {
        layer.clear(Pixel4(15));
    } else {
        if (f == kKick) pet_.impulse(px(kFeetX + 8, kFeetY - 40), px(-500, 400));
        if (f >= kDrag && f <= kDragEnd) pet_.moveTo(px(kFeetX + (f - kDrag + 1), kFeetY));
        if (f == kPull) {
            const FixedVec2 feet = pet_.position();
            grabAt_ = {feet.x, feet.y - Fixed::fromInt(20)};
            grab_ = pet_.grab(grabAt_);
            if (grab_ < 0) {
                failed_ = true;
                return false;
            }
        }
        if (f >= kPull && f < kRelease) {
            const int k = f - kPull + 1 < 10 ? f - kPull + 1 : 10;
            pet_.pull(grab_, {grabAt_.x + Fixed::fromInt(k), grabAt_.y - Fixed::fromInt(2 * k)});
        }
        if (f == kRelease) {
            pet_.release(grab_, px(-300, 300));
            grab_ = -1;
        }
        if (f == kBreathe) {
            ShapeError e;
            pet_.breathe(Breath{}, &e);  // the defaults are in range
        }
        const uint32_t t0 = clock ? clock() : 0;
        pet_.tick(kFrameMicros);
        if (clock) out.simTicks = clock() - t0;
    }

    out.index = f;
    out.phase = phaseOf(f);
    const uint32_t paint0 = scratch.paintTicks;
    const uint32_t t0 = clock ? clock() : 0;
    out.redrew = pet_.flush(layer, scratch);
    if (clock && out.redrew) {
        out.paintTicks = scratch.paintTicks - paint0;
        out.rasterTicks = (clock() - t0) - out.paintTicks;
    }
    out.asleep = pet_.asleep();
    out.crc = shapeLayerCrc(layer);
    ++next_;
    return true;
}

}  // namespace shape
}  // namespace enjin2
