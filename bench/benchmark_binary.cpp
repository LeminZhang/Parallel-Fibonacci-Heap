#include "../src/BinaryHeap.h"

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

// Additional warmup
constexpr int kWarmupInserts = 5000;

enum class OpType {
    Insert,
    DeleteMin,
    DecreaseKey,
};

struct Operation {
    OpType type = OpType::Insert;
    int value = 0;
    int handle_id = -1;
    int new_value = 0;
};

struct BenchmarkConfig {
    float insertLoad = 1.0;
    float deleteLoad = 0.0;
    float decreaseLoad = 0.0;
    int threads = 1;
    int ops = 10000;
    unsigned seed = 42;
};

struct BenchmarkResult {
    double elapsed_ms = 0.0;
    int executed_ops = 0;
    int insert_ops = 0;
    int delete_ops = 0;
    int decrease_ops = 0;
    long long delete_value_sum = 0;
};

struct GeneratedWorkload {
    std::vector<Operation> ops;
    std::vector<Operation> warmup_ops;
    // # of decreaseKey
    int protected_handle_count = 0;
};

BenchmarkConfig parse_args(int argc, char** argv) {
    BenchmarkConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--threads" && i + 1 < argc) {
            config.threads = std::stoi(argv[++i]);
        } else if (arg == "--ops" && i + 1 < argc) {
            config.ops = std::stoi(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            config.seed = static_cast<unsigned>(std::stoul(argv[++i]));
        } else if (arg == "--workload" && i + 1 < argc) {
            if (i + 3 >= argc) {
                throw std::invalid_argument ("Error: --workload requires exactly 3 numbers");
            }
            try {
                float insertLoad = std::stof((argv[++i]));
                float minLoad = std::stof((argv[++i]));
                float decreaseLoad = std::stof((argv[++i]));
                const float totalLoad = insertLoad + minLoad + decreaseLoad;
                if (insertLoad < 0.0f || minLoad < 0.0f || decreaseLoad < 0.0f ||
                    insertLoad > 1.0f || minLoad > 1.0f || decreaseLoad > 1.0f ||
                    std::fabs(totalLoad - 1.0f) > 1e-6f) {
                    throw std::invalid_argument ("Error: --invalid workload distribution");
                }
                config.insertLoad = insertLoad;
                config.deleteLoad = minLoad;
                config.decreaseLoad = decreaseLoad;
            } catch (const std::exception&) {
                throw std::invalid_argument ("Error: --workload values must be valid numbers");
            }
        } else {
            throw std::invalid_argument("Invalid argument: " + arg);
        }
    }

    if (config.threads <= 0) {
        throw std::invalid_argument("--threads must be positive");
    }
    if (config.ops <= 0) {
        throw std::invalid_argument("--ops must be positive");
    }

    return config;
}

/**
 * Generate operations based on given distribution
 * Generate a warmup set to avoid illegal decreaseKey, [2000000, 3000000]
 * DeleteMin will never hit this portion, decareseKey will randomly decrease their values
 */
GeneratedWorkload generate_workload(const BenchmarkConfig& config) {
    GeneratedWorkload workload;
    std::vector<Operation>& ops = workload.ops;
    ops.reserve(static_cast<size_t>(config.ops));

    // Number of alive nodes
    int live_count = kWarmupInserts;
    int next_decrease_value = 1'999'999;
    // Amount of each type operations is fixed
    const int insertTarget = static_cast<int>(config.insertLoad * config.ops);
    const int deleteTarget = static_cast<int>(config.deleteLoad * config.ops);
    const int decreaseTarget = config.ops - insertTarget - deleteTarget;
    int insertRemaining = insertTarget;
    int deleteRemaining = deleteTarget;
    int decreaseRemaining = decreaseTarget;
    // Generate random order decrease, each node once
    live_count += decreaseTarget;

    std::vector<int> random_decrease_handles;
    random_decrease_handles.reserve(decreaseTarget);
    for (int i = 0; i < decreaseTarget; ++i) {
        random_decrease_handles.push_back(i);
    }
    std::mt19937 rng(config.seed);
    std::shuffle(random_decrease_handles.begin(), random_decrease_handles.end(), rng);
    // Could be deleted node in: range 0 - 1000000
    std::uniform_int_distribution<int> valueDist(1, 1'000'000);

    for (int step = 0; step < config.ops; ++step) {
        std::vector<OpType> candidates;
        candidates.reserve(3);

        if (insertRemaining > 0) {
            candidates.push_back(OpType::Insert);
        }
        if (deleteRemaining > 0 && live_count > 0) {
            candidates.push_back(OpType::DeleteMin);
        }
        if (decreaseRemaining > 0 && live_count > 0) {
            candidates.push_back(OpType::DecreaseKey);
        }

        std::uniform_int_distribution<size_t> candidateDist(0, candidates.size() - 1);
        const OpType type = candidates[candidateDist(rng)];

        if (type == OpType::Insert) {
            ops.push_back(Operation{OpType::Insert, valueDist(rng), -1, -1});
            ++live_count;
            --insertRemaining;
        } else if (type == OpType::DeleteMin) {
            ops.push_back(Operation{OpType::DeleteMin, -1, -1, -1});
            --live_count;
            --deleteRemaining;
        } else {
            ops.push_back(Operation{
                OpType::DecreaseKey,
                -1,
                random_decrease_handles[static_cast<size_t>(decreaseRemaining - 1)],
                next_decrease_value});
            --decreaseRemaining;
            --next_decrease_value;
        }
    }

    std::vector<Operation>& warmup_ops = workload.warmup_ops;
    // Our warmup consists of normal warmup (5000) + decreasewarmup (ensure legal ops)
    const int protectedWarmupCount = kWarmupInserts + decreaseTarget;
    warmup_ops.reserve(static_cast<size_t>(protectedWarmupCount));
    workload.protected_handle_count = decreaseTarget;

    std::uniform_int_distribution<int> valueDistWarmup(1, 1'000'000);
    for (int step = 0; step < kWarmupInserts; ++step) {
        // normal warmup shares the same random distribution as other insert
        warmup_ops.push_back(Operation{OpType::Insert, valueDistWarmup(rng), -1, -1});
    }

    for (int step = 0; step < decreaseTarget; ++step) {
        // Special large insert, ensure our decreaseTarget is not deleted by deleteMin
        // deleteMin will consume normal insert only
        warmup_ops.push_back(Operation{OpType::Insert, 2'000'000 + step, step, -1});
    }

    return workload;
}

void execute_operation(
    BinaryHeap& heap,
    const Operation& op,
    BinaryHeapNode* node,
    BenchmarkResult& local_result
) {
    switch (op.type) {
    case OpType::Insert: {
        (void)heap.insert(op.handle_id, op.value);
        local_result.insert_ops++;
        break;
    }
    case OpType::DeleteMin: {
        const DeleteMinResult result = heap.deleteMin();
        local_result.delete_ops++;
        // A rough metric for minimum relaxation
        local_result.delete_value_sum += static_cast<long long>(result.value);
        break;
    }
    case OpType::DecreaseKey: {
        heap.decreaseKey(node, op.new_value);
        local_result.decrease_ops++;
        break;
    }
    }

    local_result.executed_ops++;
}

BenchmarkResult run_parallel(
    const GeneratedWorkload& workload,
    int threads
) {
    BinaryHeap heap;
    BenchmarkResult result;
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(threads));
    std::vector<BenchmarkResult> thread_results(static_cast<size_t>(threads));
    std::vector<BinaryHeapNode*> protected_handles(
        static_cast<size_t>(workload.protected_handle_count),
        nullptr);
    for (const Operation& warmup_op : workload.warmup_ops) {
        BinaryHeapNode* node = heap.insert(warmup_op.handle_id, warmup_op.value);
        if (warmup_op.handle_id >= 0) {
            protected_handles[static_cast<size_t>(warmup_op.handle_id)] = node;
        }
    }

    const auto start = std::chrono::steady_clock::now();
    // Static Assignment to minimize overhead
    for (int tid = 0; tid < threads; ++tid) {
        workers.emplace_back([&, tid]() {
            BenchmarkResult& local_result = thread_results[static_cast<size_t>(tid)];
            const size_t begin =
                static_cast<size_t>(tid) * workload.ops.size() / static_cast<size_t>(threads);
            const size_t end =
                static_cast<size_t>(tid + 1) * workload.ops.size() / static_cast<size_t>(threads);
            for (size_t index = begin; index < end; ++index) {
                const Operation& op = workload.ops[index];
                BinaryHeapNode* node = nullptr;
                if (op.type == OpType::DecreaseKey &&
                    op.handle_id >= 0 &&
                    static_cast<size_t>(op.handle_id) < protected_handles.size()) {
                    node = protected_handles[static_cast<size_t>(op.handle_id)];
                }
                execute_operation(heap, op, node, local_result);
            }
        });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }

    const auto end = std::chrono::steady_clock::now();

    for (const BenchmarkResult& local_result : thread_results) {
        result.executed_ops += local_result.executed_ops;
        result.insert_ops += local_result.insert_ops;
        result.delete_ops += local_result.delete_ops;
        result.decrease_ops += local_result.decrease_ops;
        result.delete_value_sum += local_result.delete_value_sum;
    }

    result.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

BenchmarkResult run_benchmark(
    const BenchmarkConfig& config,
    const GeneratedWorkload& workload
) {
    return run_parallel(workload, config.threads);
}

std::string format_workload(float insert, float del, float decrease) {
    return
    std::to_string(insert) + "/" +
    std::to_string(del) + "/" +
    std::to_string(decrease);
}

void print_result(
    const BenchmarkConfig& config,
    const BenchmarkResult& result
) {
    const double throughput =
        result.elapsed_ms > 0.0
            ? (1000.0 * static_cast<double>(result.executed_ops) / result.elapsed_ms)
            : 0.0;

    std::cout
        << std::fixed << std::setprecision(3)
        << "impl=binary"
        << " workload=" << format_workload(config.insertLoad, config.deleteLoad, config.decreaseLoad)
        << " threads=" << config.threads
        << " ops=" << config.ops
        << " seed=" << config.seed
        << " executed_ops=" << result.executed_ops
        << " insert_ops=" << result.insert_ops
        << " delete_ops=" << result.delete_ops
        << " delete_value_sum=" << result.delete_value_sum
        << " decrease_ops=" << result.decrease_ops
        << " time_ms=" << result.elapsed_ms
        << " throughput_ops_per_sec=" << throughput
        << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const BenchmarkConfig config = parse_args(argc, argv);
        const GeneratedWorkload workload = generate_workload(config);
        const BenchmarkResult result = run_benchmark(config, workload);
        print_result(config, result);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
