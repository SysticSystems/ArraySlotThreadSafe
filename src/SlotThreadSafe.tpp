#include <format>
#include <thread>
#include <systic/system/concurrency/SlotThreadSafe.hpp>

namespace Systic::System::Concurrency {

    template <typename T>
    requires std::is_copy_assignable_v<T>
    SlotThreadSafe<T>::SlotThreadSafe(const std::size_t& size) {
        SlotThreadSafe<T>::validateArraySize(size);
        /**
         * Metadata array holds size + control + vacancy memory
         * Metadata size = 32bit + Size / 64  bit + Size / 64 bit.
         */
        const std::size_t numberOfBitmaskSlots = size >> 6;

        this->metadata = std::make_unique<std::uint64_t[]>( (numberOfBitmaskSlots << 1) + 1 );

        // Initialize the real array.
        this->array = std::unique_ptr<T*[]>(new T*[size]);

        // Initialize the size.
        this->size = reinterpret_cast<std::uint64_t*>(this->metadata.get());
        *this->size = size;

        // Initialize the Bit Control array.
        this->controls = reinterpret_cast<std::uint64_t*>(this->metadata.get() + 1);

        // Initialize the Bit Vacancy array.
        this->vacancy = reinterpret_cast<std::uint64_t*>(this->metadata.get() + numberOfBitmaskSlots + 1);
    }

    template <typename T>
    requires std::is_copy_assignable_v<T>
    SlotThreadSafe<T>::~SlotThreadSafe() {
        const std::size_t numberOfBitmaskSlots = *this->size >> 6;

        for (std::size_t i = 0; i < numberOfBitmaskSlots; ++i) {
            std::atomic_ref<std::uint64_t> busyChunk(this->controls[i]);

            // Spin until the 64 threads working in this chunk are done
            while (busyChunk.load(std::memory_order_acquire) != 0) {
                std::this_thread::yield(); // Give the worker threads CPU time to finish
            }
        }
        this->array.reset();
        this->metadata.reset();
        this->vacancy = nullptr;
        this->controls = nullptr;
        this->vacancy = nullptr;
        this->size = nullptr;
    }

    template <typename T>
    requires std::is_copy_assignable_v<T>
    void SlotThreadSafe<T>::validateArraySize(const std::size_t& size) {
        // Check if size is a power of 2 and at least 64
        if (size < 64 || std::popcount(size) != 1) {

            throw std::invalid_argument(
                std::format("Size must be exactly a power of 2 and >= 64. But {} was provided.", size)
            );
        }
    }

    #ifdef SYSTIC_RELEASE_WITH_DEBUG_INFO
        template <typename T>
        requires std::is_copy_assignable_v<T>
        [[nodiscard]] const std::uint64_t* SlotThreadSafe<T>::getSize() const {
            return this->size;
        }

        template <typename T>
        requires std::is_copy_assignable_v<T>
        [[nodiscard]] const std::uint64_t* SlotThreadSafe<T>::getVacancy() const {
            return this->vacancy;
        }

        template <typename T>
        requires std::is_copy_assignable_v<T>
        [[nodiscard]] const std::uint64_t* SlotThreadSafe<T>::getControls() const {
            return this->controls;
        }
        
        template <typename T>
        requires std::is_copy_assignable_v<T>
        [[nodiscard]] const std::uint64_t* SlotThreadSafe<T>::getMetadata() const {
            return this->metadata.get();
        }
    #endif
}
