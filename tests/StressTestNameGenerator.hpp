#pragma once
#include <string>
#include "StressLevel.hpp"

namespace Systic::System::Concurrency::Test {
    struct StressTestNameGenerator {
        template <typename T>
        static std::string GetName(int i) {
            std::string stressStr;
            switch (T::stress) {
                case StressLevel::NO_STRESS: stressStr = "NO_STRESS"; break;
                case StressLevel::LVL1:      stressStr = "LVL1";      break;
                case StressLevel::LVL2:      stressStr = "LVL2";      break;
                case StressLevel::LVL3:      stressStr = "LVL3";      break;
                default:                     stressStr = "LVL_UNKNOWN"; break;
            }

            // Format: {StressLevel}__ArraySize({size})__DataSize({size})
            return stressStr +
                   "__ArraySize(" + std::to_string(T::arraySize) + ')' +
                   "__DataSize(" + std::to_string(T::slotSize) + ')';
        }
    };
}