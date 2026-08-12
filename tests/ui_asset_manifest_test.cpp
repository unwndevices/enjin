// Content-addressed asset manifest + woken AssetRegistry (unwn #204).
//
// Three seams under test:
//   1. The manifest is a scene-JSON section carrying hashes + dims, never
//      asset bytes; it round-trips as a dump fixed point, DOM and streaming
//      readers agree.
//   2. AssetRegistry owns heap-backed bitmaps alongside its borrowed
//      compiled-in ones; findBitmap resolves owned first, clearOwned frees.
//   3. ScenePlayer loads manifest assets from an injected byte source at
//      activate and frees them at teardown — identical resolution wherever the
//      byte source lives (this test's is in-memory, standing in for LittleFS /
//      the local dir / the WASM authoring buffer).
#include <enjin2/graphics/sprite_asset.hpp>
#include <enjin2/ui/asset_manifest.hpp>
#include <enjin2/ui/asset_registry.hpp>
#include <enjin2/ui/scene_json.hpp>
#include <enjin2/ui/scene_player.hpp>
#include <enjin2/ui/scene_stream.hpp>
#include <enjin2/ui/widgets/icon.hpp>

#include "scene_fixture.h"

#include <cstdio>
#include <cstring>
#include <map>
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

// A whole-string streaming source (the shape the firmware feeds off LittleFS).
struct StringStreamSource : SceneStreamSource {
    explicit StringStreamSource(const std::string& text) : text_(text) {}
    int read(char* buf, size_t maxLen) override {
        const size_t remaining = text_.size() - pos_;
        const size_t n = maxLen < remaining ? maxLen : remaining;
        std::memcpy(buf, text_.data() + pos_, n);
        pos_ += n;
        return static_cast<int>(n);
    }
    std::string text_;
    size_t pos_ = 0;
};

// A manifest-less v2 scene: loading it must tear down a prior scene's assets.
static const char* const kNoManifestScene = R"json({
  "version": 2,
  "scene": "plain",
  "entities": [ { "components": { "id": { "id": "z" }, "label": { "text": "hi" } } } ]
})json";

// An in-memory content-addressed store — the test's stand-in for LittleFS.
struct MemAssetSource : AssetSource {
    std::map<std::string, std::vector<uint8_t>> store;
    bool read(const std::string& hash, std::vector<uint8_t>& out) override {
        auto it = store.find(hash);
        if (it == store.end()) return false;
        out = it->second;
        return true;
    }
};

// Encode a tiny static .njn v2 bitmap (cols=rows=1) into a byte vector.
static std::vector<uint8_t> makeStaticNjn(uint8_t w, uint8_t h, const std::vector<uint8_t>& px) {
    std::vector<uint8_t> file(sizeof(NjnHeader) + njnPackedByteSize(w * h));
    size_t n = njnEncodeV2(w, h, 1, 1, px.data(), px.size(), file.data(), file.size());
    file.resize(n);
    return file;
}

// ----- 1. manifest is data, not bytes; and round-trips -----

static void test_manifest_parse() {
    JsonValue v;
    parseJson(std::string(R"([
        {"hash": "abc123", "kind": "bitmap", "w": 16, "h": 16},
        {"hash": "def456", "kind": "sprite", "w": 8, "h": 8, "cols": 4, "rows": 2},
        {"kind": "bitmap", "w": 4, "h": 4}
    ])"), v);
    const std::vector<AssetManifestEntry> m = parseAssetManifest(v);
    ASSERT(m.size() == 2, "manifest: hash-less entry is skipped, two survive");
    ASSERT(m[0].hash == "abc123" && m[0].kind == AssetKind::Bitmap, "manifest: bitmap entry");
    ASSERT(m[0].cols == 1 && m[0].rows == 1, "manifest: static bitmap defaults to 1x1 grid");
    ASSERT(m[1].kind == AssetKind::Sprite && m[1].cols == 4 && m[1].rows == 2,
           "manifest: sprite sheet carries its grid");
    ASSERT(m[1].w == 8 && m[1].h == 8, "manifest: cell dims parsed");
}

static const char* const kManifestScene = R"json({
  "version": 2,
  "scene": "with_assets",
  "manifest": [
    { "hash": "deadbeef", "kind": "bitmap", "w": 4, "h": 2 },
    { "hash": "cafef00d", "kind": "sprite", "w": 8, "h": 8, "cols": 2, "rows": 2 }
  ],
  "entities": [
    { "components": { "id": { "id": "a" }, "icon": { "bitmap": "deadbeef" } } }
  ]
})json";

static void test_manifest_round_trips_without_bytes() {
    SceneDoc docA;
    SceneVmWorld worldA;
    AssetRegistry assets;
    ASSERT(readSceneDocJson(kManifestScene, docA, worldA, assets),
           "manifest: scene with a manifest loads");
    ASSERT(docA.manifest.type == JsonValue::Type::Array && docA.manifest.array.size() == 2,
           "manifest: section parsed as a two-entry array");

    const std::string dumpA = writeSceneDocJson(docA, worldA, assets);
    ASSERT(dumpA.find("\"manifest\"") != std::string::npos, "manifest: survives the writer");
    ASSERT(dumpA.find("deadbeef") != std::string::npos, "manifest: hash survives");
    // The scene carries hashes + dims only — never pixel bytes.
    ASSERT(dumpA.find("pixels") == std::string::npos && dumpA.find("data") == std::string::npos,
           "manifest: no asset bytes in the scene JSON");

    SceneDoc docB;
    SceneVmWorld worldB;
    ASSERT(readSceneDocJson(dumpA, docB, worldB, assets), "manifest: dump reloads");
    const std::string dumpB = writeSceneDocJson(docB, worldB, assets);
    ASSERT(dumpA == dumpB, "manifest: dump -> reload -> dump is byte-identical");
}

static void test_streaming_reader_agrees() {
    SceneDoc dom, stream;
    SceneVmWorld wDom, wStream;
    AssetRegistry assets;
    readSceneDocJson(kManifestScene, dom, wDom, assets);

    StringStreamSource src(kManifestScene);
    ASSERT(readSceneDocStream(src, stream, wStream, assets),
           "manifest: streaming reader loads the scene");
    ASSERT(stream.manifest.type == JsonValue::Type::Array &&
               stream.manifest.array.size() == 2,
           "manifest: streaming reader parses the manifest too");
    ASSERT(writeSceneDocJson(dom, wDom, assets) == writeSceneDocJson(stream, wStream, assets),
           "manifest: DOM and streaming loads dump identically");
}

// ----- 2. registry owned entries -----

static void test_registry_owned_entries() {
    AssetRegistry reg;
    static const uint8_t kBorrowed[4] = {1, 2, 3, 4};
    reg.registerBitmap("compiled_icon", kBorrowed);

    const uint8_t owned[4] = {5, 6, 7, 8};
    ASSERT(reg.registerOwnedBitmap("hashA", owned, 4, 2, 2), "registry: owned bitmap registers");
    ASSERT(!reg.registerOwnedBitmap("hashA", owned, 4, 2, 2), "registry: no duplicate name");
    ASSERT(reg.ownedBitmapCount() == 1, "registry: one owned entry");

    const uint8_t* got = reg.findBitmap("hashA");
    ASSERT(got != nullptr && got[0] == 5 && got[3] == 8, "registry: owned resolves to its pixels");
    ASSERT(reg.findBitmap("compiled_icon") == kBorrowed, "registry: borrowed still resolves");
    const AssetRegistry::OwnedBitmap* e = reg.findOwnedBitmap("hashA");
    ASSERT(e && e->w == 2 && e->h == 2, "registry: owned carries its dims");
    ASSERT(std::string(reg.bitmapName(got)) == "hashA", "registry: reverse lookup for owned");

    reg.clearOwned();
    ASSERT(reg.ownedBitmapCount() == 0, "registry: clearOwned frees owned entries");
    ASSERT(reg.findBitmap("hashA") == nullptr, "registry: owned no longer resolves after clear");
    ASSERT(reg.findBitmap("compiled_icon") == kBorrowed, "registry: clearOwned spares borrowed");
}

// ----- 3. player loads at activate, frees at teardown -----

static void test_player_loads_and_frees_owned() {
    static ScenePlayer player; // static: the World is large for the stack
    MemAssetSource source;
    // deadbeef: a 4x2 bitmap with two transparent (index-15) pixels.
    source.store["deadbeef"] = makeStaticNjn(4, 2, {0, 1, 2, 3, 15, 14, 8, 15});
    player.setAssetSource(&source);

    ASSERT(player.loadText(kManifestScene), "player: scene with manifest loads");
    ASSERT(player.assets().ownedBitmapCount() == 1,
           "player: the resident manifest asset loaded at activate (the other is absent)");
    const uint8_t* px = player.assets().findBitmap("deadbeef");
    ASSERT(px != nullptr, "player: icon bitmap name resolves to owned pixels");
    // Index 15 became the icon transparent sentinel; opaque values are intact.
    ASSERT(px[0] == 0 && px[3] == 3 && px[5] == 14, "player: opaque pixels decode 1:1");
    ASSERT(px[4] == IconComponent::kTransparent && px[7] == IconComponent::kTransparent,
           "player: index-15 pixels map to the transparent sentinel");

    // The real AC3 check: the icon *entity's* bitmap pointer, resolved once
    // during entity deserialization, must point at the owned buffer — i.e. the
    // manifest loaded BEFORE entities parsed, not after.
    const ScenePlayer::World* w = player.world();
    ASSERT(w != nullptr, "player: world is live after load");
    const auto& icons = w->components<IconComponent>();
    ASSERT(icons.size() == 1, "player: the scene has one icon entity");
    const IconComponent* ic = w->get<IconComponent>(icons.entityAt(0));
    ASSERT(ic != nullptr && ic->bitmap == px,
           "player: icon entity resolved its bitmap at parse (manifest loaded first)");

    // Loading a manifest-less scene tears the previous owned assets down.
    ASSERT(player.loadText(kNoManifestScene), "player: a second scene loads");
    ASSERT(player.assets().ownedBitmapCount() == 0,
           "player: teardown freed the previous scene's owned assets");
    ASSERT(player.assets().findBitmap("deadbeef") == nullptr,
           "player: the old asset no longer resolves");
}

int main() {
    test_manifest_parse();
    test_manifest_round_trips_without_bytes();
    test_streaming_reader_agrees();
    test_registry_owned_entries();
    test_player_loads_and_frees_owned();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
