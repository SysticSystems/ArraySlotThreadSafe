/**
 * @file ThreadSafe.hpp
 * @namespace Systic::System::Concurrency
 * @brief Deadlock-Immune Artifact Wrapper (Value Semantics Edition)
 * * DESIGN PHILOSOPHY:
 * 1. Hardware-Aware: Uses Atomic "Single-Grip" for primitives.
 * 2. Mutex-Guarded: Uses "Privacy Curtains" for complex/large data (>16 bytes).
 * 3. Deadlock-Proof: Deletes operator= to prevent circular wait dependencies.
 * 4. Cache-Optimized: Aligned to 64-byte boundaries to prevent False Sharing.
 */

#pragma once

#include <mutex>
#include <atomic>
#include <type_traits>

namespace Systic::System::Concurrency {

    /**
     * @brief The Mutex-Based Wrapper (The "Curtain")
     * Used for complex objects and large data (like your 512-byte artifacts).
     * * JUSTIFICATION:
     * Since 512 bytes exceeds the CPU's 16-byte register limit, we use a local
     * mutex to ensure "Full-Block Integrity" during copies.
     */
    template <typename T, typename = void>
    class alignas(64) ThreadSafe {
    private:
        T data;
        mutable std::mutex mtx;

    public:
        // Constructor: Initial state
        explicit ThreadSafe(T val) : data(std::move(val)) {}

        /**
         * @brief Safe-Birth Copy Constructor
         * JUSTIFICATION: The destination ('this') is private to the current
         * thread during construction. We only lock the source. This is 100%
         * deadlock-proof because it never holds two active locks.
         */
        ThreadSafe(const ThreadSafe& other) {
            std::lock_guard<std::mutex> lock(other.mtx);
            data = other.data;
        }

        /**
         * @brief FORBIDDEN: Assignment Operator
         * JUSTIFICATION: Deleting this prevents the "Global Shared Memory"
         * mess. It eliminates the risk of Thread A (A=B) and Thread B (B=A)
         * locking each other into a freeze.
         */
        ThreadSafe& operator=(const ThreadSafe&) = delete;

        // Returns a safe snapshot of the data
        T get() const {
            std::lock_guard<std::mutex> lock(mtx);
            return data;
        }
    };

    /**
     * @brief The Atomic-Based Specialization (The "Fast-Grip")
     * Used for arithmetic types (int, float, bool, pointers).
     * * JUSTIFICATION:
     * Small data fits in CPU registers. We bypass the mutex entirely to
     * achieve hardware-level speed with zero locking overhead.
     */
    template <typename T>
    class alignas(std::atomic<T>::is_always_lock_free ? alignof(std::atomic<T>) : 64)
    ThreadSafe<T, typename std::enable_if<std::is_arithmetic<T>::value || std::is_pointer<T>::value>::type> {
    private:
        std::atomic<T> data;

    public:
        explicit ThreadSafe(T val) : data(val) {}

        // Atomics handle their own memory ordering
        ThreadSafe(const ThreadSafe& other) : data(other.data.load(std::memory_order_relaxed)) {}

        // Maintain the "No-Shit" rule (No assignment)
        ThreadSafe& operator=(const ThreadSafe&) = delete;

        // Direct hardware-load
        T get() const { return data.load(std::memory_order_relaxed); }

        // Implicit conversion for "Invisible" usage in logic
        operator T() const { return get(); }
    };

} // namespace Systic::Tools::Concurrency