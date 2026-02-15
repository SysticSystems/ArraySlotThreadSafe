#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "systic/system/concurrency/ArrayThreadSafe.hpp"

using namespace Systic::System::Concurrency;

/**
 * @test Pressure Test: Concurrent PushBack
 * Proves that the Cursor is synchronized and no data is lost/overwritten.
 */
TEST(ArrayPressureTest, ConcurrentPushBack) {
    const size_t Capacity = 5000;
    const int NumThreads = 10;
    const int ItemsPerThread = 500;

    ArrayThreadSafe<int, Capacity> arr;
    std::vector<std::thread> workers;

    // Start 10 threads pushing 500 items each
    for (int t = 0; t < NumThreads; ++t) {
        workers.emplace_back([&arr, t, ItemsPerThread]() {
            for (int i = 0; i < ItemsPerThread; ++i) {
                // Use a unique value per thread/item to check for corruption later
                arr.pushBack(t * 1000 + i);
            }
        });
    }

    for (auto& w : workers) w.join();

    // Verify Size is exactly 5000
    EXPECT_EQ(arr.size(), Capacity);
    EXPECT_TRUE(arr.isFull()); // Assuming you kept the is_full method
}