#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <ratio>
#include <stdexcept>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

import Systic.System.Concurrency;

namespace Systic::System::Concurrency::Test {
    struct Dummy { int id; };

    TEST(SlotThreadSafeExtremeTest, HighlyContendedInserts) {
        constexpr std::size_t ARRAY_SIZE = 1ULL << 11;       // 2048
        constexpr std::size_t TOTAL_ATTEMPTS = 1ULL << 12;   // 4096
        constexpr std::size_t NUM_THREADS = 1ULL << 10;      // 1024
        // NOLINTNEXTLINE(altera-id-dependent-backward-branch)
        constexpr std::size_t ATTEMPTS_PER_THREAD = TOTAL_ATTEMPTS / NUM_THREADS;
        constexpr int MAGIC_ID = 42;

        // NOLINTNEXTLINE(misc-include-cleaner)
        SlotThreadSafe<Dummy> pool(ARRAY_SIZE);
        
        std::atomic<std::size_t> success_count{0};
        std::atomic<std::size_t> fail_count{0};
        std::vector<std::thread> threads;
        threads.reserve(NUM_THREADS);

        const Dummy item{MAGIC_ID};
        const Dummy* ptr = &item;

        auto start_time = std::chrono::high_resolution_clock::now();

        for (std::size_t i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([&]() -> void {
                // NOLINTNEXTLINE(altera-unroll-loops, altera-id-dependent-backward-branch)
                for (std::size_t j = 0; j < ATTEMPTS_PER_THREAD; ++j) {
                    try {
                        pool.add(ptr);
                        success_count.fetch_add(1U, std::memory_order_relaxed);
                    } catch (const std::runtime_error&) {
                        fail_count.fetch_add(1U, std::memory_order_relaxed);
                    }
                }
            });
        }

        // NOLINTNEXTLINE(altera-unroll-loops)
        for (auto& thread_ref : threads) {
            thread_ref.join();
        }

        const auto end_time = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double, std::milli> diff = end_time - start_time;

        std::cout << "--- EXTREME STRESS TEST RESULTS ---\n";
        std::cout << "Threads used: " << NUM_THREADS << "\n";
        std::cout << "Insert attempts: " << TOTAL_ATTEMPTS << "\n";
        std::cout << "Successful inserts: " << success_count.load() << "\n";
        std::cout << "Failed inserts (Array Full): " << fail_count.load() << "\n";
        std::cout << "Time elapsed: " << diff.count() << " ms\n";

        ASSERT_EQ(success_count.load(), ARRAY_SIZE) << "Not exactly 2048 items were added!";
        ASSERT_EQ(fail_count.load(), TOTAL_ATTEMPTS - ARRAY_SIZE) << "Fail count mismatch.";
    }
} // namespace Systic::System::Concurrency::Test
