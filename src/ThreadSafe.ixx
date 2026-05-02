module;

// Ensure the compiler is actually C++23 ready
#if __cplusplus < 202302L
#error "This Systic Module requires C++23. Upgrade your compiler, Bully!"
#endif

#include <cstdint>
#include <bit>
#include <systic/system/concurrency/SlotThreadSafe.hpp>
#include "./SlotThreadSafe.tpp"

export module Systic.System.Concurrency;

export namespace Systic::System::Concurrency {
    using ::Systic::System::Concurrency::SlotThreadSafe;
}