// bench_layered — layered-sprite storage-representation measurement (Tomodachi #98).
//
// Compares the two 4bpp arena representations frozen by ADR-0005:
//   * unpacked  — one palette byte per pixel in the arena; and
//   * packed    — two palette nibbles per byte in the arena.
//
// Both read the same `.njn` LIMG bytes (always one byte per pixel on disk). The
// benchmark loads a reference-sized asset into each representation, reports the
// arena footprint and load time from the built-in instrumentation, then drives
// a realistic drawing frame: several animated layered sprites drawn onto one
// compositor layer with a full four-layer dirty recomposite per frame.
//
// Frame times are reported as percentiles (p50/p95/p99/max) because the spec's
// acceptance signal is tail behaviour, not average throughput. This native
// harness cannot reproduce display DMA, active audio, or device PSRAM cache
// contention; ADR-0005 records that limitation and those on-device signals
// remain to be confirmed on hardware.
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <enjin2/graphics/asset_arena.hpp>
#include <enjin2/graphics/layer_compositor.hpp>
#include <enjin2/graphics/layered_asset.hpp>
#include <enjin2/graphics/layered_sprite.hpp>
#include <enjin2/instrumentation/clock.hpp>
#include <enjin2/graphics/njn2.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

using namespace enjin2;

namespace {

// ---------------------------------------------------------------------------
// Reference-sized fixture.
//
// The authored reference (`tomo_tune2.aseprite`) is a 96×110, 23-frame, 7-layer
// document whose cropped + deduplicated pixels total roughly 72 KiB at one byte
// per pixel (~37 KiB packed). It lives out of tree (#99), so this harness
// synthesises the same volume and shape: 96×110 canvas, 7 distinct parts, 23
// frames, unequal durations, one looping clip, and an intermittent sparkle.
// ---------------------------------------------------------------------------
constexpr uint16_t kCanvasW = 96;
constexpr uint16_t kCanvasH = 110;
constexpr int kParts = 7;
constexpr int kFrames = 23;

NjnLayered makeReferenceLikeAsset() {
    NjnLayered l;
    l.canvasW = kCanvasW;
    l.canvasH = kCanvasH;

    // Seven distinct full-canvas images: same pixel volume as the reference's
    // per-layer dedup, no cross-image reuse (each layer is distinct art).
    for (int i = 0; i < kParts; ++i) {
        NjnPartImage img;
        img.w = kCanvasW;
        img.h = kCanvasH;
        img.pixels.resize(static_cast<size_t>(kCanvasW) * kCanvasH);
        for (size_t px = 0; px < img.pixels.size(); ++px) {
            // Deterministic, opaque (never index 15) pattern distinct per image.
            img.pixels[px] = static_cast<uint8_t>(((px * 7u + i * 3u + (px >> 5)) % 15));
        }
        l.images.push_back(std::move(img));
    }

    for (int p = 0; p < kParts; ++p) {
        NjnPart part;
        std::memset(part.name, 0, sizeof(part.name));
        std::snprintf(part.name, sizeof(part.name), "layer%d", p);
        l.parts.push_back(part);
    }

    // Frame-major refs: parts drift by a couple of pixels per frame; the last
    // part is an intermittent sparkle, invisible on most frames.
    for (int f = 0; f < kFrames; ++f) {
        for (int p = 0; p < kParts; ++p) {
            NjnFramePartRef ref{};
            if (p == kParts - 1 && (f % 4) != 0) {
                ref.imageIndex = NJN2_LAYERED_INVISIBLE;
                ref.offsetX = 0;
                ref.offsetY = 0;
            } else {
                ref.imageIndex = static_cast<uint16_t>(p);
                ref.offsetX = static_cast<int16_t>((f + p) % 5 - 2);
                ref.offsetY = static_cast<int16_t>((f * 2 + p) % 5 - 2);
            }
            l.refs.push_back(ref);
        }
    }

    l.durations.assign(kFrames, 60);
    l.durations[0] = 200;
    l.durations[kFrames - 1] = 200;

    NjnClip clip;
    std::memset(clip.name, 0, sizeof(clip.name));
    std::memcpy(clip.name, "default", 7);
    clip.loopMode = NjnLoopMode::Loop;
    for (int f = 0; f < kFrames; ++f) {
        clip.frames.push_back({static_cast<uint16_t>(f), l.durations[f], 0});
    }
    l.clips.push_back(std::move(clip));
    return l;
}

std::vector<uint8_t> encode(const NjnLayered& asset) {
    NjnV2Writer w;
    njn2WriteLayered(w, asset);
    std::vector<uint8_t> bytes;
    w.finalise(bytes);
    return bytes;
}

// ---------------------------------------------------------------------------
// One loaded representation: bounded arena + store + bound sprite instances.
// ---------------------------------------------------------------------------
struct Loaded {
    std::unique_ptr<AssetArena> arena;
    std::unique_ptr<LayeredAssetStore> store;
    std::vector<LayeredSprite> sprites;
    const LayeredAsset* view = nullptr;
    LayeredAssetStore::Handle handle = LayeredAssetStore::INVALID_HANDLE;
    size_t arenaBytes = 0;
    uint64_t loadMicros = 0;
    uint32_t allocationFailures = 0;
};

Loaded loadRepresentation(const std::vector<uint8_t>& bytes, PixelStorage storage,
                          int spriteCount) {
    Loaded out;
    out.arena = std::make_unique<AssetArena>();
    out.arena->allocateBacking(AssetArena::DEFAULT_CAPACITY);
    out.store = std::make_unique<LayeredAssetStore>(*out.arena, storage);
    out.store->setClock(&steadyMicros);

    out.handle = out.store->loadFromMemory(bytes.data(), bytes.size());
    out.view = out.store->retain(out.handle);
    out.arenaBytes = out.store->currentBytes();
    out.loadMicros = out.store->metrics().loadMicrosTotal;
    out.allocationFailures = out.store->metrics().allocationFailures;

    out.sprites.resize(static_cast<size_t>(spriteCount));
    for (auto& s : out.sprites) s.bind(out.view);
    return out;
}

// A frame-time sample set: draws every sprite onto layer 0, then composites the
// four device compositor layers over the whole dirty canvas.
struct Workload {
    LayerCompositor<160, 160>* comp;
    Loaded* loaded;
    uint16_t frame = 0;

    void run() {
        comp->setBackdrop(0, Pixel4(0));
        int i = 0;
        for (auto& s : loaded->sprites) {
            const uint16_t fr = static_cast<uint16_t>((frame + i * 3) % kFrames);
            s.setFrame(fr);
            s.setPosition(static_cast<int16_t>(8 + i * 22),
                          static_cast<int16_t>(8 + ((frame * 2 + i * 7) % 90)));
            s.draw(comp->layers[0]);
            ++i;
        }
        comp->compositeDirty();
        ++frame;
    }
};

struct Percentiles {
    uint64_t p50 = 0, p95 = 0, p99 = 0, max = 0;
};

Percentiles measureFrames(Workload& w, int frames, std::vector<uint64_t>& samples) {
    samples.clear();
    samples.reserve(static_cast<size_t>(frames));
    for (int f = 0; f < frames; ++f) {
        const uint64_t t0 = steadyMicros();
        w.run();
        samples.push_back(steadyMicros() - t0);
    }
    std::vector<uint64_t> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    auto at = [&](double q) {
        const size_t idx = static_cast<size_t>(q * (sorted.size() - 1) + 0.5);
        return sorted[idx];
    };
    Percentiles p;
    p.p50 = at(0.50);
    p.p95 = at(0.95);
    p.p99 = at(0.99);
    p.max = sorted.back();
    ankerl::nanobench::doNotOptimizeAway(w.comp->output.getPixel(80, 80).value);
    return p;
}

const char* storageName(PixelStorage s) {
    return s == PixelStorage::Packed4bpp ? "packed" : "unpacked";
}

} // namespace

int main() {
    const std::vector<uint8_t> bytes = encode(makeReferenceLikeAsset());
    std::printf("=== bench_layered: storage representation (#98) ===\n");
    std::printf("fixture: %ux%u canvas, %d parts, %d frames, .njn %zu bytes\n",
                kCanvasW, kCanvasH, kParts, kFrames, bytes.size());

    const PixelStorage paths[2] = {PixelStorage::Unpacked8, PixelStorage::Packed4bpp};
    Loaded loaded[2] = {
        loadRepresentation(bytes, paths[0], 6),
        loadRepresentation(bytes, paths[1], 6),
    };

    for (int i = 0; i < 2; ++i) {
        std::printf("  %-8s arena: %6zu bytes  load: %5llu us  alloc failures: %u\n",
                    storageName(paths[i]), loaded[i].arenaBytes,
                    static_cast<unsigned long long>(loaded[i].loadMicros),
                    loaded[i].allocationFailures);
    }
    std::printf("  packed arena is %zu%% of unpacked\n",
                loaded[1].arenaBytes * 100 / loaded[0].arenaBytes);

    LayerCompositor<160, 160> comp;
    comp.setVisible(4, false);  // four device compositor canvases
    for (uint8_t l = 1; l <= 3; ++l) {
        for (int16_t y = 0; y < 160; y += 4) {
            for (int16_t x = 0; x < 160; x += 4) {
                comp.layers[l].setPixel(x, y, Pixel4(static_cast<uint8_t>((x + y + l) % 8)));
            }
        }
    }

    const int kSampleFrames = 4000;
    std::vector<uint64_t> samples;
    Percentiles pct[2];
    Workload w{&comp, nullptr, 0};

    for (int i = 0; i < 2; ++i) {
        w.loaded = &loaded[i];
        w.frame = 0;
        pct[i] = measureFrames(w, kSampleFrames, samples);
    }

    for (int i = 0; i < 2; ++i) {
        std::printf("  %-8s frame us  p50: %4llu  p95: %4llu  p99: %4llu  max: %5llu\n",
                    storageName(paths[i]),
                    static_cast<unsigned long long>(pct[i].p50),
                    static_cast<unsigned long long>(pct[i].p95),
                    static_cast<unsigned long long>(pct[i].p99),
                    static_cast<unsigned long long>(pct[i].max));
    }

    // Nanobench confirmation of per-frame ops/sec, same workload.
    ankerl::nanobench::Bench bench;
    bench.title("layered storage (#98)").warmup(20).epochs(50).epochIterations(1);
    for (int i = 0; i < 2; ++i) {
        w.loaded = &loaded[i];
        w.frame = 0;
        bench.run(std::string("frame: ") + storageName(paths[i]), [&] { w.run(); });
    }

    mkdir("bench-results", 0755);
    std::ofstream out("bench-results/bench_layered.json");
    ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, out);

    return 0;
}
