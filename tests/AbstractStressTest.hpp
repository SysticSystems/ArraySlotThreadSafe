#pragma once
#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <thread>
#include <barrier>
#include "StressTestOperatorInterface.hpp"

namespace Systic::System::Concurrency::Test {
    template <typename T>
    class AbstractStressTest : public ::testing::Test {
        private:
            /**
            * @brief The operator to use for the stress test.
            */
            StressTestOperatorInterface<T::slotSize>* operationTester;

        protected:
            /**
             * @brief The array of slots to use for the stress test.
            */
            TestSlot<T::slotSize>* slots;
            /**
             * Method to ignite parallel stress test at the very same start point
             * to maximimze the chance of collisions / contention.
             * @tparam Func
             * @param func
             * @param numThreads
             */
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
            /**
             * @brief Constructor for the abstract stress test.
             * @param op The operator to use for the stress test.
            */
            AbstractStressTest(StressTestOperatorInterface<T::slotSize>* op) : operationTester(op) {}

            /**
             * @brief Assert that the slots are added to the array.
            */
            void assertAdd() {
                int numberOfAddsToPerform = T::arraySize >> static_cast<int>(T::stress);

                AbstractStressTest::runMultithreadedStressOperation([this, numberOfAddsToPerform](const int tid) {
                    for (int i = tid * numberOfAddsToPerform; i < tid * numberOfAddsToPerform + numberOfAddsToPerform; ++i) {
                        this->operationTester->assertAdd(this->slots + i);
                    }
                }, T::threadCount);

              this->operationTester->assertSlotsExists(this->slots, T::arraySize);
            }
            
            /**
             * @brief Assert that the size of the array is correct.
            */
            void assertSize() {
                this->operationTester->assertSize();
            }

            /**
             * @brief Assert that the slots are deleted from the array.
            */
            void assertDelete() {
                int numberOfDeletesToPerform = T::arraySize >> static_cast<int>(T::stress);
                std::uint64_t deletedIndexes[T::arraySize];

                AbstractStressTest::runMultithreadedStressOperation([this, &numberOfDeletesToPerform, &deletedIndexes](std::uint64_t tid) {
                    for (int i = 0; i < numberOfDeletesToPerform; ++i) {
                        uint64_t idx = std::chrono::high_resolution_clock::now().time_since_epoch().count() % T::arraySize;
                        this->operationTester->assertDeleteAt(idx);
                        deletedIndexes[i + tid] = idx;
                    }
                }, T::threadCount);
            }

    };
}