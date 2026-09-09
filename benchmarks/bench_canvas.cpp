#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/layer_compositor.hpp>
#include <enjin2/graphics/primitives.hpp>
#include <fstream>
#include <sys/stat.h>

int main() {
    ankerl::nanobench::Bench bench;
    bench.title("canvas").warmup(10).epochs(100).epochIterations(1);

    // --- Canvas4<128,128> benchmarks ---
    enjin2::Canvas4<128, 128> canvas4;

    bench.run("canvas4: setPixel", [&] {
        canvas4.setPixel(64, 64, enjin2::Pixel4(7));
        ankerl::nanobench::doNotOptimizeAway(canvas4.getPixel(64, 64));
    });

    bench.run("canvas4: clear", [&] {
        canvas4.clear(enjin2::Pixel4(0));
        ankerl::nanobench::doNotOptimizeAway(canvas4.getPixel(0, 0));
    });

    bench.run("canvas4: fillRect 32x32", [&] {
        canvas4.fillRect(0, 0, 32, 32, enjin2::Pixel4(3));
        ankerl::nanobench::doNotOptimizeAway(canvas4.getPixel(16, 16));
    });

    bench.run("canvas4: drawCircle r16", [&] {
        enjin2::Primitives<enjin2::Pixel4>::drawCircle(canvas4, 64, 64, 16, enjin2::Pixel4(5));
        ankerl::nanobench::doNotOptimizeAway(canvas4.getPixel(64, 48));
    });

    // Canvas4 blit — same-size sprite canvas blitted onto destination canvas
    // blit() requires matching template dimensions: Canvas4<W,H>.blit(const Canvas4<W,H>&, ...)
    enjin2::Canvas4<128, 128> sprite;
    sprite.clear(enjin2::Pixel4(5));

    bench.run("canvas4: blit 128x128 sprite", [&] {
        canvas4.blit(sprite, 0, 0, enjin2::Pixel4(0));
        ankerl::nanobench::doNotOptimizeAway(canvas4.getPixel(64, 64));
    });

    // --- Canvas8<128,128> benchmarks ---
    enjin2::Canvas8<128, 128> canvas8;

    bench.run("canvas8: setPixel", [&] {
        canvas8.setPixel(64, 64, 200);
        ankerl::nanobench::doNotOptimizeAway(canvas8.getPixel(64, 64));
    });

    bench.run("canvas8: fillRect 32x32", [&] {
        canvas8.fillRect(0, 0, 32, 32, 128);
        ankerl::nanobench::doNotOptimizeAway(canvas8.getPixel(16, 16));
    });

    // --- LayerCompositor<128,128> benchmark (Canvas4 only) ---
    enjin2::LayerCompositor<128, 128> compositor;

    bench.run("compositor: composite 5 layers", [&] {
        compositor.clearAll();
        compositor.layers[0].fillRect(0, 0, 128, 128, enjin2::Pixel4(1));
        compositor.composite();
        ankerl::nanobench::doNotOptimizeAway(compositor.output.getPixel(64, 64));
    });

    // --- compositeDirty full-frame recomposite (#66 hot path) ---
    // The on-device cost that #66 targets: an animating full-screen applet
    // dirties every tile, so recompositeTile runs over the whole canvas each
    // frame. Seed opaque content on the back layer and a half-transparent
    // pattern on the upper layers, then force a full-frame recomposite.
    auto* dirtyComp = new enjin2::LayerCompositor<128, 128>();
    dirtyComp->layers[0].fillRect(0, 0, 128, 128, enjin2::Pixel4(1));
    for (uint8_t l = 1; l < enjin2::ENJIN_LAYER_COUNT; ++l) {
        for (int16_t y = 0; y < 128; ++y)
            for (int16_t x = 0; x < 128; ++x)
                dirtyComp->layers[l].setPixel(
                    x, y, enjin2::Pixel4(((x + y + l) & 3) == 0 ? (l + 2) : 15));
    }
    if (enjin2::ENJIN_LAYER_COUNT >= 3) dirtyComp->setTint(2, enjin2::Remap::darken());

    bench.run("compositor: compositeDirty full-frame (5 layers)", [&] {
        for (uint8_t l = 0; l < enjin2::ENJIN_LAYER_COUNT; ++l)
            dirtyComp->invalidateLayer(l);
        dirtyComp->compositeDirty();
        ankerl::nanobench::doNotOptimizeAway(dirtyComp->output.getPixel(64, 64));
    });

    // Write JSON results
    mkdir("bench-results", 0755);
    std::ofstream out("bench-results/bench_canvas.json");
    ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, out);

    return 0;
}
