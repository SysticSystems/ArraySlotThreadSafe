#include <benchmark/benchmark.h>
#include "systic/core.hpp"

static void BM_ComputeFactorial(benchmark::State& state) {
    systic::CoreEngine engine;
    for (auto _ : state) {
        int result = engine.compute_factorial(10);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_ComputeFactorial);

static void BM_ComputeSum(benchmark::State& state) {
    systic::CoreEngine engine;
    for (auto _ : state) {
        int result = engine.compute_sum(42, 73);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_ComputeSum);
