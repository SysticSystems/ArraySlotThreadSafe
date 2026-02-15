#pragma once
#include <cstdint>
#include <chrono>

namespace Systic::System::Concurrency::Test {
    template <std::size_t SLOT_SIZE>
    struct TestSlot {
        std::uint8_t array[SLOT_SIZE]{};
        std::size_t size = SLOT_SIZE;

        TestSlot() {
            for (std::size_t i = 0; i < SLOT_SIZE; ++i) {
                auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                this->array[i] = static_cast<uint8_t>(now & 0xFF);
            }
        }

        bool operator==(const TestSlot &slot) const {
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
        TestSlot& operator=(const TestSlot& slot) {
            if (&slot != this) {
                this->array[0] = slot.array[0];
            }
            return *this;
        }

#ifdef SYSTIC_RELEASE_WITH_DEBUG
        friend std::ostream& operator<<(std::ostream& os, const TestSlot& slot) {
            for (std::size_t i = 0; i < SLOT_SIZE; ++i) {
                if (i > 0) {
                    os << " , ";
                }
                os << slot.array[i];
            }
            return os;
        }
#endif
    };
}