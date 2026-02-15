#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "systic/system/concurrency/ArrayThreadSafe.hpp"

using namespace Systic::System::Concurrency;

TEST(ArrayPressureTest, ReadWriteContention) {
    ArrayThreadSafe<int, 100> arr;
    std::atomic<bool> stop{false};

    // 1. WRITER THREAD: Constantly clears and fills
    std::thread writer([&]() {
        while (!stop) {
            arr.clear();
            for (int i = 0; i < 100; ++i) {
                arr.pushBack(i);
            }
        }
    });

    // 2. READER THREADS: Constantly accessing indices
    auto reader_logic = [&]() {
        while (!stop) {
            // Test At() safety
            auto val = arr.at(50);
            // We don't care if it's nullopt (during clear),
            // we just care that it doesn't crash or hang.

            // Test executeCallbackInReadMode iteration
            arr.executeCallbackInReadMode([](const auto& state) {
                int sum = 0;
                for(size_t i = 0; i < state.Cursor; ++i) {
                    sum += state.buffer[i];
                }
            });
        }
    };

    std::vector<std::thread> readers;
    for(int i=0; i<4; ++i) readers.emplace_back(reader_logic);

    // Run for 1 second of heavy pressure
    std::this_thread::sleep_for(std::chrono::seconds(1));

    stop = true;
    writer.join();
    for (auto& r : readers) r.join();

    SUCCEED(); // If we reached here without a crash or deadlock, we passed.
}