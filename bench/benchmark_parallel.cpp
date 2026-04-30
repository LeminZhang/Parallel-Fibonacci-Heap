#include "../src/ParallelFibHeap.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <random>

namespace {

void benchmark_insert() {
    vector<int> n_threads = {2, 4, 6, 8};

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
                << " MMops=" << n_op / (baseline_time_ms / 1000.0) / 1000000.0
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
                    << " MMops=" << n_op / (time_ms / 1000.0) / 1000000.0
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
    vector<int> n_threads = {2, 4, 6, 8};
    vector<size_t> n_ops = {100, 1000, 10000, 100000};
    int iterations;
    const int warmup_iterations = 3;
    const int batch_size = 100;

    for (size_t n_op : n_ops) {
        iterations = 400000 / n_op;

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
            " number of nodes=" << n_op
            << " batch_size=" << batch_size
            << " threads=1"
            << " time_ms=" << baseline_time_ms
            << " Mops=" << n_op / (baseline_time_ms / 1000.0) / 1000000.0
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
                " number of nodes=" << n_op
                << " batch_size=" << batch_size
                << " threads=" << threads
                << " time_ms=" << time_ms
                << " Mops=" << n_op / (time_ms / 1000.0) / 1000000.0
                << " speedup=" << speedup
                << endl; 
        }
    }
}

double test_decrease_key(vector<int> values, int num_threads = 4, int batch_size = 1000) {
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

    auto decrease_start = std::chrono::high_resolution_clock::now();
    #pragma omp parallel for num_threads(num_threads)
    for (int i = 0; i < (int)values.size() / batch_size; i++) {
        for (int j = 0; j < batch_size; j+=2) {
            heap.obtainMutexesForDecreaseKey(nodes[i][j]); // Obtain necessary mutexes for decrease key operation
        }
    }
    #pragma omp parallel for num_threads(num_threads)
    for (int i = 0; i < (int)values.size() / batch_size; i++) {
        for (int j = 0; j < batch_size; j+=2) {
            *(nodes[i][j]->value) = -1;
            heap.decreaseKey(nodes[i][j]);
        }
    }
    auto decrease_end = std::chrono::high_resolution_clock::now();
    auto decrease_duration_ms = std::chrono::duration<double, std::milli>(decrease_end - decrease_start).count();

    for (int i = 0; i < (int)values.size() / batch_size; i++) {
        for (int j = 0; j < batch_size; j++) {
            delete test_values[i][j];
        }
    }

    return decrease_duration_ms;
}

void benchmark_decrease_key() {
    vector<int> n_threads = {2, 4, 6, 8};
    vector<size_t> n_ops = {100, 1000, 10000, 100000, 1000000};
    int iterations;
    const int warmup_iterations = 3;
    const int batch_size = 100;

    for (size_t n_op : n_ops) {
        iterations = 1000000 / n_op;

        // Baseline with 1 thread
        vector<int> values(n_op);
        for (size_t i = 0; i < n_op; i++) {
            values[i] = rand() % 1000000000;
        }

        double baseline_time_ms = 0.0;
        for (int iter = 0; iter < warmup_iterations; iter++) {
            test_decrease_key(values, 1, batch_size);
        }
        for (int iter = 0; iter < iterations; iter++) {
            baseline_time_ms += test_decrease_key(values, 1, batch_size);
        }
        baseline_time_ms /= iterations; // Average over iterations
        cout <<
            " number of nodes=" << n_op
            << " batch_size=" << batch_size
            << " threads=1"
            << " time_ms=" << baseline_time_ms
            << " Mops=" << n_op / (baseline_time_ms / 1000.0) / 1000000.0
            << endl;

        // Test with multiple threads
        for (int threads : n_threads) {
            double time_ms = 0.0;
            for (int iter = 0; iter < warmup_iterations; iter++) {
                test_decrease_key(values, threads, batch_size);
            }
            for (int iter = 0; iter < iterations; iter++) {
                time_ms += test_decrease_key(values, threads, batch_size);
            }
            time_ms /= iterations; // Average over iterations
            double speedup = baseline_time_ms / time_ms;
            cout <<
                " number of nodes=" << n_op
                << " batch_size=" << batch_size
                << " threads=" << threads
                << " time_ms=" << time_ms
                << " Mops=" << n_op / (time_ms / 1000.0) / 1000000.0
                << " speedup=" << speedup
                << endl; 
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    // Create benchmark_result directory
    system("mkdir -p benchmark_result");
    
    // Save original cout buffer
    std::streambuf* original_cout = std::cout.rdbuf();
    
    // ===== Run benchmark_insert =====
    {
        std::ofstream insert_file("benchmark_result/insert.txt");
        std::cout.rdbuf(insert_file.rdbuf());
        std::cout << "===== INSERT BENCHMARK =====" << std::endl;
        benchmark_insert();
        insert_file.close();
    }
    
    // ===== Run benchmark_extract_min =====
    {
        std::ofstream extract_file("benchmark_result/extract_min.txt");
        std::cout.rdbuf(extract_file.rdbuf());
        std::cout << "===== EXTRACT_MIN BENCHMARK =====" << std::endl;
        benchmark_extract_min();
        extract_file.close();
    }
    
    // ===== Run benchmark_decrease_key =====
    {
        std::ofstream decrease_file("benchmark_result/decress_key.txt");
        std::cout.rdbuf(decrease_file.rdbuf());
        std::cout << "===== DECREASE_KEY BENCHMARK =====" << std::endl;
        benchmark_decrease_key();
        decrease_file.close();
    }
    
    // Restore original cout
    std::cout.rdbuf(original_cout);
    
    std::cout << "✓ All benchmarks completed!" << std::endl;
    std::cout << "  - benchmark_result/insert.txt" << std::endl;
    std::cout << "  - benchmark_result/extract_min.txt" << std::endl;
    std::cout << "  - benchmark_result/decress_key.txt" << std::endl;
    
    return 0;
}
