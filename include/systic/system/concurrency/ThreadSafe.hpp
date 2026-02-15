#pragma once
#include <mutex>
#include <shared_mutex>
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
    protected:
        T data;
        mutable std::shared_mutex mtx; // Changed from std::mutex to std::shared_mutex

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
            std::unique_lock<std::shared_mutex> lock(other.mtx); // Explicit template argument
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
            std::shared_lock<std::shared_mutex> lock(mtx); // Shared lock for read
            return data;
        }

        /**
         * @brief Manipulate the data within an Exclusive Write Access.
         * @complexity O(Func) + Unique Lock Overhead.
         */
        template <typename Func>
        auto executeCallbackInWriteMode(Func&& func) {
            std::unique_lock<std::shared_mutex> lock(mtx); // Exclusive lock
            return func(data);
        }

        /**
         * @brief Manipulate the data within a Shared Read Access.
         * @complexity O(Func) + Shared Lock Overhead.
         */
        template <typename Func>
        auto executeCallbackInReadMode(Func&& func) const {
            std::shared_lock<std::shared_mutex> lock(mtx); // Shared lock
            return func(data);
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

} // namespace Systic::System::Concurrency
