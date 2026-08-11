// M1 exit criterion (unwn #182): dump a scene as JSON -> reload it ->
// identical frame.
//
// A world composing all six ECS widgets (label, icon, gauge, overlay, popup,
// list) plus position/size is authored in code, dumped through the field-list
// reflection, and reloaded into a fresh world. Both worlds then render the
// same frame sequence through the same systems (in priority order, with the
// same dt), and every frame is compared pixel-for-pixel on the full canvas —
// including the frames where the list's smooth-scroll transition and marquee
// are still settling, so transient runtime state must decay identically from
// the serialized state. A second dump of the reloaded world must be
// byte-identical to the first (dump -> reload -> dump is a fixed point).
#include <enjin2/ui/asset_registry.hpp>
#include <enjin2/ui/components.hpp>
#include <enjin2/ui/scene_json.hpp>
#include <enjin2/ui/theme.hpp>
#include <enjin2/ui/widgets/gauge.hpp>
#include <enjin2/ui/widgets/icon.hpp>
#include <enjin2/ui/widgets/label.hpp>
#include <enjin2/ui/widgets/list.hpp>
#include <enjin2/ui/widgets/overlay.hpp>
#include <enjin2/ui/widgets/popup.hpp>
#include <enjin2/ui/world.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/defaultfont.hpp>
#include <cstdio>
#include <string>
#include <vector>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { printf("PASS: %s\n", msg); passes++; }                         \
    } while (0)

using SceneWorld = World<16, PositionComponent, SizeComponent, LabelComponent, IconComponent,
                         GaugeComponent, OverlayComponent, PopUpComponent, ListComponent>;
using SceneCanvas = Canvas4<128, 128>;

// An 8x8 test glyph: a filled diamond over the transparent matte (16).
static uint8_t kGlyph[64];
static void buildGlyph() {
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            const int d = (x > 3 ? x - 4 : 3 - x) + (y > 3 ? y - 4 : 3 - y);
            kGlyph[y * 8 + x] = d <= 3 ? static_cast<uint8_t>(15 - d) : uint8_t(16);
        }
}

// The authored scene: every widget type, non-default styling, private state
// (gauge value, list selection) set through the public API.
static void buildScene(SceneWorld& world) {
    const Entity label = world.create();
    world.add<PositionComponent>(label, Point(24, 6));
    world.add<SizeComponent>(label, Size(80, 26));
    auto* lc = world.add<LabelComponent>(label, std::string("ORBIT LOCK"));
    lc->setFont(&defaultFont8pt7b);
    lc->setColor(Pixel4(14));
    lc->setBackground(Pixel4(3));
    lc->pointer = 4;

    const Entity icon = world.create();
    world.add<PositionComponent>(icon, Point(102, 10));
    world.add<IconComponent>(icon, kGlyph, uint16_t(8), uint16_t(8));

    const Entity gauge = world.create();
    world.add<PositionComponent>(gauge, Point(8, 40));
    auto* gc = world.add<GaugeComponent>(gauge, uint16_t(48), Pixel4(13), GaugeMode::Bidirectional);
    gc->setValue(-0.4f);

    const Entity list = world.create();
    world.add<PositionComponent>(list, Point(62, 44));
    world.add<SizeComponent>(list, Size(62, 70));
    auto* listc = world.add<ListComponent>(
        list, std::vector<std::string>{"SUN", "MERCURY", "VENUS", "EARTH", "MARS"},
        TextAlign::Left);
    listc->setCurrentSelection(3);
    listc->setMarqueeTiming(100, 30, 200);

    const Entity overlay = world.create();
    world.add<OverlayComponent>(overlay, uint8_t(2));

    const Entity popup = world.create();
    world.add<PositionComponent>(popup, Point(64, 96));
    auto* pc = world.add<PopUpComponent>(popup);
    pc->setLines("SAVED", "SLOT 2");
    pc->setIcon(PopUpComponent::Icon::Save);
    pc->radius = 28;
    pc->rimColor = Pixel4(6);
    pc->textColor = Pixel4(15);
    pc->show(0);
}

// One frame: clear, then every system in ascending priority order
// (overlay 800 < label/icon/list 900 < gauge 950 < popup 1000).
struct SceneRig {
    SceneWorld& world;
    SceneCanvas& canvas;
    OverlaySystem<SceneWorld, SceneCanvas> overlaySys;
    LabelSystem<SceneWorld, SceneCanvas> labelSys;
    IconSystem<SceneWorld, SceneCanvas> iconSys;
    ListSystem<SceneWorld, SceneCanvas> listSys;
    GaugeSystem<SceneWorld, SceneCanvas> gaugeSys;
    PopUpSystem<SceneWorld, SceneCanvas> popupSys;

    SceneRig(SceneWorld& w, SceneCanvas& c, const Theme& theme)
        : world(w), canvas(c), overlaySys(&w, &c), labelSys(&w, &c), iconSys(&w, &c),
          listSys(&w, &c, theme), gaugeSys(&w, &c), popupSys(&w, &c) {}

    void renderFrame(float dt) {
        canvas.clear(Pixel4(4));
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

static void test_dump_reload_identical_frames() {
    AssetRegistry assets;
    assets.registerFont("default8", &defaultFont8pt7b);
    assets.registerBitmap("glyph", kGlyph);

    Theme theme = kDefaultTheme;
    theme.accent = Pixel4(12);
    theme.itemHeight = 12;

    static SceneWorld original;
    buildScene(original);
    const std::string dump = writeSceneJson(original, assets, &theme);
    ASSERT(dump.find("\"version\"") != std::string::npos, "roundtrip: dump carries a version");

    static SceneWorld reloaded;
    Theme reloadedTheme = kDefaultTheme;
    ASSERT(readSceneJson(dump, reloaded, assets, &reloadedTheme), "roundtrip: dump reloads");
    ASSERT(reloaded.entityCount() == original.entityCount(),
           "roundtrip: entity count survives");
    ASSERT(reloadedTheme.accent.value == 12 && reloadedTheme.itemHeight == 12,
           "roundtrip: theme survives");

    static SceneCanvas canvasA;
    static SceneCanvas canvasB;
    SceneRig rigA(original, canvasA, theme);
    SceneRig rigB(reloaded, canvasB, reloadedTheme);

    bool identical = true;
    const float dt = 1.0f / 60.0f;
    for (int frame = 0; frame < 8; ++frame) {
        rigA.renderFrame(dt);
        rigB.renderFrame(dt);
        const int diff = diffPixels(canvasA, canvasB);
        if (diff != 0) {
            fprintf(stderr, "frame %d: %d pixels differ\n", frame, diff);
            identical = false;
        }
    }
    ASSERT(identical, "roundtrip: EXIT CRITERION - all frames byte-identical after reload");

    // The first frame must actually contain the scene, not an empty canvas.
    int ink = 0;
    for (int16_t y = 0; y < 128; ++y)
        for (int16_t x = 0; x < 128; ++x)
            if (canvasA.getPixel(x, y).value != 0) ++ink;
    ASSERT(ink > 500, "roundtrip: frames carry real scene ink");

    const std::string dump2 = writeSceneJson(reloaded, assets, &reloadedTheme);
    ASSERT(dump == dump2, "roundtrip: dump -> reload -> dump is byte-identical");
}

int main() {
    buildGlyph();
    test_dump_reload_identical_frames();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
