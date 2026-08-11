// Reflection layer unit test (unwn #182, M1 / G1+G2): stable component
// identity + field-list-driven property visitation over the 6 ECS widgets
// plus Position/Size/Theme, the AssetRegistry name<->pointer table, and the
// hand-rolled JSON writer / tolerant reader underneath scene serialization.
//
// The identity table is pinned verbatim: these IDs and names are written into
// scene files, so a change here is a file-format break, not a refactor.
#include <enjin2/ui/asset_registry.hpp>
#include <enjin2/ui/components.hpp>
#include <enjin2/ui/json.hpp>
#include <enjin2/ui/reflect.hpp>
#include <enjin2/ui/scene_json.hpp>
#include <enjin2/ui/theme.hpp>
#include <enjin2/ui/widgets/gauge.hpp>
#include <enjin2/ui/widgets/icon.hpp>
#include <enjin2/ui/widgets/label.hpp>
#include <enjin2/ui/widgets/list.hpp>
#include <enjin2/ui/widgets/overlay.hpp>
#include <enjin2/ui/widgets/popup.hpp>
#include <enjin2/ui/world.hpp>
#include <enjin2/graphics/defaultfont.hpp>
#include <cstdio>
#include <cstring>
#include <string>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { printf("PASS: %s\n", msg); passes++; }                         \
    } while (0)

// ----- G1: stable identity ---------------------------------------------------

// The persistent ID/name table, pinned. Never renumber, never reuse.
static void test_stable_ids_and_names() {
    ASSERT(ComponentTraits<PositionComponent>::kTypeId == 1 &&
               strcmp(ComponentTraits<PositionComponent>::kName, "position") == 0,
           "identity: position = 1");
    ASSERT(ComponentTraits<SizeComponent>::kTypeId == 2 &&
               strcmp(ComponentTraits<SizeComponent>::kName, "size") == 0,
           "identity: size = 2");
    ASSERT(ComponentTraits<Theme>::kTypeId == 3 &&
               strcmp(ComponentTraits<Theme>::kName, "theme") == 0,
           "identity: theme = 3");
    ASSERT(ComponentTraits<LabelComponent>::kTypeId == 4 &&
               strcmp(ComponentTraits<LabelComponent>::kName, "label") == 0,
           "identity: label = 4");
    ASSERT(ComponentTraits<IconComponent>::kTypeId == 5 &&
               strcmp(ComponentTraits<IconComponent>::kName, "icon") == 0,
           "identity: icon = 5");
    ASSERT(ComponentTraits<GaugeComponent>::kTypeId == 6 &&
               strcmp(ComponentTraits<GaugeComponent>::kName, "gauge") == 0,
           "identity: gauge = 6");
    ASSERT(ComponentTraits<OverlayComponent>::kTypeId == 7 &&
               strcmp(ComponentTraits<OverlayComponent>::kName, "overlay") == 0,
           "identity: overlay = 7");
    ASSERT(ComponentTraits<PopUpComponent>::kTypeId == 8 &&
               strcmp(ComponentTraits<PopUpComponent>::kName, "popup") == 0,
           "identity: popup = 8");
    ASSERT(ComponentTraits<ListComponent>::kTypeId == 9 &&
               strcmp(ComponentTraits<ListComponent>::kName, "list") == 0,
           "identity: list = 9");

    // Uniqueness across the whole reflected set, both axes.
    const ComponentTypeId ids[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    const char* names[] = {"position", "size", "theme", "label", "icon",
                           "gauge", "overlay", "popup", "list"};
    bool unique = true;
    for (int i = 0; i < 9; ++i)
        for (int j = i + 1; j < 9; ++j)
            if (ids[i] == ids[j] || strcmp(names[i], names[j]) == 0) unique = false;
    ASSERT(unique, "identity: ids and names are pairwise distinct");

    ASSERT(IsReflected<LabelComponent>::value, "identity: label is reflected");
    ASSERT(!IsReflected<RenderComponent>::value, "identity: RenderComponent is not reflected");
}

// ----- G2: field visitation --------------------------------------------------

template<typename T>
static int countFields(const T& c) {
    int n = 0;
    ComponentTraits<T>::visitFields(c, [&](const char*, auto) { ++n; });
    return n;
}

// One field-list entry per serializable property; counts pin the surface.
static void test_field_counts() {
    ASSERT(countFields(PositionComponent{}) == 1, "fields: position has 1 (lastPos transient)");
    ASSERT(countFields(SizeComponent{}) == 3, "fields: size has 3");
    ASSERT(countFields(Theme{}) == 10, "fields: theme has 10");
    ASSERT(countFields(LabelComponent{}) == 6, "fields: label has 6");
    ASSERT(countFields(IconComponent{}) == 5, "fields: icon has 5");
    ASSERT(countFields(GaugeComponent{}) == 4, "fields: gauge has 4");
    ASSERT(countFields(OverlayComponent{}) == 2, "fields: overlay has 2");
    ASSERT(countFields(PopUpComponent{}) == 10, "fields: popup has 10");
    ASSERT(countFields(ListComponent{}) == 10, "fields: list has 10");
}

// Mutable visitation sets by name; prop entries route through the setter and
// keep the component's invariants (a gauge's value clamps to its mode).
static void test_set_by_name() {
    LabelComponent label;
    ComponentTraits<LabelComponent>::visitFields(label, [&](const char* name, auto acc) {
        using ValueT = typename decltype(acc)::value_type;
        if constexpr (std::is_same_v<ValueT, std::string>) {
            if (strcmp(name, "text") == 0) acc.set("HELLO");
        }
    });
    ASSERT(label.text == "HELLO", "visit: field set-by-name writes the member");

    GaugeComponent gauge(56, Pixel4(13), GaugeMode::Unidirectional);
    ComponentTraits<GaugeComponent>::visitFields(gauge, [&](const char* name, auto acc) {
        using ValueT = typename decltype(acc)::value_type;
        if constexpr (std::is_same_v<ValueT, float>) {
            if (strcmp(name, "value") == 0) acc.set(7.0f);
        }
    });
    ASSERT(gauge.value() == 1.0f, "visit: prop set routes through setValue and clamps");

    float readBack = -1.0f;
    const GaugeComponent& constGauge = gauge;
    ComponentTraits<GaugeComponent>::visitFields(constGauge, [&](const char* name, auto acc) {
        using ValueT = typename decltype(acc)::value_type;
        if constexpr (std::is_same_v<ValueT, float>) {
            if (strcmp(name, "value") == 0) readBack = acc.get();
        }
    });
    ASSERT(readBack == 1.0f, "visit: const visitation reads through the getter");
}

// ----- Asset registry --------------------------------------------------------

static const uint8_t kBitmap[4] = {1, 2, 3, 4};

static void test_asset_registry() {
    AssetRegistry assets;
    ASSERT(assets.registerFont("default8", &defaultFont8pt7b), "assets: font registers");
    ASSERT(!assets.registerFont("default8", &defaultFont8pt7b), "assets: duplicate name rejected");
    ASSERT(assets.findFont("default8") == &defaultFont8pt7b, "assets: font resolves by name");
    ASSERT(assets.findFont("nope") == nullptr, "assets: unknown font name is nullptr");
    ASSERT(strcmp(assets.fontName(&defaultFont8pt7b), "default8") == 0,
           "assets: font reverse lookup");
    ASSERT(assets.fontName(nullptr) == nullptr, "assets: unregistered font pointer is nullptr");

    ASSERT(assets.registerBitmap("glyph", kBitmap), "assets: bitmap registers");
    ASSERT(assets.findBitmap("glyph") == kBitmap, "assets: bitmap resolves by name");
    ASSERT(strcmp(assets.bitmapName(kBitmap), "glyph") == 0, "assets: bitmap reverse lookup");

    ASSERT(assets.fontCount() == 1 && assets.bitmapCount() == 1 &&
               strcmp(assets.fontAt(0).name, "default8") == 0 &&
               strcmp(assets.bitmapAt(0).name, "glyph") == 0,
           "assets: compiled-in set is enumerable");
}

// ----- JSON writer / parser --------------------------------------------------

static void test_json_writer_parser() {
    JsonWriter w;
    w.beginObject();
    w.key("s");
    w.value("a\"b\\c\nd");
    w.key("n");
    w.value(int64_t(-42));
    w.key("f");
    w.value(0.25f);
    w.key("b");
    w.value(true);
    w.key("arr");
    w.beginArray();
    w.value(int64_t(1));
    w.value(int64_t(2));
    w.endArray();
    w.key("nested");
    w.beginObject();
    w.key("x");
    w.null();
    w.endObject();
    w.endObject();

    JsonValue root;
    ASSERT(parseJson(w.str(), root), "json: writer output parses");
    ASSERT(root.find("s") && root.find("s")->str == "a\"b\\c\nd",
           "json: string escapes round-trip");
    ASSERT(root.find("n") && root.find("n")->number == -42.0, "json: integer round-trips");
    ASSERT(root.find("f") && static_cast<float>(root.find("f")->number) == 0.25f,
           "json: float round-trips");
    ASSERT(root.find("b") && root.find("b")->boolean, "json: bool round-trips");
    ASSERT(root.find("arr") && root.find("arr")->array.size() == 2, "json: array round-trips");
    ASSERT(root.find("nested") && root.find("nested")->find("x") &&
               root.find("nested")->find("x")->type == JsonValue::Type::Null,
           "json: nested object + null round-trip");

    JsonValue bad;
    ASSERT(!parseJson(std::string("{\"unterminated\": "), bad), "json: truncated input fails");
    ASSERT(!parseJson(std::string("{} trailing"), bad), "json: trailing garbage fails");

    JsonValue esc;
    ASSERT(parseJson(std::string("\"\\u0041\\t\""), esc) && esc.str == "A\t",
           "json: \\uXXXX escapes decode");
}

// ----- Component JSON: tolerant reader --------------------------------------

static void test_component_roundtrip_and_tolerance() {
    AssetRegistry assets;
    assets.registerFont("default8", &defaultFont8pt7b);

    LabelComponent out("SAT");
    out.setFont(&defaultFont8pt7b);
    out.setColor(Pixel4(12));
    out.pointer = 5;

    JsonWriter w;
    writeComponentJson(w, out, assets);
    JsonValue obj;
    ASSERT(parseJson(w.str(), obj), "component: dump parses");

    LabelComponent in;
    readComponentJson(obj, in, assets);
    ASSERT(in.text == "SAT" && in.font == &defaultFont8pt7b && in.color.value == 12 &&
               in.pointer == 5 && in.fontSize == 1,
           "component: label round-trips including font ref");

    // Bidirectional gauge with a negative value: field-list order (mode before
    // value) must hold or the value clamps to 0 on load.
    GaugeComponent gaugeOut(56, Pixel4(13), GaugeMode::Bidirectional);
    gaugeOut.setValue(-0.4f);
    JsonWriter gw;
    writeComponentJson(gw, gaugeOut, assets);
    JsonValue gobj;
    parseJson(gw.str(), gobj);
    GaugeComponent gaugeIn;
    readComponentJson(gobj, gaugeIn, assets);
    ASSERT(gaugeIn.mode == GaugeMode::Bidirectional && gaugeIn.value() == gaugeOut.value(),
           "component: gauge mode applies before value (negative survives)");

    // Tolerance: unknown fields skipped, wrong types skipped, missing = default.
    JsonValue tol;
    parseJson(std::string("{\"text\": \"OK\", \"mystery\": 9, \"fontSize\": \"huge\"}"), tol);
    LabelComponent tolerant;
    readComponentJson(tol, tolerant, assets);
    ASSERT(tolerant.text == "OK" && tolerant.fontSize == 1 && tolerant.color.value == 14,
           "component: unknown/wrong-typed/missing fields keep defaults");

    // A dangling asset name resolves to the widget's no-asset state.
    JsonValue dangling;
    parseJson(std::string("{\"font\": \"not-compiled-in\"}"), dangling);
    LabelComponent noAsset;
    noAsset.setFont(&defaultFont8pt7b);
    readComponentJson(dangling, noAsset, assets);
    ASSERT(noAsset.font == nullptr, "component: dangling font name resolves to nullptr");
}

// ----- Scene JSON: structure + tolerance ------------------------------------

using ReflectWorld = World<8, PositionComponent, SizeComponent, LabelComponent, GaugeComponent>;

static void test_scene_structure_and_tolerance() {
    AssetRegistry assets;

    ReflectWorld world;
    const Entity e = world.create();
    world.add<PositionComponent>(e, Point(3, 4));
    world.add<LabelComponent>(e, std::string("EISEI"));

    const std::string dump = writeSceneJson(world, assets, &kDefaultTheme);
    JsonValue root;
    ASSERT(parseJson(dump, root), "scene: dump parses");
    ASSERT(root.find("version") &&
               root.find("version")->number == static_cast<double>(kSceneJsonVersion),
           "scene: version field present");
    ASSERT(root.find("theme") && root.find("theme")->find("background"),
           "scene: root theme object present");
    ASSERT(root.find("entities") && root.find("entities")->array.size() == 1,
           "scene: one entity dumped");

    // Unknown component names skip; known ones load; theme reads back.
    const std::string doc =
        "{\"version\": 99, \"theme\": {\"accent\": 5},"
        " \"entities\": ["
        "  {\"components\": {\"martian\": {\"x\": 1}, \"label\": {\"text\": \"OK\"}}},"
        "  {\"components\": {\"position\": {\"position\": {\"x\": 7, \"y\": 8}}}}"
        " ]}";
    ReflectWorld loaded;
    Theme theme = kDefaultTheme;
    ASSERT(readSceneJson(doc, loaded, assets, &theme), "scene: tolerant doc loads");
    ASSERT(loaded.entityCount() == 2, "scene: both entities created");
    ASSERT(theme.accent.value == 5 && theme.foreground.value == 15,
           "scene: theme fields merge over defaults");

    bool foundLabel = false;
    bool foundPos = false;
    const auto& labels = loaded.components<LabelComponent>();
    for (size_t i = 0; i < labels.size(); ++i)
        if (labels.componentAt(i).text == "OK") foundLabel = true;
    const auto& positions = loaded.components<PositionComponent>();
    for (size_t i = 0; i < positions.size(); ++i)
        if (positions.componentAt(i).position.x == 7) foundPos = true;
    ASSERT(foundLabel && foundPos, "scene: known components load, unknown skipped");

    ASSERT(!readSceneJson(std::string("not json"), loaded, assets), "scene: malformed fails");
}

int main() {
    test_stable_ids_and_names();
    test_field_counts();
    test_set_by_name();
    test_asset_registry();
    test_json_writer_parser();
    test_component_roundtrip_and_tolerance();
    test_scene_structure_and_tolerance();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
