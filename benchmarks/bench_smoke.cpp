#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

int main() {
    ankerl::nanobench::Bench bench;
    bench.title("smoke").warmup(3).epochs(5);

    bench.run("noop", [] {
        ankerl::nanobench::doNotOptimizeAway(0);
    });

    return 0;
}
