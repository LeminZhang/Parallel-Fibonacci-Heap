#pragma once

#include "SequentialFibHeap.h"

#include <cstddef>
#include <mutex>
#include <vector>

/**
 * Coarse-grained thread-safe Fibonacci Heap.
 *
 * A single global mutex guards each public heap operation.
 * The underlying sequential heap is still node-oriented. This wrapper
 * adds a handle-based decreaseKey adapter for benchmark workloads by
 * tracking the current live node pointer for each logical handle.
 */
class CoarseGrainedFibHeap {
public:
    CoarseGrainedFibHeap() = default;

    CoarseGrainedFibHeap(const CoarseGrainedFibHeap&) = delete;
    CoarseGrainedFibHeap& operator=(const CoarseGrainedFibHeap&) = delete;

    size_t size() const;
    bool isEmpty() const;
    FibNode* min() const;
    FibNode* insert(FibNode* node);
    void decreaseKey(FibNode* node, int newVal);
    void decreaseKey(int handle_id, int newVal);
    DeleteMinResult deleteMin();

private:
    // Grow the handle table on demand before indexing by handle_id.
    void resize_handle_table(int handle_id);

    mutable std::mutex mutex_;
    SequentialFibHeap heap_;
    // handle_id -> current live node for the coarse exact heap.
    std::vector<FibNode*> current_nodes_;
};
