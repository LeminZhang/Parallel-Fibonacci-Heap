#include "../src/FineGrainedFibHeap.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void test_parallel_insert_updates_size() {
    FineGrainedFibHeap heap;

    constexpr int kThreads = 4;
    constexpr int kValuesPerThread = 100;
    std::vector<std::thread> workers;
    workers.reserve(kThreads);

    for (int tid = 0; tid < kThreads; ++tid) {
        workers.emplace_back([&heap, tid]() {
            const int base = tid * kValuesPerThread;
            for (int i = 0; i < kValuesPerThread; ++i) {
                heap.insert(new FibNode(base + i, base + i));
            }
        });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }

    assert(heap.size() == static_cast<size_t>(kThreads * kValuesPerThread));
    assert(!heap.isEmpty());
}

void test_relaxed_delete_min_drains_heap() {
    FineGrainedFibHeap heap;

    constexpr int kValues = 64;
    for (int i = 0; i < kValues; ++i) {
        heap.insert(new FibNode(kValues - i, i));
    }

    int deleted = 0;
    while (true) {
        try {
            DeleteMinResult result = heap.deleteMin();
            assert(result.handle_id >= 0);
            deleted++;
        } catch (const std::runtime_error&) {
            break;
        }
    }

    assert(deleted == kValues);
    assert(heap.isEmpty());
    assert(heap.size() == 0);
}

void test_handle_based_decrease_key_reinsertion() {
    FineGrainedFibHeap heap;

    FibNode* a = heap.insert(new FibNode(100, 0));
    (void)a;
    heap.insert(new FibNode(200, 1));
    heap.insert(new FibNode(300, 2));

    heap.decreaseKey(1, 50);

    bool saw_handle_one = false;
    int deletes = 0;
    while (true) {
        try {
            DeleteMinResult result = heap.deleteMin();
            if (result.handle_id == 1) {
                assert(!saw_handle_one);
                assert(result.value == 50);
                saw_handle_one = true;
            }
            deletes++;
        } catch (const std::runtime_error&) {
            break;
        }
    }

    assert(saw_handle_one);
    assert(deletes == 3);
    assert(heap.isEmpty());
}

}  // namespace

int main() {
    test_parallel_insert_updates_size();
    test_relaxed_delete_min_drains_heap();
    test_handle_based_decrease_key_reinsertion();

    std::cout << "FineGrainedFibHeap tests passed.\n";
    return 0;
}
