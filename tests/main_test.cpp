#include <gtest/gtest.h>
#include "systic/core.hpp"

TEST(SysticCoreTest, ComputeFactorial) {
    systic::CoreEngine engine;
    EXPECT_EQ(engine.compute_factorial(0), 1);
    EXPECT_EQ(engine.compute_factorial(1), 1);
    EXPECT_EQ(engine.compute_factorial(5), 120);
}

TEST(SysticCoreTest, ComputeSum) {
    systic::CoreEngine engine;
    EXPECT_EQ(engine.compute_sum(5, 7), 12);
}
