#include <gtest/gtest.h>
import Systic.System.Concurrency;

#ifdef SYSTIC_RELEASE_WITH_DEBUG_INFO
    namespace Systic::System::Concurrency::Test {
        template <std::size_t SLOT_SIZE>
        struct SLOT_TYPE {
            std::uint8_t value[SLOT_SIZE];
        };

        class SlotThreadSafeInitializationTest : public ::testing::Test {
            protected:
                template <std::size_t SLOT_SIZE>
                void testSize(SlotThreadSafe<SLOT_TYPE<SLOT_SIZE>>& array, std::size_t size) {
                    ASSERT_EQ(*array.getSize(), size) << "Array size does not match the original size.";
                }
            public:
                /**
                 * Method to set up Test environment.
                 * Detect available and ram and cpu power to be used for stressful testing.
                 */
                void SetUp() override {
                    /**
                     * @TODO: Develop global library to provide a cross-platform to retrieve system hardware metrics.
                     * And based on those metric generate test parameters.
                     */
                }

                /**
                 * Test initiation of SlotThreadSafe with a specified size.
                 */
                void testInitiation() {
                    // This must fail and rise an exception cause size isn't 2^number.
                    EXPECT_THROW(
                        SlotThreadSafe<SLOT_TYPE<1>> slotThreadSafe(999),
                        std::invalid_argument
                    );
                    // Test initiation of SlotThreadSafe with 10 MB slots and 1 * 1024 Slot = 1G data size.
                    constexpr std::size_t slotSize = 10 * 1024 * 1024;
                    constexpr std::size_t arraySize = 1 * 1024;
                    SlotThreadSafe<SLOT_TYPE<slotSize>> slotThreadSafe(arraySize);
                    this->testSize<slotSize>(slotThreadSafe, arraySize);
                    this->testMemoryLayout<slotSize>(slotThreadSafe);
                }

                template <std::size_t SLOT_SIZE>
                 void testMemoryLayout(SlotThreadSafe<SLOT_TYPE<SLOT_SIZE>>& array) {
                    const std::size_t* sizePointer = array.getSize();
                    const std::uint64_t* metadata = array.getMetadata();

                    ASSERT_EQ(sizePointer, metadata) << "Under metadata memory layout, the first 64bit must be the size";

                    ASSERT_EQ(
                        array.getControls(),
                        metadata + 1
                    ) << "Under metadata memory layout, the 2nd 64bit must be the first cell of controls bitmask array";

                    ASSERT_EQ(
                        array.getVacancy(),
                        metadata + (*array.getSize() >> 6) + 1
                    ) << "Under metadata memory layout, the (size >> 6 + 1)nd 64bit must be the first cell of controls bitmask array";
                }
            };

        TEST_F(SlotThreadSafeInitializationTest, SlotThreadSafeInitiation) {
            this->testInitiation();
        }
    }
#endif