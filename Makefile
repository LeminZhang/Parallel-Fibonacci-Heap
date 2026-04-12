CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic
LDFLAGS ?=

SRC_DIR := src
TEST_DIR := tests
BUILD_DIR := build

SEQUENTIAL_TEST_BIN := $(BUILD_DIR)/SequentialFibHeapTest.exe
COARSE_TEST_BIN := $(BUILD_DIR)/CoarseGrainedFibHeapTest.exe
TEST_BINS := $(SEQUENTIAL_TEST_BIN) $(COARSE_TEST_BIN)

.PHONY: all test clean

all: $(TEST_BINS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(SEQUENTIAL_TEST_BIN): $(TEST_DIR)/SequentialFibHeapTest.cpp $(SRC_DIR)/SequentialFibHeap.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(COARSE_TEST_BIN): $(TEST_DIR)/CoarseGrainedFibHeapTest.cpp $(SRC_DIR)/CoarseGrainedFibHeap.cpp $(SRC_DIR)/SequentialFibHeap.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

test: $(TEST_BINS)
	./$(SEQUENTIAL_TEST_BIN)
	./$(COARSE_TEST_BIN)

clean:
	rm -rf $(BUILD_DIR)
