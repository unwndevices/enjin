// bench_shape: Shape fill rasterizer cost on the prototype Star at rest
// (Tomodachi #329, ADR-0016). nanobench reports instructions per op where the
// PMU is available; later tickets (#331 Shape group, #332 paint, #334
// Lattice) track these numbers.
//
//   cmake -DENJIN2_BUILD_BENCHMARKS=ON ... && ./benchmarks/bench_shape

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <fstream>
#include <sys/stat.h>

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/shape/shape_raster.hpp>

#include "shape_star_golden.hpp"

using namespace enjin2;
using namespace enjin2::shape;

namespace {

struct Loaded {
    FixedVec2 points[256];
    Ring      rings[4];
    Shape     shape;
};

void load(const star_golden::GoldenShape& g, Loaded& out)
{
    uint16_t at = 0;
    for (uint16_t r = 0; r < g.ringCount; ++r)
    {
        out.rings[r] = Ring{out.points + at, g.ringSizes[r], true};
        for (uint16_t i = 0; i < g.ringSizes[r]; ++i, ++at)
            out.points[at] = {Fixed::fromQ8_8(g.points[2 * at]), Fixed::fromQ8_8(g.points[2 * at + 1])};
    }
    out.shape.rings = out.rings;
    out.shape.ringCount = g.ringCount;
    out.shape.paint.rule = g.evenOdd ? FillRule::EvenOdd : FillRule::NonZero;
}

Loaded   star[13];
uint32_t words[13][Rasterizer::kMaxMaskWidth / 32 * 160];
RowMask  masks[13];

} // namespace

int main()
{
    static Rasterizer raster;
    static Canvas4<160, 160> canvas;
    const Rect clip(0, 0, 160, 160);
    for (unsigned i = 0; i < 13; ++i)
    {
        load(star_golden::kShapes[i], star[i]);
        masks[i] = RowMask(words[i], sizeof(words[i]) / sizeof(words[i][0]));
    }
    const unsigned body = 3;  // kShapes[3] is the Star's Body

    ankerl::nanobench::Bench bench;
    bench.title("shape").warmup(10).epochs(100).epochIterations(1);

    bench.run("star body: fill", [&] {
        raster.fill(star[body].shape, clip, masks[body]);
        ankerl::nanobench::doNotOptimizeAway(masks[body].row(0)[0]);
    });

    bench.run("star body: paint", [&] {
        paintFill(canvas, masks[body], ColourRole::Body, Remap::identity());
        ankerl::nanobench::doNotOptimizeAway(canvas.getPixel(80, 90));
    });

    bench.run("star (13 Shapes): fill", [&] {
        for (unsigned i = 0; i < 13; ++i)
            raster.fill(star[i].shape, clip, masks[i]);
        ankerl::nanobench::doNotOptimizeAway(masks[0].row(0)[0]);
    });

    bench.run("star (13 Shapes): fill + paint", [&] {
        for (unsigned i = 0; i < 13; ++i)
        {
            raster.fill(star[i].shape, clip, masks[i]);
            paintFill(canvas, masks[i], ColourRole::Body, Remap::identity());
        }
        ankerl::nanobench::doNotOptimizeAway(canvas.getPixel(80, 90));
    });

    // Write JSON results (scripts/convert-bench.py reads them)
    mkdir("bench-results", 0755);
    std::ofstream out("bench-results/bench_shape.json");
    ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, out);
    return 0;
}
