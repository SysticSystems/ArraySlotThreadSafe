#pragma once
#include <string>
#include "../TestSlot.hpp"
#include "../StressTestOperatorInterface.hpp"

import Systic.System.Concurrency;

namespace Systic::System::Concurrency::Test {
    template <std::size_t ARRAY_SIZE, std::size_t SLOT_SIZE>
    class SlotThreadSafeTestOperator : public StressTestOperatorInterface<SLOT_SIZE> {

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
            void assertAdd(TestSlot<SLOT_SIZE>* slot) override {
                std::uint64_t idx = this->safeArray->add(slot);
                bool status = this->isSlotExists(
                    *slot,
                    idx
                );

                ASSERT_TRUE(status) << "Could not find added item";
            }

            void assertOccupied(const std::uint64_t idx) {
                bool isOccupied = this->safeArray->template peek<bool>(
                    idx,
                    [](const TestSlot<SLOT_SIZE>* foundSlot) -> bool {
                        return foundSlot != nullptr;
                    }
                );
                ASSERT_TRUE(isOccupied) << "The slot was empty at index " << idx;
            }
            /**
             *
             */
            void assertSize() override {
                ASSERT_EQ(ARRAY_SIZE, *this->safeArray->getSize()) << "Array size mismatch";
            }

            /**
             *
             * @param slots
             * @param numberOfSlots
             */
            void assertSlotsExists(TestSlot<SLOT_SIZE>* slots, size_t numberOfSlots) override {

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
            void assetSlotsDeletedByIndexes(std::uint64_t indexes[], std::size_t numberOfSlots) override {
                for (std::size_t idx = 0; idx < numberOfSlots; idx++) {
                    assertDeleteAt(indexes[idx]);
                }
            }

            void assertInsertAt(TestSlot<SLOT_SIZE>& slot, std::size_t idx) override {
                this->safeArray->insertAt(&slot, idx);

                ASSERT_TRUE(this->isSlotExists(slot, idx)) << std::format(
                    "Could not find slot inserted at {}",
                    idx
                );
            }

            void assertDeleteAt(std::size_t idx) override {
                this->safeArray->removeAt(idx);
                ASSERT_TRUE(
                    this->isSlotDeleted(idx)
                ) << std::format("Slot was not deleted at {}", idx);
            }

            void assertDelete() override {}
    };
}