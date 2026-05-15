#include "systic/core.hpp"

namespace systic {

int CoreEngine::compute_factorial(int n) const {
    if (n <= 1) return 1;
    return n * compute_factorial(n - 1);
}

int CoreEngine::compute_sum(int a, int b) const {
    return a + b;
}

} // namespace systic