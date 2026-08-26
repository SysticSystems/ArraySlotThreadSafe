#pragma once

#include "../../tests/StressTestParam.hpp"
#include <array>
#include <barrier>
#include <benchmark/benchmark.h>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

import Systic.System.Concurrency.SlotThreadSafe;

namespace Systic::System::Concurrency::Benchmarks {

/**
 * @class SlotThreadSafeBenchOperator
 * @brief Persistent multi-threaded executor controlling Google Benchmark timing state directly.
 */
template <typename P>
class SlotThreadSafeBenchOperator {
private:
    using SlotType = Test::TestSlot<P::slotSize>;

    SlotThreadSafe<SlotType> safeArray;
    std::array<std::unique_ptr<SlotType>, P::arraySize> slots;

    static constexpr int numThreads = P::threadCount;
    static constexpr int itemsPerThread = static_cast<int>(P::arraySize) >> static_cast<int>(P::stress);

    std::atomic<bool> stopFlag{false};
    std::barrier<> syncPoint;
    std::barrier<> finishPoint;
    std::vector<std::jthread> workers;

public:
    SlotThreadSafeBenchOperator()
        : safeArray(P::arraySize),
          syncPoint(numThreads + 1),
          finishPoint(numThreads + 1)
    {
        workers.reserve(numThreads);
        for (int i = 0; i < numThreads; ++i) {
            workers.emplace_back([this, i] {
                const int startIdx = i * itemsPerThread;
                const int endIdx = startIdx + itemsPerThread;

                while (true) {
                    // 1. Await trigger from driver thread
                    syncPoint.arrive_and_wait();
                    if (stopFlag.load(std::memory_order_relaxed)) break;

                    // 2. CONCURRENT FILL
                    for (int idx = startIdx; idx < endIdx; ++idx) {
                        auto result = this->safeArray.add(std::move(this->slots[static_cast<std::size_t>(idx)]));
                        benchmark::DoNotOptimize(result);
                    }

                    // 3. CONCURRENT DRAIN
                    for (int idx = startIdx; idx < endIdx; ++idx) {
                        auto result = this->safeArray.removeAt(static_cast<std::size_t>(idx));
                        benchmark::DoNotOptimize(result);
                    }

                    // 4. Signal work complete
                    finishPoint.arrive_and_wait();
                }
            });
        }
    }

    ~SlotThreadSafeBenchOperator() {
        stopFlag.store(true, std::memory_order_relaxed);
        syncPoint.arrive_and_wait();
    }

    /**
     * @brief Manages iteration lifecycle and Google Benchmark timing boundaries directly.
     */
    void runBenchmark(benchmark::State& state) {
        for (auto _ : state) {
            // Pause timer for memory allocations
            state.PauseTiming();
            for (std::size_t i = 0; i < P::arraySize; ++i) {
                this->slots[i] = std::make_unique<SlotType>();
            }

            // Start timer right before releasing worker threads
            state.ResumeTiming();
            syncPoint.arrive_and_wait();   // Workers fire
            finishPoint.arrive_and_wait(); // Workers complete

            // Pause timer immediately after execution completes
            state.PauseTiming();
        }

        // Compute total throughput accurately
        const std::size_t itemsPerIteration = static_cast<std::size_t>(P::arraySize) * 2ULL;
        state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(itemsPerIteration));
    }
};

} // namespace Systic::System::Concurrency::Benchmarks