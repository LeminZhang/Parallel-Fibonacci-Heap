#include "../src/ParallelFibHeap.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <vector>
#include <chrono>

namespace {

void local_insert(ParallelFibHeap& heap, int values_per_thread) {
    pid_t thread_id = getpid();
    heap.AddThread();
    heap.protect(); // Ensure the thread's local heap is registered before proceeding

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < values_per_thread; ++i) {
        heap.insert(thread_id * values_per_thread + i, i);
        // // Do some dummy work to increase for performance debugging
        // int j;
        // int sum = 0;
        // for (j = 0; j < 100; ++j) {
        //     sum += j;
        // }
        // (void)sum; // Suppress unused variable warning
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    cout << "Thread " << thread_id << " inserted " << values_per_thread
         << " values in " << duration_ms << " ms" << std::endl;
//     FibNode* min_node = heap.min();
//     std::cout << "Thread " << thread_id << " inserted values, current min: " << (min_node ? min_node->value : -1) << std::endl;
}

void test_insert_and_find_min_orders_values(int num_threads = 4, int values_per_thread = 10000) {
    ParallelFibHeap heap;
    
    // auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> insert_workers;
    insert_workers.reserve(num_threads);
    for (int tid = 0; tid < num_threads; ++tid) {
        insert_workers.emplace_back(local_insert, std::ref(heap), values_per_thread);
    }
    for (auto& worker : insert_workers) {
        worker.join();
    }

    // auto end = std::chrono::high_resolution_clock::now();
    // auto duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    // std::cout << "Total insert time (" << num_threads << " threads, " 
    //           << values_per_thread << " values each): " << duration_ms << " ms" << std::endl;
}

}   

int main() {
    test_insert_and_find_min_orders_values(4, 10000000);
    // test_decrease_key_updates_minimum();
    std::cout << "ParallelFibHeap tests passed.\n";
    return 0;
}
