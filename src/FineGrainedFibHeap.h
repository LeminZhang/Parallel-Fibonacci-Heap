#pragma once

#include "SequentialFibHeap.h"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <vector>

/**
 * Partial fine-grained Fibonacci heap.
 *
 * Insert is split across root-list sections guarded by dummy-node locks.
 * Exact deleteMin / decreaseKey take an exclusive coordination lock.
 */
class FineGrainedFibHeap {
public:
    explicit FineGrainedFibHeap(size_t section_count = 64);
    ~FineGrainedFibHeap();

    FineGrainedFibHeap(const FineGrainedFibHeap&) = delete;
    FineGrainedFibHeap& operator=(const FineGrainedFibHeap&) = delete;

    size_t size() const;
    bool isEmpty() const;
    FibNode* min() const;
    FibNode* insert(int handle_id, int value);
    void decreaseKey(FibNode* node, int newVal);
    DeleteMinResult deleteMin();

private:
    size_t get_random_index();
    void insert_after(FibNode* anchor, FibNode* node);
    void detach_from_list(FibNode* node);
    std::vector<FibNode*> collect_children(FibNode* child_start);
    std::vector<FibNode*> collect_all_roots();
    void clear_root_sections();
    void rebuild_min_from_roots();
    void consolidate_all();
    void link(FibNode* y, FibNode* x);
    void cut(FibNode* node, FibNode* parent);
    void cascading_cut(FibNode* node);
    void destroy_all();

    mutable std::shared_mutex coord_mutex_;
    std::vector<FibNode*> dummy_list_;
    std::vector<std::mutex> dummy_list_locks_;
    std::atomic<size_t> size_;
    mutable std::mutex min_mutex_;
    FibNode* min_node_;
    std::mt19937 rng_;
    mutable std::mutex rng_mutex_;
};
