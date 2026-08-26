// NOLINTfile(readability-redundant-declaration)
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>

#include <gtest/gtest.h>

import Systic.System.Concurrency.SlotThreadSafe;

namespace Systic::System::Concurrency::Test {

    template <std::size_t SLOT_SIZE>
    struct SlotType {
        std::uint8_t value[SLOT_SIZE];
    };

    class SlotThreadSafeInitializationTest : public ::testing::Test {
    protected:
        template <std::size_t SLOT_SIZE>
        void testSize(const SlotThreadSafe<SlotType<SLOT_SIZE>>& array, std::size_t expectedSize) {
            ASSERT_NE(array.getSize(), nullptr) << "Size pointer must not be null.";
            ASSERT_EQ(*array.getSize(), expectedSize) << "Array size does not match the initialized capacity.";
        }

        template <std::size_t SLOT_SIZE>
        void testMemoryLayout(const SlotThreadSafe<SlotType<SLOT_SIZE>>& array) {
            const std::size_t* sizePointer = array.getSize();
            const std::uint64_t* metadata = array.getMetadata();

            ASSERT_NE(sizePointer, nullptr);
            ASSERT_NE(metadata, nullptr);

            // 1. First 64-bit word must be the size integer
            ASSERT_EQ(reinterpret_cast<const void*>(sizePointer), reinterpret_cast<const void*>(metadata))
                << "Under metadata layout, the first 64-bit word must store the array size.";

            // 2. Second 64-bit word must mark the start of the controls bitmask array
            ASSERT_EQ(array.getControls(), metadata + 1)
                << "Under metadata layout, controls bitmask must immediately follow the size field.";

            // 3. Vacancy pointer must sit directly after size + control slots: metadata + (size >> 6) + 1
            const std::size_t controlSlotsCount = *array.getSize() >> 6U;
            ASSERT_EQ(array.getVacancy(), metadata + controlSlotsCount + 1U)
                << "Under metadata layout, vacancy atomic must sit directly after control bitmask slots.";
        }

    public:
        void SetUp() override {
            // TODO(systic): Integrate cross-platform hardware metrics provider to adjust stress parameters dynamically.
        }

        /**
         * Test instantiation of SlotThreadSafe under valid power-of-two and invalid sizes.
         */
        void testInitiation() {
            // Must throw when size is not a power of 2
            EXPECT_THROW(
                SlotThreadSafe<SlotType<1>> slotThreadSafe(999),
                std::invalid_argument
            );

            // Test instantiation with 1024 slots (power of 2)
            constexpr std::size_t slotSize = 1024;
            constexpr std::size_t arraySize = 1024;

            const SlotThreadSafe<SlotType<slotSize>> slotThreadSafe(arraySize);
            this->testSize<slotSize>(slotThreadSafe, arraySize);
            this->testMemoryLayout<slotSize>(slotThreadSafe);
        }
    };

    TEST_F(SlotThreadSafeInitializationTest, SlotThreadSafeInitiation) {
        this->testInitiation();
    }

} // namespace Systic::System::Concurrency::Test