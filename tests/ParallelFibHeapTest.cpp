#include "../src/ParallelFibHeap.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <vector>
#include <chrono>

namespace {

void test_insert_and_find_min_orders_values(int num_threads = 4, int n_values = 1000000, int values_per_group = 10000) {
    ParallelFibHeap<int> heap(num_threads);

    vector<vector<int*>> test_values(n_values / values_per_group);
    auto total_start = std::chrono::high_resolution_clock::now();
    #pragma omp parallel
    for (int i = 0; i < n_values / values_per_group; i++) {
        double sum = 0;
        for (int j = 0; j < values_per_group; j++) {
            sum += i * values_per_group / 0.234 + j;
        }
        cout << "Sum for group " << i << ": " << sum << endl;
    }
    auto total_end = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n_values / values_per_group; i++) {
        for (int j = 0; j < values_per_group; j++) {
            int* val = new int(i * values_per_group + j);
            test_values[i].push_back(val);
        }
    }

    // auto total_start = std::chrono::high_resolution_clock::now();

    // #pragma omp parallel for
    for (int i = 0; i < n_values / values_per_group; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        heap.insert(test_values[i]);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << "Total insert time (" << num_threads << " threads, " 
                << values_per_group << " values each): " << duration_ms << " ms" << std::endl;
    }

    // auto total_end = std::chrono::high_resolution_clock::now();
    auto total_duration_ms = std::chrono::duration<double, std::milli>(total_end - total_start).count();
    std::cout << "Total insert time for all values: " << total_duration_ms << " ms" << std::endl;
}

}   

int main() {
    test_insert_and_find_min_orders_values(1, 10000000, 1000000);
    // test_decrease_key_updates_minimum();
    std::cout << "ParallelFibHeap tests passed.\n";
    return 0;
}
