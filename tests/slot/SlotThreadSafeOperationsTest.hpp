#pragma once
#include <string>
#include "./TestSlot.hpp"

import Systic.System.Concurrency;

namespace Systic::System::Concurrency::Test {
    template <std::size_t ARRAY_SIZE, std::size_t SLOT_SIZE>
    class SlotThreadSafeOperationsTest {

        private:
            std::unique_ptr<SlotThreadSafe<TestSlot<SLOT_SIZE>>> safeArray = std::make_unique<SlotThreadSafe<TestSlot<SLOT_SIZE>>>(ARRAY_SIZE);

            bool isSlotDeleted(std::size_t idx) {
                return this->safeArray->template peek<bool>(
                    idx,
                    [](const TestSlot<SLOT_SIZE>* foundSlot) -> bool {
                       return foundSlot == nullptr;
                    }
                );
            }

            bool isSlotExists(const TestSlot<SLOT_SIZE>& slot, std::size_t idx) {
                return this->safeArray->template peek<bool>(
                    idx,
                    [slot](const TestSlot<SLOT_SIZE>* foundSlot) -> bool {
                       return *foundSlot == slot;
                    }
                );
            }

        public:
            /**
             *
             * @param slot
             */
            void assertAdd(const TestSlot<SLOT_SIZE>* slot) {
                std::uint64_t idx = this->safeArray->add(slot);
                bool status = this->isSlotExists(
                    *slot,
                    idx
                );
                ASSERT_TRUE(status) << "Could not find added item";
            }

            void assertOccupied(const std::uint64_t idx) {
                bool isOccupied = this-safeArray->template peek<bool>(
                    idx,
                    [](const TestSlot<SLOT_SIZE>* foundSlot, const std::uint64_t bitmaskSlot, const std::uint64_t mask) -> bool {
                        return (bitmaskSlot & mask) != 0;
                    }
                );
                ASSERT_TRUE(isOccupied) << "The thread safety control bit was not set to 1 for the slot at index " << idx;
            }
            /**
             *
             * @param arraySize
             */
            void assertSize(const std::size_t arraySize) {
                ASSERT_EQ(arraySize, this->safeArray->size()) << "Array size mismatch";
            }

            /**
             *
             * @param slots
             * @param numberOfSlots
             */
            void assertSlotsExists(TestSlot<SLOT_SIZE>* slots, const size_t numberOfSlots) {

                for (size_t idx = 0; idx < numberOfSlots; idx++) {
                    bool isFound = false;
                    TestSlot<SLOT_SIZE>* slot = &slots[idx];

                    this->assertOccupied(idx);

                    for (std::size_t i = 0; i < ARRAY_SIZE; i++) {
                        if (this->safeArray->template peek<bool>(
                            i,
                            [slot](const TestSlot<SLOT_SIZE>* foundSlot) -> bool {
                                return foundSlot == slot;
                        })) {
                            isFound = true;
                            break;
                        }
                    }
                    ASSERT_TRUE(
                        isFound
                   ) << "Slot Index = " << " was not found.";
                }
            }

            /**
             *
             * @param indexes
             * @param numberOfSlots
             */
            void assetSlotsDeletedByIndex(std::uint64_t indexes[], std::size_t numberOfSlots) {
                for (std::size_t idx = 0; idx < numberOfSlots; idx++) {
                    assertDeleteAt(indexes[idx]);
                }
            }

            void assertInsertAt(const TestSlot<SLOT_SIZE>& slot, std::size_t idx) {
                this->safeArray.insertAt(slot, idx);

                ASSERT_TRUE(this->isSlotExists(*slot, idx)) << std::format(
                    "Could not find slot inserted at {}",
                    idx
                );
            }

            void assertDeleteAt(const std::size_t idx) {
                this->safeArray->removeAt(idx);
                ASSERT_TRUE(
                    this->isSlotDeleted(idx)
                ) << std::format("Slot was not deleted at {}", idx);
            }
    };
}