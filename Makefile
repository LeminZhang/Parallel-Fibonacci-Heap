
# Platform detection
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    # macOS configuration
    CXX := clang++
    CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Xclang -fopenmp -I/opt/homebrew/opt/libomp/include -O3
    LDFLAGS := -L/opt/homebrew/opt/libomp/lib -lomp
else
    # Linux and other Unix-like systems
    CXX := g++
    CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -fopenmp -O3
    LDFLAGS :=
endif

SRC_DIR := src
TEST_DIR := tests
BENCH_DIR := bench
BUILD_DIR := build

SEQUENTIAL_TEST_BIN := $(BUILD_DIR)/SequentialFibHeapTest.exe
COARSE_TEST_BIN := $(BUILD_DIR)/CoarseGrainedFibHeapTest.exe
BINARY_TEST_BIN := $(BUILD_DIR)/BinaryHeapTest.exe
PARALLEL_TEST_BIN := $(BUILD_DIR)/ParallelFibHeapTest.exe
BENCHMARK_BIN := $(BUILD_DIR)/benchmark.exe
BENCHMARK_BINARY_BIN := $(BUILD_DIR)/benchmark_binary.exe
BENCHMARK_PARALLEL_BIN := $(BUILD_DIR)/benchmark_parallel.exe
BENCHMARK_DIJKSTRA_BIN := $(BUILD_DIR)/benchmark_dijkstra.exe
TEST_BINS := $(SEQUENTIAL_TEST_BIN) $(COARSE_TEST_BIN) $(BINARY_TEST_BIN) $(PARALLEL_TEST_BIN)

.PHONY: all test benchmark clean

all: $(TEST_BINS) $(BENCHMARK_BIN) $(BENCHMARK_BINARY_BIN) $(BENCHMARK_PARALLEL_BIN) $(BENCHMARK_DIJKSTRA_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(SEQUENTIAL_TEST_BIN): $(TEST_DIR)/SequentialFibHeapTest.cpp $(SRC_DIR)/SequentialFibHeap.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(COARSE_TEST_BIN): $(TEST_DIR)/CoarseGrainedFibHeapTest.cpp $(SRC_DIR)/CoarseGrainedFibHeap.cpp $(SRC_DIR)/SequentialFibHeap.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(PARALLEL_TEST_BIN): $(TEST_DIR)/ParallelFibHeapTest.cpp $(SRC_DIR)/ParallelFibHeap.cpp  | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BENCHMARK_BIN): $(BENCH_DIR)/benchmark.cpp $(SRC_DIR)/CoarseGrainedFibHeap.cpp $(SRC_DIR)/SequentialFibHeap.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	
$(BINARY_TEST_BIN): $(TEST_DIR)/BinaryHeapTest.cpp $(SRC_DIR)/BinaryHeap.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BENCHMARK_BINARY_BIN): $(BENCH_DIR)/benchmark_binary.cpp $(SRC_DIR)/BinaryHeap.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BENCHMARK_PARALLEL_BIN): $(BENCH_DIR)/benchmark_parallel.cpp $(SRC_DIR)/ParallelFibHeap.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BENCHMARK_DIJKSTRA_BIN): $(BENCH_DIR)/benchmark_dijkstra.cpp $(SRC_DIR)/ParallelFibHeap.cpp $(SRC_DIR)/Dijkstra.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

test: $(TEST_BINS)
	./$(SEQUENTIAL_TEST_BIN)
	./$(COARSE_TEST_BIN)
	./$(BINARY_TEST_BIN)

benchmark: $(BENCHMARK_BIN)

clean:
	rm -rf $(BUILD_DIR)
