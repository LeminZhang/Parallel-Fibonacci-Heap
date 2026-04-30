#include <vector>
#include <shared_mutex>
#include <unordered_map>
#include <sys/types.h>
#include <mutex>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <memory>
#include <chrono>
#include <iostream>
#include <omp.h>
#include <atomic>
#include <cmath>

using namespace std;

namespace parallel_affinity {

void restrict_process_to_efficiency_cores();
void pin_current_thread_to_efficiency_core();

}  // namespace parallel_affinity

template<typename T>
struct HeapNode {
    T * value;

    bool marked;
    int degree;

    HeapNode * left;
    HeapNode * right;
    HeapNode * child;
    HeapNode * parent;

    mutex * root_mutex;
    int worker_index;

    HeapNode(T * val, int worker_index) : value(val), marked(false), degree(0), left(this), right(this), child(nullptr), parent(nullptr) {
        root_mutex = new mutex();
        this->worker_index = worker_index;
    }

    ~HeapNode() {
        if (parent == nullptr) {
            // If this node is a root, we need to delete the mutex
            delete root_mutex;
        }
    }

    HeapNode * link(HeapNode * other) {
        if (this == other) {
            throw std::runtime_error("Cannot link a node to itself");
        }
        HeapNode * parent, * child;
        if (*(this->value) < *(other->value)) {
            parent = this;
            child = other; 
        }
        else {
            parent = other;
            child = this;
        }

        child->removeFromList(); // Remove child from root list

        // Link other to this node
        child->parent = parent;
        if (parent->child == nullptr) {
            parent->child = child;
            child->left = child;
            child->right = child;
        } else {
            // Insert child into the child list of parent
            child->insertIntoList(parent->child);
        }
        parent->degree++;
        parent->marked = false;
        delete child->root_mutex;
        child->root_mutex = nullptr; // Child nodes do not have their own mutex, they will use the mutex of the root node
        child->worker_index = parent->worker_index; // Update the worker index for the child node

        return parent;
    }

    void insertIntoList(HeapNode<T>* &list) {
        // Insert this node into the list of the given node
        if (list == nullptr) {
            this->left = this;
            this->right = this;
            list = this;
        } else {
            this->right = list;
            this->left = list->left;
            list->left->right = this;
            list->left = this;
        }
    }

    void removeFromList() {
        // Remove this node from its current list
        if (this->right != this) {
            this->left->right = this->right;
            this->right->left = this->left;
        }
    }

    HeapNode * makeRoot(HeapNode<T>* &list, int worker_index) {
        // Make this node a root and insert it into the given list
        if (this->parent == nullptr) {
            throw std::runtime_error("Only nodes with a parent can be made root");
        }
        this->parent->degree--; // Decrement the degree of the parent
        if (this->parent->child == this) {
            if (this->right != this) {
                this->parent->child = this->right; // Update the child pointer of the parent if necessary
            } else {
                this->parent->child = nullptr;
            }
        }
        this->removeFromList(); // Remove the node from its current list

        this->worker_index = worker_index;
        this->parent = nullptr;
        this->marked = false;
        this->root_mutex = new mutex(); // The new root node needs to have its own mutex
        this->insertIntoList(list);
        return this;
    }
};

template<typename T>
class ParallelWorker {
public:
    size_t worker_size = 0;  // The number of trees in the root list of this worker
    HeapNode<T>* first_root = nullptr;
    mutable mutex worker_mutex;

    ParallelWorker() = default;
    ~ParallelWorker() = default;

    // Only call when worker_mutex is locked
    void insert(HeapNode<T>* node, size_t size) {
        // Merge the new node list with the existing root list
        if (first_root == nullptr) {
            first_root = node;
        } else {
            // Merge the two circular lists
            HeapNode<T>* first_root_left = first_root->left;
            HeapNode<T>* node_left = node->left;
            first_root->left = node_left;
            node_left->right = first_root;
            node->left = first_root_left;
            first_root_left->right = node;
        }

        worker_size += size;
    }

    // Only call when worker_mutex is locked and node is guaranteed to be in this worker's root list
    void remove(HeapNode<T>* node) {
        if (node == first_root) {
            if (node->right == node) {
                first_root = nullptr;
            } else {
                first_root = node->right;
                node->removeFromList();
            }
        } else {
            node->removeFromList();
        }

        delete node;
        worker_size--;
    }

    // Only call when worker_mutex is locked
    void findMin(HeapNode<T>* &node, int max_degree) {
        if (first_root == nullptr) {
            node = nullptr;
            return;
        }

        // Consolidate while finding the minimum node

        std::vector<HeapNode<T>*> table(max_degree + 1, nullptr);
        HeapNode<T>* min_node = first_root;
        HeapNode<T>* curr = first_root;
        size_t original_size = worker_size;
        for (int i = 0; i < (int)original_size; i++) {
            HeapNode<T>* next = curr->right; // The original list

            if (table[curr->degree] == nullptr) {
                table[curr->degree] = curr;
            } else {
                // We have a duplicate degree, we need to link the two trees
                int d = curr->degree;
                while (table[d] != nullptr) {
                    HeapNode<T>* y = table[d];
                    curr = curr->link(y);
                    table[d] = nullptr;
                    d++;
                    worker_size--; 
                }
                table[d] = curr;
                first_root = curr; // Update the first root pointer to the newly linked tree, this is important for the correctness of the algorithm, otherwise we might lose access to some nodes in the root list after consolidation
            }
            if (!(*(min_node->value) < *(curr->value))) {
                min_node = curr;
            }
            curr = next;
        }

        node = min_node;
    }
};

template<typename T>
class ParallelFibHeap
{
private:
    vector<unique_ptr<ParallelWorker<T>>> workers;
    atomic<size_t> total_size = 0; // The total number of trees in the root lists of all workers
    unsigned getAvailableWorker() {
        // Find the worker with the smallest local heap size
        unsigned target_index = 0;
        size_t target_size = workers[0]->worker_size;
        for (unsigned i = 1; i < workers.size(); i++) {
            if (workers[i]->worker_size < target_size) {
                target_index = i;
                target_size = workers[i]->worker_size;
            }
        }
        return target_index;
    }
public:
    ParallelFibHeap(int num_workers) {
        for (int i = 0; i < num_workers; i++) {
            workers.push_back(make_unique<ParallelWorker<T>>());
        }
    }

    ~ParallelFibHeap() {
        // Delete all nodes in the heap
        for (size_t i = 0; i < workers.size(); i++) {
            workers[i]->worker_mutex.lock();
            HeapNode<T>* curr = workers[i]->first_root;
            size_t count = 0;
            while (curr != nullptr && count < workers[i]->worker_size) {
                HeapNode<T>* next = curr->right; // The original list
                delete curr;
                curr = next;
                count++;
            }
            workers[i]->worker_mutex.unlock();
        }
    }

    void insert(vector<T*> values, vector<HeapNode<T>*>& nodes) {
        // Find the worker with the smallest local heap size
        unsigned target_index = getAvailableWorker();

        // Create a whole list of new nodes for the input values
        HeapNode<T>* new_node = nullptr;
        for (const auto& val : values) {
            HeapNode<T>* node = new HeapNode<T>(val, target_index);
            nodes.push_back(node);
            if (new_node == nullptr) {
                new_node = node;
            } else {
                // Insert node into the new list
                node->right = new_node;
                node->left = new_node->left;
                new_node->left->right = node;
                new_node->left = node;
            }
        }

        // Add the new node to the root list
        workers[target_index]->worker_mutex.lock();
        workers[target_index]->insert(new_node, values.size());
        workers[target_index]->worker_mutex.unlock();

        total_size += values.size();
    }

    int extractMin(T* & min_value) {
        int max_deg = static_cast<int>(std::log2(static_cast<double>(total_size.load()))) + 2;

        // Acquire all worker mutexes to ensure a consistent view of the heap
        for (size_t i = 0; i < workers.size(); i++) {
            workers[i]->worker_mutex.lock();
        }

        // Find the minimum node among all workers
        vector<HeapNode<T>*> min_nodes(workers.size(), nullptr);

        #pragma omp parallel for num_threads(workers.size())
        for (size_t i = 0; i < workers.size(); i++) {
            parallel_affinity::pin_current_thread_to_efficiency_core();
            workers[i]->findMin(min_nodes[i], max_deg);
        }

        HeapNode<T>* global_min_node = nullptr;
        for (size_t i = 0; i < workers.size(); i++) {
            if (min_nodes[i] != nullptr) {
                if (global_min_node == nullptr || *(min_nodes[i]->value) < *(global_min_node->value)) {
                    global_min_node = min_nodes[i];
                }
            }
        }
        if (global_min_node != nullptr) {
            min_value = global_min_node->value;
        }
        else {
            // The heap is empty, we can unlock the mutexes and return
            for (int i = workers.size() - 1; i >= 0; i--) {
                workers[i]->worker_mutex.unlock();
            }
            min_value = nullptr;
            return -1; // Indicate that the heap is empty
        }

        // Reinsert the children of the minimum node into the root list of the worker that contained the minimum node
        if (global_min_node->child != nullptr) {
            HeapNode<T>* child = global_min_node->child;
            do {
                child->parent = nullptr; // Update the parent pointer for the child node
                child->worker_index = global_min_node->worker_index; // Update the worker index for the child node
                child->root_mutex = new mutex(); // Child nodes that are being reinserted into the root list need to have their own mutex
                child->marked = false; // Unmark the child node
                child = child->right;
            } while (child != global_min_node->child);
    
            workers[global_min_node->worker_index]->insert(child, global_min_node->degree); // Insert the child into the worker's root list, size is 0 because we are just reinserting existing nodes
        }

        // Remove the minimum node
        size_t worker_index = global_min_node->worker_index;
        workers[worker_index]->remove(global_min_node);

        // Unlock all worker mutexes after the operation is done
        for (int i = (int)workers.size() - 1; i >= 0; i--) {
            workers[i]->worker_mutex.unlock();
        }

        total_size--; // Decrement the total size of the heap

        return 0;
    }

    void obtainMutexesForDecreaseKey(HeapNode<T>* node) {
        workers[node->worker_index]->worker_mutex.lock();
        if (node->parent == nullptr) {
            node->root_mutex = nullptr;
            workers[node->worker_index]->worker_mutex.unlock();
            return;
        }
        HeapNode<T>* root = node;
        while (root->parent != nullptr) {
            root = root->parent;
        }
        node->root_mutex = root->root_mutex;
        workers[node->worker_index]->worker_mutex.unlock();
    }

    void decreaseKey(HeapNode<T>* node) {
        unsigned worker_index = getAvailableWorker();
        HeapNode<T>* new_list = nullptr;
        size_t new_list_size = 0;

        if (node->root_mutex == nullptr) {
            return;
        }

        mutex* root_mutex = node->root_mutex;
        root_mutex->lock();

        HeapNode<T>* parent = node->parent;
        if (parent == nullptr) {
            root_mutex->unlock();
            return;
        }

        if (*(node->value) < *(parent->value)) {
            // We need to cut the node and move it to the root list
            node->makeRoot(new_list, worker_index);
            new_list_size++;

            while (parent != nullptr && parent->marked) {
                // We need to cut the parent node as well
                HeapNode<T>* grandparent = parent->parent;
                
                parent->makeRoot(new_list, worker_index);
                new_list_size++;

                parent = grandparent; // Move up to the next level
            }

            if (parent != nullptr && parent->parent != nullptr) {
                parent->marked = true; // Mark the parent node if it is not a root
            }
        }

        root_mutex->unlock();

        if (new_list != nullptr) {
            workers[worker_index]->worker_mutex.lock();
            workers[worker_index]->insert(new_list, new_list_size); 
            workers[worker_index]->worker_mutex.unlock();
        }
    }

    void rebalance() {
        // Acquire all worker mutexes to ensure a consistent view of the heap
        for (size_t i = 0; i < workers.size(); i++) {
            workers[i]->worker_mutex.lock();
        }

        vector<vector<HeapNode<T>*>> new_lists(workers.size(), vector<HeapNode<T>*>(workers.size(), nullptr));
        vector<vector<size_t>> new_lists_sizes(workers.size(), vector<size_t>(workers.size(), 0));

        #pragma omp parallel for num_threads(workers.size())
        for (size_t i = 0; i < workers.size(); i++) {
            parallel_affinity::pin_current_thread_to_efficiency_core();
            int count = 0;
            HeapNode<T>* curr = workers[i]->first_root;
            while (curr != nullptr && count < (int)workers[i]->worker_size) {
                HeapNode<T>* next = curr->right; // The original list
                curr->worker_index = count % workers.size(); // Update the worker index for the current node
                curr->insertIntoList(new_lists[curr->worker_index][i]);
                new_lists_sizes[curr->worker_index][i]++;
                curr = next;
                count++;
            }
        }

        #pragma omp parallel for num_threads(workers.size())
        for (size_t i = 0; i < workers.size(); i++) {
            workers[i]->first_root = nullptr; // Clear the current root list
            workers[i]->worker_size = 0; // Reset the worker size
            for (size_t j = 0; j < workers.size(); j++) {
                if (new_lists[i][j] != nullptr) {
                    workers[i]->insert(new_lists[i][j], new_lists_sizes[i][j]); // Insert the newly consolidated node into the worker's root list, size is 0 because we are just reinserting existing nodes
                }
            }
        }

        // Unlock all worker mutexes after the operation is done
        for (int i = (int)workers.size() - 1; i >= 0; i--) {
            workers[i]->worker_mutex.unlock();
        }
    }

    bool isEmpty() {
        return total_size.load() == 0;
    }
};
