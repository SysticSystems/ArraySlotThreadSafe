#pragma once
#include <chrono>

namespace Systic::System::Concurrency::Benchmarks {
    template <std::size_t SLOT_SIZE>
    struct BenchmarkTestSlot {
        std::uint8_t array[SLOT_SIZE]{};
        std::size_t size = SLOT_SIZE;

        BenchmarkTestSlot() {
            for (std::size_t i = 0; i < SLOT_SIZE; ++i) {
                auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                this->array[i] = static_cast<uint8_t>(now & 0xFF);
            }
        }

        bool operator==(const BenchmarkTestSlot &slot) const {
            // Quick check for size
            if (this->size != slot.size) return false;

            // Calculate how many 64-bit jumps we can make
            for (std::size_t i = 0; i < SLOT_SIZE; ++i) {
                if (slot.array[i] != this->array[i]) {
                    return false;
                }
            }
            return true;
        }
        BenchmarkTestSlot& operator=(const BenchmarkTestSlot& slot) {
            if (&slot != this) {
                this->array[0] = slot.array[0];
            }
            return *this;
        }

        friend std::ostream& operator<<(std::ostream& os, const BenchmarkTestSlot<SLOT_SIZE>& slot) {
            for (std::size_t i = 0; i < SLOT_SIZE; ++i) {
                if (i > 0) {
                    os << " , ";
                }
                os << static_cast<std::uint64_t>(slot.array[i]);
            }
            return os;
        }
    };
}