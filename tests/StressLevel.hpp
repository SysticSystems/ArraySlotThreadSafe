#pragma once

/**
 * Enum to assign a number of threads per stress level.
 * In order to make the math easy for cpu, values here are only the exponent part of the real number.
 * for exemple for LVL1 , number of threads = 2^6.
 *
 */
namespace Systic::System::Concurrency::Test {
    enum class StressLevel : int {
    NO_STRESS = 1,
    LVL1 = 6,
    LVL2 = 8,
    LVL3 = 10
};
}

