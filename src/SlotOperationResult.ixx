module;

#include <cstdint>

export module Systic.System.Concurrency.SlotThreadSafe:SlotOperationResult;

export namespace Systic::System::Concurrency {

    /**
     * @enum SlotOperationStatus
     * Represents the fine-grained result of operations inside SlotThreadSafe.
     * Guaranteed to fit inside a single 8-bit register (std::uint8_t) for zero-cost branching.
     */
    enum class SlotOperationStatus : std::int8_t {
        Success = 0,
        ErrorArrayFull = -1,
        ErrorOutOfBounds = -2,
        ErrorNullPointer = -3,
        ErrorSlotEmpty = -4
    };

    /**
     * @MethodKind Hot
     * Fast inline check for success without branching penalty.
     */
    [[nodiscard]] constexpr bool isSuccess(SlotOperationStatus status) noexcept {
        return status == SlotOperationStatus::Success;
    }


    /**
     * @struct SlotOperationResult
     * Represents the combined operational state and associated payload.
     * @tparam ValueType Type of the payload (e.g., std::size_t index).
     */
    template <typename ValueType = std::size_t>
    struct SlotOperationResult {
        SlotOperationStatus status{SlotOperationStatus::Success};
        ValueType value{};

        [[nodiscard]] constexpr bool isSuccess() const noexcept {
            return status == SlotOperationStatus::Success;
        }

        [[nodiscard]] constexpr SlotOperationStatus getStatus() const noexcept {
            return status;
        }

        [[nodiscard]] constexpr ValueType getValue() const noexcept {
            return value;
        }
    };

    /**
     * Specialization for void return types (e.g., void peek operations).
     */
    template <>
    struct SlotOperationResult<void> {
        SlotOperationStatus status{SlotOperationStatus::Success};

        [[nodiscard]] constexpr bool isSuccess() const noexcept {
            return status == SlotOperationStatus::Success;
        }

        [[nodiscard]] constexpr SlotOperationStatus getStatus() const noexcept {
            return status;
        }
    };
} // namespace Systic::System::Concurrency