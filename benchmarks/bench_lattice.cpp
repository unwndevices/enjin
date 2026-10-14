// bench_lattice: the Lattice deformer's cost per 30 Hz tick (Tomodachi #334,
// ADR-0016): the 5×5 sim, the warp of the prototype Star's points, and a whole
// moving-pet frame (tick + warp + fill + paint of the Star's 13 Shapes, all
// bound `field`). nanobench reports instructions per op where the PMU is
// available.
//
//   cmake -DENJIN2_BUILD_BENCHMARKS=ON ... && ./benchmarks/bench_lattice

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <fstream>
#include <sys/stat.h>

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/shape/lattice.hpp>
#include <enjin2/shape/shape_group.hpp>

#include "shape_star_golden.hpp"

using namespace enjin2;
using namespace enjin2::shape;

namespace {

constexpr unsigned kShapes = 13;

FixedVec2 px(int x, int y) { return {Fixed::fromInt(x), Fixed::fromInt(y)}; }

// The Star at rest (canvas px) as 13 Shapes, emitted as one field item each.
struct StarSource : ShapeItemSource {
    FixedVec2 points[kShapes][256];
    Ring      rings[kShapes][4];
    Shape     shapes[kShapes];
    unsigned  totalPoints = 0;

    StarSource()
    {
        for (unsigned s = 0; s < kShapes; ++s)
        {
            const star_golden::GoldenShape& g = star_golden::kShapes[s];
            uint16_t at = 0;
            for (uint16_t r = 0; r < g.ringCount; ++r)
            {
                rings[s][r] = Ring{points[s] + at, g.ringSizes[r], true};
                for (uint16_t i = 0; i < g.ringSizes[r]; ++i, ++at)
                    points[s][at] = {Fixed::fromQ8_8(g.points[2 * at]), Fixed::fromQ8_8(g.points[2 * at + 1])};
            }
            totalPoints += at;
            shapes[s].rings = rings[s];
            shapes[s].ringCount = g.ringCount;
            shapes[s].paint.rule = g.evenOdd ? FillRule::EvenOdd : FillRule::NonZero;
        }
    }
    bool forEach(ShapeDrawScratch&, ShapeEmit emit, void* ctx) const override
    {
        bool ok = true;
        for (unsigned s = 0; s < kShapes; ++s)
            ok = emit(ctx, shapes[s], FixedVec2{}, false, Binding::Field) && ok;
        return ok;
    }
};

StarSource       star;
ShapeDrawScratch scratch;
Canvas4<160, 160> layer;

// A 5×5 Lattice over the Star's box (it sits around x 44..116, y 46..120
// in canvas px), kept awake: sleep needs more quiet ticks than a run has.
Lattice awakeLattice()
{
    LatticeTuning t;
    t.sleepTicks = 255;
    Lattice l;
    l.configure(px(40, 40), px(120, 124), 5, 5, t);
    l.impulse(px(86, 60), px(-80, 380));
    return l;
}

} // namespace

int main()
{
    ankerl::nanobench::Bench bench;
    bench.title("lattice").warmup(10).epochs(100).epochIterations(1);

    Lattice sim = awakeLattice();
    unsigned n = 0;
    bench.run("5x5 sim: one 30 Hz tick (4 sub-steps)", [&] {
        if (++n % 60 == 0) sim.impulse(px(86, 60), px(-80, 380));  // keep it moving
        sim.tick(33334);
        ankerl::nanobench::doNotOptimizeAway(sim.maxDisplacement());
    });

    Lattice warp = awakeLattice();
    for (int i = 0; i < 3; ++i) warp.tick(33334);
    bench.run("warp: the Star's points (13 Shapes)", [&] {
        Fixed sum{};
        for (unsigned s = 0; s < kShapes; ++s)
            for (uint16_t r = 0; r < star.shapes[s].ringCount; ++r)
                for (uint16_t i = 0; i < star.rings[s][r].count; ++i)
                    sum += warp.displacementAt(star.rings[s][r].points[i]).x;
        ankerl::nanobench::doNotOptimizeAway(sum);
    });

    ShapeGroup group;
    group.lattice() = awakeLattice();
    layer.clear(Pixel4(15));
    unsigned f = 0;
    bench.run("moving Star frame: tick + warp + fill + paint", [&] {
        if (++f % 60 == 0) group.impulse(px(86, 60), px(-80, 380));
        group.tick(33334);
        group.redraw(layer, scratch, star);
        ankerl::nanobench::doNotOptimizeAway(layer.getPixel(80, 90));
    });

    ShapeGroup still;
    bench.run("resting Star frame: redraw only (reference)", [&] {
        still.redraw(layer, scratch, star);
        ankerl::nanobench::doNotOptimizeAway(layer.getPixel(80, 90));
    });
    std::printf("Star points warped per frame: %u\n", star.totalPoints);

    // Write JSON results (scripts/convert-bench.py reads them)
    mkdir("bench-results", 0755);
    std::ofstream out("bench-results/bench_lattice.json");
    ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, out);
    return 0;
}
