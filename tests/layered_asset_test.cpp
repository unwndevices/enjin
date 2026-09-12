/**
 * @file layered_asset_test.cpp
 * @brief Shared C++ layered-asset owner/loader + bounded per-applet arena
 *        (ADR-0004, Tomodachi #95).
 *
 * Test IDs:
 *   LAYLA-01  A layered asset loads into the arena and exposes its canvas,
 *            images, parts, frame references, offsets, invisibility, durations
 *            and clips; arena accounting reflects the copy.
 *   LAYLA-02  Part pixels are owned by the arena, not the input buffer.
 *   LAYLA-03  A malformed asset is rejected atomically (no handle, unchanged
 *            arena/registry, prior asset still valid).
 *   LAYLA-04  Insufficient capacity is rejected atomically and counted.
 *   LAYLA-05  A full registry is rejected atomically.
 *   LAYLA-06  Multiple retained instances share the same immutable view/pixels.
 *   LAYLA-07  Freeing a handle cannot dangle retained instances.
 *   LAYLA-08  reclaimAll() reclaims the arena in full.
 *   LAYLA-09  Freeing the arena's tip asset reclaims its bytes.
 *   LAYLA-10  Unknown chunks are skipped; the asset still loads.
 *   LAYLA-11  An unsupported layered schema version is rejected atomically.
 *   LAYLA-12  peakBytes() tracks the high-water mark.
 *   LAYLA-13  A duplicate required chunk is rejected atomically.
 *   LAYLA-14  An out-of-range part-image reference is rejected atomically.
 *   LAYLA-15  A truncated record is rejected atomically.
 *   LAYLA-16  A malformed directory entry is rejected atomically.
 *   LAYLA-17  The arena rejects a byte count that would overflow its bounds.
 *   LAYLA-18  Instrumentation reports asset count, current/peak bytes, load
 *            count, allocation failures and injected load time; reset clears
 *            the counters without touching the registry or arena.
 *   LAYLA-19  Packed 4bpp and unpacked 8-bit arena representations decode to
 *            identical palette indices; packed uses fewer bytes.
 *   LAYLA-20  A default store uses the frozen default representation.
 */

#include <enjin2/graphics/asset_arena.hpp>
#include <enjin2/graphics/layered_asset.hpp>
#include <enjin2/graphics/njn2.hpp>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <algorithm>
#include <limits>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                             \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, (msg)); \
            ++failures;                                               \
        } else {                                                      \
            ++passes;                                                 \
        }                                                             \
    } while (0)

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

/// 4x4 canvas, 2 frames, 2 parts, 2 images, invisible + moving-offset refs,
/// unequal durations, one "default" loop clip. Pixels are distinctive so the
/// ownership-copy test can prove the arena copy is independent.
static NjnLayered makeFixture() {
    NjnLayered l;
    l.schemaVersion = NJN2_LAYERED_SCHEMA_VERSION;
    l.canvasW = 4;
    l.canvasH = 4;

    NjnPartImage img0;
    img0.w = 2;
    img0.h = 2;
    img0.pixels = {0, 1, 2, 3};
    l.images.push_back(std::move(img0));

    NjnPartImage img1;
    img1.w = 1;
    img1.h = 1;
    img1.pixels = {5};
    l.images.push_back(std::move(img1));

    NjnPart body;
    std::memset(body.name, 0, sizeof(body.name));
    std::memcpy(body.name, "body", 4);
    l.parts.push_back(body);

    NjnPart eye;
    std::memset(eye.name, 0, sizeof(eye.name));
    std::memcpy(eye.name, "eye", 3);
    l.parts.push_back(eye);

    l.refs.push_back({0, 0, 0});
    l.refs.push_back({1, 1, -2});
    l.refs.push_back({0, -1, 0});
    l.refs.push_back({NJN2_LAYERED_INVISIBLE, 0, 0});

    l.durations = {100, 150};

    NjnClip clip;
    std::memset(clip.name, 0, sizeof(clip.name));
    std::memcpy(clip.name, "default", 7);
    clip.loopMode = NjnLoopMode::Loop;
    clip.frames.push_back({0, 100, 0});
    clip.frames.push_back({1, 150, 0});
    l.clips.push_back(std::move(clip));

    return l;
}

static std::vector<uint8_t> writeLayered(const NjnLayered& l) {
    NjnV2Writer w;
    njn2WriteLayered(w, l);
    std::vector<uint8_t> out;
    w.finalise(out);
    return out;
}

static uint32_t chunkDataOffset(const std::vector<uint8_t>& buf, int entryIndex) {
    const uint8_t* e = buf.data() + NJN2_FILE_HEADER_SIZE +
                       entryIndex * NJN2_DIR_ENTRY_SIZE + 4;
    return static_cast<uint32_t>(e[0]) | (static_cast<uint32_t>(e[1]) << 8) |
           (static_cast<uint32_t>(e[2]) << 16) | (static_cast<uint32_t>(e[3]) << 24);
}

static void setChunkSize(std::vector<uint8_t>& buf, int entryIndex, uint32_t newSize) {
    uint8_t* e = buf.data() + NJN2_FILE_HEADER_SIZE +
                 entryIndex * NJN2_DIR_ENTRY_SIZE + 8;
    e[0] = static_cast<uint8_t>(newSize & 0xFF);
    e[1] = static_cast<uint8_t>((newSize >> 8) & 0xFF);
    e[2] = static_cast<uint8_t>((newSize >> 16) & 0xFF);
    e[3] = static_cast<uint8_t>((newSize >> 24) & 0xFF);
}

// ---------------------------------------------------------------------------
// LAYLA-01: load + observable records + accounting
// ---------------------------------------------------------------------------
static void test_LAYLA_01_load_and_accounting() {
    printf("--- LAYLA-01: load + records + accounting ---\n");
    AssetArena arena;
    ASSERT(arena.allocateBacking(256 * 1024), "LAYLA-01: backing allocated");
    LayeredAssetStore store(arena);

    const std::vector<uint8_t> buf = writeLayered(makeFixture());
    std::string err;
    const auto h = store.loadFromMemory(buf.data(), buf.size(), &err);
    ASSERT(h != LayeredAssetStore::INVALID_HANDLE, err.empty() ? "load ok" : err.c_str());

    const LayeredAsset* a = store.get(h);
    ASSERT(a != nullptr, "LAYLA-01: asset exposed to caller");
    ASSERT(a->canvasW == 4 && a->canvasH == 4, "LAYLA-01: canvas extent");
    ASSERT(a->numFrames == 2 && a->numParts == 2 && a->numImages == 2,
           "LAYLA-01: counts");
    ASSERT(a->numClips == 1, "LAYLA-01: one clip");

    ASSERT(a->images[0].w == 2 && a->images[0].h == 2, "LAYLA-01: image0 dims");
    ASSERT(a->images[0].pixels[0] == 0 && a->images[0].pixels[3] == 3,
           "LAYLA-01: image0 pixels");
    ASSERT(a->images[1].pixels[0] == 5, "LAYLA-01: image1 pixel");

    ASSERT(std::strncmp(a->parts[0].name, "body", 4) == 0, "LAYLA-01: part0 name");
    ASSERT(std::strncmp(a->parts[1].name, "eye", 3) == 0, "LAYLA-01: part1 name");

    ASSERT(a->refs[1].imageIndex == 1 && a->refs[1].offsetX == 1 &&
           a->refs[1].offsetY == -2, "LAYLA-01: moving negative offset");
    ASSERT(a->refs[3].imageIndex == NJN2_LAYERED_INVISIBLE, "LAYLA-01: invisibility");
    ASSERT(a->durations[1] == 150, "LAYLA-01: durations");

    ASSERT(std::strncmp(a->clips[0].name, "default", 7) == 0, "LAYLA-01: clip name");
    ASSERT(a->clips[0].loopMode == NjnLoopMode::Loop, "LAYLA-01: clip loop");
    ASSERT(a->clips[0].numFrames == 2, "LAYLA-01: clip frames");
    ASSERT(a->clips[0].frames[1].durationMs == 150, "LAYLA-01: clip frame duration");

    ASSERT(store.assetCount() == 1, "LAYLA-01: one asset registered");
    ASSERT(store.currentBytes() > 0, "LAYLA-01: arena accounting reflects copy");
    ASSERT(store.currentBytes() == arena.used(), "LAYLA-01: store mirrors arena");
    ASSERT(store.loadCount() == 1, "LAYLA-01: load counted");
}

// ---------------------------------------------------------------------------
// LAYLA-02: pixels are arena-owned copies
// ---------------------------------------------------------------------------
static void test_LAYLA_02_pixels_owned() {
    printf("--- LAYLA-02: arena owns the pixel copy ---\n");
    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);

    std::vector<uint8_t> buf = writeLayered(makeFixture());
    const auto h = store.loadFromMemory(buf.data(), buf.size());
    const LayeredAsset* a = store.get(h);
    ASSERT(a != nullptr, "LAYLA-02: loaded");

    // Scramble the source bytes: the arena copy must not observe it.
    std::fill(buf.begin(), buf.end(), 0xAA);
    ASSERT(a->images[0].pixels[0] == 0 && a->images[0].pixels[1] == 1 &&
           a->images[0].pixels[2] == 2 && a->images[0].pixels[3] == 3,
           "LAYLA-02: pixels copied, not aliased");
}

// ---------------------------------------------------------------------------
// LAYLA-03: malformed reject is atomic
// ---------------------------------------------------------------------------
static void test_LAYLA_03_malformed_atomic() {
    printf("--- LAYLA-03: malformed reject is atomic ---\n");
    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);

    const std::vector<uint8_t> good = writeLayered(makeFixture());
    const auto h = store.loadFromMemory(good.data(), good.size());
    ASSERT(h != LayeredAssetStore::INVALID_HANDLE, "LAYLA-03: prior asset loads");
    const size_t used = store.currentBytes();
    const int count = store.assetCount();

    std::vector<uint8_t> bad = good;
    bad[0] = 'X';  // break magic
    std::string err;
    const auto rejected = store.loadFromMemory(bad.data(), bad.size(), &err);
    ASSERT(rejected == LayeredAssetStore::INVALID_HANDLE, "LAYLA-03: rejected");
    ASSERT(!err.empty(), "LAYLA-03: clear failure reason");
    ASSERT(store.currentBytes() == used, "LAYLA-03: arena usage unchanged");
    ASSERT(store.assetCount() == count, "LAYLA-03: registry unchanged");
    ASSERT(store.get(h) != nullptr, "LAYLA-03: prior asset still valid");
    ASSERT(store.get(h)->images[0].pixels[0] == 0, "LAYLA-03: prior pixels intact");
}

// ---------------------------------------------------------------------------
// LAYLA-04: capacity reject is atomic and counted
// ---------------------------------------------------------------------------
static void test_LAYLA_04_capacity_atomic() {
    printf("--- LAYLA-04: capacity reject is atomic ---\n");
    const std::vector<uint8_t> buf = writeLayered(makeFixture());

    // Measure one asset's footprint on a generous arena.
    AssetArena gauge;
    gauge.allocateBacking(256 * 1024);
    LayeredAssetStore gaugeStore(gauge);
    const auto gh = gaugeStore.loadFromMemory(buf.data(), buf.size());
    ASSERT(gh != LayeredAssetStore::INVALID_HANDLE, "LAYLA-04: gauge load");
    const size_t one = gaugeStore.currentBytes();

    AssetArena small;
    small.allocateBacking(one);  // exactly one asset
    LayeredAssetStore store(small);
    const auto h1 = store.loadFromMemory(buf.data(), buf.size());
    ASSERT(h1 != LayeredAssetStore::INVALID_HANDLE, "LAYLA-04: first fits");
    const size_t used = store.currentBytes();

    std::string err;
    const auto h2 = store.loadFromMemory(buf.data(), buf.size(), &err);
    ASSERT(h2 == LayeredAssetStore::INVALID_HANDLE, "LAYLA-04: second rejected");
    ASSERT(store.currentBytes() == used, "LAYLA-04: arena usage unchanged");
    ASSERT(store.assetCount() == 1, "LAYLA-04: registry unchanged");
    ASSERT(store.allocationFailures() == 1, "LAYLA-04: failure counted");
    ASSERT(store.get(h1) != nullptr, "LAYLA-04: prior asset still valid");
}

// ---------------------------------------------------------------------------
// LAYLA-05: full registry reject is atomic
// ---------------------------------------------------------------------------
static void test_LAYLA_05_store_full() {
    printf("--- LAYLA-05: full registry reject is atomic ---\n");
    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);
    const std::vector<uint8_t> buf = writeLayered(makeFixture());

    int loaded = 0;
    for (int i = 0; i < LayeredAssetStore::MAX_ASSETS; ++i) {
        if (store.loadFromMemory(buf.data(), buf.size()) !=
            LayeredAssetStore::INVALID_HANDLE) {
            ++loaded;
        }
    }
    ASSERT(loaded == LayeredAssetStore::MAX_ASSETS, "LAYLA-05: all slots filled");
    const size_t used = store.currentBytes();

    std::string err;
    const auto overflow = store.loadFromMemory(buf.data(), buf.size(), &err);
    ASSERT(overflow == LayeredAssetStore::INVALID_HANDLE, "LAYLA-05: overflow rejected");
    ASSERT(store.currentBytes() == used, "LAYLA-05: arena usage unchanged");
    ASSERT(store.assetCount() == LayeredAssetStore::MAX_ASSETS, "LAYLA-05: count stable");
}

// ---------------------------------------------------------------------------
// LAYLA-06: retained instances share immutable data
// ---------------------------------------------------------------------------
static void test_LAYLA_06_shared_immutable() {
    printf("--- LAYLA-06: retained instances share immutable data ---\n");
    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);
    const std::vector<uint8_t> buf = writeLayered(makeFixture());
    const auto h = store.loadFromMemory(buf.data(), buf.size());

    const LayeredAsset* first = store.retain(h);
    const LayeredAsset* second = store.retain(h);
    ASSERT(first != nullptr && second != nullptr, "LAYLA-06: retains succeed");
    ASSERT(first == second, "LAYLA-06: one shared immutable view");
    ASSERT(first->images[0].pixels == second->images[0].pixels,
           "LAYLA-06: shared pixel pointer");
}

// ---------------------------------------------------------------------------
// LAYLA-07: free cannot dangle retained instances
// ---------------------------------------------------------------------------
static void test_LAYLA_07_free_no_dangle() {
    printf("--- LAYLA-07: free cannot dangle retained instances ---\n");
    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);
    const std::vector<uint8_t> buf = writeLayered(makeFixture());
    const auto h = store.loadFromMemory(buf.data(), buf.size());

    const LayeredAsset* retained = store.retain(h);
    ASSERT(retained != nullptr, "LAYLA-07: retained before free");
    const uint8_t* pixels = retained->images[0].pixels;

    store.free(h);
    ASSERT(store.get(h) == nullptr, "LAYLA-07: public handle invalidated");
    ASSERT(store.retain(h) == nullptr, "LAYLA-07: cannot retain a freed handle");
    ASSERT(store.assetCount() == 1, "LAYLA-07: slot held by the retained instance");
    ASSERT(retained->images[0].pixels == pixels, "LAYLA-07: retained view still valid");
    ASSERT(retained->images[0].pixels[2] == 2, "LAYLA-07: retained pixels intact");

    store.release(h);
    ASSERT(store.assetCount() == 0, "LAYLA-07: slot reclaimed after last release");
    ASSERT(store.currentBytes() == 0, "LAYLA-07: tip bytes reclaimed on release");
}

// ---------------------------------------------------------------------------
// LAYLA-08: reclaimAll
// ---------------------------------------------------------------------------
static void test_LAYLA_08_reclaim_all() {
    printf("--- LAYLA-08: reclaimAll in full ---\n");
    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);
    const std::vector<uint8_t> buf = writeLayered(makeFixture());
    const auto a = store.loadFromMemory(buf.data(), buf.size());
    const auto b = store.loadFromMemory(buf.data(), buf.size());
    ASSERT(a != LayeredAssetStore::INVALID_HANDLE && b != LayeredAssetStore::INVALID_HANDLE,
           "LAYLA-08: two assets loaded");
    ASSERT(store.currentBytes() > 0, "LAYLA-08: bytes in use");

    store.reclaimAll();
    ASSERT(store.assetCount() == 0, "LAYLA-08: registry cleared");
    ASSERT(store.currentBytes() == 0, "LAYLA-08: arena reclaimed in full");
    ASSERT(arena.valid(), "LAYLA-08: backing store retained");
    ASSERT(arena.capacity() > 0, "LAYLA-08: capacity retained for reuse");
}

// ---------------------------------------------------------------------------
// LAYLA-09: tip-free reclaims bytes
// ---------------------------------------------------------------------------
static void test_LAYLA_09_tip_reclaim() {
    printf("--- LAYLA-09: tip free reclaims bytes ---\n");
    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);
    const std::vector<uint8_t> buf = writeLayered(makeFixture());
    const auto h = store.loadFromMemory(buf.data(), buf.size());
    ASSERT(store.currentBytes() > 0, "LAYLA-09: bytes in use");

    store.free(h);  // no retained owners
    ASSERT(store.currentBytes() == 0, "LAYLA-09: tip asset reclaimed on free");
    ASSERT(store.assetCount() == 0, "LAYLA-09: slot cleared");
}

// ---------------------------------------------------------------------------
// LAYLA-10: unknown chunks skipped
// ---------------------------------------------------------------------------
static void test_LAYLA_10_unknown_chunk() {
    printf("--- LAYLA-10: unknown chunks skipped ---\n");
    NjnV2Writer w;
    njn2WriteLayered(w, makeFixture());
    w.beginChunk(njnTag('Z', 'Z', 'Z', 'Z'));
    w.writeU8(0xDE);
    w.writeU8(0xAD);
    w.endChunk();
    std::vector<uint8_t> buf;
    w.finalise(buf);

    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);
    const auto h = store.loadFromMemory(buf.data(), buf.size());
    ASSERT(h != LayeredAssetStore::INVALID_HANDLE, "LAYLA-10: loads despite unknown chunk");
    ASSERT(store.get(h)->canvasW == 4, "LAYLA-10: canvas decoded");
}

// ---------------------------------------------------------------------------
// LAYLA-11: unsupported schema rejected atomically
// ---------------------------------------------------------------------------
static void test_LAYLA_11_schema() {
    printf("--- LAYLA-11: unsupported schema rejected atomically ---\n");
    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);

    std::vector<uint8_t> buf = writeLayered(makeFixture());
    buf[chunkDataOffset(buf, 0)] = 2;  // schemaVersion = 2
    const size_t used = store.currentBytes();
    const auto h = store.loadFromMemory(buf.data(), buf.size());
    ASSERT(h == LayeredAssetStore::INVALID_HANDLE, "LAYLA-11: rejected");
    ASSERT(store.currentBytes() == used, "LAYLA-11: arena unchanged");
    ASSERT(store.assetCount() == 0, "LAYLA-11: nothing published");
}

// ---------------------------------------------------------------------------
// LAYLA-12: peak high-water mark
// ---------------------------------------------------------------------------
static void test_LAYLA_12_peak() {
    printf("--- LAYLA-12: peak high-water mark ---\n");
    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);
    const std::vector<uint8_t> buf = writeLayered(makeFixture());

    const auto a = store.loadFromMemory(buf.data(), buf.size());
    const size_t afterFirst = store.peakBytes();
    ASSERT(a != LayeredAssetStore::INVALID_HANDLE, "LAYLA-12: first load");
    ASSERT(afterFirst == store.currentBytes(), "LAYLA-12: peak equals current");

    const auto b = store.loadFromMemory(buf.data(), buf.size());
    ASSERT(b != LayeredAssetStore::INVALID_HANDLE, "LAYLA-12: second load");
    ASSERT(store.peakBytes() > afterFirst, "LAYLA-12: peak advanced");

    const size_t peakBefore = store.peakBytes();
    store.reclaimAll();
    ASSERT(store.currentBytes() == 0, "LAYLA-12: reclaimed");
    ASSERT(store.peakBytes() == peakBefore, "LAYLA-12: peak retains high-water");
}

// ---------------------------------------------------------------------------
// LAYLA-13: duplicate required chunk rejected atomically
// ---------------------------------------------------------------------------
static void test_LAYLA_13_duplicate_chunk() {
    printf("--- LAYLA-13: duplicate required chunk rejected ---\n");
    NjnV2Writer w;
    njn2WriteLayered(w, makeFixture());
    w.beginChunk(NJN2_CHUNK_LHDR);  // second LHDR
    w.writeU8(NJN2_LAYERED_SCHEMA_VERSION);
    w.writeU16LE(4); w.writeU16LE(4);
    w.writeU16LE(2); w.writeU16LE(2); w.writeU16LE(2);
    w.endChunk();
    std::vector<uint8_t> buf;
    w.finalise(buf);

    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);
    const size_t used = store.currentBytes();
    std::string err;
    const auto h = store.loadFromMemory(buf.data(), buf.size(), &err);
    ASSERT(h == LayeredAssetStore::INVALID_HANDLE, "LAYLA-13: rejected");
    ASSERT(!err.empty(), "LAYLA-13: reason reported");
    ASSERT(store.currentBytes() == used && store.assetCount() == 0,
           "LAYLA-13: nothing published");
}

// ---------------------------------------------------------------------------
// LAYLA-14: invalid part-image reference rejected atomically
// ---------------------------------------------------------------------------
static void test_LAYLA_14_invalid_reference() {
    printf("--- LAYLA-14: invalid reference rejected ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    buf[chunkDataOffset(buf, 3)] = 0x05;  // first LREF imageIndex = 5 (>= 2)

    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);
    const size_t used = store.currentBytes();
    const auto h = store.loadFromMemory(buf.data(), buf.size());
    ASSERT(h == LayeredAssetStore::INVALID_HANDLE, "LAYLA-14: rejected");
    ASSERT(store.currentBytes() == used && store.assetCount() == 0,
           "LAYLA-14: nothing published");
}

// ---------------------------------------------------------------------------
// LAYLA-15: truncated record rejected atomically
// ---------------------------------------------------------------------------
static void test_LAYLA_15_truncated_record() {
    printf("--- LAYLA-15: truncated record rejected ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    setChunkSize(buf, 1, 6);  // LIMG declares 6 bytes instead of 13

    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);
    const size_t used = store.currentBytes();
    const auto h = store.loadFromMemory(buf.data(), buf.size());
    ASSERT(h == LayeredAssetStore::INVALID_HANDLE, "LAYLA-15: rejected");
    ASSERT(store.currentBytes() == used && store.assetCount() == 0,
           "LAYLA-15: nothing published");
}

// ---------------------------------------------------------------------------
// LAYLA-16: malformed directory entry rejected atomically
// ---------------------------------------------------------------------------
static void test_LAYLA_16_bad_directory() {
    printf("--- LAYLA-16: malformed directory entry rejected ---\n");
    std::vector<uint8_t> buf = writeLayered(makeFixture());
    // Corrupt the first directory entry's offset beyond the file.
    uint8_t* e = buf.data() + NJN2_FILE_HEADER_SIZE + 4;
    e[0] = 0xFF; e[1] = 0xFF; e[2] = 0xFF; e[3] = 0x0F;

    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);
    const size_t used = store.currentBytes();
    const auto h = store.loadFromMemory(buf.data(), buf.size());
    ASSERT(h == LayeredAssetStore::INVALID_HANDLE, "LAYLA-16: rejected");
    ASSERT(store.currentBytes() == used && store.assetCount() == 0,
           "LAYLA-16: nothing published");
}

// ---------------------------------------------------------------------------
// LAYLA-17: arena byte-count overflow is rejected, not wrapped
// ---------------------------------------------------------------------------
static void test_LAYLA_17_arena_overflow() {
    printf("--- LAYLA-17: arena overflow-safe bounds ---\n");
    AssetArena arena;
    arena.allocateBacking(1024);
    const size_t huge = std::numeric_limits<size_t>::max();
    ASSERT(!arena.canAllocate(huge, AssetArena::ALIGNMENT),
           "LAYLA-17: SIZE_MAX cannot fit");
    ASSERT(arena.allocate(huge, AssetArena::ALIGNMENT) == nullptr,
           "LAYLA-17: oversized allocate returns nullptr");
    ASSERT(arena.used() == 0, "LAYLA-17: failed allocate does not advance");
}

// ---------------------------------------------------------------------------
// LAYLA-18: instrumentation reports counts, bytes, failures and load time
// ---------------------------------------------------------------------------
static uint64_t g_fakeMicros = 0;
static uint64_t fakeClockStep100() {
    g_fakeMicros += 100;
    return g_fakeMicros;
}

static void test_LAYLA_18_metrics() {
    printf("--- LAYLA-18: instrumentation metrics ---\n");
    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);
    store.setClock(&fakeClockStep100);
    g_fakeMicros = 0;

    const std::vector<uint8_t> buf = writeLayered(makeFixture());
    const auto h = store.loadFromMemory(buf.data(), buf.size());
    ASSERT(h != LayeredAssetStore::INVALID_HANDLE, "LAYLA-18: load ok");

    LayeredAssetMetrics m = store.metrics();
    ASSERT(m.assetCount == 1, "LAYLA-18: asset count reported");
    ASSERT(m.currentBytes == arena.used() && m.currentBytes > 0,
           "LAYLA-18: current bytes mirror arena");
    ASSERT(m.peakBytes >= m.currentBytes, "LAYLA-18: peak >= current");
    ASSERT(m.loadCount == 1, "LAYLA-18: load count reported");
    ASSERT(m.allocationFailures == 0, "LAYLA-18: no failure reported");
    ASSERT(m.loadMicrosTotal == 100, "LAYLA-18: injected load time reported");
    ASSERT(m.loadMicrosMax == 100, "LAYLA-18: injected max load time reported");

    // A second load accumulates total and keeps the max.
    const auto h2 = store.loadFromMemory(buf.data(), buf.size());
    ASSERT(h2 != LayeredAssetStore::INVALID_HANDLE, "LAYLA-18: second load ok");
    m = store.metrics();
    ASSERT(m.loadCount == 2, "LAYLA-18: two loads counted");
    ASSERT(m.loadMicrosTotal == 200, "LAYLA-18: load time accumulates");
    ASSERT(m.loadMicrosMax == 100, "LAYLA-18: max stays the longest load");

    // resetMetrics clears counters, never arena bytes.
    store.resetMetrics();
    m = store.metrics();
    ASSERT(m.loadCount == 0 && m.loadMicrosTotal == 0 && m.loadMicrosMax == 0,
           "LAYLA-18: reset clears load metrics");
    ASSERT(m.assetCount == 2 && m.currentBytes > 0,
           "LAYLA-18: reset leaves registry and arena alone");

    // A null clock disables timing but not loading.
    store.setClock(nullptr);
    const auto h3 = store.loadFromMemory(buf.data(), buf.size());
    ASSERT(h3 != LayeredAssetStore::INVALID_HANDLE, "LAYLA-18: load without clock");
    ASSERT(store.metrics().loadMicrosTotal == 0,
           "LAYLA-18: disabled clock reports zero time");
}

// ---------------------------------------------------------------------------
// LAYLA-19: packed and unpacked arena representations decode identically
// ---------------------------------------------------------------------------
static NjnLayered makeStorageFixture() {
    // The 2x2 fixture's images are smaller than the 32-byte arena alignment,
    // so add one large image whose packed form (128 B) is half its unpacked
    // form (256 B) and both exceed one alignment quantum — the case the
    // measurement actually cares about.
    NjnLayered l = makeFixture();
    NjnPartImage big;
    big.w = 16;
    big.h = 16;
    big.pixels.resize(16 * 16);
    for (size_t i = 0; i < big.pixels.size(); ++i) {
        big.pixels[i] = static_cast<uint8_t>(i % 15);
    }
    l.images.push_back(std::move(big));
    return l;
}

static void test_LAYLA_19_storage_paths() {
    printf("--- LAYLA-19: packed vs unpacked pixel storage ---\n");
    const std::vector<uint8_t> buf = writeLayered(makeStorageFixture());

    AssetArena unpackedArena;
    unpackedArena.allocateBacking(256 * 1024);
    LayeredAssetStore unpacked(unpackedArena, PixelStorage::Unpacked8);

    AssetArena packedArena;
    packedArena.allocateBacking(256 * 1024);
    LayeredAssetStore packed(packedArena, PixelStorage::Packed4bpp);

    const auto hu = unpacked.loadFromMemory(buf.data(), buf.size());
    const auto hp = packed.loadFromMemory(buf.data(), buf.size());
    ASSERT(hu != LayeredAssetStore::INVALID_HANDLE, "LAYLA-19: unpacked load");
    ASSERT(hp != LayeredAssetStore::INVALID_HANDLE, "LAYLA-19: packed load");

    const LayeredAsset* u = unpacked.get(hu);
    const LayeredAsset* p = packed.get(hp);
    ASSERT(u != nullptr && p != nullptr, "LAYLA-19: both assets exposed");
    ASSERT(u->storage == PixelStorage::Unpacked8 &&
           p->storage == PixelStorage::Packed4bpp, "LAYLA-19: storage recorded");

    for (uint16_t img = 0; img < u->numImages; ++img) {
        const uint32_t count =
            static_cast<uint32_t>(u->images[img].w) * u->images[img].h;
        bool same = true;
        for (uint32_t idx = 0; idx < count; ++idx) {
            if (layeredPixelAt(u->images[img], u->storage, idx) !=
                layeredPixelAt(p->images[img], p->storage, idx)) {
                same = false;
                break;
            }
        }
        ASSERT(same, "LAYLA-19: packed decodes to the same palette indices");
    }

    ASSERT(packed.currentBytes() < unpacked.currentBytes(),
           "LAYLA-19: packed uses fewer arena bytes");
}

// ---------------------------------------------------------------------------
// LAYLA-20: the frozen default representation is what a default store uses
// ---------------------------------------------------------------------------
static void test_LAYLA_20_default_storage() {
    printf("--- LAYLA-20: frozen default storage ---\n");
    AssetArena arena;
    arena.allocateBacking(256 * 1024);
    LayeredAssetStore store(arena);
    ASSERT(store.storage() == LayeredAssetStore::DEFAULT_STORAGE,
           "LAYLA-20: default store uses the frozen representation");

    const std::vector<uint8_t> buf = writeLayered(makeFixture());
    const auto h = store.loadFromMemory(buf.data(), buf.size());
    ASSERT(store.get(h)->storage == LayeredAssetStore::DEFAULT_STORAGE,
           "LAYLA-20: loaded asset carries the frozen representation");
}

int main() {
    printf("=== layered_asset_test: C++ owner/loader + per-applet arena (#95) ===\n");

    test_LAYLA_01_load_and_accounting();
    test_LAYLA_02_pixels_owned();
    test_LAYLA_03_malformed_atomic();
    test_LAYLA_04_capacity_atomic();
    test_LAYLA_05_store_full();
    test_LAYLA_06_shared_immutable();
    test_LAYLA_07_free_no_dangle();
    test_LAYLA_08_reclaim_all();
    test_LAYLA_09_tip_reclaim();
    test_LAYLA_10_unknown_chunk();
    test_LAYLA_11_schema();
    test_LAYLA_12_peak();
    test_LAYLA_13_duplicate_chunk();
    test_LAYLA_14_invalid_reference();
    test_LAYLA_15_truncated_record();
    test_LAYLA_16_bad_directory();
    test_LAYLA_17_arena_overflow();
    test_LAYLA_18_metrics();
    test_LAYLA_19_storage_paths();
    test_LAYLA_20_default_storage();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
