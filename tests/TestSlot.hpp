#pragma once
#include <cstdint>
#include <atomic>
#include <chrono>

namespace Systic::System::Concurrency::Test {
    template <std::size_t SLOT_SIZE>
    struct TestSlot {
        std::uint8_t array[SLOT_SIZE]{};
        std::size_t size = SLOT_SIZE;

        TestSlot() {
            static std::atomic<std::size_t> salt{0};
            auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            for (std::size_t i = 0; i < SLOT_SIZE; ++i) {
                this->array[i] = static_cast<uint8_t>((now + i + salt) & 0xFF);
            }
            salt++;
        }

        bool operator==(const TestSlot &slot) const {
            if (this->size != slot.size) return false;
            for (std::size_t i = 0; i < SLOT_SIZE; ++i) {
                if (slot.array[i] != this->array[i]) return false;
            }
            return true;
        }

        TestSlot& operator=(const TestSlot& slot) {
            if (this != &slot) {
                this->size = slot.size;
                for (std::size_t i = 0; i < SLOT_SIZE; ++i) {
                    this->array[i] = slot.array[i];
                }
            }
            return *this;
        }

#ifdef NDEBUG
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