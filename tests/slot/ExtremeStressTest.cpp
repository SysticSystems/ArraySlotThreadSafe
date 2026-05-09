#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <gtest/gtest.h>
import Systic.System.Concurrency;

namespace Systic::System::Concurrency::Test {
    struct Dummy { int id; };

    TEST(SlotThreadSafeExtremeTest, HighlyContendedInserts) {
        constexpr std::size_t ARRAY_SIZE = 1 << 11;       // 2048
        constexpr std::size_t TOTAL_ATTEMPTS = 1 << 12;   // 4096
        constexpr std::size_t NUM_THREADS = 1 << 10;      // 1024
        constexpr std::size_t ATTEMPTS_PER_THREAD = TOTAL_ATTEMPTS / NUM_THREADS;

        SlotThreadSafe<Dummy> pool(ARRAY_SIZE);
        
        std::atomic<std::size_t> success_count{0};
        std::atomic<std::size_t> fail_count{0};
        std::vector<std::thread> threads;
        threads.reserve(NUM_THREADS);

        Dummy item{42};
        const Dummy* ptr = &item;

        auto start_time = std::chrono::high_resolution_clock::now();

        for (std::size_t i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([&]() {
                for (std::size_t j = 0; j < ATTEMPTS_PER_THREAD; ++j) {
                    try {
                        pool.add(ptr);
                        success_count.fetch_add(1, std::memory_order_relaxed);
                    } catch (const std::runtime_error&) {
                        fail_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> diff = end_time - start_time;

        std::cout << "--- EXTREME STRESS TEST RESULTS ---\n";
        std::cout << "Threads used: " << NUM_THREADS << "\n";
        std::cout << "Insert attempts: " << TOTAL_ATTEMPTS << "\n";
        std::cout << "Successful inserts: " << success_count.load() << "\n";
        std::cout << "Failed inserts (Array Full): " << fail_count.load() << "\n";
        std::cout << "Time elapsed: " << diff.count() << " ms\n";

        ASSERT_EQ(success_count.load(), ARRAY_SIZE) << "Not exactly 2048 items were added!";
        ASSERT_EQ(fail_count.load(), TOTAL_ATTEMPTS - ARRAY_SIZE) << "Fail count mismatch.";
    }
}



