#pragma once
#include "FibNode.h"
#include <cstddef>

struct DeleteMinResult {
    int value;
    int handle_id;
};

/**
 * Sequential Fibonacci Heap (int keys).
 * Fredman & Tarjan (1987).
 *
 *   insert:      O(1)      amortized
 *   decreaseKey: O(1)      amortized
 *   deleteMin:   O(log n)  amortized
 */
class SequentialFibHeap {
public:
    SequentialFibHeap();
    ~SequentialFibHeap();

    // Disable copy
    SequentialFibHeap(const SequentialFibHeap&) = delete;
    SequentialFibHeap& operator=(const SequentialFibHeap&) = delete;

    size_t size()    const;
    bool isEmpty() const;
    FibNode* min() const;
    FibNode* insert(int handle_id, int value);   // O(1)
    void decreaseKey(FibNode* node, int newVal); // O(1)
    DeleteMinResult deleteMin();                 // O(log n)

private:
    FibNode* min_node_;
    size_t   size_;

    void consolidate();
    void link(FibNode* y, FibNode* x);
    void cut(FibNode* node, FibNode* parent);
    void cascading_cut(FibNode* node);
    void destroy_all(FibNode* start);
};
