#pragma once
#include <benchmark/benchmark.h>
#include <systic/system/concurrency/SlotThreadSafe.hpp>
#include "BenchmarkTestSlot.hpp"
#include <cstddef>

namespace Systic::System::Concurrency::Benchmarks {
    template <std::size_t ARRAY_SIZE, std::size_t SLOT_SIZE>
    class SlotBenchFixture : public benchmark::Fixture {
    public:
        // Use a static instance to share across threads in one process run.
        static SlotThreadSafe<BenchmarkTestSlot<SLOT_SIZE>> safeArray(ARRAY_SIZE);
        BenchmarkTestSlot<SLOT_SIZE>* sampleSlot = new BenchmarkTestSlot<SLOT_SIZE>();
    };
}
