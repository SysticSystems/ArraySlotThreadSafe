#pragma once

// Keep the includes outside the namespace to avoid polluting it
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    #include <intrin.h> 
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))
    #include <immintrin.h>
#else
    #include <thread>
#endif

namespace Systic::System::Concurrency {

    /**
     * @brief Relaxes the CPU during a spin-lock.
     * High-performance, cross-platform replacement for _mm_pause.
     */
    inline void Pause() noexcept {
        #if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
            _mm_pause();
        #elif defined(__GNUC__) || defined(__clang__)
            #if defined(__i386__) || defined(__x86_64__)
                _mm_pause();
            #elif defined(__arm__) || defined(__aarch64__)
                __asm__ volatile("yield");
            #else
                std::this_thread::yield();
            #endif
        #else
            std::this_thread::yield();
        #endif
    }

} // namespace Systic::System::Currency