#include "CoarseGrainedFibHeap.h"

#include <stdexcept>
#include <utility>

void CoarseGrainedFibHeap::resize_handle_table(int handle_id) {
    if (handle_id < 0) {
        return;
    }
    const size_t slot = static_cast<size_t>(handle_id);
    // Expand current_nodes_ once we insert
    // Double the size when resizing, starting from 64
    if (slot >= current_nodes_.size()) {
        size_t new_size = current_nodes_.empty() ? 64 : current_nodes_.size();
        while (slot >= new_size) {
            new_size *= 2;
        }
        current_nodes_.resize(new_size, nullptr);
    }
}

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

FibNode* CoarseGrainedFibHeap::insert(FibNode* node) {
    std::lock_guard<std::mutex> lock(mutex_);
    FibNode* inserted = heap_.insert(node);
    if (inserted != nullptr && inserted->handle_id >= 0) {
        // Keep the latest node pointer for handle-based decreaseKey.
        resize_handle_table(inserted->handle_id);
        current_nodes_[static_cast<size_t>(inserted->handle_id)] = inserted;
    }
    return inserted;
}

void CoarseGrainedFibHeap::decreaseKey(FibNode* node, int newVal) {
    if (node != nullptr && node->handle_id >= 0) {
        decreaseKey(node->handle_id, newVal);
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    heap_.decreaseKey(node, newVal);
}

void CoarseGrainedFibHeap::decreaseKey(int handle_id, int newVal) {
    std::lock_guard<std::mutex> lock(mutex_);
    const size_t slot = static_cast<size_t>(handle_id);
    if (slot >= current_nodes_.size()) {
        throw std::runtime_error("decreaseKey handle has no active node");
    }
    // Coarse mode still performs an exact in-place decrease on the live node.
    FibNode* node = current_nodes_[slot];
    if (node == nullptr) {
        throw std::runtime_error("decreaseKey handle has no active node");
    }
    heap_.decreaseKey(node, newVal);
}

DeleteMinResult CoarseGrainedFibHeap::deleteMin() {
    std::lock_guard<std::mutex> lock(mutex_);
    DeleteMinResult result = heap_.deleteMin();
    if (result.handle_id >= 0) {
        // Once the live node is deleted, the handle no longer maps to a node.
        const size_t slot = static_cast<size_t>(result.handle_id);
        if (slot < current_nodes_.size()) {
            current_nodes_[slot] = nullptr;
        }
    }
    return result;
}
