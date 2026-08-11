// M2 loader/writer exit test (unwn #183): the ratified schema round-trips
// wholesale — behavior model included — and a file-loaded scene *behaves*
// byte-identically to the scene it was dumped from.
//
//   1. fixture text -> (docA, worldA); dumpA -> (docB, worldB); dumpB.
//      dumpA == dumpB byte-exact (dump -> reload -> dump is a fixed point,
//      now covering state/timers/animations/on/slots/bindings, not just
//      components).
//   2. Two SceneVMs drive both copies through the same event/tick script —
//      activation, list arrival, scrolling, debounce expiry, load, dismiss —
//      rendering every frame through the same widget systems; every frame
//      must match pixel-for-pixel. Behavior is exercised, not just layout.
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/ui/scene_json.hpp>
#include <enjin2/ui/scene_vm.hpp>
#include <enjin2/ui/systems.hpp>
#include <enjin2/ui/theme.hpp>

#include "scene_fixture.h"

#include <cstdio>
#include <string>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { printf("PASS: %s\n", msg); passes++; }                         \
    } while (0)

using SceneCanvas = Canvas4<128, 128>;
using VM = SceneVM<SceneVmWorld>;

// One frame: clear, then every widget system in ascending priority order.
struct RenderRig {
    SceneVmWorld& world;
    SceneCanvas& canvas;
    OverlaySystem<SceneVmWorld, SceneCanvas> overlaySys;
    LabelSystem<SceneVmWorld, SceneCanvas> labelSys;
    IconSystem<SceneVmWorld, SceneCanvas> iconSys;
    ListSystem<SceneVmWorld, SceneCanvas> listSys;
    GaugeSystem<SceneVmWorld, SceneCanvas> gaugeSys;
    PopUpSystem<SceneVmWorld, SceneCanvas> popupSys;

    RenderRig(SceneVmWorld& w, SceneCanvas& c, const Theme& theme)
        : world(w), canvas(c), overlaySys(&w, &c), labelSys(&w, &c), iconSys(&w, &c),
          listSys(&w, &c, theme), gaugeSys(&w, &c), popupSys(&w, &c) {}

    void renderFrame(float dt) {
        canvas.clear(Pixel4(0));
        overlaySys.update(dt);
        labelSys.update(dt);
        iconSys.update(dt);
        listSys.update(dt);
        gaugeSys.update(dt);
        popupSys.update(dt);
    }
};

static int diffPixels(const SceneCanvas& a, const SceneCanvas& b) {
    int diff = 0;
    for (int16_t y = 0; y < 128; ++y)
        for (int16_t x = 0; x < 128; ++x)
            if (a.getPixel(x, y).value != b.getPixel(x, y).value) ++diff;
    return diff;
}

static JsonValue parse(const char* text) {
    JsonValue v;
    parseJson(std::string(text), v);
    return v;
}

// The paired scene copies under test, driven in lockstep.
struct Pair {
    SceneDoc docA, docB;
    SceneVmWorld worldA, worldB;
    AssetRegistry assets;
    std::string dumpA;

    bool load() {
        if (!readSceneDocJson(kDatumManagerScene, docA, worldA, assets)) return false;
        dumpA = writeSceneDocJson(docA, worldA, assets);
        return readSceneDocJson(dumpA, docB, worldB, assets);
    }
};

static void test_wholesale_fixed_point() {
    static Pair p; // static: two Worlds are large for the stack
    ASSERT(p.load(), "doc: fixture loads, dump reloads");
    const std::string dumpB = writeSceneDocJson(p.docB, p.worldB, p.assets);
    ASSERT(p.dumpA == dumpB, "doc: EXIT CRITERION - dump -> reload -> dump is byte-identical");

    ASSERT(p.docB.scene == "datum_manager", "doc: scene name survives");
    ASSERT(p.docB.state.type == JsonValue::Type::Object &&
               p.docB.state.find("previewInFlight") != nullptr,
           "doc: state bag survives");
    ASSERT(p.docB.timers.find("previewDebounce") != nullptr, "doc: timers survive");
    ASSERT(p.docB.animations.find("enter") != nullptr, "doc: animation tracks survive");
    ASSERT(p.docB.on.find("host.listArrived") != nullptr, "doc: event tables survive");
    ASSERT(p.dumpA.find("\"version\"") != std::string::npos, "doc: dump carries a version");

    // The cpp: slot property bag rides through untouched.
    bool bagOk = false;
    const auto& slots = p.worldB.components<SlotComponent>();
    for (size_t i = 0; i < slots.size(); ++i) {
        const SlotComponent* s = p.worldB.get<SlotComponent>(slots.entityAt(i));
        if (s && s->slot == "DatumBandsPreview") {
            const JsonValue* bands = s->props.find("bands");
            bagOk = bands && bands->number == 20.0;
        }
    }
    ASSERT(bagOk, "doc: slot property bag survives verbatim");
}

static void test_reloaded_scene_behaves_identically() {
    static Pair p;
    ASSERT(p.load(), "parity: pair loads");

    VM vmA(&p.docA, &p.worldA, &p.assets);
    VM vmB(&p.docB, &p.worldB, &p.assets);

    static SceneCanvas canvasA, canvasB;
    const Theme theme = kDefaultTheme;
    RenderRig rigA(p.worldA, canvasA, theme);
    RenderRig rigB(p.worldB, canvasB, theme);

    constexpr float kDt = 16.0f / 1000.0f;
    int frame = 0;
    bool identical = true;
    auto step = [&](int frames) {
        for (int i = 0; i < frames; ++i) {
            vmA.tick(16.0);
            vmB.tick(16.0);
            rigA.renderFrame(kDt);
            rigB.renderFrame(kDt);
            const int diff = diffPixels(canvasA, canvasB);
            if (diff != 0) {
                fprintf(stderr, "frame %d: %d pixels differ\n", frame, diff);
                identical = false;
            }
            ++frame;
        }
    };
    auto both = [&](const char* event, const JsonValue* payload = nullptr) {
        vmA.dispatch(event, payload);
        vmB.dispatch(event, payload);
    };

    // The datum-manager drive: enter anim, retry, list arrival, scrolling,
    // debounce expiry, load, status dismiss — behavior exercised end to end.
    both("scene.activate");
    step(20); // enter animation + one listRetry period
    JsonValue payload = parse(
        R"({"names": ["OddEven", "SineMove", "Pulsar", "Mirror", "Comb"], "loadedIndex": 2})");
    both("host.listArrived", &payload);
    step(10);
    both("input.encoder.cw");
    step(25); // crosses the 400ms debounce -> preview.request + latch
    both("input.tap.select");
    both("host.loadResult.ok");
    step(70); // crosses the 1000ms statusDismiss

    ASSERT(identical, "parity: EXIT CRITERION - every frame byte-identical, behavior exercised");
    ASSERT(frame == 125, "parity: drive covered 2 seconds of virtual time");

    // The drive must have actually done something visible.
    int ink = 0;
    for (int16_t y = 0; y < 128; ++y)
        for (int16_t x = 0; x < 128; ++x)
            if (canvasA.getPixel(x, y).value != 0) ++ink;
    ASSERT(ink > 200, "parity: frames carry real scene ink");
    ASSERT(vmA.lookup("statusLabel.text").str.empty(), "parity: dismiss timer ran");
}

int main() {
    test_wholesale_fixed_point();
    test_reloaded_scene_behaves_identically();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
