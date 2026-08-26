#pragma once
#include "StressLevel.hpp"
#include "TestSlot.hpp"
#include <memory>

namespace Systic::System::Concurrency::Test {
    template <std::size_t ARRAY_SIZE, std::size_t SLOT_SIZE, StressLevel LEVEL>
    struct StressTestParam {
        static constexpr std::size_t arraySize = ARRAY_SIZE;
        static constexpr std::size_t slotSize = SLOT_SIZE;
        static constexpr StressLevel stress = LEVEL;

        // Heap-allocated array owned safely by unique_ptr
        inline static std::array<std::unique_ptr<TestSlot<SLOT_SIZE>>, ARRAY_SIZE> slots = []{
            std::array<std::unique_ptr<TestSlot<SLOT_SIZE>>, ARRAY_SIZE> arr;
            for (auto& slot : arr) {
                slot = std::make_unique<TestSlot<SLOT_SIZE>>();
            }
            return arr;
        }();

        // number of threads = 2^(LEVEL).
        static constexpr int threadCount = 0b1 << static_cast<int>(LEVEL);
    };
}