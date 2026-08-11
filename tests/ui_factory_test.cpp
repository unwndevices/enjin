// G3 name factory (unwn #183, M2): components and widget verbs by string name.
//
// The acceptance criterion is literal: "Name factory constructs
// widgets/components from string names." Every reflected component the world
// composes must be constructible from its ComponentTraits name, the palette
// enumeration must cover exactly that set, and the `call` action's verb
// dispatch must route through the widgets' public API (so invariants hold:
// selection clamps against items, popup show() arms its countdown).
#include <enjin2/ui/factory.hpp>
#include <enjin2/ui/scene_json.hpp>

#include "scene_fixture.h"

#include <cstdio>
#include <cstring>
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

static JsonValue parseArgs(const char* text) {
    JsonValue v;
    parseJson(std::string(text), v);
    return v;
}

static void test_construct_by_name() {
    SceneVmWorld world;
    const Entity e = world.create();

    const char* names[] = {"id",      "position", "size", "label", "icon",    "gauge",
                           "overlay", "popup",    "list", "slot",  "bindings"};
    for (const char* name : names) {
        char msg[64];
        snprintf(msg, sizeof(msg), "factory: constructs '%s' by name", name);
        ASSERT(addComponentByName(world, e, name), msg);
    }
    ASSERT(world.get<ListComponent>(e) != nullptr, "factory: constructed list is attached");
    ASSERT(world.get<SlotComponent>(e) != nullptr, "factory: constructed slot is attached");

    ASSERT(!addComponentByName(world, e, "wormhole"),
           "factory: unknown component name is refused");
    ASSERT(!addComponentByName(world, e, "theme"),
           "factory: theme is not an entity component");
}

static void test_palette_enumeration() {
    std::vector<std::string> names;
    std::vector<ComponentTypeId> ids;
    forEachComponentName<SceneVmWorld>([&](const char* name, ComponentTypeId id) {
        names.push_back(name);
        ids.push_back(id);
    });
    ASSERT(names.size() == 11, "palette: enumerates every reflected component");
    // Pack order is declaration order; ids are the permanent allocation table.
    ASSERT(names.front() == "id" && ids.front() == 10, "palette: first entry is id/10");
    bool sawSlot = false;
    for (size_t i = 0; i < names.size(); ++i)
        if (names[i] == "slot" && ids[i] == 11) sawSlot = true;
    ASSERT(sawSlot, "palette: slot/11 enumerated");
}

static void test_list_verbs() {
    SceneVmWorld world;
    const Entity e = world.create();
    world.add<ListComponent>(e, std::vector<std::string>{"A", "B", "C"});

    JsonValue none = parseArgs("[]");
    ASSERT(callWidgetVerb(world, e, "moveDown", none), "verbs: moveDown handled");
    ASSERT(world.get<ListComponent>(e)->currentSelectionIndex() == 1,
           "verbs: moveDown moved the cursor");
    ASSERT(callWidgetVerb(world, e, "moveUp", none), "verbs: moveUp handled");
    ASSERT(world.get<ListComponent>(e)->currentSelectionIndex() == 0,
           "verbs: moveUp moved the cursor back");
    callWidgetVerb(world, e, "moveUp", none);
    ASSERT(world.get<ListComponent>(e)->currentSelectionIndex() == 0,
           "verbs: moveUp clamps at the top");

    ASSERT(callWidgetVerb(world, e, "setSelection", parseArgs("[2]")),
           "verbs: setSelection handled");
    ASSERT(world.get<ListComponent>(e)->currentSelectionIndex() == 2,
           "verbs: setSelection jumped the cursor");
    callWidgetVerb(world, e, "setSelection", parseArgs("[99]"));
    ASSERT(world.get<ListComponent>(e)->currentSelectionIndex() == 2,
           "verbs: out-of-range setSelection is a no-op (spec), not a clamp");

    ASSERT(callWidgetVerb(world, e, "setItems", parseArgs("[[\"X\", \"Y\"]]")),
           "verbs: setItems handled");
    ASSERT(world.get<ListComponent>(e)->itemCount() == 2, "verbs: setItems swapped items");
    ASSERT(world.get<ListComponent>(e)->currentSelectionIndex() == 1,
           "verbs: setItems reclamped the cursor to the new range");

    ASSERT(!callWidgetVerb(world, e, "explode", none), "verbs: unknown verb is refused");
}

static void test_popup_verbs() {
    SceneVmWorld world;
    const Entity e = world.create();
    world.add<PopUpComponent>(e);

    ASSERT(callWidgetVerb(world, e, "show", parseArgs("[750]")), "verbs: popup show handled");
    ASSERT(world.get<PopUpComponent>(e)->visible, "verbs: show made the popup visible");
    ASSERT(world.get<PopUpComponent>(e)->autoHideMs == 750, "verbs: show armed auto-hide");
    ASSERT(callWidgetVerb(world, e, "hide", parseArgs("[]")), "verbs: popup hide handled");
    ASSERT(!world.get<PopUpComponent>(e)->visible, "verbs: hide hid the popup");
}

static void test_verb_needs_a_component() {
    SceneVmWorld world;
    const Entity e = world.create();
    ASSERT(!callWidgetVerb(world, e, "moveUp", parseArgs("[]")),
           "verbs: entity without a verb-bearing component refuses");
}

int main() {
    test_construct_by_name();
    test_palette_enumeration();
    test_list_verbs();
    test_popup_verbs();
    test_verb_needs_a_component();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
