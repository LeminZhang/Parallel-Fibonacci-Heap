#include "ParallelFibHeap.h"
#include <unistd.h>
#include <mutex>
#include <stdexcept>

ParallelFibHeap::ParallelFibHeap(/* args */)
{
}

ParallelFibHeap::~ParallelFibHeap()
{
}

SequentialFibHeap* ParallelFibHeap::AddThread()
{
    unique_lock<shared_mutex> lock(global_mutex);
    SequentialFibHeap* local_heap = new SequentialFibHeap();
    local_heaps.push_back(local_heap);

    pid_t pid = getpid();

    thread_heap_map[pid] = local_heap;
    
    return local_heap;
}

void ParallelFibHeap::RemoveThread(SequentialFibHeap* local_heap)
{
    unique_lock<shared_mutex> lock(global_mutex);
    auto it = std::find(local_heaps.begin(), local_heaps.end(), local_heap);
    if (it != local_heaps.end()) {
        local_heaps.erase(it);
        delete local_heap;
    }

    pid_t pid = getpid();
    thread_heap_map.erase(pid);
}

size_t ParallelFibHeap::size() const
{
    shared_lock<shared_mutex> lock(global_mutex);
    size_t total_size = 0;
    for (const SequentialFibHeap* local_heap : local_heaps) {
        total_size += local_heap->size();
    }
    return total_size;
}

bool ParallelFibHeap::isEmpty() const
{
    return size() == 0;
}

FibNode* ParallelFibHeap::min() const
{
    shared_lock<shared_mutex> lock(global_mutex);
    FibNode* global_min = nullptr;
    for (const SequentialFibHeap* local_heap : local_heaps) {
        if (local_heap->min()) {
            if (!global_min || local_heap->min()->value < global_min->value) {
                global_min = local_heap->min();
            }
        }
    }
    return global_min;
}

FibNode* ParallelFibHeap::insert(int handle_id, int value)
{
    // For simplicity, we insert into the first local heap. In a real implementation,
    // we might want to use thread-local storage or a more sophisticated strategy.
    // shared_lock<shared_mutex> lock(global_mutex);
    if (local_heaps.empty()) {
        throw std::runtime_error("No local heaps available");
    }

    pid_t pid = getpid();
    auto it = thread_heap_map.find(pid);
    if (it == thread_heap_map.end()) {
        throw std::runtime_error("Current thread does not have a local heap");
    }
    SequentialFibHeap* local_heap = it->second;
    return local_heap->insert(handle_id, value);
}

void ParallelFibHeap::protect()
{
    unique_lock<shared_mutex> lock(global_mutex);
}