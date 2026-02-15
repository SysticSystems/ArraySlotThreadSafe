#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "systic/system/concurrency/ArrayThreadSafe.hpp"

using namespace Systic::System::Concurrency;

TEST(ArrayPressureTest, AtomicShiftIntegrity) {
    ArrayThreadSafe<int, 100> arr;
    for(int i=0; i<50; ++i) arr.pushBack(1); // Fill half with 1s

    std::atomic<bool> stop{false};

    // Thread 1: Constantly inserting and removing at the front
    std::thread worker([&]() {
        while(!stop) {
            arr.insertAt(0, 99);
            arr.removeAt(0);
        }
    });

    // Thread 2: Constantly checking if elements are valid
    std::thread checker([&]() {
        while(!stop) {
            arr.executeCallbackInReadMode([](const auto& state) {
                for(size_t i=0; i < state.Cursor; ++i) {
                    // Elements should only ever be 1 or 99.
                    // If we see 0 or garbage, the shift was not atomic!
                    ASSERT_TRUE(state.buffer[i] == 1 || state.buffer[i] == 99);
                }
            });
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop = true;
    worker.join();
    checker.join();
}