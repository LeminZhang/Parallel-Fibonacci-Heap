#include "BinaryHeap.h"
#include <iostream>

BinaryHeap::BinaryHeap() {
}

BinaryHeap::~BinaryHeap() {
    for (BinaryHeapNode* node : heap_data) {
        delete node;
    }
}

BinaryHeapNode* BinaryHeap::min() const
{
    if (heap_data.empty()) {
        return nullptr;
    }
    return heap_data[0];
}

size_t BinaryHeap::size() const
{
    return heap_data.size();
}

bool BinaryHeap::isEmpty() const
{
    return heap_data.empty();
}

BinaryHeapNode* BinaryHeap::insert(int handle_id, int value)
{
    BinaryHeapNode* newNode = new BinaryHeapNode(handle_id, value, heap_data.size());
    heap_data.push_back(newNode);
    bubbleUp(heap_data.size() - 1);
    return newNode;
}

void BinaryHeap::decreaseKey(BinaryHeapNode* node, int newVal)
{
    size_t index = node->index_in_heap;
    if (newVal > node->value) {
        throw std::invalid_argument("New value is greater than current value");
    }
    node->value = newVal;
    bubbleUp(index);
}

DeleteMinResult BinaryHeap::deleteMin()
{
    if (heap_data.empty()) {
        throw std::runtime_error("Heap is empty");
    }
    BinaryHeapNode* minNode = heap_data[0];
    DeleteMinResult result{minNode->value, minNode->handle_id};
    delete minNode;
    heap_data[0] = heap_data.back();
    heap_data.pop_back();

    bubbleDown(0);

    return result;
}

void BinaryHeap::bubbleUp(size_t index)
{
    // Maintain heap property after insert or decreaseKey.
    while (index > 0) {
        size_t parentIndex = (index - 1) / 2;
        if (heap_data[index]->value < heap_data[parentIndex]->value) {
            size_t tmp = heap_data[index]->index_in_heap;
            heap_data[index]->index_in_heap = heap_data[parentIndex]->index_in_heap;
            heap_data[parentIndex]->index_in_heap = tmp;

            std::swap(heap_data[index], heap_data[parentIndex]);
            index = parentIndex;
        } else {
            break;
        }
    }
}

void BinaryHeap::bubbleDown(size_t index)
{
    // Maintain heap property after deleteMin.
    size_t smallest = index;
    size_t leftChild = 2 * index + 1;
    size_t rightChild = 2 * index + 2;

    if (leftChild < heap_data.size() && heap_data[leftChild]->value < heap_data[smallest]->value) {
        smallest = leftChild;
    }
    if (rightChild < heap_data.size() && heap_data[rightChild]->value < heap_data[smallest]->value) {
        smallest = rightChild;
    }
     
    if (smallest != index) {
        size_t tmp = heap_data[index]->index_in_heap;
        heap_data[index]->index_in_heap = heap_data[smallest]->index_in_heap;
        heap_data[smallest]->index_in_heap = tmp;

        std::swap(heap_data[index], heap_data[smallest]);
        bubbleDown(smallest);
    }
}

void BinaryHeap::debug_print() const    
{
    for (const auto& node : heap_data) {
        std::cout << "Handle ID: " << node->handle_id << ", Value: " << node->value << std::endl;
    }
}