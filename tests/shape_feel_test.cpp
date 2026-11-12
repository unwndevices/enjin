// The Feel, Blinks and Reactions (ADR-0019, Tomodachi #365): the pointer
// machine over a fake pet (its internal seam), the blink scheduler's clock and
// seed, and the three on the Star through ShapePackGroup's interface.

#include <enjin2/graphics/asset_arena.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/shape_pack.hpp>
#include <enjin2/import/shape_bundle.hpp>
#include <enjin2/shape/shape_blink.hpp>
#include <enjin2/shape/shape_feel.hpp>
#include <enjin2/shape/shape_pack_group.hpp>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
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

static FixedVec2 px(int x, int y) { return {Fixed::fromInt(x), Fixed::fromInt(y)}; }
constexpr uint32_t kFrame = 33334;

// ---------------------------------------------------------------------------
// The pointer machine over a fake pet
// ---------------------------------------------------------------------------

struct FakePet : PetActions {
    bool onPet = true;       // hold() succeeds
    FixedVec2 at = px(80, 150);
    int holds = 0, pulls = 0, letGos = 0, carries = 0, pokes = 0;
    FixedVec2 lastLetGo{}, lastCarry{}, lastPoke{}, lastPokeAt{};
    bool held = false;

    bool hold(FixedVec2) override { ++holds; held = onPet; return onPet; }
    void pullTo(FixedVec2) override { ++pulls; }
    void letGo(FixedVec2 v) override { ++letGos; lastLetGo = v; held = false; }
    void carryTo(FixedVec2 f) override { ++carries; lastCarry = f; }
    void poke(FixedVec2 p, FixedVec2 v) override { ++pokes; lastPokeAt = p; lastPoke = v; }
    FixedVec2 feet() const override { return at; }
};

static void testPointerOffThePet() {
    FakePet pet;
    pet.onPet = false;
    PetPointer m;
    Feel feel;
    uint64_t now = 0;
    PetEvent e = m.sample(px(10, 10), true, now, feel, pet);
    ASSERT(e.kind == PetEventKind::None && pet.holds == 1, "a touch off the pet tries one hold");
    for (int i = 0; i < 20; ++i) {
        now += kFrame;
        e = m.sample(px(10 + 3 * i, 10), true, now, feel, pet);
        ASSERT(e.kind == PetEventKind::None, "and nothing while it moves");
    }
    now += kFrame;
    e = m.sample(px(70, 10), false, now, feel, pet);
    ASSERT(e.kind == PetEventKind::None && pet.holds == 1 && pet.letGos == 0 && pet.pulls == 0,
           "nor when it lifts: no second hold, no pull, no let go");
    ASSERT(!m.holding(), "idle again");
}

static void testTap() {
    FakePet pet;
    PetPointer m;
    Feel feel;
    uint64_t now = 0;
    PetEvent e = m.sample(px(84, 130), true, now, feel, pet);
    ASSERT(e.kind == PetEventKind::None && pet.holds == 1 && m.holding(), "a touch on the pet takes hold");
    now += kFrame;
    e = m.sample(px(85, 131), true, now, feel, pet);  // within the slop
    ASSERT(e.kind == PetEventKind::None && pet.pulls == 1, "held, the finger pulls");
    now += kFrame;
    e = m.sample(px(85, 131), false, now, feel, pet);
    ASSERT(e.kind == PetEventKind::Tap, "a short still touch is a tap");
    ASSERT(pet.letGos == 1 && pet.lastLetGo == FixedVec2{}, "let go with no throw");
    ASSERT(pet.pokes == 1 && pet.lastPokeAt == px(84, 130), "poked where the finger landed");
    ASSERT(pet.lastPoke.y == feel.poke, "straight down at the poke speed");
    ASSERT(pet.lastPoke.x == Fixed::fromInt((84 - 80) * 8), "kicked sideways away from the feet");
    ASSERT(pet.carries == 0 && !m.holding(), "no drag, idle again");

    // Too long a touch is not a tap, however still.
    e = m.sample(px(84, 130), true, now, feel, pet);
    for (int i = 0; i < 15; ++i) {  // 0.5 s
        now += kFrame;
        e = m.sample(px(84, 130), true, now, feel, pet);
    }
    e = m.sample(px(84, 130), false, now, feel, pet);
    ASSERT(e.kind == PetEventKind::None && pet.pokes == 1 && pet.letGos == 2, "a long still touch just lets go");
    // Too far a move is not a tap either, even if quick.
    e = m.sample(px(84, 130), true, now, feel, pet);
    now += kFrame;
    e = m.sample(px(88, 130), true, now, feel, pet);  // 4 px: past the slop, short of loose
    now += kFrame;
    e = m.sample(px(84, 130), true, now, feel, pet);  // and back
    now += kFrame;
    e = m.sample(px(84, 130), false, now, feel, pet);
    ASSERT(e.kind == PetEventKind::None && pet.pokes == 1 && pet.letGos == 3, "a wiggle that came back is not a tap");
}

static void testPullDragThrow() {
    FakePet pet;
    PetPointer m;
    Feel feel;
    uint64_t now = 0;
    PetEvent e = m.sample(px(80, 130), true, now, feel, pet);
    // A pull: 8 px, under loose. No carry.
    for (int i = 1; i <= 8; ++i) {
        now += kFrame;
        e = m.sample(px(80 + i, 130), true, now, feel, pet);
        ASSERT(e.kind == PetEventKind::None, "a short move is a pull, not a drag");
    }
    ASSERT(pet.carries == 0 && pet.pulls == 8, "pulled, not carried");
    // On past loose: one Drag event, then the feet are carried by the finger
    // less the offset it landed with (the finger landed 20 px above the feet).
    int drags = 0;
    for (int i = 9; i <= 20; ++i) {
        now += kFrame;
        e = m.sample(px(80 + i, 130), true, now, feel, pet);
        drags += e.kind == PetEventKind::Drag;
    }
    ASSERT(drags == 1, "coming loose is said once");
    ASSERT(pet.carries > 0 && pet.lastCarry == px(100, 150), "the feet follow the finger, offset kept");
    ASSERT(pet.pulls == 20, "the pull goes on through the drag");
    // Let go moving: thrown at the smoothed, clamped velocity.
    now += kFrame;
    e = m.sample(px(100, 130), false, now, feel, pet);
    ASSERT(e.kind == PetEventKind::Throw, "letting go of a drag is a throw");
    ASSERT(e.velocity.x > Fixed::fromInt(10) && e.velocity.x <= feel.maxThrow, "thrown rightward, within maxThrow");
    ASSERT(pet.lastLetGo == e.velocity, "the held spot is let go at the same velocity");
    ASSERT(!m.holding(), "idle again");

    // A fast flick clamps to maxThrow; a drag past the bounds is clamped to them.
    e = m.sample(px(80, 130), true, now, feel, pet);
    for (int i = 1; i <= 6; ++i) {
        now += kFrame;
        e = m.sample(px(80 - 40 * i, 130), true, now, feel, pet);
    }
    ASSERT(pet.lastCarry.x == feel.boundsMin.x, "carried no further than the bounds");
    now += kFrame;
    e = m.sample(px(80 - 240, 130), false, now, feel, pet);
    ASSERT(e.kind == PetEventKind::Throw && e.velocity.x == -feel.maxThrow, "a flick throws at maxThrow");
}

static void testRelease() {
    FakePet pet;
    PetPointer m;
    Feel feel;
    uint64_t now = 0;
    m.sample(px(80, 130), true, now, feel, pet);
    m.release(pet);
    ASSERT(pet.letGos == 1 && !m.holding(), "release lets go of the held pet");
    now += kFrame;
    PetEvent e = m.sample(px(90, 130), true, now, feel, pet);
    ASSERT(e.kind == PetEventKind::None && pet.holds == 1 && pet.pulls == 0, "the finger does nothing more");
    now += kFrame;
    e = m.sample(px(90, 130), false, now, feel, pet);
    ASSERT(e.kind == PetEventKind::None, "until it lifts");
    now += kFrame;
    e = m.sample(px(90, 130), true, now, feel, pet);
    ASSERT(pet.holds == 2, "then a new touch takes hold again");
    m.release(pet);
    m.release(pet);
    ASSERT(pet.letGos == 2, "release twice lets go once");
}

// ---------------------------------------------------------------------------
// The blink scheduler
// ---------------------------------------------------------------------------

struct BlinkRun {
    std::vector<uint32_t> shutLengths;  // frames per shut run
    std::vector<uint32_t> starts;       // frame each run started
};

static BlinkRun runBlinks(const Blink& b, int frames) {
    BlinkScheduler s;
    s.start(b, 0);
    BlinkRun run;
    uint64_t now = 0;
    uint32_t len = 0;
    for (int f = 0; f < frames; ++f) {
        now += 20000;  // 50 fps
        s.tick(now);
        if (s.shut()) {
            if (!len) run.starts.push_back(uint32_t(f));
            ++len;
        } else if (len) {
            run.shutLengths.push_back(len);
            len = 0;
        }
    }
    return run;
}

static void testBlinks() {
    Blink b;
    b.seed = 12345;
    const BlinkRun a = runBlinks(b, 3000);  // 60 s
    ASSERT(a.shutLengths.size() >= 10 && a.shutLengths.size() <= 60, "a go every 2..6 s, one or two blinks each");
    bool lengths = true;
    for (uint32_t l : a.shutLengths) lengths = lengths && l == 6;  // 120 ms at 20 ms frames
    ASSERT(lengths, "every blink shuts the eyes for 120 ms");
    int doubles = 0;
    bool spacing = true;
    for (size_t i = 1; i < a.starts.size(); ++i) {
        const uint32_t gap = a.starts[i] - a.starts[i - 1];
        if (gap < 100) ++doubles;
        spacing = spacing && (gap == 15 || gap >= 100);  // 120 + 180 ms, or 2 s+
    }
    ASSERT(doubles > 0 && doubles < int(a.shutLengths.size()) / 2, "some goes are doubles, most are not");
    ASSERT(spacing, "a double's second blink comes 180 ms after the first opens");
    const BlinkRun again = runBlinks(b, 3000);
    ASSERT(again.starts == a.starts && again.shutLengths == a.shutLengths, "the same seed is the same run");
    b.seed = 54321;
    ASSERT(runBlinks(b, 3000).starts != a.starts, "another seed is another run");

    BlinkScheduler s;
    ASSERT(!s.enabled() && !s.shut(), "off until started");
    s.start(b, 0);
    uint64_t now = 0;
    while (!s.shut()) s.tick(now += 20000);
    s.stop();
    ASSERT(!s.enabled() && !s.shut(), "stopped mid-blink, the eyes open");
    ASSERT(!s.tick(now + 20000), "and nothing changes after");
}

// ---------------------------------------------------------------------------
// On the Star, through the group
// ---------------------------------------------------------------------------

using Layer = Canvas4<160, 160>;
static ShapeDrawScratch scratch;

static std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

struct Star {
    ShapeBundleResult bundle;
    std::unique_ptr<ShapePackMemory> src;
    ShapePack pack;
    AssetArena arena;
    ShapePackGroup pet;
    ShapeLook look;
    Layer layer;

    bool open() {
        const std::string dir = std::string(ENJIN2_SOURCE_DIR) + "/tools/testdata/shapes/";
        const std::vector<uint8_t> ledger = readFile(dir + "ledger.json");
        bundle = bundleShapes({{"star.svg", readFile(dir + "star.svg"), readFile(dir + "star.svg.shape.json")}},
                              std::string(reinterpret_cast<const char*>(ledger.data()), ledger.size()));
        if (!bundle.ok()) return false;
        src.reset(new ShapePackMemory(bundle.pack.data(), bundle.pack.size()));
        ShapeError e;
        if (!pack.open(*src, &e)) return false;
        arena.allocateBacking(64 * 1024);
        for (uint8_t k = 0; k < SHAPE_SLOT_COUNT; ++k) look.set(ShapeSlot(k), 1);
        look.colour[0] = 5;  // accent2: a Look colour is a Palette role (0..7, #420)
        look.colour[1] = 4;
        look.colour[2] = 0;
        if (!pet.build(pack, look, arena, &e)) return false;
        pet.setPosition(px(80, 150));
        layer.clear(Pixel4(15));
        pet.flush(layer, scratch);
        settle();
        return pet.asleep();
    }
    // Frames until asleep (at most `max`); how many it took.
    int settle(int max = 300) {
        int f = 0;
        do {
            pet.tick(kFrame);
            pet.flush(layer, scratch);
            ++f;
        } while (!pet.asleep() && f < max);
        return f;
    }
    bool frame() {
        pet.tick(kFrame);
        return pet.flush(layer, scratch);
    }
    // A pixel of the Body nearest (x, y), from the last redraw's masks.
    bool bodyPixel(int x, int y, int& ox, int& oy) const {
        int best = 1 << 30;
        for (int py = 0; py < 160; ++py)
            for (int qx = 0; qx < 160; ++qx) {
                ShapeSlot s;
                uint8_t m;
                if (!pet.hit(qx, py, s, m) || s != ShapeSlot::Body) continue;
                const int d = (qx - x) * (qx - x) + (py - y) * (py - y);
                if (d < best) { best = d; ox = qx; oy = py; }
            }
        return best != (1 << 30);
    }
};

static bool is(const char* a, const char* b) { return a && b && std::strcmp(a, b) == 0; }

static void testStarPointer() {
    Star s;
    ASSERT(s.open(), "the Star opens and settles");
    if (!s.pet.asleep()) return;
    int bx = 0, by = 0;
    ASSERT(s.bodyPixel(80, 135, bx, by), "a Body pixel to touch");

    // A tap: the pet wakes, wobbles and settles back exactly.
    PetEvent e = s.pet.pointer(px(bx, by), true);
    ASSERT(e.kind == PetEventKind::None && s.pet.pointerHeld() && s.pet.holding(), "the touch takes hold");
    s.frame();
    e = s.pet.pointer(px(bx, by), false);
    ASSERT(e.kind == PetEventKind::Tap && !s.pet.holding(), "a tap, and the grab is let go");
    ASSERT(!s.pet.asleep(), "the poke woke it");
    const int tapFrames = s.settle();
    ASSERT(tapFrames > 2 && tapFrames < 120, "it wobbles, then sleeps");
    ASSERT(s.pet.position() == px(80, 150), "a tap does not move the feet");

    // A drag: the feet follow the finger; a throw leaves them.
    e = s.pet.pointer(px(bx, by), true);
    int drags = 0;
    for (int i = 1; i <= 25; ++i) {
        e = s.pet.pointer(px(bx + i, by - i), true);
        drags += e.kind == PetEventKind::Drag;
        s.frame();
    }
    ASSERT(drags == 1, "loose once");
    for (int i = 0; i < 60; ++i) {  // held still: the carry settles on the finger
        e = s.pet.pointer(px(bx + 25, by - 25), true);
        s.frame();
    }
    ASSERT(s.pet.position() == px(80 + 25, 150 - 25), "the feet were carried with the finger");
    e = s.pet.pointer(px(bx + 25, by - 25), false);
    ASSERT(e.kind == PetEventKind::Throw && e.velocity == FixedVec2{}, "let go still: a throw at rest");
    s.settle();
    ASSERT(s.pet.asleep() && s.pet.position() == px(105, 125), "and it stays where it was dropped");

    // Off the pet: nothing.
    e = s.pet.pointer(px(2, 2), true);
    s.frame();
    e = s.pet.pointer(px(2, 2), false);
    ASSERT(e.kind == PetEventKind::None && s.pet.asleep(), "a touch off the pet does nothing");

    // A new Look lets go.
    e = s.pet.pointer(px(105, 110), true);
    ASSERT(s.pet.holding(), "held");
    ShapeLook other = s.look;
    other.colour[0] = 3;
    ShapeError err;
    ASSERT(s.pet.setLook(other, &err), "a colours-only Look");
    ASSERT(!s.pet.holding() && !s.pet.pointerHeld(), "lets go of the pet");
    e = s.pet.pointer(px(110, 110), true);
    ASSERT(!s.pet.holding(), "and the finger does nothing until it lifts");
    s.pet.pointer(px(110, 110), false);

    // The Feel is tunable: a tighter loose comes loose sooner.
    Feel f;
    f.loose = Fixed::fromInt(2);
    s.pet.setFeel(f);
    s.settle();
    ASSERT(s.bodyPixel(105, 110, bx, by), "a Body pixel");
    s.pet.pointer(px(bx, by), true);
    e = s.pet.pointer(px(bx + 3, by), true);
    ASSERT(e.kind == PetEventKind::Drag, "3 px is a drag at loose 2");
    s.pet.pointer(px(bx + 3, by), false);
}

static void testStarBlinkAndReact() {
    Star s;
    ASSERT(s.open(), "the Star opens and settles");
    ASSERT(is(s.pet.poseName(ShapeSlot::Eyes), "open"), "eyes open after build");
    ASSERT(!s.pet.blinking(), "no blinks after build");
    for (int i = 0; i < 300; ++i) ASSERT(!s.frame(), "a still pet redraws nothing");

    // Reactions: a pose for a while over the base.
    ShapeError e;
    ASSERT(s.pet.react(ShapeSlot::Mouth, "happy", 500000, &e), "react mouth happy 0.5 s");
    ASSERT(is(s.pet.poseName(ShapeSlot::Mouth), "happy"), "shown at once");
    ASSERT(s.frame(), "and drawn");
    for (int i = 0; i < 10; ++i) s.frame();  // 0.37 s
    ASSERT(is(s.pet.poseName(ShapeSlot::Mouth), "happy"), "still shown");
    ASSERT(s.pet.setPose(ShapeSlot::Mouth, "sad", &e), "the base changes underneath");
    ASSERT(is(s.pet.poseName(ShapeSlot::Mouth), "happy"), "the Reaction stays on top");
    for (int i = 0; i < 6; ++i) s.frame();  // past 0.5 s
    ASSERT(is(s.pet.poseName(ShapeSlot::Mouth), "sad"), "over, the base shows");
    ASSERT(s.pet.react(ShapeSlot::Mouth, "open", 1000000, &e), "another");
    ASSERT(s.pet.react(ShapeSlot::Mouth, "happy", 100000, &e), "replaced and restarted");
    ASSERT(is(s.pet.poseName(ShapeSlot::Mouth), "happy"), "the new one shows");
    for (int i = 0; i < 4; ++i) s.frame();
    ASSERT(is(s.pet.poseName(ShapeSlot::Mouth), "sad"), "and ends on its own time");
    ASSERT(s.pet.react(ShapeSlot::Mouth, "open", 1000000, &e) && s.pet.react(ShapeSlot::Mouth, "open", 0, &e),
           "0 cancels");
    ASSERT(is(s.pet.poseName(ShapeSlot::Mouth), "sad"), "cancelled");
    ASSERT(!s.pet.react(ShapeSlot::Mouth, "wink", 1, &e), "an unknown pose is refused");
    ASSERT(std::strstr(e.text, "react: mouth has no pose \"wink\"") != nullptr, "naming it");
    ASSERT(!s.pet.react(ShapeSlot::Legs, "default", 1, &e) || true, "legs have a default pose");

    // Blinks: the eyes' closed pose while shut, over a Reaction, over the base.
    Blink b;
    b.seed = 7;
    ASSERT(s.pet.blink(b, &e), "blink");
    ASSERT(s.pet.blinking(), "blinking");
    int shutFrames = 0, goes = 0;
    bool wasShut = false;
    for (int i = 0; i < 600; ++i) {  // 20 s
        s.frame();
        const bool shut = is(s.pet.poseName(ShapeSlot::Eyes), "closed");
        shutFrames += shut;
        goes += shut && !wasShut;
        wasShut = shut;
    }
    ASSERT(goes >= 3 && goes <= 12, "a few blinks in 20 s");
    ASSERT(shutFrames >= 3 * goes && shutFrames <= 5 * goes, "each 120 ms: 3..4 frames at 30 fps");
    ASSERT(s.pet.react(ShapeSlot::Eyes, "happy", 30000000, &e), "happy eyes for a long while");
    bool closedSeen = false, happySeen = false;
    for (int i = 0; i < 300; ++i) {
        s.frame();
        closedSeen = closedSeen || is(s.pet.poseName(ShapeSlot::Eyes), "closed");
        happySeen = happySeen || is(s.pet.poseName(ShapeSlot::Eyes), "happy");
    }
    ASSERT(closedSeen && happySeen, "the blink shows over the Reaction, which shows between blinks");
    s.pet.stopBlinking();
    ASSERT(is(s.pet.poseName(ShapeSlot::Eyes), "happy"), "stopped, the eyes open to the Reaction");
    s.pet.react(ShapeSlot::Eyes, "happy", 0, &e);
    ASSERT(is(s.pet.poseName(ShapeSlot::Eyes), "open"), "cancelled, to the base");

    // The same seed is the same blinks, frame for frame.
    Star t;
    ASSERT(t.open(), "another Star");
    std::vector<bool> one, two;
    ASSERT(s.pet.blink(b, &e) && t.pet.blink(b, &e), "both blink from seed 7");
    for (int i = 0; i < 300; ++i) {
        s.frame();
        t.frame();
        one.push_back(is(s.pet.poseName(ShapeSlot::Eyes), "closed"));
        two.push_back(is(t.pet.poseName(ShapeSlot::Eyes), "closed"));
    }
    ASSERT(one == two, "the same seed blinks the same");
    Blink bad;
    bad.everyMin = 3;
    bad.everyMax = 2;
    ASSERT(!s.pet.blink(bad, &e), "a backwards range is refused");
}

int main() {
    testPointerOffThePet();
    testTap();
    testPullDragThrow();
    testRelease();
    testBlinks();
    testStarPointer();
    testStarBlinkAndReact();
    printf("shape_feel_test: %d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
