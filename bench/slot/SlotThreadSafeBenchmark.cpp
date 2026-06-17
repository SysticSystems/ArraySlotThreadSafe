#include "BenchmarkTestSlot.hpp"
#include <benchmark/benchmark.h>

import Systic.System.Concurrency;

namespace Systic::System::Concurrency::Benchmarks {
    template <std::size_t ARRAY_SIZE, std::size_t SLOT_SIZE>
    void static BM_SlotThreadSafe_AddRemove(benchmark::State& state) {
        SlotThreadSafe<BenchmarkTestSlot<SLOT_SIZE>> safeArray(ARRAY_SIZE);
        BenchmarkTestSlot<SLOT_SIZE> sampleSlot;

        for (auto _ : state) {
            std::size_t id = safeArray.add(&sampleSlot);
            benchmark::DoNotOptimize(id);
            safeArray.removeAt(id);
        }
    }

    BENCHMARK_TEMPLATE(BM_SlotThreadSafe_AddRemove, 1024, 64);
}
