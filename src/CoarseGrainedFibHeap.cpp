#include "CoarseGrainedFibHeap.h"

#include <utility>

size_t CoarseGrainedFibHeap::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return heap_.size();
}

bool CoarseGrainedFibHeap::isEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return heap_.isEmpty();
}

FibNode* CoarseGrainedFibHeap::min() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return heap_.min();
}

FibNode* CoarseGrainedFibHeap::insert(int handle_id, int value) {
    std::lock_guard<std::mutex> lock(mutex_);
    return heap_.insert(handle_id, value);
}

void CoarseGrainedFibHeap::decreaseKey(FibNode* node, int newVal) {
    std::lock_guard<std::mutex> lock(mutex_);
    heap_.decreaseKey(node, newVal);
}

DeleteMinResult CoarseGrainedFibHeap::deleteMin() {
    std::lock_guard<std::mutex> lock(mutex_);
    return heap_.deleteMin();
}
