module;

#include <atomic>
#include <cstdint>
#include <bit>
#include <memory>
#include <unistd.h>

#ifndef NDEBUG
#include <iostream>
#endif

export module Systic.System.Concurrency.SlotThreadSafe;

import :CpuIntrinsics;
export import :SlotOperationResult;

export namespace Systic::System::Concurrency {

    /**
     * @class SlotThreadSafe
     * @tparam T The type of elements stored in the array.
     *
     * @UsageRecommendation
     * Dynamic Allocation: Use when the total capacity is fixed at construction and never needs resizing.
     * Power-of-Two Geometry: Use when the size can be constrained to 2^n to leverage bit-shift index mapping.
     * Fine-Grained Concurrency: Use when threads need to operate on individual slots simultaneously without locking the container.
     * High-Frequency Lookup: Use to replace linear boolean scans with hardware-accelerated bit-scanning.
     *
     * @Capabilities
     * L1 Cache Dominance: Metadata is packed at 1 bit/slot, keeping the entire search-space resident in the L1 cache.
     * Instruction-Level Parallelism: Evaluates a full Machine Word (W) of slots in a single CPU cycle using countr_zero.
     * Branchless Traversal: All index calculations use Bit-Shifts (S) and Masks (M), bypassing the CPU's branch predictor entirely.
     * Lock-Free Sovereignty: Concurrent access via Atomic Fetch-Or/And without mutex-induced context switches.
     */
    template <typename T>
    requires std::is_copy_assignable_v<T>
    class SlotThreadSafe {
        private:
            /**
             * Property that holds raw addresses of the array slots.
             */
            T** array{nullptr};

            /**
             * Raw heap block for packed metadata:
             * [ Size (1 word) | Controls (N/64 words) | Vacancy (N/64 words) ]
             */
            std::uint64_t* metadata{nullptr};

            /**
             * Raw pointer pointing into the metadata array for control bits.
             */
            std::uint64_t* controls{nullptr};

            /**
             * Raw pointer pointing into the metadata array for vacancy bits.
             */
            std::uint64_t* vacancy{nullptr};

            /**
             * Property that holds the size of the array (points to metadata[0]).
             */
            std::uint64_t* size{nullptr};

            /**
             * @MethodKind Cold
             * Validate the array size.
             */
            void validateArraySize(const std::size_t& size) const {
                if (size < 64 || std::popcount(size) != 1) [[unlikely]] {
                    throw std::invalid_argument("[FATAL] Systic::SlotThreadSafe - Geometry violation. "
                                                "Size must be a power of 2 and >= 64.\n");
                }
            }

            /**
             * @MethodKind Hot
             * Check if the index is within bounds.
             */
            [[nodiscard]] SlotOperationStatus boundsCheck(const std::size_t& index) const noexcept {
                if (index >= *this->size) [[unlikely]] {
                    return SlotOperationStatus::ErrorOutOfBounds;
                }
                return SlotOperationStatus::Success;
            }

            /**
             * @MethodKind Hot
             * Find the bitmask slot from the index.
             */
            [[nodiscard]] static constexpr std::size_t findBitmaskSlotFromIndex(const std::size_t& index) noexcept {
                return index >> 6;
            }

            /**
             * @MethodKind Hot
             * Find the bit index from the index.
             */
            [[nodiscard]] static constexpr std::size_t findBitIndexFromIndex(const std::size_t& index) noexcept {
                return index & 0x3F;
            }

            /**
             * @MethodKind Hot
             * Perform a safe operation on the array slot within the lock bit boundary.
             */
            template <typename ReturnType, typename Func>
            ReturnType safeArraySlotOperation(const std::size_t& index, const Func callback) const {
                const std::size_t bitIndex = SlotThreadSafe<T>::findBitIndexFromIndex(index);
                const std::size_t bitMaskSlot = SlotThreadSafe<T>::findBitmaskSlotFromIndex(index);
                const std::uint64_t mask = 1ULL << bitIndex;
                const std::atomic_ref<std::uint64_t> controlSlot(this->controls[bitMaskSlot]);

                while (true) {
                    // Take a peak at the current slot bits (std::memory_order_relaxed).
                    uint64_t currentSlotBits = controlSlot.load(std::memory_order_relaxed);
                    if (!(currentSlotBits & mask)) {
                        // Try to set the control bit to 1 if the currentSlotBits hasn't been changed during the operation.
                        if (controlSlot.compare_exchange_weak(currentSlotBits, currentSlotBits | mask, std::memory_order_acquire)) {
                            if constexpr(std::is_void_v<ReturnType>) {
                                // --- LOCK GRANTED --- Perform the callback.
                                callback(this->array + index, bitMaskSlot, mask);
                                // --- RELEASE ---
                                (void) controlSlot.fetch_and(~mask, std::memory_order_release);
                                return;
                            } else {
                                // --- LOCK GRANTED --- Perform the callback.
                                auto result = callback(this->array + index, bitMaskSlot, mask);
                                // --- RELEASE ---
                                (void) controlSlot.fetch_and(~mask, std::memory_order_release);
                                return result;
                            }
                        }
                    }
                    // Pause to prevent busy-waiting and to make cpu relax.
                    Systic::System::Concurrency::Pause();
                }
            }
            /**
             * Find a vacant slot in the array.
             * @MethodKind Hot
             * @return The index of the vacant slot, or static_cast<std::size_t>(-1) if full.
             */
            [[nodiscard]] std::size_t findVacantSlot() const noexcept {
                const std::size_t numberOfChunks = *this->size >> 6;

                for (std::size_t i = 0; i < numberOfChunks; ++i) {
                    std::atomic_ref<std::uint64_t> vacancySlot(this->vacancy[i]);
                    std::uint64_t vacancySlotBits = vacancySlot.load(std::memory_order_relaxed);

                    while (vacancySlotBits != ~0ULL) {
                        const std::size_t relativeIndex = std::countr_zero(~vacancySlotBits);
                        const std::uint64_t mask = 1ULL << relativeIndex;

                        if (vacancySlot.compare_exchange_weak(vacancySlotBits, vacancySlotBits | mask, std::memory_order_acquire)) {
                            return (i << 6) + relativeIndex;
                        }
                    }
                }

                return static_cast<std::size_t>(-1);
            }

        public:
            /**
             * @MethodKind Cold
             * Constructor allocating raw continuous blocks for pointers and packed metadata.
             */
            explicit SlotThreadSafe(const std::size_t& sizeVal) {
                this->validateArraySize(sizeVal);

                const std::size_t numberOfBitmaskSlots = sizeVal >> 6;

                this->metadata = new std::uint64_t[(numberOfBitmaskSlots << 1) + 1]();
                this->array = new T*[sizeVal]();

                this->size = reinterpret_cast<std::uint64_t*>(this->metadata);
                *this->size = sizeVal;

                this->controls = reinterpret_cast<std::uint64_t*>(this->metadata + 1);
                this->vacancy = reinterpret_cast<std::uint64_t*>(this->metadata + numberOfBitmaskSlots + 1);
            }
            
            /**
             * @MethodKind Cold
             * Destructor cleaning up raw memory and releasing all owned objects.
             */
            ~SlotThreadSafe() noexcept {
                if (!this->size || !*this->size) [[unlikely]] {
                    return;
                }

                const std::size_t numberOfBitmaskSlots = *this->size >> 6;

                for (std::size_t i = 0; i < numberOfBitmaskSlots; ++i) {
                    std::atomic_ref<std::uint64_t> busyChunk(this->controls[i]);

                    while (busyChunk.load(std::memory_order_acquire) != 0) {
                        Pause();
                    }
                }

                if (this->array) {
                    const std::size_t totalSlots = *this->size;
                    for (std::size_t i = 0; i < totalSlots; ++i) {
                        if (T* ptr = this->array[i]) {
                            delete ptr;
                            this->array[i] = nullptr;
                        }
                    }
                }

                delete[] this->array;
                delete[] this->metadata;
                this->vacancy = nullptr;
                this->controls = nullptr;
                this->size = nullptr;
            }

            /**
             * @MethodKind Hot
             * Transfers ownership of a unique_ptr into a vacant slot.
             * @param item Unique pointer owning the item.
             * @return Result containing SlotOperationStatus and the claimed index payload.
             */
            [[nodiscard]] SlotOperationResult<std::size_t> add(std::unique_ptr<T> item) noexcept {
                if (!item) [[unlikely]] {
                    return {SlotOperationStatus::ErrorNullPointer, static_cast<std::size_t>(-1)};
                }

                const std::size_t index = this->findVacantSlot();
                if (index == static_cast<std::size_t>(-1)) {
                    return {SlotOperationStatus::ErrorArrayFull, static_cast<std::size_t>(-1)};
                }

                T* rawPtr = item.release();
                this->template safeArraySlotOperation<void>(index, [rawPtr](T** slot, const std::size_t, const std::uint64_t) noexcept {
                    *slot = rawPtr;
                });

                return {SlotOperationStatus::Success, index};
            }

            /**
             * @MethodKind Hot
             * Insert an item at a specific index, overwriting and deleting any existing item.
             * @param item Unique pointer owning the item.
             * @param index Target slot index.
             * @return Result containing SlotOperationStatus and target index.
             */
            [[nodiscard]] SlotOperationResult<std::size_t> insertAt(std::unique_ptr<T> item, const std::size_t& index) noexcept {
                if (!item) [[unlikely]] {
                    return {SlotOperationStatus::ErrorNullPointer, index};
                }

                const SlotOperationStatus boundsStatus = this->boundsCheck(index);
                if (boundsStatus != SlotOperationStatus::Success) {
                    return {boundsStatus, index};
                }

                // 1. Un-envelope upfront (guaranteed to be placed in slot)
                T* rawPtr = item.release();

                // 2. Perform atomic slot write and overwrite existing data
                this->template safeArraySlotOperation<void>(index, [this, rawPtr](T** slot, const std::size_t bitmaskSlotIndex, const std::uint64_t mask) noexcept {
                    // Erase existing data if present
                    if (*slot != nullptr) {
                        delete *slot;
                    } else {
                        // If the slot was previously vacant, update vacancy bitmask to marked/occupied
                        std::atomic_ref<std::uint64_t> vacancySlot(this->vacancy[bitmaskSlotIndex]);
                        (void) vacancySlot.fetch_or(mask, std::memory_order_release);
                    }

                    // Store new object
                    *slot = rawPtr;
                });

                return {SlotOperationStatus::Success, index};
            }

            /**
             * @MethodKind Hot
             * Inspects an item while keeping the slot locked against concurrent modification.
             */
            template <typename ReturnType, typename Func>
            [[nodiscard]] SlotOperationResult<ReturnType> peek(const std::size_t& index, const Func callback) const noexcept {
                const SlotOperationStatus boundsStatus = this->boundsCheck(index);
                if (boundsStatus != SlotOperationStatus::Success) {
                    return {boundsStatus};
                }

                if constexpr (std::is_void_v<ReturnType>) {
                    this->template safeArraySlotOperation<void>(
                        index,
                        [&](T** slot, const std::size_t bitmaskSlot, const std::uint64_t mask) noexcept {
                            const T* protectedSlot = *slot;
                            if constexpr (std::is_invocable_v<Func, const T*>) {
                                callback(protectedSlot);
                            } else {
                                callback(protectedSlot, bitmaskSlot, mask);
                            }
                        }
                    );

                    return {SlotOperationStatus::Success};
                } else {
                    ReturnType value = this->template safeArraySlotOperation<ReturnType>(
                        index,
                        [&](T** slot, const std::size_t bitmaskSlot, const std::uint64_t mask) noexcept {
                            const T* protectedSlot = *slot;
                            if constexpr (std::is_invocable_v<Func, const T*>) {
                                return callback(protectedSlot);
                            } else {
                                return callback(protectedSlot, bitmaskSlot, mask);
                            }
                        }
                    );

                    return {SlotOperationStatus::Success, value};
                }
            }

            /**
             * @MethodKind Hot
             * Deletes the item at index, resets slot memory, and releases the vacancy bit.
             * @param index Target slot index.
             * @return Result containing SlotOperationStatus and freed index.
             */
            [[nodiscard]] SlotOperationResult<std::size_t> removeAt(const std::size_t& index) noexcept {
                const SlotOperationStatus boundsStatus = this->boundsCheck(index);
                if (boundsStatus != SlotOperationStatus::Success) {
                    return {boundsStatus, index};
                }

                SlotOperationStatus status = SlotOperationStatus::Success;

                this->template safeArraySlotOperation<void>(index, [this, &status](T** slot, const std::size_t bitmaskSlotIndex, const std::uint64_t mask) noexcept {
                    if (*slot == nullptr) {
                        status = SlotOperationStatus::ErrorSlotEmpty;
                        return;
                    }

                    delete *slot;
                    *slot = nullptr;


                    std::atomic_ref<std::uint64_t> vacancySlot(this->vacancy[bitmaskSlotIndex]);
                    (void) vacancySlot.fetch_and(~mask, std::memory_order_release);
                });

                return {status, index};
            }

            /**
             * @MethodKind Hot
             * Get array capacity.
             */
            [[nodiscard]] const std::size_t* getSize() const noexcept {
                return reinterpret_cast<const std::size_t*>(this->size);
            }

            /**
             * @MethodKind Hot
             * Get direct pointer to vacancy bits.
             */
            [[nodiscard]] const std::uint64_t* getVacancy() const noexcept {
                return this->vacancy;
            }

        #ifndef RelWithDebInfo
                const std::uint64_t* getControls() const noexcept {
                        return this->controls;
                    }

                const std::uint64_t* getMetadata() const noexcept {
                        return this->metadata;
                    }
        #endif

        #ifndef NDEBUG

                friend std::ostream& operator<<(std::ostream& os, const SlotThreadSafe<T>& item) {
                    for (std::size_t i = 0; i < *item.getSize(); ++i) {
                        os << '[' << i << "=> (";
                        if (item.array[i] != nullptr) {
                            os << *(item.array[i]);
                        }
                        os << ")],";
                    }
                    return os;
                }
            #endif
    };
} // namespace Systic::System::Concurrency