#pragma once

#include "SequentialFibHeap.h"

#include <cstddef>
#include <mutex>

/**
 * Coarse-grained thread-safe Fibonacci Heap.
 *
 * A single global mutex guards each public heap operation.
 * Returned node handles may be passed back into other heap
 * operations, but callers should not read or mutate node fields
 * directly without external synchronization. In particular,
 * min() only returns a snapshot pointer while the mutex is held;
 * the pointer should not be dereferenced concurrently with other
 * heap operations.
 */
class CoarseGrainedFibHeap {
public:
    CoarseGrainedFibHeap() = default;

    CoarseGrainedFibHeap(const CoarseGrainedFibHeap&) = delete;
    CoarseGrainedFibHeap& operator=(const CoarseGrainedFibHeap&) = delete;

    size_t size() const;
    bool isEmpty() const;
    FibNode* min() const;
    FibNode* insert(int handle_id, int value);
    void decreaseKey(FibNode* node, int newVal);
    DeleteMinResult deleteMin();

private:
    mutable std::mutex mutex_;
    SequentialFibHeap heap_;
};
