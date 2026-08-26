#include "../../tests/StressLevel.hpp"
#include "../../tests/StressTestParam.hpp"
#include "SlotThreadSafeBenchOperator.hpp"
#include <benchmark/benchmark.h>

namespace Systic::System::Concurrency::Benchmarks {

    template <typename P>
    static void BM_SlotThreadSafe_MultithreadedContention(benchmark::State& state) {
        // Construct operator and worker threads ONCE outside the iteration loop
        SlotThreadSafeBenchOperator<P> benchOperator;
        benchOperator.runBenchmark(state);
    }

    using Param_128_NOSTRESS = Test::StressTestParam<128, 512, Test::StressLevel::NO_STRESS>;
    using Param_128_LVL1     = Test::StressTestParam<128, 512, Test::StressLevel::LVL1>;

    using Param_512_NOSTRESS = Test::StressTestParam<512, 1024, Test::StressLevel::NO_STRESS>;
    using Param_512_LVL1     = Test::StressTestParam<512, 1024, Test::StressLevel::LVL1>;
    using Param_512_LVL2     = Test::StressTestParam<512, 1024, Test::StressLevel::LVL2>;

    BENCHMARK_TEMPLATE(BM_SlotThreadSafe_MultithreadedContention, Param_128_NOSTRESS)->UseRealTime();
    BENCHMARK_TEMPLATE(BM_SlotThreadSafe_MultithreadedContention, Param_128_LVL1)->UseRealTime();

    BENCHMARK_TEMPLATE(BM_SlotThreadSafe_MultithreadedContention, Param_512_NOSTRESS)->UseRealTime();
    BENCHMARK_TEMPLATE(BM_SlotThreadSafe_MultithreadedContention, Param_512_LVL1)->UseRealTime();
    BENCHMARK_TEMPLATE(BM_SlotThreadSafe_MultithreadedContention, Param_512_LVL2)->UseRealTime();

} // namespace Systic::System::Concurrency::Benchmarks