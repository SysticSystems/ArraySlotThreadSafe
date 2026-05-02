#include <gtest/gtest.h>
#include "AbstractStressTest.hpp"
#include "StressTestSuite.hpp"
#include "StressTestNameGenerator.hpp"
#include "SlotThreadSafeTestOperator.hpp"

import Systic.System.Concurrency;

namespace Systic::System::Concurrency::Test {

    template <typename T>
    class SlotThreadSafeStressTest : public AbstractStressTest<T> {
        private:

        public:
            SlotThreadSafeStressTest() : AbstractStressTest<T>(
                new SlotThreadSafeTestOperator<T::arraySize, T::slotSize>()
            ) {
                this->slots = T::slots;
            }
        };


    // Syntax: TYPED_TEST_SUITE(CaseName, Types);
    TYPED_TEST_SUITE(SlotThreadSafeStressTest, StressTestSuite, StressTestNameGenerator);

    // Inside here, 'TypeParam' refers to the current entry from SlotThreadSafeStreeSuite
    TYPED_TEST(SlotThreadSafeStressTest, SlotThreadSafeStress) {

        this->assertAdd();

        this->assertDelete();

    }
}