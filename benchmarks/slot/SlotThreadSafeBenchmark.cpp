#include <benchmark/benchmark.h>
#include <barrier>
#include <systic/system/concurrency/SlotThreadSafe.hpp>

namespace Systic::System::Concurrency::Benchmarks {
    template <std::size_t ARRAY_SIZE, std::size_t SLOT_SIZE>
    void BM_SlotThreadSafe_AddRemove() {
    }
}

