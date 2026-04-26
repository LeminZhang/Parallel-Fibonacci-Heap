#include "../src/CoarseGrainedFibHeap.h"
#include "../src/FineGrainedFibHeap.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kWarmupInserts = 5000;
constexpr int dummyNodeNum = 1024;

enum class ImplType {
    Coarse,
    Fine,
};

enum class OpType {
    Insert,
    DeleteMin,
    DecreaseKey,
};

struct BenchmarkConfig {
    ImplType impl = ImplType::Coarse;
    float insertLoad = 1.0f;
    float deleteLoad = 0.0f;
    float decreaseLoad = 0.0f;
    int threads = 1;
    int ops = 10000;
    unsigned seed = 42;
    size_t promisingSize = 8;
    size_t spillSections = 4;
};

struct Operation {
    OpType type = OpType::Insert;
    int value = 0;
    int handle_id = -1;
    int new_value = 0;
};

struct GeneratedWorkload {
    std::vector<Operation> warmup_ops;
    std::vector<Operation> ops;
    int protected_handle_count = 0;
};

struct BenchmarkResult {
    double elapsed_ms = 0.0;
    int executed_ops = 0;
    int insert_ops = 0;
    int delete_ops = 0;
    int decrease_ops = 0;
    long long delete_value_sum = 0;
};

void print_usage(const char* program_name) {
    std::cerr
        << "Usage: " << program_name
        << " --impl <coarse|fine>"
        << " --threads N"
        << " --ops N"
        << " --seed N"
        << " --workload <insert_ratio> <delete_ratio> <decrease_ratio>\n";
}

BenchmarkConfig parse_args(int argc, char** argv) {
    BenchmarkConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--impl" && i + 1 < argc) {
            const std::string impl = argv[++i];
            if (impl == "coarse") {
                config.impl = ImplType::Coarse;
            } else if (impl == "fine") {
                config.impl = ImplType::Fine;
            } else {
                throw std::invalid_argument("Unknown impl type");
            }
        } else if (arg == "--threads" && i + 1 < argc) {
            config.threads = std::stoi(argv[++i]);
        } else if (arg == "--ops" && i + 1 < argc) {
            config.ops = std::stoi(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            config.seed = static_cast<unsigned>(std::stoul(argv[++i]));
        } else if (arg == "--promising-size" && i + 1 < argc) {
            config.promisingSize = static_cast<size_t>(std::stoull(argv[++i]));
        } else if (arg == "--spill-sections" && i + 1 < argc) {
            config.spillSections = static_cast<size_t>(std::stoull(argv[++i]));
        } else if (arg == "--workload" && i + 3 < argc) {
            const float insertLoad = std::stof(argv[++i]);
            const float deleteLoad = std::stof(argv[++i]);
            const float decreaseLoad = std::stof(argv[++i]);
            const float totalLoad = insertLoad + deleteLoad + decreaseLoad;
            if (insertLoad < 0.0f || deleteLoad < 0.0f || decreaseLoad < 0.0f ||
                insertLoad > 1.0f || deleteLoad > 1.0f || decreaseLoad > 1.0f ||
                std::fabs(totalLoad - 1.0f) > 1e-6f) {
                throw std::invalid_argument("Invalid workload distribution");
            }
            config.insertLoad = insertLoad;
            config.deleteLoad = deleteLoad;
            config.decreaseLoad = decreaseLoad;
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
    if (config.promisingSize == 0) {
        throw std::invalid_argument("--promising-size must be positive");
    }
    if (config.spillSections == 0) {
        throw std::invalid_argument("--spill-sections must be positive");
    }

    return config;
}

GeneratedWorkload generate_workload(const BenchmarkConfig& config) {
    GeneratedWorkload workload;
    std::vector<Operation>& ops = workload.ops;
    ops.reserve(static_cast<size_t>(config.ops));

    const int insertTarget = static_cast<int>(config.insertLoad * config.ops);
    const int deleteTarget = static_cast<int>(config.deleteLoad * config.ops);
    const int decreaseTarget = config.ops - insertTarget - deleteTarget;
    int live_count = kWarmupInserts + decreaseTarget;
    int next_decrease_value = 1'999'999;
    int insertRemaining = insertTarget;
    int deleteRemaining = deleteTarget;
    int decreaseRemaining = decreaseTarget;

    std::mt19937 rng(config.seed);
    std::uniform_int_distribution<int> valueDist(1, 1'000'000);
    std::vector<int> random_decrease_handles;
    random_decrease_handles.reserve(static_cast<size_t>(decreaseTarget));
    for (int i = 0; i < decreaseTarget; ++i) {
        random_decrease_handles.push_back(i);
    }
    std::shuffle(random_decrease_handles.begin(), random_decrease_handles.end(), rng);

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
    const int protectedWarmupCount = kWarmupInserts + decreaseTarget;
    warmup_ops.reserve(static_cast<size_t>(protectedWarmupCount));
    workload.protected_handle_count = decreaseTarget;

    for (int i = 0; i < kWarmupInserts; ++i) {
        warmup_ops.push_back(Operation{OpType::Insert, valueDist(rng), -1, -1});
    }
    for (int i = 0; i < decreaseTarget; ++i) {
        warmup_ops.push_back(Operation{OpType::Insert, 2'000'000 + i, i, -1});
    }

    return workload;
}

template <typename HeapType>
void execute_operation(
    HeapType& heap,
    const Operation& op,
    BenchmarkResult& local_result
) {
    if (op.type == OpType::Insert) {
        (void)heap.insert(new FibNode(op.value, op.handle_id));
        local_result.insert_ops++;
    } else if (op.type == OpType::DeleteMin) {
        const DeleteMinResult result = heap.deleteMin();
        local_result.delete_ops++;
        local_result.delete_value_sum += static_cast<long long>(result.value);
    } else {
        try {
            heap.decreaseKey(op.handle_id, op.new_value);
            local_result.decrease_ops++;
        } catch (const std::runtime_error& ex) {
            if (std::string(ex.what()) != "decreaseKey handle has no active node") {
                throw;
            }
        }
    }
    local_result.executed_ops++;
}

template <typename HeapType>
BenchmarkResult run_parallel(
    HeapType& heap,
    const GeneratedWorkload& workload,
    int threads
) {
    BenchmarkResult result;
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(threads));
    std::vector<BenchmarkResult> thread_results(static_cast<size_t>(threads));

    for (const Operation& op : workload.warmup_ops) {
        (void)heap.insert(new FibNode(op.value, op.handle_id));
    }

    const auto start = std::chrono::steady_clock::now();

    for (int tid = 0; tid < threads; ++tid) {
        workers.emplace_back([&, tid]() {
            BenchmarkResult& local_result = thread_results[static_cast<size_t>(tid)];
            const size_t begin =
                static_cast<size_t>(tid) * workload.ops.size() / static_cast<size_t>(threads);
            const size_t end =
                static_cast<size_t>(tid + 1) * workload.ops.size() / static_cast<size_t>(threads);
            for (size_t index = begin; index < end; ++index) {
                execute_operation(heap, workload.ops[index], local_result);
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
    switch (config.impl) {
    case ImplType::Coarse: {
        CoarseGrainedFibHeap heap;
        return run_parallel(heap, workload, config.threads);
    }
    case ImplType::Fine: {
        FineGrainedFibHeap heap(
            dummyNodeNum,
            config.promisingSize,
            config.spillSections);
        return run_parallel(heap, workload, config.threads);
    }
    }
    throw std::invalid_argument("Unknown impl type");
}

const char* print_impl(ImplType impl) {
    switch (impl) {
    case ImplType::Coarse:
        return "coarse";
    case ImplType::Fine:
        return "fine";
    }
    return "unknown";
}

std::string format_workload(float insert, float del, float decrease) {
    return std::to_string(insert) + "/" + std::to_string(del) + "/" + std::to_string(decrease);
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
        << "impl=" << print_impl(config.impl)
        << " workload=" << format_workload(
               config.insertLoad,
               config.deleteLoad,
               config.decreaseLoad)
        << " threads=" << config.threads
        << " ops=" << config.ops
        << " seed=" << config.seed
        << " promising_size=" << config.promisingSize
        << " spill_sections=" << config.spillSections
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
        print_usage(argv[0]);
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
