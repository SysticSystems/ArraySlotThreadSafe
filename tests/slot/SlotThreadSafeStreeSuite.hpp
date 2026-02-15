#pragma once
#include <gtest/gtest.h>
#include "StressLevel.hpp"
#include "SlotThreadSafePressureParam.hpp"

namespace Systic::System::Concurrency::Test {

    using SlotThreadSafeStreeSuite = ::testing::Types<
        SlotThreadSafePressureParam<128, 512, StressLevel::NO_STRESS>,
        SlotThreadSafePressureParam<128, 512, StressLevel::LVL1>,


        SlotThreadSafePressureParam<512, 1024, StressLevel::NO_STRESS>,
        SlotThreadSafePressureParam<512, 1024, StressLevel::LVL1>,
        SlotThreadSafePressureParam<512, 1024, StressLevel::LVL2>,

        SlotThreadSafePressureParam<1024, 1024*1024, StressLevel::NO_STRESS>,
        SlotThreadSafePressureParam<1024, 1024*1024, StressLevel::LVL1>,
        SlotThreadSafePressureParam<1024, 1024*1024, StressLevel::LVL2>,
        SlotThreadSafePressureParam<2048, 1024*1024, StressLevel::LVL3>
  //      SlotThreadSafePressureParam<1024*1024, 1024, StressLevel::LVL3>

    >;
}