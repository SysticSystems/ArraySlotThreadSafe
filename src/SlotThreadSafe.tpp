namespace Systic::System::Concurrency {

    template <typename T>
    requires std::is_copy_assignable_v<T>
    SlotThreadSafe<T>::SlotThreadSafe(const std::size_t& size) {
        SlotThreadSafe<T>::validateArraySize(size);
        
        const std::size_t numberOfBitmaskSlots = size >> 6;

        this->metadata = std::make_unique<std::uint64_t[]>((numberOfBitmaskSlots << 1) + 1);
        this->array = std::unique_ptr<T*[]>(new T*[size]);

        this->size = reinterpret_cast<std::uint64_t*>(this->metadata.get());
        *this->size = size;

        this->controls = reinterpret_cast<std::uint64_t*>(this->metadata.get() + 1);
        this->vacancy = reinterpret_cast<std::uint64_t*>(this->metadata.get() + numberOfBitmaskSlots + 1);
    }

    template <typename T>
    requires std::is_copy_assignable_v<T>
    SlotThreadSafe<T>::~SlotThreadSafe() {
        const std::size_t numberOfBitmaskSlots = *this->size >> 6;

        for (std::size_t i = 0; i < numberOfBitmaskSlots; ++i) {
            std::atomic_ref<std::uint64_t> busyChunk(this->controls[i]);

            while (busyChunk.load(std::memory_order_acquire) != 0) {
                // Using the inline Pause instruction pulled from your :CpuIntrinsics partition 
                // via the module interface map rather than heavy OS thread context yielding!
                Pause(); 
            }
        }
        this->array.reset();
        this->metadata.reset();
        this->vacancy = nullptr;
        this->controls = nullptr;
        this->size = nullptr;
    }

    template <typename T>
    requires std::is_copy_assignable_v<T>
    void SlotThreadSafe<T>::validateArraySize(const std::size_t& size) {
        if (size < 64 || std::popcount(size) != 1) [[unlikely]] {
            
            const char msg[] = "[FATAL] Systic::SlotThreadSafe - Geometry violation. "
                            "Size must be a power of 2 and >= 64.\n";
            
            // Naked syscall to FD 2 (stderr). No buffers, no vtables, no garbage.
            auto written = ::write(2, msg, sizeof(msg) - 1);
            (void)written; 

            // Induce a direct hardware trap or a standard quick exit
            #if defined(__GNUC__) || defined(__clang__)
                __builtin_trap(); 
            #else
                _exit(1); // Clean POSIX immediate process termination
            #endif
        }
    }

    // Aligned to match your clean NDEBUG optimization architecture
    #ifndef NDEBUG
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

} // namespace Systic::System::Concurrency