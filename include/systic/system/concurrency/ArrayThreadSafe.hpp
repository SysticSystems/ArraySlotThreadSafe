#pragma once

#include "ThreadSafe.hpp"
#include <array>
#include <optional>
#include <algorithm>

namespace Systic::System::Concurrency {

/**
 * @struct ArrayState
 * @brief Internal state grouping the contiguous buffer and the occupancy cursor.
 * @tparam T Type of element.
 * @tparam ARRAY_SIZE Compile-time fixed capacity.
 */
template <typename T, std::size_t ARRAY_SIZE>
struct ArrayState {
    std::array<T, ARRAY_SIZE> buffer;
    std::size_t Cursor = 0;
};

/**
 * @class ArrayThreadSafe
 * @brief A thread-safe, fixed-size contiguous container.
 * Inherits from ThreadSafe to provide synchronized access to both data and cursor.
 */
template <typename T, std::size_t ARRAY_SIZE>
class ArrayThreadSafe : public ThreadSafe<ArrayState<T, ARRAY_SIZE>> {
public:
    ArrayThreadSafe() : ThreadSafe<ArrayState<T, ARRAY_SIZE>>(ArrayState<T, ARRAY_SIZE>()) {};

    /**
     * @brief Atomic append to the end of the available data.
     * @complexity O(1) + Mutex Overhead.
     * @return true if successful, false if capacity ARRAY_SIZE is reached.
     */
    bool pushBack(const T& value) {
        std::unique_lock lock(this->mtx);
        if (this->data.Cursor < ARRAY_SIZE) {
            this->data.buffer[this->data.Cursor++] = value;
            return true;
        }
        return false;
    }

    /**
     * @brief Inserts an element at a specific index, shifting existing elements.
     * @complexity O(ARRAY_SIZE) due to contiguous memory shifting.
     * @return true if inserted, false if index out of bounds or array full.
     */
    bool insertAt(std::size_t index, const T& value) {
        std::unique_lock lock(this->mtx);
        
        if (index > this->data.Cursor || this->data.Cursor >= ARRAY_SIZE) {
            return false;
        }

        // Shift elements to the right to create a gap
        if (index < this->data.Cursor) {
            for (std::size_t i = this->data.Cursor; i > index; --i) {
                this->data.buffer[i] = std::move(this->data.buffer[i - 1]);
            }
        }

        this->data.buffer[index] = value;
        this->data.Cursor++;
        return true;
    }

    /**
     * @brief Removes an element at a specific index, shifting remaining elements.
     * @complexity O(ARRAY_SIZE) due to contiguous memory shifting.
     * @return true if removed, false if index out of bounds.
     */
    bool removeAt(std::size_t index) {
        std::unique_lock lock(this->mtx);
        
        if (index >= this->data.Cursor) {
            return false;
        }

        // Shift elements to the left to fill the gap
        for (std::size_t i = index; i < this->data.Cursor - 1; ++i) {
            this->data.buffer[i] = std::move(this->data.buffer[i + 1]);
        }

        this->data.Cursor--;
        return true;
    }

    /**
     * @brief Accesses an element with bounds checking.
     * @complexity O(1) + Mutex Overhead.
     */
    std::optional<T> at(std::size_t index) const {
        std::shared_lock lock(this->mtx);
        if (index < this->data.Cursor) {
            return this->data.buffer[index];
        }
        return std::nullopt;
    }

    /**
     * @brief Returns current number of elements (occupancy).
     * @complexity O(1).
     */
    std::size_t size() const {
        std::shared_lock lock(this->mtx);
        return this->data.Cursor;
    }

    /**
     * @brief Returns max capacity (ARRAY_SIZE).
     * @complexity O(1).
     */
    constexpr std::size_t capacity() const noexcept {
        return ARRAY_SIZE;
    }

    /**
     * @brief Resets the cursor to 0.
     * @complexity O(1).
     */
    void clear() {
        std::unique_lock lock(this->mtx);
        this->data.Cursor = 0;
    }
    
    /**
     * @brief Checks if the array has reached its maximum capacity.
     * @complexity O(1) + Shared Lock Overhead.
     * @return true if Cursor == ARRAY_SIZE, false otherwise.
     */
    bool isFull() const {
        std::shared_lock lock(this->mtx);
        return this->data.Cursor >= ARRAY_SIZE;
    }
};

} // namespace systic::system::concurrency