// SceneVM interpreter (unwn #183, M2): the ratified scene_vm.py semantics in C++.
//
// The fixture is the DatumManagerScene behavior port — the firmware's most
// behavior-heavy scene, the one the prototype was ratified against. The five
// "hard on paper" drives from the prototype README are asserted end to end:
//
//   1. activate -> list arrives -> scroll -> debounce fires -> preview.request
//      emits and previewInFlight latches
//   2. scroll while in-flight -> exactly one preview.cancel, debounce re-arms
//   3. tap mid-preview -> cancel + datum.load, then guards visibly block
//      while isLoading
//   4. load succeeds -> "Loaded!" + dismiss timer + ui.exitScene effect;
//      deactivate + re-activate -> no stale state leaks
//   5. deactivate while a preview is in flight -> cancel emitted on the way out
//
// Plus the M2 additions on top of the prototype: bindings writing into `cpp:`
// slot property bags, and animation tracks landing on real reflected fields.
#include <enjin2/ui/scene_json.hpp>
#include <enjin2/ui/scene_vm.hpp>

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

using VM = SceneVM<SceneVmWorld>;

static int countHost(const SceneEffects& fx, const char* name) {
    int n = 0;
    for (const SceneEffect& e : fx)
        if (e.kind == SceneEffect::Kind::Host && e.name == name) ++n;
    return n;
}

static const SceneEffect* findHost(const SceneEffects& fx, const char* name) {
    for (const SceneEffect& e : fx)
        if (e.kind == SceneEffect::Kind::Host && e.name == name) return &e;
    return nullptr;
}

static JsonValue parse(const char* text) {
    JsonValue v;
    parseJson(std::string(text), v);
    return v;
}

// The canned host reply: 5 presets, the loaded one at index 2.
static JsonValue listPayload() {
    return parse(R"({"names": ["OddEven", "SineMove", "Pulsar", "Mirror", "Comb"],
                     "loadedIndex": 2})");
}

static void test_activation_and_list_retry() {
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    ASSERT(readSceneDocJson(kDatumManagerScene, doc, world, assets),
           "vm: fixture document loads");
    VM vm(&doc, &world, &assets);

    SceneEffects fx = vm.dispatch("scene.activate");
    ASSERT(countHost(fx, "datum.list.request") == 1, "activate: requests the list");
    ASSERT(vm.lookup("statusLabel.text").str == "Requesting list...",
           "activate: status label set through entity.prop path");
    ASSERT(vm.timerRemaining("listRetry") == 500.0, "activate: listRetry armed");
    ASSERT(vm.animPlaying("enter"), "activate: enter animation playing");
    ASSERT(vm.lookup("dimmer.opacity").number == 15.0,
           "activate: anim play snaps tracks to their from values");

    // Retry fires while the list never arrives — and keeps repeating.
    fx = vm.tick(500);
    ASSERT(countHost(fx, "datum.list.request") == 1, "retry: re-requests after 500ms");
    fx = vm.tick(500);
    ASSERT(countHost(fx, "datum.list.request") == 1, "retry: repeats (repeat: true)");

    // Animation has long finished: opacity eased 15 -> 0, gauge -1 -> 0.4.
    ASSERT(!vm.animPlaying("enter"), "anim: enter finished after 1000ms");
    ASSERT(vm.lookup("dimmer.opacity").number == 0.0, "anim: overlay opacity landed on to");
    const double v = vm.lookup("meter.value").number;
    ASSERT(v > 0.39 && v < 0.41, "anim: gauge value landed on to (float field)");
}

static void test_hard_case_1_debounce_latch() {
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(kDatumManagerScene, doc, world, assets);
    VM vm(&doc, &world, &assets);

    vm.dispatch("scene.activate");
    JsonValue payload = listPayload();
    SceneEffects fx = vm.dispatch("host.listArrived", &payload);
    ASSERT(vm.lookup("presetList.items").array.size() == 5,
           "listArrived: items set through the widget verb");
    ASSERT(vm.lookup("presetList.selectedIndex").number == 2.0,
           "listArrived: selection jumped to the loaded entry");
    ASSERT(vm.lookup("listReady").truthy(), "listArrived: listReady latched");
    ASSERT(vm.timerRemaining("listRetry") < 0.0, "listArrived: retry cancelled");
    ASSERT(vm.timerRemaining("previewDebounce") == 400.0, "listArrived: debounce armed");

    fx = vm.tick(160);
    ASSERT(countHost(fx, "datum.preview.request") == 0, "debounce: silent before expiry");
    vm.dispatch("input.encoder.cw"); // scroll re-arms the debounce
    ASSERT(vm.lookup("presetList.selectedIndex").number == 3.0, "scroll: cursor moved down");
    ASSERT(vm.timerRemaining("previewDebounce") == 400.0, "scroll: debounce re-armed in full");

    fx = vm.tick(400);
    const SceneEffect* req = findHost(fx, "datum.preview.request");
    ASSERT(req != nullptr, "debounce: preview.request fires at expiry");
    ASSERT(req && req->args.find("selection") && req->args.find("selection")->number == 3.0,
           "debounce: @presetList.selectedIndex resolved into args");
    ASSERT(vm.lookup("previewInFlight").truthy(), "debounce: previewInFlight latched");
}

static void test_hard_case_2_scroll_cancels_in_flight() {
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(kDatumManagerScene, doc, world, assets);
    VM vm(&doc, &world, &assets);

    vm.dispatch("scene.activate");
    JsonValue payload = listPayload();
    vm.dispatch("host.listArrived", &payload);
    vm.tick(400); // debounce fires, preview in flight

    SceneEffects fx = vm.dispatch("input.encoder.ccw");
    ASSERT(countHost(fx, "datum.preview.cancel") == 1,
           "in-flight scroll: exactly one preview.cancel");
    ASSERT(!vm.lookup("previewInFlight").truthy(), "in-flight scroll: latch released");
    ASSERT(vm.timerRemaining("previewDebounce") == 400.0, "in-flight scroll: debounce re-arms");

    fx = vm.dispatch("input.encoder.ccw");
    ASSERT(countHost(fx, "datum.preview.cancel") == 0,
           "second scroll: no cancel when nothing is in flight");
}

static void test_hard_case_3_tap_loads_and_guards_block() {
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(kDatumManagerScene, doc, world, assets);
    VM vm(&doc, &world, &assets);

    vm.dispatch("scene.activate");
    JsonValue payload = listPayload();
    vm.dispatch("host.listArrived", &payload);
    vm.tick(400); // in flight

    SceneEffects fx = vm.dispatch("input.tap.select");
    ASSERT(countHost(fx, "datum.preview.cancel") == 1, "tap mid-preview: cancels first");
    const SceneEffect* load = findHost(fx, "datum.load");
    ASSERT(load != nullptr, "tap: datum.load emitted");
    ASSERT(load && load->args.find("slot") && load->args.find("slot")->number == 2.0,
           "tap: @state.currentSlot resolved into args");
    ASSERT(vm.lookup("isLoading").truthy(), "tap: isLoading latched");
    ASSERT(vm.timerRemaining("previewDebounce") < 0.0, "tap: debounce cancelled");

    // Guards must now visibly block scrolling and re-tapping.
    fx = vm.dispatch("input.encoder.cw");
    ASSERT(fx.empty() && vm.lookup("presetList.selectedIndex").number == 2.0,
           "guard: scroll blocked while isLoading");
    fx = vm.dispatch("input.tap.select");
    ASSERT(countHost(fx, "datum.load") == 0, "guard: re-tap blocked while isLoading");
}

static void test_hard_case_4_load_ok_and_reactivation_reset() {
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(kDatumManagerScene, doc, world, assets);
    VM vm(&doc, &world, &assets);

    vm.dispatch("scene.activate");
    JsonValue payload = listPayload();
    vm.dispatch("host.listArrived", &payload);
    vm.tick(400);
    vm.dispatch("input.tap.select");

    SceneEffects fx = vm.dispatch("host.loadResult.ok");
    ASSERT(vm.lookup("statusLabel.text").str == "Loaded!", "load ok: status set");
    ASSERT(vm.timerRemaining("statusDismiss") == 1000.0, "load ok: dismiss timer armed");
    ASSERT(countHost(fx, "ui.exitScene") == 1, "load ok: explicit exit effect emitted");
    ASSERT(vm.lookup("showMiniPreview").truthy(), "load ok: mini preview shown");
    ASSERT(vm.lookup("previewBands.visible").truthy(),
           "binding: showMiniPreview reached the slot bag");

    fx = vm.tick(1000);
    ASSERT(vm.lookup("statusLabel.text").str.empty(), "dismiss: status cleared after 1s");

    // Deactivate, re-activate: the C++ scene needed manual resets for exactly this.
    vm.dispatch("scene.deactivate");
    vm.dispatch("scene.activate");
    ASSERT(!vm.lookup("isLoading").truthy(), "re-activate: isLoading reset");
    ASSERT(!vm.lookup("previewInFlight").truthy(), "re-activate: previewInFlight reset");
    ASSERT(!vm.lookup("showMiniPreview").truthy(), "re-activate: mini preview reset");
    ASSERT(!vm.lookup("previewBands.visible").truthy(),
           "re-activate: slot bag binding followed the reset");
}

static void test_hard_case_5_deactivate_mid_flight() {
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(kDatumManagerScene, doc, world, assets);
    VM vm(&doc, &world, &assets);

    vm.dispatch("scene.activate");
    JsonValue payload = listPayload();
    vm.dispatch("host.listArrived", &payload);
    vm.tick(400); // in flight

    SceneEffects fx = vm.dispatch("scene.deactivate");
    ASSERT(countHost(fx, "datum.preview.cancel") == 1,
           "deactivate mid-flight: cancel emitted on the way out");
    ASSERT(vm.timerRemaining("previewDebounce") < 0.0, "deactivate: debounce cancelled");
    ASSERT(vm.timerRemaining("listRetry") < 0.0, "deactivate: retry cancelled");
}

static void test_progress_binding_and_scene_switch() {
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(kDatumManagerScene, doc, world, assets);
    VM vm(&doc, &world, &assets);

    vm.dispatch("scene.activate");
    JsonValue progress = parse(R"({"progress": 0.62})");
    vm.dispatch("host.preview.progress", &progress);
    ASSERT(vm.lookup("progressBar.visible").truthy(),
           "binding: showProgressBar reached the slot bag");
    ASSERT(vm.lookup("progressBar.progress").number == 0.62,
           "binding: previewProgress copied into the slot bag via @event payload");

    SceneEffects fx = vm.dispatch("input.tap.set");
    bool sawSwitch = false;
    for (const SceneEffect& e : fx)
        if (e.kind == SceneEffect::Kind::SceneSwitch && e.name == "control") sawSwitch = true;
    ASSERT(sawSwitch, "sceneSwitch: back tap emits the switch effect");
}

// A stub `param.` resolver: echoes the key (and format, when given) so a test
// can prove exactly what the VM handed the app — key stripped of `param.`, plus
// the binding's format override. Mirrors the app resolver's String return.
static JsonValue stubParamResolver(const std::string& key, const std::string& format) {
    JsonValue v;
    v.type = JsonValue::Type::String;
    v.str = format.empty() ? key : (key + "/" + format);
    return v;
}

// A one-label scene: `label.text` bound to a `param.` source in each form.
static const char* const kParamBindScene = R"json({
  "version": 2,
  "scene": "param_bind",
  "entities": [
    { "components": {
        "id": { "id": "readout" },
        "label": { "text": "init" },
        "bindings": { "bindings": { "text": %BIND% } } } }
  ]
})json";

static std::string paramBindDoc(const char* bindJson) {
    std::string s = kParamBindScene;
    s.replace(s.find("%BIND%"), 6, bindJson);
    return s;
}

static void test_param_source_third_lookup() {
    // lookup() gains the `param.` source: with a resolver installed it reads the
    // live cell (here the stub), without one it stays tolerant (Null).
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(paramBindDoc(R"("param.daisy.key")"), doc, world, assets);

    VM bare(&doc, &world, &assets); // no resolver
    ASSERT(bare.lookup("param.daisy.key").type == JsonValue::Type::Null,
           "param: unset resolver reads Null (tolerant)");

    VM vm(&doc, &world, &assets, &stubParamResolver);
    ASSERT(vm.lookup("param.daisy.key").str == "daisy.key",
           "param: lookup strips the prefix and resolves through the source");
    ASSERT(vm.lookup("param.no.such").str == "no.such",
           "param: an unknown key still routes to the resolver (its miss to make)");
}

static void test_param_binding_shapes() {
    // Bare string form: `{ "text": "param.daisy.key" }` — no format override.
    {
        SceneDoc doc;
        SceneVmWorld world;
        AssetRegistry assets;
        readSceneDocJson(paramBindDoc(R"("param.daisy.key")"), doc, world, assets);
        VM vm(&doc, &world, &assets, &stubParamResolver);
        ASSERT(vm.lookup("readout.text").str == "daisy.key",
               "binding: bare-string param source applied to the label");
    }
    // Object form with a format override: `{ "from": ..., "format": ... }`.
    {
        SceneDoc doc;
        SceneVmWorld world;
        AssetRegistry assets;
        readSceneDocJson(paramBindDoc(R"({"from":"param.daisy.key","format":"note_cents"})"),
                         doc, world, assets);
        VM vm(&doc, &world, &assets, &stubParamResolver);
        ASSERT(vm.lookup("readout.text").str == "daisy.key/note_cents",
               "binding: {from, format} passes the override to the resolver");
    }
    // Object form without a format: equivalent to the bare string.
    {
        SceneDoc doc;
        SceneVmWorld world;
        AssetRegistry assets;
        readSceneDocJson(paramBindDoc(R"({"from":"param.daisy.key"})"), doc, world, assets);
        VM vm(&doc, &world, &assets, &stubParamResolver);
        ASSERT(vm.lookup("readout.text").str == "daisy.key",
               "binding: {from} with no format is the bare-string shorthand");
    }
    // A non-param `from` in object form still resolves through the normal path
    // (state var / entity.prop); the resolver is untouched.
    {
        SceneDoc doc;
        SceneVmWorld world;
        AssetRegistry assets;
        readSceneDocJson(paramBindDoc(R"({"from":"readout.text"})"), doc, world, assets);
        VM vm(&doc, &world, &assets, &stubParamResolver);
        ASSERT(vm.lookup("readout.text").str == "init",
               "binding: object form over a non-param source uses the value lookup");
    }
}

static void test_tolerance() {
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(kDatumManagerScene, doc, world, assets);
    VM vm(&doc, &world, &assets);

    SceneEffects fx = vm.dispatch("no.such.event");
    ASSERT(fx.empty(), "tolerant: unknown event is a no-op");
    ASSERT(vm.lookup("no.such.path").type == JsonValue::Type::Null,
           "tolerant: unknown path reads Null");
    ASSERT(!vm.lookup("").truthy(), "tolerant: empty guard atom is falsy");
}

int main() {
    test_activation_and_list_retry();
    test_hard_case_1_debounce_latch();
    test_hard_case_2_scroll_cancels_in_flight();
    test_hard_case_3_tap_loads_and_guards_block();
    test_hard_case_4_load_ok_and_reactivation_reset();
    test_hard_case_5_deactivate_mid_flight();
    test_progress_binding_and_scene_switch();
    test_param_source_third_lookup();
    test_param_binding_shapes();
    test_tolerance();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
