#pragma once
#include <atomic>
#include <cstdint>
#include <fstream>
#include <bit>
#include <iostream>
#include <memory>
#include <systic/system/concurrency/Pause.hpp>

namespace Systic::System::Concurrency {
    /**
     * @class SlotThreadSafe
     * @tparam T The type of elements stored in the array.

     * @UsageRecommendation
         * Dynamic Allocation: Use when the total capacity is fixed at construction and never needs resizing.
         * Power-of-Two Geometry: Use when the size can be constrained to 2^n to leverage bit-shift index mapping.
         * Fine-Grained Concurrency: Use when threads need to operate on individual slots simultaneously without locking the container.
         * High-Frequency Lookup: Use to replace linear boolean scans with hardware-accelerated bit-scanning.

     * @Capabilities
         * L1 Cache Dominance: Metadata is packed at 1 bit/slot, keeping the entire search-space resident in the L1 cache.
         * Instruction-Level Parallelism: Evaluates a full Machine Word (W) of slots in a single CPU cycle using __builtin_ctzll.
         * Branchless Traversal: All index calculations use Bit-Shifts (S) and Masks (M), bypassing the CPU's branch predictor entirely.
         * Lock-Free Sovereignty: Concurrent access via Atomic Fetch-Or/And without mutex-induced context switches.

     * @Tradeoffs
         * The Metadata Tax: Requires an auxiliary control array (N/64) * 2. We sacrifice ~3.1% of RAM (for 64-bit T) to achieve a 64x search speedup.
         * Geometric Rigidity: Strictly adheres to 2^n sizing. Non-conforming sizes upscale to the next power of two, inducing Internal Fragmentation.
         * ISA Dependency: Hard-coded for BMI1/BMI2 instructions. Running on legacy hardware results in a significant Performance Penalty.

     * @Algorithm Twisted Bit-Scanning (TBS)
         * 1. Density Packing: Vacancy managed via external bitset where 1 bit equals 1 slot, reducing metadata footprint by 98.4%.
         * 2. Hardware Handshake: Vacancy discovery utilizes the Count Trailing Zeros (CTZ) instruction on inverted bitmasks.
         * 3. Atomic Claim: State changes are executed via atomic bitwise fetch_or/fetch_and to prevent global thread contention.
         * Cursor_Contention: We intentionally avoided a "global hint" or "next_available" cursor.
         * While a cursor could minimize search time to O(1), maintaining it would require
         * synchronized updates across the entire array. This would introduce a global
         * contention point, effectively serializing parallel inserts. We prefer the
         * O(N/64) bit-scan because it allows threads to operate on different memory
         * words independently, preserving true hardware parallelism.

     * @Complexity
         * Access: O(1) via direct pointer arithmetic.
         * Find Vacant: O(N/64) worst-case; O(1) average case via word-parallel scanning.
         * Insert/Remove: O(1) via single atomic bit-flip and direct write.
         * Space: O(N + (N/64) * 2) bytes.
     */
    template <typename T>
    requires std::is_copy_assignable_v<T>
    class SlotThreadSafe {
        private:
            /**
             *  Property that hold addresses of the real array slots.
             */
            std::unique_ptr<T*[]> array;

            /**
             * Property that hold the metadata array.
             * It contains:
             * - Size (1 x uint64_t)
             * - Control Bits (Size / 64 x uint64_t)
             * - Vacancy Bits (Size / 64 x uint64_t)
             */
            std::unique_ptr<std::uint64_t[]> metadata;

            /**
             * Property that hold the control bits.
             */
            std::uint64_t* controls;

            /**
             * Property that hold the vacancy bits.
             */
            std::uint64_t*  vacancy;

            /**
             * Property that hold the size of the array
             */
            std::uint64_t* size;

            /**
             * @MethodKind Cold
             * Validate the array size.
             * @param size The size of the array.
             */
            static void validateArraySize(const std::size_t& size);

            /**
             * @MethodKind Hot
             * Check if the index is within bounds.
             * @param index The index to check.
             * @throws std::out_of_range if the index is out of bounds.
             */
            void boundsCheck(const std::size_t& index) const {
                if (index >= *this->size) {
                    throw std::out_of_range("SlotThreadSafe: index out of range");
                }
            }

            /**
             * @MethodKind Hot
             * Find the bitmask slot from the index.
             * @param index The index to find.
             */
            [[nodiscard]] static std::size_t findBitmaskSlotFromIndex(const std::size_t& index) {
                // bitMask slot is index / 64 (shift right by 6). 
                return index >> 6;
            }

            /**
             * @MethodKind Hot
             * Find the bit index from the index.
             * @param index The index to find.
             * @return The bit index.
             */
            [[nodiscard]] static std::size_t findBitIndexFromIndex(const std::size_t& index) {
                // bitIndex is index % 64 (index & 0x3F).
                return index & 0x3F;
            }

            /**
             * @MethodKind Hot
             * Perform a safe operation on the array slot.
             * @param index The index of the slot.
             * @param callback The operation to perform.
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
                                callback(this->array.get() + index, bitMaskSlot, mask);
                                // --- RELEASE ---
                                (void) controlSlot.fetch_and(~mask, std::memory_order_release);
                                return;
                            } else {
                                // --- LOCK GRANTED --- Perform the callback.
                                auto result = callback(this->array.get() + index, bitMaskSlot, mask);
                                // --- RELEASE ---
                                (void) controlSlot.fetch_and(~mask, std::memory_order_release);
                                return result;
                            }
                        }
                    }
                    // Pause to prevent busy-waiting and to make cpu relax.
                    Pause();
                }
            }

            /**
             * Find a vacant slot in the array.
             * @MethodKind Hot
             * This is hot method that is called frequently, so it is placed in header for hot calls.
             * @Algorithm
             * 1. First get the number of 64bit (std::uint64_t) slots in that vacancy array.
             * 2. Iterate over those chunks.
             * 3. For each chunk, cast it to std::uint64_t and get position of its Most significant bit.
             * @return The index of the vacant slot.
             * @throws runtime_error if couldn't claim a slot.
             */
            [[nodiscard]] std::size_t findVacantSlot() const {
                // 1. Scale down: How many 64-bit chunks? (Shift is safer than rotr)
                const std::size_t numberOfChunks = *this->size >> 6;

                for (std::size_t i = 0; i < numberOfChunks; ++i) {
                    std::atomic_ref<std::uint64_t> vacancySlot(this->vacancy[i]);
                    std::uint64_t vacancySlotBits = vacancySlot.load(std::memory_order_relaxed);

                    /**
                     * THE SNIPER LOOP:
                     * We don't give up on this chunk until it is physically full (~0ULL).
                     * If compare_exchange_weak fails, it means another thread 'stole' the bit 
                     * we were aiming for. However, the hardware automatically updates 'vBits' 
                     * with the NEW state of the memory. We immediately retry on the same chunk 
                     * to grab the next available '0' without restarting the scan.
                     */
                    while (vacancySlotBits != ~0ULL) {
                        // Find the first '0' from the LEFT (MSB).
                        const std::size_t relativeIndex = std::countr_zero(~vacancySlotBits);
                        const std::uint64_t mask = 1ULL << relativeIndex;

                        if (vacancySlot.compare_exchange_weak(vacancySlotBits, vacancySlotBits | mask, std::memory_order_acquire)) {
                            return (i << 6) + relativeIndex;
                        }
                    }
                }

                throw std::runtime_error("No vacant slot available");
            }

        public:
            /**
             * @MethodKind Cold
             * Constructor that initialize the array with the given size.
             * @param size The size of the array.
             * @throws std::bad_alloc if the array cannot be allocated.
             */
            explicit SlotThreadSafe(const std::size_t& size);

            ~SlotThreadSafe();

            /**
             * @MethodKind Hot
             * Add an item to the array.
             * @param item The item to add.
             * @return The index of the added item.
             * @throws std::runtime_error if there is no vacant slot.
             */
            std::size_t add(const T* item) {
                const std::size_t index = this->findVacantSlot();

                this->insertAt(
                    item,
                    index
                );
                return index;
            }

            /**
             * @MethodKind Hot
             * Insert an item at a specific index.
             * @param item The item to insert.
             * @param index The index to insert the item at.
             */
            void insertAt(const T* item, const std::size_t& index) {

                this->template safeArraySlotOperation<void>(index, [item, this, index](T** slot, const std::size_t, const std::uint64_t) {
                    // Insert the item
                    *slot = const_cast<T*>(item);

                });
            }

            /**
             * @MethodKind Hot
             * Peek at an item without copying it and with no other thread modify it.
             * The pointer 'slot' is guaranteed to be valid ONLY during the callback.
             */
            template <typename ReturnType, typename Func>
            ReturnType peek(const std::size_t& index, const Func callback) const {
                return this->template safeArraySlotOperation<ReturnType>(index, [&](T** slot, const std::size_t bitmaskSlot, const std::uint64_t mask) {
                    // The Control Bit is 1.
                    // No other thread can 'add' or 'remove' this specific slot.
                    const T* protectedSlot = *slot;
                    // Choice A: User only wants the data (1 argument)
                    if constexpr (std::is_invocable_v<Func, const T*>) {
                        return callback(protectedSlot);
                    }
                    // Choice B: User wants the full surgical set (3 arguments)
                    else {
                        return callback(protectedSlot, bitmaskSlot, mask);
                    }
                    // Once this lambda exits, safeArraySlotOperation flips the bit back to 0.
                });
            }

            /**
             * @MethodKind Hot
             * Remove an item at a specific index.
             * @param index The index to remove the item at.
             */
            void removeAt(const std::size_t& index) {
                this->template safeArraySlotOperation<void>(index, [this](T** slot, const std::size_t bitmaskSlotIndex, const std::uint64_t mask) {
                    // Remove the item
                    *slot = nullptr;
                    std::atomic_ref<std::uint64_t> vacancySlot(this->vacancy[bitmaskSlotIndex]);
                    // No need for loop here since we already hold the control lock.
                    (void) vacancySlot.fetch_and(~mask, std::memory_order_release);
                });
            }
            #ifdef SYSTIC_RELEASE_WITH_DEBUG_INFO
                /**
                 * @MethodKind Cold
                 * Get the metadata array.
                 * @usage Internal : Use for unit testing only.
                 * @return The control array.
                 */
                [[nodiscard]] const std::uint64_t *getMetadata() const;

                /**
                 * @MethodKind Cold
                 * Get the metadata array.
                 * @usage Internal : Use for unit testing only.
                 * @return The control array.
                 */
                [[nodiscard]] const std::uint64_t *getControls() const;

                /**
                 * @MethodKind Cold
                 * Get the metadata array.
                 * @usage Internal : Use for unit testing only.
                 * @return The vacancy array.
                 */
                [[nodiscard]] const std::uint64_t *getVacancy() const;

                /**
                 * @MethodKind Cold
                 * Get the size of the array.
                 * @usage Internal : Use for unit testing only.
                 * @return The size of the array.
                 */
                [[nodiscard]] const std::size_t *getSize() const;

                friend std::ostream& operator<<(std::ostream& os, const SlotThreadSafe<T>& item) {
                    for (std::size_t i = 0; i < *item.getSize(); ++i) {
                        os << '[' << i << "=> (";
                        if (item.array.get()[i] != nullptr) {
                            os << **(item.array.get() + i);
                        }
                        os << ")],";
                    }
                    return os;
                }
            #endif
    };
}