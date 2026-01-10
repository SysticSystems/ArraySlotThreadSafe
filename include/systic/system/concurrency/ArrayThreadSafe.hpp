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
 * @tparam N Compile-time fixed capacity.
 */
template <typename T, std::size_t N>
struct ArrayState {
    std::array<T, N> Buffer;
    std::size_t Cursor = 0;
};

/**
 * @class ArrayThreadSafe
 * @brief A thread-safe, fixed-size contiguous container.
 * Inherits from ThreadSafe to provide synchronized access to both data and cursor.
 */
template <typename T, std::size_t N>
class ArrayThreadSafe : public ThreadSafe<ArrayState<T, N>> {
public:
    ArrayThreadSafe() : ThreadSafe<ArrayState<T, N>>(ArrayState<T, N>{}) {}

    /**
     * @brief Atomic append to the end of the available data.
     * @complexity O(1) + Mutex Overhead.
     * @return true if successful, false if capacity N is reached.
     */
    bool pushBack(const T& value) {
        std::unique_lock lock(this->_mtx);
        if (this->_data.Cursor < N) {
            this->_data.Buffer[this->_data.Cursor++] = value;
            return true;
        }
        return false;
    }

    /**
     * @brief Inserts an element at a specific index, shifting existing elements.
     * @complexity O(N) due to contiguous memory shifting.
     * @return true if inserted, false if index out of bounds or array full.
     */
    bool insertAt(std::size_t index, const T& value) {
        std::unique_lock lock(this->_mtx);
        
        if (index > this->_data.Cursor || this->_data.Cursor >= N) {
            return false;
        }

        // Shift elements to the right to create a gap
        if (index < this->_data.Cursor) {
            for (std::size_t i = this->_data.Cursor; i > index; --i) {
                this->_data.Buffer[i] = std::move(this->_data.Buffer[i - 1]);
            }
        }

        this->_data.Buffer[index] = value;
        this->_data.Cursor++;
        return true;
    }

    /**
     * @brief Removes an element at a specific index, shifting remaining elements.
     * @complexity O(N) due to contiguous memory shifting.
     * @return true if removed, false if index out of bounds.
     */
    bool removeAt(std::size_t index) {
        std::unique_lock lock(this->_mtx);
        
        if (index >= this->_data.Cursor) {
            return false;
        }

        // Shift elements to the left to fill the gap
        for (std::size_t i = index; i < this->_data.Cursor - 1; ++i) {
            this->_data.Buffer[i] = std::move(this->_data.Buffer[i + 1]);
        }

        this->_data.Cursor--;
        return true;
    }

    /**
     * @brief Accesses an element with bounds checking.
     * @complexity O(1) + Mutex Overhead.
     */
    std::optional<T> at(std::size_t index) const {
        std::shared_lock lock(this->_mtx);
        if (index < this->_data.Cursor) {
            return this->_data.Buffer[index];
        }
        return std::nullopt;
    }

    /**
     * @brief Returns current number of elements (occupancy).
     * @complexity O(1).
     */
    std::size_t size() const {
        std::shared_lock lock(this->_mtx);
        return this->_data.Cursor;
    }

    /**
     * @brief Returns max capacity (N).
     * @complexity O(1).
     */
    constexpr std::size_t capacity() const noexcept {
        return N;
    }

    /**
     * @brief Resets the cursor to 0.
     * @complexity O(1).
     */
    void clear() {
        std::unique_lock lock(this->_mtx);
        this->_data.Cursor = 0;
    }
};

} // namespace systic::system::concurrency