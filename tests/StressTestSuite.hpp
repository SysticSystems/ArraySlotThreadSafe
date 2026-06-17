#pragma once
#include <gtest/gtest.h>
#include "StressLevel.hpp"
#include "StressTestParam.hpp"

namespace Systic::System::Concurrency::Test {

    using StressTestSuite = ::testing::Types<
        StressTestParam<128, 512, StressLevel::NO_STRESS>,
        StressTestParam<128, 512, StressLevel::LVL1>,


        StressTestParam<512, 1024, StressLevel::NO_STRESS>,
        StressTestParam<512, 1024, StressLevel::LVL1>,
        StressTestParam<512, 1024, StressLevel::LVL2>
    >;
}