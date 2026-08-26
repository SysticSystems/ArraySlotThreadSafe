#include "./SlotThreadSafeTestOperator.hpp"
#include "../StressTestNameGenerator.hpp"
#include "../StressTestSuite.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

import Systic.System.Concurrency.SlotThreadSafe;

namespace Systic::System::Concurrency::Test {

/**
 * @class SlotThreadSafeStressTest
 * @brief Typed GoogleTest fixture for high-concurrency stress testing of `SlotThreadSafe`.
 *
 * Executes multi-threaded workload patterns using `std::barrier` synchronization points to maximize
 * thread contention and systematically verify bit-scanning slot allocation, lookup, capacity,
 * overfill protection, and deallocation invariants.
 *
 * @tparam T Type parameter struct from `StressTestSuite` containing static metadata:
 *           `arraySize`, `slotSize`, `threadCount`, `stress`, and initial pre-allocated `slots`.
 */
template <typename T>
class SlotThreadSafeStressTest : public ::testing::Test {
private:
    using SlotType = TestSlot<T::slotSize>;

    /**
     * @brief High-level operation test harness for managing individual container assertions.
     */
    std::unique_ptr<SlotThreadSafeTestOperator<T::arraySize, T::slotSize>> operationTester;

    /**
     * @brief Local storage snapshot of unique slots available for testing allocations.
     */
    std::array<std::unique_ptr<SlotType>, T::arraySize> slots;

    /**
     * @brief Synchronizes worker threads to ignite parallel workloads at the exact same instance.
     *
     * Uses C++20 `std::barrier` to synchronize thread startup and completion phases, maximizing
     * hardware cache-line contention and bitmask race conditions.
     *
     * @tparam Func Invokable operation closure type `void(int threadId)`.
     * @param func Core lambda workload to execute in parallel.
     * @param numThreads Total count of concurrent worker threads.
     */
    template <typename Func>
    static void runMultithreadedStressOperation(const Func& func, const int numThreads) {
        std::barrier syncPoint(numThreads + 1);
        std::barrier finishPoint(numThreads + 1);

        std::vector<std::jthread> workers;
        workers.reserve(static_cast<std::size_t>(numThreads));

        for (int i = 0; i < numThreads; ++i) {
            workers.emplace_back([&func, &syncPoint, &finishPoint, i] {
                syncPoint.arrive_and_wait();
                func(i);
                finishPoint.arrive_and_wait();
            });
        }

        // Simultaneously unblock all worker threads from the calling thread
        syncPoint.arrive_and_wait();

        // Await completion of all worker thread iterations
        finishPoint.arrive_and_wait();
    }

public:
    SlotThreadSafeStressTest()
        : operationTester(std::make_unique<SlotThreadSafeTestOperator<T::arraySize, T::slotSize>>()) {
        this->slots = std::move(T::slots);
    }

    ~SlotThreadSafeStressTest() override = default;

    SlotThreadSafeStressTest(const SlotThreadSafeStressTest&) = delete;
    SlotThreadSafeStressTest& operator=(const SlotThreadSafeStressTest&) = delete;
    SlotThreadSafeStressTest(SlotThreadSafeStressTest&&) noexcept = delete;
    SlotThreadSafeStressTest& operator=(SlotThreadSafeStressTest&&) noexcept = delete;

    /**
     * @brief Asserts parallel insertion across threads and verifies target slot presence.
     */
    void assertAdd() {
        const int numberOfAddsToPerform = static_cast<int>(T::arraySize) >> static_cast<int>(T::stress);

        // 1. Snapshot raw non-owning pointers before transferring unique_ptr ownership
        std::array<const SlotType*, T::arraySize> expectedSlots{};
        for (std::size_t i = 0; i < T::arraySize; ++i) {
            expectedSlots[i] = this->slots[i].get();
        }

        // 2. Execute synchronized parallel allocations across worker threads
        SlotThreadSafeStressTest::runMultithreadedStressOperation(
            [this, numberOfAddsToPerform](const int tid) {
                const int startIdx = tid * numberOfAddsToPerform;
                const int endIdx = startIdx + numberOfAddsToPerform;

                for (int i = startIdx; i < endIdx; ++i) {
                    this->operationTester->assertAdd(std::move(this->slots[i]));
                }
            },
            T::threadCount
        );

        // 3. Confirm all allocated slots exist within the slot container
        const std::size_t totalExpectedAdds = static_cast<std::size_t>(numberOfAddsToPerform * T::threadCount);
        this->operationTester->assertSlotsExists(expectedSlots, totalExpectedAdds);
    }

    /**
     * @brief Asserts underlying array size matches configured compile-time capacity.
     */
    void assertSize() const {
        this->operationTester->assertSize();
    }

    /**
     * @brief Asserts high-concurrency slot removals across partitioned thread indices.
     */
    void assertDelete() {
        const int numberOfDeletesToPerform = static_cast<int>(T::arraySize) >> static_cast<int>(T::stress);

        SlotThreadSafeStressTest::runMultithreadedStressOperation(
            [this, numberOfDeletesToPerform](const int tid) {
                const int startIdx = tid * numberOfDeletesToPerform;
                const int endIdx = startIdx + numberOfDeletesToPerform;

                // Deallocate slots assigned strictly to this thread range
                for (int i = startIdx; i < endIdx; ++i) {
                    this->operationTester->assertDeleteAt(static_cast<std::size_t>(i));
                }
            },
            T::threadCount
        );
    }

    /**
     * @brief Asserts system behavior when total insertion requests exceed total capacity under contention.
     *
     * Floods the container with 2x its total capacity. Verifies that exactly `arraySize` insertions
     * succeed and the remaining attempts return `SlotOperationStatus::ErrorArrayFull` cleanly.
     */
    void assertOverfillAdd() {
        constexpr std::size_t totalAttempts = T::arraySize * 2ULL;
        const int numThreads = T::threadCount;
        const std::size_t attemptsPerThread = totalAttempts / static_cast<std::size_t>(numThreads);

        std::atomic<std::size_t> successCount{0};
        std::atomic<std::size_t> fullCount{0};

        // Pre-allocate unique slots to prevent pointer-aliasing hazards
        std::vector<std::unique_ptr<SlotType>> extraSlots;
        extraSlots.reserve(totalAttempts);
        for (std::size_t i = 0; i < totalAttempts; ++i) {
            extraSlots.emplace_back(std::make_unique<SlotType>());
        }

        // Execute parallel overfill under barrier synchronization
        SlotThreadSafeStressTest::runMultithreadedStressOperation(
            [this, &extraSlots, &successCount, &fullCount, attemptsPerThread](const int tid) {
                const std::size_t startIdx = static_cast<std::size_t>(tid) * attemptsPerThread;
                const std::size_t endIdx = startIdx + attemptsPerThread;

                for (std::size_t i = startIdx; i < endIdx; ++i) {
                    const SlotOperationStatus status = this->operationTester->tryAdd(std::move(extraSlots[i]));
                    if (status == SlotOperationStatus::Success) {
                        successCount.fetch_add(1U, std::memory_order_relaxed);
                    } else if (status == SlotOperationStatus::ErrorArrayFull) {
                        fullCount.fetch_add(1U, std::memory_order_relaxed);
                    }
                }
            },
            numThreads
        );

        // Verify system invariants under capacity exhaustion
        EXPECT_EQ(successCount.load(), T::arraySize)
            << "Container accepted a different number of items than its configured maximum capacity!";

        EXPECT_EQ(fullCount.load(), totalAttempts - T::arraySize)
            << "Total rejected attempts did not match overflow delta!";
    }
};

// Register GoogleTest typed parameter suite
TYPED_TEST_SUITE(SlotThreadSafeStressTest, StressTestSuite, StressTestNameGenerator);

/**
 * @brief Primary stress test execution entry point.
 */
TYPED_TEST(SlotThreadSafeStressTest, SlotThreadSafeStress) {
    this->assertSize();
    this->assertAdd();
    this->assertDelete();
    this->assertOverfillAdd();
}

} // namespace Systic::System::Concurrency::Test