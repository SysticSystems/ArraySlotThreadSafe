#pragma once

#include "../TestSlot.hpp"
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <gtest/gtest.h>

import Systic.System.Concurrency.SlotThreadSafe;

namespace Systic::System::Concurrency::Test {

/**
 * @class SlotThreadSafeTestOperator
 * @brief High-level test fixture operator for verifying lock-free `SlotThreadSafe` invariants.
 *
 * Provides a suite of thread-safe assertions to validate insertion, occupancy,
 * exact-value lookup, and deletion inside a `SlotThreadSafe` container.
 *
 * @tparam ARRAY_SIZE Capacity of the underlying slot container.
 * @tparam SLOT_SIZE Size configuration of the `TestSlot` payload.
 */
template <std::size_t ARRAY_SIZE, std::size_t SLOT_SIZE>
class SlotThreadSafeTestOperator {
private:
    using SlotType = TestSlot<SLOT_SIZE>;

    /// Managed container under test.
    std::unique_ptr<SlotThreadSafe<SlotType>> safeArray =
        std::make_unique<SlotThreadSafe<SlotType>>(ARRAY_SIZE);

    /**
     * @brief Checks if a given slot index is in an empty/deleted state.
     * @param idx Index to inspect.
     * @return True if the slot returns ErrorSlotEmpty on peek.
     */
    [[nodiscard]] bool isSlotDeleted(std::size_t idx) const noexcept {
        const auto result = this->safeArray->template peek<bool>(
            idx,
            [](const SlotType* slot) noexcept {
                return slot == nullptr;
            }
        );
        return result.getStatus() == SlotOperationStatus::Success && result.getValue();
    }

    /**
     * @brief Compares a slot in the container against a expected raw pointer payload.
     * @param rawExpected Non-owning pointer to the expected `SlotType`.
     * @param idx Container slot index to compare.
     * @return True if the slot is occupied and its payload equals `*rawExpected`.
     */
    [[nodiscard]] bool isSlotMatching(const SlotType* rawExpected, std::size_t idx) const noexcept {
        if (rawExpected == nullptr) {
            return false;
        }

        const auto result = this->safeArray->template peek<bool>(
            idx,
            [rawExpected](const SlotType* foundSlot) noexcept -> bool {
                if (foundSlot == nullptr) {
                    return false;
                }
                return *foundSlot == *rawExpected;
            }
        );

        return result.isSuccess() && result.getValue();
    }

public:
    SlotThreadSafeTestOperator() = default;
    ~SlotThreadSafeTestOperator() = default;

    SlotThreadSafeTestOperator(const SlotThreadSafeTestOperator&) = delete;
    SlotThreadSafeTestOperator& operator=(const SlotThreadSafeTestOperator&) = delete;
    SlotThreadSafeTestOperator(SlotThreadSafeTestOperator&&) noexcept = default;
    SlotThreadSafeTestOperator& operator=(SlotThreadSafeTestOperator&&) noexcept = default;

    /**
     * @brief Asserts total capacity of the underlying safe array matches ARRAY_SIZE.
     */
    void assertSize() const {
        ASSERT_EQ(ARRAY_SIZE, *this->safeArray->getSize()) << "Array size mismatch";
    }

    /**
     * @brief Asserts that a specific slot index is marked as occupied.
     * @param idx Slot index to test.
     */
    void assertOccupied(const std::size_t idx) const {
        const auto result = this->safeArray->template peek<void>(
            idx,
            [](const SlotType* /*slot*/) noexcept {}
        );
        ASSERT_EQ(result.getStatus(), SlotOperationStatus::Success)
            << std::format("The slot was expected to be occupied at index {}", idx);
    }

    /**
     * @brief Transfers heap ownership of `slot` to `safeArray` and verifies correct allocation.
     * @param slot Unique pointer containing the slot to insert.
     */
    void assertAdd(std::unique_ptr<SlotType> slot) {
        const SlotType* rawSlotPtr = slot.get();
        ASSERT_NE(rawSlotPtr, nullptr) << "Cannot add a null slot pointer";

        const auto result = this->safeArray->add(std::move(slot));
        ASSERT_EQ(result.getStatus(), SlotOperationStatus::Success) << "Failed to add slot";

        const std::size_t idx = result.getValue();
        EXPECT_TRUE(this->isSlotMatching(rawSlotPtr, idx))
            << std::format("Slot value mismatch after add at index {}", idx);
    }

    /**
     * @brief Moves `slot` into a designated index and asserts successful placement.
     * @param slot Unique pointer to insert.
     * @param idx Exact destination index.
     */
    void assertInsertAt(std::unique_ptr<SlotType> slot, std::size_t idx) {
        const SlotType* rawSlotPtr = slot.get();
        ASSERT_NE(rawSlotPtr, nullptr) << "Cannot insert a null slot pointer";

        const auto result = this->safeArray->insertAt(std::move(slot), idx);
        ASSERT_EQ(result.getStatus(), SlotOperationStatus::Success)
            << std::format("Failed to insert slot at index {}", idx);

        ASSERT_TRUE(this->isSlotMatching(rawSlotPtr, idx))
            << std::format("Could not verify inserted slot value at index {}", idx);
    }

    /**
     * @brief Verifies that every expected slot in `expectedSlots` exists somewhere inside `safeArray`.
     * @param expectedSlots Non-owning array of raw pointers representing target slots.
     * @param numberOfSlots Count of slots in the array to validate.
     */
    void assertSlotsExists(const std::array<const SlotType*, ARRAY_SIZE>& expectedSlots, std::size_t numberOfSlots) const {
        std::size_t validPointersCount = 0;

        for (std::size_t idx = 0; idx < numberOfSlots; ++idx) {
            const SlotType* targetSlotPtr = expectedSlots[idx];

            if (targetSlotPtr == nullptr) {
                continue;
            }

            validPointersCount++;
            bool isFound = false;

            for (std::size_t i = 0; i < ARRAY_SIZE; ++i) {
                if (this->isSlotMatching(targetSlotPtr, i)) {
                    isFound = true;
                    break;
                }
            }

            ASSERT_TRUE(isFound)
                << std::format("Slot matching value at index {} was not found in container", idx);
        }

        ASSERT_GT(validPointersCount, 0)
            << "assertSlotsExists was invoked with an array containing purely nullptr entries!";
    }

    /**
     * @brief Removes a slot at `idx` and asserts that the slot transitions to an empty state.
     * @param idx Target index to remove.
     */
    void assertDeleteAt(std::size_t idx) {
        const auto result = this->safeArray->removeAt(idx);
        ASSERT_EQ(result.getStatus(), SlotOperationStatus::Success)
            << std::format("Failed to execute removeAt at index {}", idx);

        ASSERT_TRUE(this->isSlotDeleted(idx))
            << std::format("Slot was not reported as empty/deleted at index {}", idx);
    }

    /**
     * @brief Deletes multiple slots by an array of indices and asserts their removal.
     * @param indexes Raw array of index numbers to remove.
     * @param numberOfSlots Number of indices to process.
     */
    void assertSlotsDeletedByIndexes(const std::uint64_t indexes[], std::size_t numberOfSlots) {
        for (std::size_t idx = 0; idx < numberOfSlots; ++idx) {
            this->assertDeleteAt(static_cast<std::size_t>(indexes[idx]));
        }
    }

    /**
     * @brief Attempts slot insertion without throwing GoogleTest assertions on capacity exhaustion.
     *
     * @param item Unique pointer slot payload to insert.
     * @return SlotOperationStatus Actual operation result status.
     */
    [[nodiscard]] SlotOperationStatus tryAdd(std::unique_ptr<SlotType> item) {
        const auto result = this->safeArray->add(std::move(item));
        return result.getStatus();
    }

    /**
 * @brief Attempts slot removal without throwing GoogleTest assertions on empty or out-of-bounds indices.
     *
     * @param index Index of the slot to remove.
     * @return SlotOperationStatus Actual operation result status.
     */
    [[nodiscard]] SlotOperationStatus tryRemoveAt(std::size_t index) {
        const auto result = this->safeArray->removeAt(index);
        return result.getStatus();
    }
};

} // namespace Systic::System::Concurrency::Test