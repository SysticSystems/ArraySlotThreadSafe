#pragma once
#include "StressLevel.hpp"


namespace Systic::System::Concurrency::Test {
    template <std::size_t ARRAY_SIZE, std::size_t SLOT_SIZE, StressLevel LEVEL>
    struct SlotThreadSafePressureParam {
        static constexpr std::size_t arraySize = ARRAY_SIZE;
        static constexpr std::size_t slotSize = SLOT_SIZE;
        static constexpr StressLevel stress = LEVEL;
        inline static TestSlot<SLOT_SIZE>* slots = new TestSlot<SLOT_SIZE>[ARRAY_SIZE];

        // number of threads = 2^(LEVEL).
        static constexpr int threadCount = 0b1 << static_cast<int>(LEVEL);
    };
}