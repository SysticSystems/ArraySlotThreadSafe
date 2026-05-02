#pragma once
#include "TestSlot.hpp"

namespace Systic::System::Concurrency::Test {
    template <std::size_t SLOT_SIZE>
    class StressTestOperatorInterface {
        public:
            virtual ~StressTestOperatorInterface() = default;

            /**
             * @brief Assert that the size of the array is correct. 
            */
            virtual void assertSize() = 0;

            /**
             * @brief Assert that the slot is added to the array.
             * @param slot The slot to add.
            */
            virtual void assertAdd(TestSlot<SLOT_SIZE>* slot) = 0;

            /**
             * @brief Assert that the slot is inserted at the given index.
             * @param slot The slot to insert.
             * @param idx The index to insert at.
            */
            virtual void assertInsertAt(TestSlot<SLOT_SIZE>& slot, std::size_t idx) = 0;

            /**
             * @brief Assert that the slots exist in the array.
             * @param slots The slots to check.
             * @param numberOfSlots The number of slots to check.
            */
            virtual void assertSlotsExists(TestSlot<SLOT_SIZE>* slots, std::size_t numberOfSlots) = 0;

            /**
             * @brief Assert that the slots are deleted by index.
             * @param indexes The indexes of the slots to delete.
             * @param numberOfSlots The number of slots to delete.
            */
            virtual void assetSlotsDeletedByIndexes(std::uint64_t indexes[], std::size_t numberOfSlots) = 0;

            /**
             * @brief Assert that the slot is deleted at the given index.
             * @param idx The index of the slot to delete.
            */
            virtual void assertDeleteAt(std::size_t idx) = 0;

            /**
             * @brief Assert that the slots are deleted from the array. This method should be called after all the operations have been performed.
            */
            virtual void assertDelete() = 0;

    };
}