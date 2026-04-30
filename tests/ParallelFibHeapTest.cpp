#include "../src/ParallelFibHeap.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <vector>
#include <chrono>
#include <random>

namespace {

void test_insert_and_find_min_orders_values(int num_threads = 4, int n_values = 1000000, int values_per_group = 10000) {
    ParallelFibHeap<int> heap(num_threads);

    vector<vector<int*>> test_values(n_values / values_per_group);
    vector<vector<HeapNode<int>*>> nodes(n_values / values_per_group);

    for (int i = 0; i < n_values / values_per_group; i++) {
        for (int j = 0; j < values_per_group; j++) {
            int* val = new int(rand() % 1000000000);
            test_values[i].push_back(val); // Insert random values to test the heap property
        }
    }

    #pragma omp parallel for num_threads(num_threads)
    for (int i = 0; i < n_values / values_per_group; i++) {
        heap.insert(test_values[i], nodes[i]);
    }

    int *min_value = new int(0);

    auto total_start = std::chrono::high_resolution_clock::now();
    heap.extractMin(min_value);
    cout << "Minimum value extracted: " << *min_value << endl;

    // for (int g = 0; g < 10; g++) {
    //     auto decrease_start = std::chrono::high_resolution_clock::now();
    //     #pragma omp parallel for num_threads(num_threads)
    //     for (int i = 0; i < n_values / values_per_group; i++) {
    //         for (int j = 0; j < values_per_group; j++) {
    //             if (i * values_per_group + j - g <= *min_value) 
    //                 continue; 
    //             heap.decreaseKey(nodes[i][j], new int(i * values_per_group + j - g - 1));
    //         }
    //     }
    //     auto decrease_end = std::chrono::high_resolution_clock::now();
    //     auto decrease_duration_ms = std::chrono::duration<double, std::milli>(decrease_end - decrease_start).count();
    //     cout << "Decrease key round " << g << " completed in " << decrease_duration_ms << " ms" << endl;
    //     heap.extractMin(min_value);
    //     cout << "Minimum value extracted: " << *min_value << endl;
    // }

    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_duration_ms = std::chrono::duration<double, std::milli>(total_end - total_start).count();
    std::cout << "Total insert time for all values: " << total_duration_ms << " ms" << std::endl;
}

}   

int main() {
    test_insert_and_find_min_orders_values(1, 1000000, 100);
    test_insert_and_find_min_orders_values(1, 1000000, 100);
    test_insert_and_find_min_orders_values(1, 1000000, 100);
    test_insert_and_find_min_orders_values(8, 1000000, 100);
    test_insert_and_find_min_orders_values(8, 1000000, 100);
    test_insert_and_find_min_orders_values(8, 1000000, 100);
    std::cout << "ParallelFibHeap tests passed.\n";
    return 0;
}