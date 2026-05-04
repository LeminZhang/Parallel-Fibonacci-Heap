#pragma once

#include "SequentialFibHeap.h"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <vector>

/**
 * Relaxed fine-grained Fibonacci heap inspired by the paper's architecture.
 *
 * Inserts are distributed across dummy-node sections. deleteMin draws from a
 * promising list of candidate minima, and spill-over / consolidate refill that
 * list without maintaining a single exact global minimum pointer.
 * The underlying heap remains node-oriented; the handle-based decreaseKey
 * overload is an adapter used by benchmark workloads.
 */
class FineGrainedFibHeap {
public:
    explicit FineGrainedFibHeap(
        size_t section_count = 1024,
        size_t promising_list_size = 8,
        size_t spill_sections_per_pass = 4);
    ~FineGrainedFibHeap();

    FineGrainedFibHeap(const FineGrainedFibHeap&) = delete;
    FineGrainedFibHeap& operator=(const FineGrainedFibHeap&) = delete;

    size_t size() const;
    bool isEmpty() const;
    FibNode* insert(FibNode* node);
    void decreaseKey(int handle_id, int newVal);
    DeleteMinResult deleteMin();

private:
    // insert-related
    void resize_handle_table(int handle_id);
    void register_inserted_handle_node(FibNode* node);

    // deleteMin-related
    void detach_from_list(FibNode* node);
    std::vector<FibNode*> detach_children(FibNode* child_start);
    std::vector<FibNode*> take_section_roots(size_t section_index);
    FibNode* try_take_promising_node();
    void invalidate_promising_node(FibNode* node);
    bool is_current_version(FibNode* node) const;
    void retire_handle(FibNode* node);
    void spill_over();
    void consolidate_random_section();
    void link(FibNode* y, FibNode* x);
    void consolidate_section(size_t section_index);

    // helpers
    size_t get_random_index();
    void insert_after(FibNode* anchor, FibNode* node);
    void destroy_tree_list(FibNode* start);
    void destroy_all();

    mutable std::shared_mutex coord_mutex_;
    std::vector<FibNode*> dummy_list_;
    mutable std::vector<std::mutex> dummy_list_locks_;
    std::vector<FibNode*> promising_list_;
    std::atomic<size_t> size_;
    size_t promising_pointer_;
    size_t deletes_since_consolidate_;
    size_t spill_section_pointer_;
    size_t promising_list_size_;
    size_t spill_sections_per_pass_;
    mutable std::mutex promising_mutex_;
    mutable std::shared_mutex generation_mutex_;
    std::vector<int> current_generations_;
};
