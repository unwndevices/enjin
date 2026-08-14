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
#include <enjin2/ui/widgets/sprite.hpp>

#include "scene_fixture.h"

#include <cmath>
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

// Stub `param.` seam (unwn #218), split like the real app resolver:
//   - resolve-raw echoes a {number,min,max} for *every* key (the stub owns no
//     table), so the format terminal always runs and the format echo is testable.
//   - the formatter echoes the key (and format override, when given) so a test
//     can prove exactly what the VM handed the app at the terminal.
static JsonValue stubParamRaw(const std::string& /*key*/) {
    JsonValue out;
    out.type = JsonValue::Type::Object;
    JsonValue n;
    n.type = JsonValue::Type::Number;
    n.number = 0.0;
    JsonValue lo;
    lo.type = JsonValue::Type::Number;
    lo.number = 0.0;
    JsonValue hi;
    hi.type = JsonValue::Type::Number;
    hi.number = 1.0;
    out.object.emplace_back("number", n);
    out.object.emplace_back("min", lo);
    out.object.emplace_back("max", hi);
    return out;
}
static std::string stubParamFormat(const std::string& key, const std::string& format, double) {
    return format.empty() ? key : (key + "/" + format);
}

// A resolve-raw that misses every key (Null) — the real app's tolerant miss for
// an unknown `param.` id. The format terminal must never run behind it.
static JsonValue missParamRaw(const std::string&) { return JsonValue{}; }

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

    VM vm(&doc, &world, &assets, &stubParamRaw, &stubParamFormat);
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
        VM vm(&doc, &world, &assets, &stubParamRaw, &stubParamFormat);
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
        VM vm(&doc, &world, &assets, &stubParamRaw, &stubParamFormat);
        ASSERT(vm.lookup("readout.text").str == "daisy.key/note_cents",
               "binding: {from, format} passes the override to the resolver");
    }
    // Object form without a format: equivalent to the bare string.
    {
        SceneDoc doc;
        SceneVmWorld world;
        AssetRegistry assets;
        readSceneDocJson(paramBindDoc(R"({"from":"param.daisy.key"})"), doc, world, assets);
        VM vm(&doc, &world, &assets, &stubParamRaw, &stubParamFormat);
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
        VM vm(&doc, &world, &assets, &stubParamRaw, &stubParamFormat);
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

// A one-label scene with state vars, for value-chain / condition-form /
// tolerant-miss dispatch tests (unwn #218). `text` binds to the substituted form.
static const char* const kFormScene = R"json({
  "version": 2,
  "scene": "form_bind",
  "state": { "greeting": "hello", "level": 3 },
  "entities": [
    { "components": {
        "id": { "id": "readout" },
        "label": { "text": "init" },
        "bindings": { "bindings": { "text": %BIND% } } } }
  ]
})json";

static std::string formBindDoc(const char* bindJson) {
    std::string s = kFormScene;
    s.replace(s.find("%BIND%"), 6, bindJson);
    return s;
}

static void test_value_chain_reserved_slots() {
    // A value-chain object may carry the reserved `map`/`ease`/`format` slots
    // (P3/P4/P5 fill their internals). Empty here, they pass the raw value
    // straight through — the binding still resolves like a bare source.
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(formBindDoc(R"({"from":"greeting","map":{"to":"frame"},"ease":"inOutCubic"})"),
                     doc, world, assets);
    VM vm(&doc, &world, &assets);
    ASSERT(vm.lookup("readout.text").str == "hello",
           "value-chain: empty map/ease slots pass the raw value through unchanged");
}

// Resolve the condition binding @p bind over kFormScene and read back the
// (string) target `readout.text`. The scene's state var `level` = 3 is the
// numeric source under comparison.
static std::string condText(const char* bind) {
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(formBindDoc(bind), doc, world, assets);
    VM vm(&doc, &world, &assets);
    return vm.lookup("readout.text").str;
}

static void test_condition_form_dispatch() {
    // A binding carrying `op` is the condition form (unwn #213): a numeric
    // comparison of the resolved `from` (state var `level` = 3) against the
    // literal threshold selects the then/else literal, written to the target.
    // Proves dispatch steered into the comparison (not the value chain, which
    // would have written the source verbatim).
    ASSERT(condText(R"({"from":"level","op":">","threshold":2,"then":"HI","else":"LO"})") == "HI",
           "condition: 3 > 2 selects the then literal");
    ASSERT(condText(R"({"from":"level","op":">","threshold":5,"then":"HI","else":"LO"})") == "LO",
           "condition: 3 > 5 false selects the else literal");
}

static void test_condition_operators() {
    // The full operator set `> >= < <= == !=`, each a numeric comparison of
    // level (3) against a literal threshold; T on the true side, F on the false.
    ASSERT(condText(R"({"from":"level","op":">","threshold":2,"then":"T","else":"F"})") == "T",
           "condition op >");
    ASSERT(condText(R"({"from":"level","op":">=","threshold":3,"then":"T","else":"F"})") == "T",
           "condition op >= (boundary equal)");
    ASSERT(condText(R"({"from":"level","op":"<","threshold":5,"then":"T","else":"F"})") == "T",
           "condition op <");
    ASSERT(condText(R"({"from":"level","op":"<=","threshold":3,"then":"T","else":"F"})") == "T",
           "condition op <= (boundary equal)");
    ASSERT(condText(R"({"from":"level","op":"==","threshold":3,"then":"T","else":"F"})") == "T",
           "condition op ==");
    ASSERT(condText(R"({"from":"level","op":"!=","threshold":4,"then":"T","else":"F"})") == "T",
           "condition op !=");
    // Each operator's false side too, so a bug that always returns true is caught.
    ASSERT(condText(R"({"from":"level","op":">","threshold":3,"then":"T","else":"F"})") == "F",
           "condition op > false side");
    ASSERT(condText(R"({"from":"level","op":"<","threshold":3,"then":"T","else":"F"})") == "F",
           "condition op < false side");
    ASSERT(condText(R"({"from":"level","op":"==","threshold":4,"then":"T","else":"F"})") == "F",
           "condition op == false side");
    ASSERT(condText(R"({"from":"level","op":"!=","threshold":3,"then":"T","else":"F"})") == "F",
           "condition op != false side");
}

static void test_condition_else_omitted() {
    // `else` omitted → the false branch resolves to the property's authored static
    // value (the synthesized Bool false is a wrong-kind literal on a string field
    // and is rejected, keeping the default). The then branch still writes.
    ASSERT(condText(R"({"from":"level","op":">","threshold":5,"then":"HI"})") == "init",
           "condition: else omitted keeps the authored default on a false condition");
    ASSERT(condText(R"({"from":"level","op":">","threshold":2,"then":"HI"})") == "HI",
           "condition: then still writes on a true condition when else omitted");
}

static void test_condition_unresolvable_from() {
    // An unresolvable `from` (unknown var) is a tolerant miss: keep the default.
    ASSERT(condText(R"({"from":"nosuchvar","op":">","threshold":2,"then":"HI","else":"LO"})") ==
               "init",
           "condition: unresolvable from keeps the authored default (tolerant miss)");
}

// A bool target (overlay.visible) driven by a condition form, for the bool
// branch defaults + truthy-fallback cases (unwn #213). `level` = 3.
static const char* const kCondBoolScene = R"json({
  "version": 2,
  "scene": "condbool",
  "state": { "level": 3 },
  "entities": [
    { "components": {
        "id": { "id": "ov" },
        "overlay": { "opacity": 5, "visible": false },
        "bindings": { "bindings": { "visible": %BIND% } } } }
  ]
})json";

static bool condVisible(const char* bind) {
    std::string s = kCondBoolScene;
    s.replace(s.find("%BIND%"), 6, bind);
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(s, doc, world, assets);
    VM vm(&doc, &world, &assets);
    return vm.lookup("ov.visible").truthy();
}

static void test_condition_bool_target_defaults() {
    // On a bool target the absent then/else branches default to true/false, so a
    // bare comparison drives visibility with no literals authored.
    ASSERT(condVisible(R"({"from":"level","op":">","threshold":2})") == true,
           "condition: bool target defaults the then branch to true");
    ASSERT(condVisible(R"({"from":"level","op":">","threshold":5})") == false,
           "condition: bool target defaults the else branch to false");
    // Explicit bool literals are honored (else fires on the false condition).
    ASSERT(condVisible(R"({"from":"level","op":">","threshold":5,"then":false,"else":true})") ==
               true,
           "condition: an explicit bool else literal is written");
    // op present but threshold absent → the plain truthy test of `from`
    // (level = 3 is truthy). The bare-string `visible: var` shorthand itself
    // rides the value chain — see test_visible_fork_retired.
    ASSERT(condVisible(R"({"from":"level","op":">"})") == true,
           "condition: op with no threshold falls back to the truthy test of from");
}

// A scene whose `visible` is bound to a bool var on a real reflected bool field,
// proving the hardcoded `visible` property-name fork retired into an ordinary
// value-chain bool prop (unwn #218).
static const char* const kVisibleScene = R"json({
  "version": 2,
  "scene": "vis",
  "state": { "shown": true },
  "entities": [
    { "components": {
        "id": { "id": "ov" },
        "overlay": { "opacity": 5, "visible": false },
        "bindings": { "bindings": { "visible": "shown" } } } }
  ]
})json";

static void test_visible_fork_retired() {
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(kVisibleScene, doc, world, assets);
    VM vm(&doc, &world, &assets);
    ASSERT(vm.lookup("ov.visible").truthy(),
           "visible: bare shorthand still drives an ordinary bool prop (name-fork retired)");
}

static void test_binding_tolerant_miss() {
    // Uniform tolerant-miss across the value chain: any miss keeps the default.
    { // unknown source var
        SceneDoc doc;
        SceneVmWorld world;
        AssetRegistry assets;
        readSceneDocJson(formBindDoc(R"("nosuchvar")"), doc, world, assets);
        VM vm(&doc, &world, &assets);
        ASSERT(vm.lookup("readout.text").str == "init",
               "tolerant: unknown value-chain source keeps the authored default");
    }
    { // malformed object: no `from`
        SceneDoc doc;
        SceneVmWorld world;
        AssetRegistry assets;
        readSceneDocJson(formBindDoc(R"({"format":"note_cents"})"), doc, world, assets);
        VM vm(&doc, &world, &assets);
        ASSERT(vm.lookup("readout.text").str == "init",
               "tolerant: malformed value-chain binding (no from) is skipped");
    }
    { // param resolve-raw miss → the format terminal never runs, default kept
        SceneDoc doc;
        SceneVmWorld world;
        AssetRegistry assets;
        readSceneDocJson(formBindDoc(R"("param.no.such")"), doc, world, assets);
        VM vm(&doc, &world, &assets, &missParamRaw, &stubParamFormat);
        ASSERT(vm.lookup("readout.text").str == "init",
               "tolerant: param resolve-raw miss keeps the default (never formats)");
    }
}

static void test_easing_state_transient() {
    // A numeric value-chain binding seeds a transient (entity,property) easing
    // anchor (unwn #218 seam; P4 tweens off it). The side table is VM-local — a
    // save reflects the world, never the anchors — so the doc round-trips
    // byte-identically before and after the VM seeds it.
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(kDatumManagerScene, doc, world, assets);
    const std::string authored = writeSceneDocJson(doc, world, assets);
    VM vm(&doc, &world, &assets); // ctor applies bindings once → seeds anchors
    ASSERT(vm.easingStateCount() >= 1,
           "easing: a numeric value-chain binding seeds a transient anchor");
    const std::string afterSeed = writeSceneDocJson(doc, world, assets);
    ASSERT(afterSeed == authored,
           "easing: the transient side table never leaks into the serialized document");
}

// ---- value-chain transforms: ease (#221) + map value->frame (#220) ----------
//
// A single controllable `param.` cell {number, min, max}: a test moves the
// source between ticks and watches the ease tween / frame-map track it.
static double g_paramNumber = 0.0;
static double g_paramMin = 0.0;
static double g_paramMax = 100.0;
static JsonValue rampParamRaw(const std::string&) {
    auto num = [](double d) {
        JsonValue v;
        v.type = JsonValue::Type::Number;
        v.number = d;
        return v;
    };
    JsonValue out;
    out.type = JsonValue::Type::Object;
    out.object.emplace_back("number", num(g_paramNumber));
    out.object.emplace_back("min", num(g_paramMin));
    out.object.emplace_back("max", num(g_paramMax));
    return out;
}

static bool near2(double a, double b) { return std::fabs(a - b) < 0.02; }

// A one-overlay scene: `dimmer.opacity` (an un-clamped numeric field) bound to
// the ramp param through the substituted value-chain form — the ease slot's
// numeric target. (Gauge `value` clamps to its mode range, so it can't observe
// a wide tween; opacity carries 0..255 straight through.)
static const char* const kEaseScene = R"json({
  "version": 2,
  "scene": "ease",
  "entities": [
    { "components": {
        "id": { "id": "dimmer" },
        "overlay": { "opacity": 0, "visible": true },
        "bindings": { "bindings": { "opacity": %BIND% } } } }
  ]
})json";

static std::string easeDoc(const char* bindJson) {
    std::string s = kEaseScene;
    s.replace(s.find("%BIND%"), 6, bindJson);
    return s;
}

static void test_ease_tween_time_based() {
    // A bound numeric value glides to its new target over the curve instead of
    // snapping, timed off the dtMs fed to tick() (unwn #221). First appearance
    // seeds the resolved value (no tween-from-zero); a target change re-anchors
    // from the current eased value with the full duration again.
    g_paramNumber = 0.0;
    g_paramMin = 0.0;
    g_paramMax = 100.0;
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(easeDoc(R"({"from":"param.x","ease":{"curve":"linear","ms":200}})"), doc, world,
                     assets);
    VM vm(&doc, &world, &assets, &rampParamRaw, &stubParamFormat);

    ASSERT(near2(vm.lookup("dimmer.opacity").number, 0.0),
           "ease: first appearance seeds the resolved value (no tween from zero)");

    g_paramNumber = 100.0;
    vm.tick(100.0); // re-anchor tick: elapsed resets to 0 -> holds the from value
    ASSERT(near2(vm.lookup("dimmer.opacity").number, 0.0),
           "ease: a target change re-anchors from the current eased value (no snap)");
    vm.tick(100.0); // 100/200 ms elapsed -> t=0.5 -> linear 50
    ASSERT(near2(vm.lookup("dimmer.opacity").number, 50.0),
           "ease: linear tween is halfway at t=0.5 (time-based off dtMs)");
    vm.tick(100.0); // 200/200 ms -> t=1 -> 100
    ASSERT(near2(vm.lookup("dimmer.opacity").number, 100.0),
           "ease: tween completes at t=1 and lands on the target");
    vm.tick(1000.0); // past the end: clamped, stays put
    ASSERT(near2(vm.lookup("dimmer.opacity").number, 100.0),
           "ease: elapsed clamps at t=1 (no overshoot)");
}

static void test_ease_parse_forms() {
    // Shorthand `ease:"linear"` defaults ms:200; an unknown curve falls back to
    // Linear. Both reach the same trajectory as the explicit linear object.
    g_paramNumber = 0.0;
    g_paramMin = 0.0;
    g_paramMax = 100.0;
    { // shorthand string, default 200 ms
        SceneDoc doc;
        SceneVmWorld world;
        AssetRegistry assets;
        readSceneDocJson(easeDoc(R"({"from":"param.x","ease":"linear"})"), doc, world, assets);
        VM vm(&doc, &world, &assets, &rampParamRaw, &stubParamFormat);
        g_paramNumber = 100.0;
        vm.tick(100.0); // re-anchor
        vm.tick(100.0); // 100/200 -> 50
        ASSERT(near2(vm.lookup("dimmer.opacity").number, 50.0),
               "ease: shorthand string parses and defaults ms:200");
    }
    { // unknown curve -> Linear fallback (same halfway value)
        SceneDoc doc;
        SceneVmWorld world;
        AssetRegistry assets;
        g_paramNumber = 0.0;
        readSceneDocJson(easeDoc(R"({"from":"param.x","ease":{"curve":"bogus","ms":200}})"), doc,
                         world, assets);
        VM vm(&doc, &world, &assets, &rampParamRaw, &stubParamFormat);
        g_paramNumber = 100.0;
        vm.tick(100.0);
        vm.tick(100.0);
        ASSERT(near2(vm.lookup("dimmer.opacity").number, 50.0),
               "ease: unknown curve falls back to Linear");
    }
}

static void test_ease_numeric_only() {
    // A non-numeric resolved value (a string state var) passes through un-eased:
    // easing is numeric-only (unwn #221, tolerant-miss for bool/enum/string).
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(formBindDoc(R"({"from":"greeting","ease":"linear"})"), doc, world, assets);
    VM vm(&doc, &world, &assets);
    ASSERT(vm.lookup("readout.text").str == "hello",
           "ease: a string source passes through un-eased (numeric-only)");
}

// ---- map value->frame (#220): a sprite `frame` bound to a live param ---------
using SpriteVmWorld = World<8, IdComponent, SpriteComponent, OverlayComponent, BindingsComponent>;
using SpriteVM = SceneVM<SpriteVmWorld>;

// A tiny opaque sheet plane; frameCount() is cols*rows, independent of content.
static const uint8_t kMapSheet[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static SpriteComponent* addBoundSprite(SpriteVmWorld& world, uint16_t cols, uint16_t rows,
                                       const char* bindJson) {
    Entity e = world.create();
    world.add<IdComponent>(e, "spr");
    SpriteComponent* s = world.add<SpriteComponent>(e);
    s->load(kMapSheet, 1, 1, cols, rows);
    s->fps = -1.0f; // externally driven (bindable frame)
    BindingsComponent* b = world.add<BindingsComponent>(e);
    b->bindings = parse(bindJson);
    return s;
}

static void test_map_value_to_frame() {
    // `map:{to:"frame"}` quantizes the raw value to a frame index off the bound
    // sprite's frameCount(): v=min -> 0, v=max -> N-1, round-to-nearest between.
    g_paramMin = 0.0;
    g_paramMax = 100.0;
    g_paramNumber = 0.0;
    SpriteVmWorld world;
    AssetRegistry assets;
    SpriteComponent* s =
        addBoundSprite(world, 4, 1, R"({"frame":{"from":"param.x","map":{"to":"frame"}}})"); // N=4
    SpriteVM vm(nullptr, &world, &assets, &rampParamRaw, &stubParamFormat);
    ASSERT(s->frame == 0, "map: v=min quantizes to frame 0");
    g_paramNumber = 100.0;
    vm.tick(16.0);
    ASSERT(s->frame == 3, "map: v=max quantizes to frame N-1");
    g_paramNumber = 50.0;
    vm.tick(16.0); // t=0.5 -> round(0.5*3)=round(1.5)=2
    ASSERT(s->frame == 2, "map: midpoint rounds to nearest frame");
}

static void test_map_frame_count_read_at_tick() {
    // N is read at tick time from the live SpriteComponent, never authored — a
    // sheet re-import with a different frame count needs no binding edit.
    g_paramMin = 0.0;
    g_paramMax = 100.0;
    g_paramNumber = 100.0;
    SpriteVmWorld world;
    AssetRegistry assets;
    SpriteComponent* s =
        addBoundSprite(world, 4, 1, R"({"frame":{"from":"param.x","map":{"to":"frame"}}})"); // N=4
    SpriteVM vm(nullptr, &world, &assets, &rampParamRaw, &stubParamFormat);
    ASSERT(s->frame == 3, "map: v=max on the 4-frame sheet is frame 3");
    s->load(kMapSheet, 1, 1, 8, 1); // re-import: N is now 8
    vm.tick(16.0);
    ASSERT(s->frame == 7, "map: N re-read at tick — a sheet swap needs no binding edit");
}

static void test_map_misuse_no_sprite() {
    // `to:"frame"` on an entity with no SpriteComponent is a silent no-op: the
    // whole binding misses, so the target property keeps its authored default.
    g_paramMin = 0.0;
    g_paramMax = 100.0;
    g_paramNumber = 100.0;
    SpriteVmWorld world;
    AssetRegistry assets;
    Entity e = world.create();
    world.add<IdComponent>(e, "ov");
    OverlayComponent* ov = world.add<OverlayComponent>(e);
    ov->opacity = 5;
    BindingsComponent* b = world.add<BindingsComponent>(e);
    b->bindings = parse(R"({"opacity":{"from":"param.x","map":{"to":"frame"}}})");
    SpriteVM vm(nullptr, &world, &assets, &rampParamRaw, &stubParamFormat);
    ASSERT(ov->opacity == 5,
           "map: to:frame with no SpriteComponent is a silent no-op (keeps default)");
}

static void test_ease_feeds_map() {
    // Order raw -> ease -> map: easing runs on the continuous value and its
    // output feeds the frame quantize, so a value->frame sprite steps smoothly
    // through the sheet instead of snapping straight to the end (unwn #221 AC).
    g_paramMin = 0.0;
    g_paramMax = 100.0;
    g_paramNumber = 0.0;
    SpriteVmWorld world;
    AssetRegistry assets;
    SpriteComponent* s = addBoundSprite(
        world, 5, 1,
        R"({"frame":{"from":"param.x","ease":{"curve":"linear","ms":200},"map":{"to":"frame"}}})"); // N=5
    SpriteVM vm(nullptr, &world, &assets, &rampParamRaw, &stubParamFormat);
    ASSERT(s->frame == 0, "ease->map: seeded at frame 0");
    g_paramNumber = 100.0;
    vm.tick(100.0); // re-anchor: eased still 0 -> frame 0 (not snapped to N-1)
    ASSERT(s->frame == 0, "ease->map: re-anchor tick holds frame 0, no snap to the end");
    vm.tick(100.0); // eased 50 -> t=0.5 -> round(0.5*4)=2
    ASSERT(s->frame == 2, "ease->map: a half-eased value quantizes to a mid frame");
    vm.tick(100.0); // eased 100 -> frame 4
    ASSERT(s->frame == 4, "ease->map: the completed tween reaches the last frame");
}

// ---- cross-kind binding targets (unwn #232) --------------------------------
//
// The value-chain terminal picks String-vs-Number from the **destination field
// kind**, not the source. Pre-#232 a bare `param.` source always wrote the
// formatted String, which readFieldValue rejects for a numeric field, so the
// property silently kept its authored default. Symmetrically, a numeric source
// written to a string field was rejected.

static void test_cross_kind_param_to_numeric() {
    // Bare `param.` (no ease/map) → a numeric field (overlay.opacity): must land
    // the raw Number, not the rejected format String.
    g_paramNumber = 42.0;
    g_paramMin = 0.0;
    g_paramMax = 100.0;
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(easeDoc(R"({"from":"param.x"})"), doc, world, assets);
    VM vm(&doc, &world, &assets, &rampParamRaw, &stubParamFormat);
    ASSERT(near2(vm.lookup("dimmer.opacity").number, 42.0),
           "cross-kind: bare param -> numeric field writes the raw Number (unwn #232)");
}

static void test_cross_kind_param_to_color() {
    // The ticket's headline symptom: a param bound to a Label `color` (Pixel4).
    g_paramNumber = 5.0;
    g_paramMin = 0.0;
    g_paramMax = 15.0;
    const char* scene = R"json({
      "version": 2, "scene": "color_bind",
      "entities": [ { "components": {
        "id": { "id": "swatch" },
        "label": { "text": "x", "color": 1 },
        "bindings": { "bindings": { "color": { "from": "param.x" } } } } } ]
    })json";
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(scene, doc, world, assets);
    VM vm(&doc, &world, &assets, &rampParamRaw, &stubParamFormat);
    ASSERT(vm.lookup("swatch.color").number == 5.0,
           "cross-kind: param -> Label color writes the palette index, not a String (unwn #232)");
}

static void test_cross_kind_num_to_text() {
    // A numeric state var (level = 3) bound to a string field renders its %.9g
    // display value instead of keeping the authored "init".
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(formBindDoc(R"({"from":"level"})"), doc, world, assets);
    VM vm(&doc, &world, &assets);
    ASSERT(vm.lookup("readout.text").str == "3",
           "cross-kind: numeric state var -> string field renders the value (unwn #232)");
}

// ---- position bindable per-axis (unwn #225) --------------------------------

static void test_position_axis_binding() {
    // position.x / .y are first-class bindable scalars. A value-chain binding to
    // position.x writes only the x axis (y untouched); the dotted path resolves
    // on the read side too, so a sibling entity could source it. A bad axis is a
    // tolerant miss.
    const char* scene = R"json({
      "version": 2, "scene": "pos",
      "state": { "px": 40 },
      "entities": [ { "components": {
        "id": { "id": "mover" },
        "position": { "position": { "x": 5, "y": 9 } },
        "bindings": { "bindings": { "position.x": { "from": "px" } } } } } ]
    })json";
    SceneDoc doc;
    SceneVmWorld world;
    AssetRegistry assets;
    readSceneDocJson(scene, doc, world, assets);
    VM vm(&doc, &world, &assets);
    ASSERT(vm.lookup("mover.position.x").number == 40.0,
           "position.x: a bound source writes the x axis (unwn #225)");
    ASSERT(vm.lookup("mover.position.y").number == 9.0,
           "position.x: the y axis is untouched by an x-only binding (unwn #225)");
    const JsonValue whole = vm.lookup("mover.position");
    const JsonValue* wx = whole.find("x");
    ASSERT(wx && wx->type == JsonValue::Type::Number && wx->number == 40.0,
           "position.x: the compound-field read reflects the per-axis write (unwn #225)");
    ASSERT(vm.lookup("mover.position.z").type == JsonValue::Type::Null,
           "position.x: an unknown axis reads Null (tolerant miss, unwn #225)");
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
    test_value_chain_reserved_slots();
    test_condition_form_dispatch();
    test_condition_operators();
    test_condition_else_omitted();
    test_condition_unresolvable_from();
    test_condition_bool_target_defaults();
    test_visible_fork_retired();
    test_binding_tolerant_miss();
    test_easing_state_transient();
    test_ease_tween_time_based();
    test_ease_parse_forms();
    test_ease_numeric_only();
    test_map_value_to_frame();
    test_map_frame_count_read_at_tick();
    test_map_misuse_no_sprite();
    test_ease_feeds_map();
    test_cross_kind_param_to_numeric();
    test_cross_kind_param_to_color();
    test_cross_kind_num_to_text();
    test_position_axis_binding();
    test_tolerance();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
