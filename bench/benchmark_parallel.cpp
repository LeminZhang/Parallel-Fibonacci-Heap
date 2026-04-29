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
            // if (n_op % batch_size != 0) {
            //     cout << "Skipping n_op=" << n_op << " because it is not a multiple of batch_size=" << batch_size << endl;
            //     continue; // Skip configurations where total operations is not a multiple of batch size
            // }
            // Get baseline with 1 thread
            vector<vector<int*>> values(n_op / batch_size, vector<int*>(batch_size));
            for (size_t i = 0; i < n_op; i++) {
                values[i / batch_size][i % batch_size] = new int(rand() % 1000000000);
            }
            double baseline_time_ms;
            ParallelFibHeap<int> baseline_heap(1);
            auto start = std::chrono::steady_clock::now();
            for (size_t i = 0; i < n_op / batch_size; i++) {
                baseline_heap.insert(values[i]);
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
                    heap.insert(values[i]);
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

}  // namespace

int main(int argc, char** argv) {
    benchmark_insert();
    // try {
    //     const BenchmarkConfig config = parse_args(argc, argv);
    //     const BenchmarkResult result = run_benchmark(config);
    //     print_result(config, result);
    //     return 0;
    // } catch (const std::exception& ex) {
    //     print_usage(argv[0]);
    //     std::cerr << "Error: " << ex.what() << '\n';
    //     return 1;
    // }
}
