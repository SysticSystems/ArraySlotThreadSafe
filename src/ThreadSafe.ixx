module;

// Ensure the compiler is actually C++23 ready
#if __cplusplus < 202302L
#error "This Systic Module requires C++23. Upgrade your compiler!"
#endif

// FORCE clang-tidy to respect the template implementation chain order
// NOLINTBEGIN(llvm-include-order)
#include <systic/system/concurrency/SlotThreadSafe.hpp>
#include "./SlotThreadSafe.tpp"
// NOLINTEND(llvm-include-order)

export module Systic.System.Concurrency;

export namespace Systic::System::Concurrency {
    using ::Systic::System::Concurrency::SlotThreadSafe; // NOLINT(misc-unused-using-decls)
} // namespace Systic::System::Concurrency