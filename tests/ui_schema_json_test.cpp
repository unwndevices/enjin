// Editor write surface (unwn #186, M3b): schema export + ScenePlayer save.
//
// The write path's contract with the editor:
//
//   - writeSchemaJson enumerates the player world's reflected components in
//     pack order, each field with the kind tag and default the inspector and
//     palette build from — the same X-macro lists that drive save/load, so
//     there is no second schema to drift
//   - ScenePlayer::saveText() is the canonical serializer: saving the fixture
//     right after load reproduces the writer's fixed point byte-for-byte
//   - a themeless document stays themeless across save (theme presence is
//     tracked, not defaulted); an authored theme survives
//   - saveText() on an inactive player is "" — never a half document
#include <enjin2/ui/scene_player.hpp>
#include <enjin2/ui/schema_json.hpp>

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

static void test_schema_lists_components_with_kinds_and_defaults() {
    const std::string schema = ScenePlayer().schemaText();
    JsonValue root;
    ASSERT(parseJson(schema, root) && root.type == JsonValue::Type::Object,
           "schema: parses as a JSON object");

    const JsonValue* version = root.find("version");
    ASSERT(version && version->number == 1.0, "schema: carries the scene format version");

    const JsonValue* components = root.find("components");
    ASSERT(components && components->type == JsonValue::Type::Array &&
               components->array.size() == 11,
           "schema: all 11 reflected world components present");

    // Pack order, matching forEachComponentName (ui_factory_test pins the set).
    const char* expected[] = {"id",      "position", "size", "label", "icon", "gauge",
                              "overlay", "popup",    "list", "slot",  "bindings"};
    bool orderOk = components->array.size() == 11;
    for (size_t i = 0; orderOk && i < components->array.size(); ++i) {
        const JsonValue* name = components->array[i].find("name");
        orderOk = name && name->str == expected[i];
    }
    ASSERT(orderOk, "schema: components come in pack order");

    // One representative per kind family: label.text string, gauge.value
    // float after its enum mode, position.position point, slot.props json.
    auto field = [&](const char* comp, const char* fname) -> const JsonValue* {
        for (const JsonValue& c : components->array) {
            const JsonValue* n = c.find("name");
            if (!n || n->str != comp) continue;
            const JsonValue* fields = c.find("fields");
            if (!fields) return nullptr;
            for (const JsonValue& f : fields->array) {
                const JsonValue* fn = f.find("name");
                if (fn && fn->str == fname) return &f;
            }
        }
        return nullptr;
    };

    const JsonValue* text = field("label", "text");
    ASSERT(text && text->find("kind")->str == "string" &&
               text->find("default")->type == JsonValue::Type::String,
           "schema: label.text is a string with a string default");

    const JsonValue* mode = field("gauge", "mode");
    const JsonValue* value = field("gauge", "value");
    ASSERT(mode && mode->find("kind")->str == "enum" && value &&
               value->find("kind")->str == "float",
           "schema: gauge mode=enum precedes value=float (field-list order)");

    const JsonValue* pos = field("position", "position");
    ASSERT(pos && pos->find("kind")->str == "point" &&
               pos->find("default")->type == JsonValue::Type::Object,
           "schema: position is a point with an object default");

    const JsonValue* props = field("slot", "props");
    ASSERT(props && props->find("kind")->str == "json",
           "schema: slot.props is the opaque json bag");

    const JsonValue* theme = root.find("theme");
    ASSERT(theme && theme->find("fields") &&
               theme->find("fields")->array.size() == 10,
           "schema: root-level theme schema rides separately (10 fields)");

    const JsonValue* fonts = root.find("fonts");
    const JsonValue* themes = root.find("themes");
    ASSERT(fonts && fonts->type == JsonValue::Type::Array && themes &&
               themes->array.size() == 1 && themes->array[0].str == "dark",
           "schema: compiled-in theme/asset enumeration present");
}

static void test_save_reproduces_the_writer_fixed_point() {
    // The independent ground truth: read + write outside the player.
    SceneDoc doc;
    ScenePlayer::World world;
    AssetRegistry assets;
    ASSERT(readSceneDocJson(kDatumManagerScene, doc, world, assets),
           "save: fixture loads outside the player");
    const std::string expected = writeSceneDocJson(doc, world, assets);

    ScenePlayer p;
    ASSERT(p.loadText(kDatumManagerScene), "save: fixture loads in the player");
    ASSERT(p.saveText() == expected,
           "save: EXIT CONTRACT - saveText matches the canonical writer byte-for-byte");

    // Running the scene must not leak runtime state into the file: saveText
    // is the authored document, snapshotted before scene.activate.
    for (int i = 0; i < 20; ++i) p.stepFrame();
    p.dispatch("input.encoder.cw", "");
    p.stepFrame();
    ASSERT(p.saveText() == expected,
           "save: frames and events never change the saved document");

    // Canonical text is a fixed point through the player too.
    ScenePlayer q;
    ASSERT(q.loadText(p.saveText()) && q.saveText() == expected,
           "save: load(saveText) -> saveText is byte-identical");
}

static void test_theme_presence_is_tracked_not_defaulted() {
    ScenePlayer p;
    ASSERT(p.loadText(R"({"version":1,"scene":"plain","entities":[]})"),
           "theme: themeless document loads");
    ASSERT(p.saveText().find("\"theme\"") == std::string::npos,
           "theme: a themeless document stays themeless on save");

    ASSERT(p.loadText(R"({"version":1,"scene":"tinted","theme":{"padding":9},"entities":[]})"),
           "theme: themed document loads");
    const std::string out = p.saveText();
    ASSERT(out.find("\"theme\"") != std::string::npos &&
               out.find("\"padding\": 9") != std::string::npos,
           "theme: an authored theme survives save");
}

static void test_inactive_player_saves_empty() {
    ScenePlayer p;
    ASSERT(p.saveText().empty(), "inactive: saveText is empty before any load");
    p.loadText("not json {");
    ASSERT(p.saveText().empty(), "inactive: a failed load never yields a half document");
}

int main() {
    test_schema_lists_components_with_kinds_and_defaults();
    test_save_reproduces_the_writer_fixed_point();
    test_theme_presence_is_tracked_not_defaulted();
    test_inactive_player_saves_empty();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
