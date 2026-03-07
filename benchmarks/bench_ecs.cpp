#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <enjin2/core/object_collection.hpp>
#include <enjin2/core/scene.hpp>
#include <enjin2/components/position.hpp>
#include <fstream>
#include <sys/stat.h>

// Minimal concrete scene — no SDL, no canvas required
class BenchScene : public enjin2::Scene {
public:
    BenchScene() : enjin2::Scene(1) {}
};

int main() {
    ankerl::nanobench::Bench bench;
    bench.title("ecs").warmup(5).epochs(50).epochIterations(1);

    // --- Object creation: ObjectCollection::addObject at varying counts ---
    for (int count : {1, 8, 16, 32, 48}) {
        bench.run("scene::addObject x" + std::to_string(count), [&] {
            enjin2::ObjectCollection coll;
            for (int i = 0; i < count; ++i) {
                auto* obj = coll.addObject<enjin2::Object>();
                ankerl::nanobench::doNotOptimizeAway(obj);
            }
        });
    }

    // --- Component attach ---
    bench.run("object::addComponent<C_Position>", [&] {
        enjin2::Object obj;
        auto* pos = obj.addComponent<enjin2::C_Position>();
        ankerl::nanobench::doNotOptimizeAway(pos);
    });

    // --- Component detach ---
    bench.run("object::removeComponent<C_Position>", [&] {
        enjin2::Object obj;
        obj.addComponent<enjin2::C_Position>();
        bool removed = obj.removeComponent<enjin2::C_Position>();
        ankerl::nanobench::doNotOptimizeAway(removed);
    });

    // --- Scene::update at varying object counts ---
    // CRITICAL: scene.activate() must be called before update() — update() has `if (!active) return;` guard
    for (int count : {1, 8, 16, 32, 48}) {
        BenchScene scene;
        scene.activate();
        for (int i = 0; i < count; ++i) {
            scene.addObject<enjin2::Object>();
        }
        bench.run("scene::update x" + std::to_string(count) + " objects", [&] {
            scene.update(0.016f);
            ankerl::nanobench::doNotOptimizeAway(scene.getObjects().size());
        });
    }

    // Write JSON results
    mkdir("bench-results", 0755);
    std::ofstream out("bench-results/bench_ecs.json");
    ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, out);

    return 0;
}
