#include "FibNode.h"
#include "SequentialFibHeap.h"
#include <vector>
#include <shared_mutex>
#include <unordered_map>
#include <sys/types.h>

using namespace std;


class ParallelFibHeap
{
private:
    vector<SequentialFibHeap*> local_heaps;
    mutable shared_mutex global_mutex;
    unordered_map<pid_t, SequentialFibHeap*> thread_heap_map;
public:
    ParallelFibHeap(/* args */);
    ~ParallelFibHeap();
    SequentialFibHeap* AddThread();
    void RemoveThread(SequentialFibHeap* local_heap);

    size_t size() const;
    bool isEmpty() const;
    FibNode* min() const;
    FibNode* insert(int handle_id, int value);
    void protect();
    // void decreaseKey(FibNode* node, int newVal);
    // DeleteMinResult deleteMin();
};
