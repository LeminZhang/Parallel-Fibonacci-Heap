#pragma once

#include <vector>

#include "SequentialFibHeap.h"

using namespace std;

struct BinaryHeapNode {
    int value;
    int handle_id;
    size_t index_in_heap;    

    BinaryHeapNode(int handle_id, int value, size_t index)
        : value(value), handle_id(handle_id), index_in_heap(index) {}
};


class BinaryHeap
{
public:
    BinaryHeap();
    ~BinaryHeap();

    // Disable copy
    BinaryHeap(const BinaryHeap&) = delete;
    BinaryHeap& operator=(const BinaryHeap&) = delete;

    size_t size()    const;
    bool isEmpty() const;
    BinaryHeapNode* min() const;
    BinaryHeapNode* insert(int handle_id, int value);   // O(1)
    void decreaseKey(BinaryHeapNode* node, int newVal); // O(log n)
    DeleteMinResult deleteMin();                 // O(log n)

    void debug_print() const;
private:
    vector<BinaryHeapNode*> heap_data; 

    void bubbleUp(size_t index);
    void bubbleDown(size_t index);
};