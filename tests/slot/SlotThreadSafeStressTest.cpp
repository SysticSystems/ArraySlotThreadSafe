#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <thread>
#include <barrier>
#include "SlotThreadSafeOperationsTest.hpp"
#include "SlotThreadSafeStreeSuite.hpp"
#include "SlotThreadSafeTestNameGenerator.hpp"

import Systic.System.Concurrency;

namespace Systic::System::Concurrency::Test {

    template <typename T>
    class SlotThreadSafeStressTest : public ::testing::Test {
        private:
            SlotThreadSafeOperationsTest<T::arraySize, T::slotSize> operationTester;
            TestSlot<T::slotSize>* slots;

            template <typename Func>
            static void runMultithreadedStressOperation(const Func& func, int numThreads) {
                std::barrier syncPoint(numThreads + 1);
                std::barrier finishPoint(numThreads + 1);
                std::vector<std::jthread> workers;
                workers.reserve(numThreads);

                for (int i = 0; i < numThreads; ++i) {
                    workers.emplace_back([&func, &syncPoint, &finishPoint, i] {
                        syncPoint.arrive_and_wait();
                        func(i);
                        finishPoint.arrive_and_wait();
                    });
                }
                syncPoint.arrive_and_wait();
                finishPoint.arrive_and_wait();
            }

        public:
            SlotThreadSafeStressTest() : operationTester() {
                this->slots = T::slots;
            }

            void assertArraySize() {
                this->operationTester->assertSize();
            }
            void assertAdd() {

                int numberOfAddsToPerform = T::arraySize >> static_cast<int>(T::stress);

                SlotThreadSafeStressTest::runMultithreadedStressOperation([this, numberOfAddsToPerform](const int tid) {
                    for (int i = tid * numberOfAddsToPerform; i < tid * numberOfAddsToPerform + numberOfAddsToPerform; ++i) {
                        this->operationTester.assertAdd(this->slots + i);
                    }
                }, T::threadCount);
              this->operationTester.assertSlotsExists(this->slots, T::arraySize);
            }

            void assertDelete() {
                int numberOfDeletesToPerform = T::arraySize >> static_cast<int>(T::stress);
                std::uint64_t deletedIndexes[T::arraySize];

                SlotThreadSafeStressTest::runMultithreadedStressOperation([this, &numberOfDeletesToPerform, &deletedIndexes](std::uint64_t tid) {
                    for (int i = 0; i < numberOfDeletesToPerform; ++i) {
                        uint64_t idx = (std::chrono::high_resolution_clock::now().time_since_epoch().count()) % T::arraySize;
                        this->operationTester.assertDeleteAt(idx);
                        deletedIndexes[i + tid] = idx;
                    }
                }, T::threadCount);
            }
        };


    // Syntax: TYPED_TEST_SUITE(CaseName, Types);
    TYPED_TEST_SUITE(SlotThreadSafeStressTest, SlotThreadSafeStreeSuite, SlotThreadSafeTestNameGenerator);

    // Inside here, 'TypeParam' refers to the current entry from SlotThreadSafeStreeSuite
    TYPED_TEST(SlotThreadSafeStressTest, SlotThreadSafeStress) {

        this->assertAdd();

        this->assertDelete();

    }
}