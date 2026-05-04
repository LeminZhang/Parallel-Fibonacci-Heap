#include "../src/CoarseGrainedFibHeap.h"

#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>

namespace {

void join_all(std::vector<std::thread>& threads) {
    for (std::thread& t : threads) {
        t.join();
    }
}

/**
 * Insert some nodes in parallel and delete all, and verify that every inserted
 * value is removed once while each thread observes a locally increasing
 * deleteMin sequence.
 */
void test_parallel_insert_then_drain() {
    CoarseGrainedFibHeap heap;

    // 400 ops in 4 threads
    constexpr int kThreads = 4;
    constexpr int kValuesPerThread = 100;
    constexpr int kTotalValues = kThreads * kValuesPerThread;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int tid = 0; tid < kThreads; ++tid) {
        threads.emplace_back([&heap, tid]() {
            const int base = tid * kValuesPerThread;
            for (int i = 0; i < kValuesPerThread; ++i) {
                heap.insert(new FibNode(base + i, base + i));
            }
        });
    }
    join_all(threads);

    assert(heap.size() == static_cast<size_t>(kTotalValues));

    std::vector<std::vector<int>> buckets(kThreads);
    threads.clear();

    for (int tid = 0; tid < kThreads; ++tid) {
        threads.emplace_back([&heap, &buckets, tid]() {
            while (true) {
                try {
                    buckets[tid].push_back(heap.deleteMin().value);
                } catch (const std::runtime_error&) {
                    break;
                }
            }
        });
    }
    join_all(threads);

    std::vector<bool> seen(kTotalValues, false);
    int removed = 0;

    for (const std::vector<int>& bucket : buckets) {
        for (size_t i = 0; i < bucket.size(); ++i) {
            if (i > 0) {
                assert(bucket[i - 1] < bucket[i]);
            }
            const int value = bucket[i];
            assert(value >= 0);
            assert(value < kTotalValues);
            assert(!seen[value]);
            seen[value] = true;
            removed++;
        }
    }

    assert(removed == kTotalValues);
    for (bool was_seen : seen) {
        assert(was_seen);
    }
    assert(heap.isEmpty());
}

/**
* Fill the heap with random nodes, and then do random decreaseKey in
* parallel, and then check that the new minimum is visible and the final
* deleteMin order remains nondecreasing.
*/
void test_parallel_decrease_keys() {
    CoarseGrainedFibHeap heap;

    constexpr int kThreads = 4;
    constexpr int kHandlesPerThread = 100;
    constexpr int kTotalHandles = kThreads * kHandlesPerThread;

    std::vector<FibNode*> handles;
    handles.reserve(kTotalHandles);

    for (int i = 0; i < kTotalHandles; ++i) {
        handles.push_back(heap.insert(new FibNode(1000 + i, i)));
    }

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int tid = 0; tid < kThreads; ++tid) {
        threads.emplace_back([&heap, &handles, tid]() {
            const int begin = tid * kHandlesPerThread;
            const int end = begin + kHandlesPerThread;
            for (int i = begin; i < end; ++i) {
                heap.decreaseKey(handles[i], -(i + 1));
            }
        });
    }
    join_all(threads);

    assert(heap.size() == static_cast<size_t>(kTotalHandles));
    assert(heap.min() != nullptr);
    assert(heap.min()->value == -kTotalHandles);

    int previous = heap.deleteMin().value;
    for (int count = 1; count < kTotalHandles; ++count) {
        const int current = heap.deleteMin().value;
        assert(previous <= current);
        previous = current;
    }

    assert(heap.isEmpty());
}

}  // namespace

int main() {
    test_parallel_insert_then_drain();
    test_parallel_decrease_keys();

    std::cout << "CoarseGrainedFibHeap tests passed.\n";
    return 0;
}
