#include "../src/CoarseGrainedFibHeap.h"

#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>

namespace {

void test_parallel_insert_delete() {
    CoarseGrainedFibHeap heap;

    constexpr int kThreads = 4;
    constexpr int kValuesPerThread = 100;
    std::vector<std::thread> insert_workers;
    insert_workers.reserve(kThreads);

    for (int tid = 0; tid < kThreads; ++tid) {
        insert_workers.emplace_back([&heap, tid]() {
            const int base = tid * kValuesPerThread;
            for (int i = 0; i < kValuesPerThread; ++i) {
                heap.insert(base + i);
            }
        });
    }

    for (std::thread& worker : insert_workers) {
        worker.join();
    }

    constexpr int kTotalValues = kThreads * kValuesPerThread;
    assert(heap.size() == static_cast<size_t>(kTotalValues));

    std::vector<std::vector<int>> results(kThreads);
    std::vector<std::thread> delete_workers;
    delete_workers.reserve(kThreads);

    for (int tid = 0; tid < kThreads; ++tid) {
        delete_workers.emplace_back([&heap, &results, tid]() {
            while (true) {
                try {
                    int x = heap.deleteMin();
                    results[tid].push_back(x);
                } catch (const std::runtime_error&) {
                    break;
                }
            }
        });
    }

    for (std::thread& worker : delete_workers) {
        worker.join();
    }

    std::vector<bool> seen(kTotalValues, false);
    int total_deleted = 0;

    for (int tid = 0; tid < kThreads; ++tid) {
        for (size_t i = 0; i < results[tid].size(); ++i) {
            if (i > 0) {
                assert(results[tid][i - 1] < results[tid][i]);
            }
            const int value = results[tid][i];
            assert(value >= 0);
            assert(value < kTotalValues);
            assert(!seen[value]);
            seen[value] = true;
            total_deleted++;
        }
    }

    assert(total_deleted == kTotalValues);
    for (bool was_seen : seen) {
        assert(was_seen);
    }
    assert(heap.isEmpty());
}

void test_parallel_decrease_key_ops() {
    CoarseGrainedFibHeap heap;

    constexpr int kThreads = 4;
    constexpr int kHandlesPerThread = 32;
    constexpr int kTotalHandles = kThreads * kHandlesPerThread;

    std::vector<FibNode*> handles;
    handles.reserve(kTotalHandles);

    for (int i = 0; i < kTotalHandles; ++i) {
        handles.push_back(heap.insert(1000 + i));
    }

    std::vector<std::thread> workers;
    workers.reserve(kThreads);

    for (int tid = 0; tid < kThreads; ++tid) {
        workers.emplace_back([&heap, &handles, tid]() {
            const int begin = tid * kHandlesPerThread;
            const int end = begin + kHandlesPerThread;
            for (int i = begin; i < end; ++i) {
                heap.decreaseKey(handles[i], -(i + 1));
            }
        });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }

    assert(heap.size() == static_cast<size_t>(kTotalHandles));
    assert(heap.min() != nullptr);
    assert(heap.min()->value == -kTotalHandles);

    int previous = heap.deleteMin();
    for (int count = 1; count < kTotalHandles; ++count) {
        const int current = heap.deleteMin();
        assert(previous <= current);
        previous = current;
    }

    assert(heap.isEmpty());
}

}  // namespace

int main() {
    test_parallel_insert_delete();
    test_parallel_decrease_key_ops();

    std::cout << "CoarseGrainedFibHeap tests passed.\n";
    return 0;
}
