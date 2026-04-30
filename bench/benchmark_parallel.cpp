#include "../src/ParallelFibHeap.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <random>

namespace {

void benchmark_insert() {
    vector<int> n_threads = {2, 4, 8};

    vector<size_t> batch_sizes;
    for (int i = 0; i <= 20; i++) {
        batch_sizes.push_back(1 << i); // 8, 16, 32, ..., 8388608
    }

    // Generate number of operations to test batches
    vector<size_t> n_ops;
    for (int i = 0; i <= 23; i++) {
        n_ops.push_back(1 << i); // 8, 16, 32, ..., 67108864
    }

    for (size_t batch_size : batch_sizes) {
        for (size_t n_op: n_ops) {
            if (n_op < batch_size) {
                continue; // Skip invalid configurations where total operations is less than batch size
            }

            // Get baseline with 1 thread
            vector<vector<int*>> values(n_op / batch_size, vector<int*>(batch_size));
            for (size_t i = 0; i < n_op; i++) {
                values[i / batch_size][i % batch_size] = new int(rand() % 1000000000);
            }
            double baseline_time_ms;
            ParallelFibHeap<int> baseline_heap(1);
            auto start = std::chrono::steady_clock::now();
            for (size_t i = 0; i < n_op / batch_size; i++) {
                vector<HeapNode<int>*> nodes; // This vector is not used.
                baseline_heap.insert(values[i], nodes);
            }
            auto end = std::chrono::steady_clock::now();
            baseline_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
            // baseline_time_ms /= 5.0;
            cout <<
                " number of operations=" << n_op
                << " batch_size=" << batch_size
                << " threads=1"
                << " time_ms=" << baseline_time_ms
                << endl;

            // Get speedup with multiple threads
            for (int threads : n_threads) {
                ParallelFibHeap<int> heap(threads);

                auto start = std::chrono::steady_clock::now();

                #pragma omp parallel for num_threads(threads)
                for (size_t i = 0; i < n_op / batch_size; i++) {
                    vector<HeapNode<int>*> nodes; // This vector is not used.
                    heap.insert(values[i], nodes);
                }

                auto end = std::chrono::steady_clock::now();
                double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
                double speedup = baseline_time_ms / time_ms;
                cout <<
                    " number of operations=" << n_op
                    << " batch_size=" << batch_size
                    << " threads=" << threads
                    << " time_ms=" << time_ms
                    << " speedup=" << speedup
                    << endl;
            }

            // Delete the allocated integers
            for (size_t i = 0; i < n_op; i++) {
                delete values[i / batch_size][i % batch_size];
            }
        }
    }
}

double test_insert_and_find_min_orders_values(vector<int> values, int num_threads = 4, int batch_size = 1000) {
    ParallelFibHeap<int> heap(num_threads);

    vector<vector<int*>> test_values(values.size() / batch_size);
    vector<vector<HeapNode<int>*>> nodes(values.size() / batch_size);

    for (int i = 0; i < (int)values.size() / batch_size; i++) {
        for (int j = 0; j < batch_size; j++) {
            int* val = new int(values[i * batch_size + j]);
            test_values[i].push_back(val); // Insert values to test the heap property
        }
    }

    #pragma omp parallel for num_threads(num_threads)
    for (int i = 0; i < (int)values.size() / batch_size; i++) {
        heap.insert(test_values[i], nodes[i]);
    }

    int *min_value = new int(0);

    auto total_start = std::chrono::high_resolution_clock::now();
    heap.extractMin(min_value);
    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_duration_ms = std::chrono::duration<double, std::milli>(total_end - total_start).count();

    for (int i = 0; i < (int)values.size() / batch_size; i++) {
        for (int j = 0; j < batch_size; j++) {
            delete test_values[i][j];
        }
    }

    return total_duration_ms;
}

void benchmark_extract_min() {
    vector<int> n_threads = {2, 4, 8};
    vector<size_t> n_ops = {100, 10000, 100000, 1000000};
    const int iterations = 10;
    const int warmup_iterations = 3;
    const int batch_size = 100;

    for (size_t n_op : n_ops) {
        // Baseline with 1 thread
        vector<int> values(n_op);
        for (size_t i = 0; i < n_op; i++) {
            values[i] = rand() % 1000000000;
        }

        double baseline_time_ms = 0.0;
        for (int iter = 0; iter < warmup_iterations; iter++) {
            test_insert_and_find_min_orders_values(values, 1, batch_size);
        }
        for (int iter = 0; iter < iterations; iter++) {
            baseline_time_ms += test_insert_and_find_min_orders_values(values, 1, batch_size);
        }
        baseline_time_ms /= iterations; // Average over iterations
        cout <<
            " number of operations=" << n_op
            << " batch_size=" << batch_size
            << " threads=1"
            << " time_ms=" << baseline_time_ms
            << endl;

        // Test with multiple threads
        for (int threads : n_threads) {
            double time_ms = 0.0;
            for (int iter = 0; iter < warmup_iterations; iter++) {
                test_insert_and_find_min_orders_values(values, threads, batch_size);
            }
            for (int iter = 0; iter < iterations; iter++) {
                time_ms += test_insert_and_find_min_orders_values(values, threads, batch_size);
            }
            time_ms /= iterations; // Average over iterations
            double speedup = baseline_time_ms / time_ms;
            cout <<
                " number of operations=" << n_op
                << " batch_size=" << batch_size
                << " threads=" << threads
                << " time_ms=" << time_ms
                << " speedup=" << speedup
                << endl; 
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    // benchmark_insert();
    benchmark_extract_min();
    
    
    // test_insert_and_find_min_orders_values(1, 100000, 100);
    // test_insert_and_find_min_orders_values(8, 100000, 100);
    std::cout << "ParallelFibHeap tests passed.\n";
    return 0;
}
