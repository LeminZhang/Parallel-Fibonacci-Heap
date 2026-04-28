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

using namespace std;

template<typename T>
struct HeapNode {
    T * value;

    bool marked;

    HeapNode * left;
    HeapNode * right;
    HeapNode * child;
    HeapNode * parent;

    struct RootInfo {
        int degree;
        mutex * root_mutex;
        int worker_index;
    };
    
    struct ChildInfo {
        HeapNode * root;
    };

    union NodeInfo {
        RootInfo root_info;
        ChildInfo child_info;
    } info;

    HeapNode(T * val, int worker_index) : value(val), marked(false), left(this), right(this), child(nullptr), parent(nullptr) {
        info.root_info.degree = 0;
        info.root_info.root_mutex = new mutex();
        info.root_info.worker_index = worker_index;
    }

    ~HeapNode() {
        if (parent == nullptr) {
            // If this node is a root, we need to delete the mutex
            delete info.root_info.root_mutex;
        }
    }

    HeapNode * link(HeapNode * other) {
        if (*(other->value) < *(this->value)) {
            return other->link(this);
        }
        // Link other to this node
        other->removeFromList(); // Remove other from its current list
        other->parent = this;
        if (this->child == nullptr) {
            this->child = other;
            other->left = other;
            other->right = other;
        } else {
            other->left = this->child;
            other->right = this->child->right;
            this->child->right->left = other;
            this->child->right = other;
        }
        this->info.root_info.degree++;
        this->marked = false;
        other->info.child_info.root = this; // Update the root pointer for the child node

        return this;
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
};

template<typename T>
class ParallelWorker {
public:
    size_t worker_size = 0;
    HeapNode<T>* first_root = nullptr;
    mutable mutex worker_mutex;

    // Delete copy and move constructors/assignment operators due to mutex
    ParallelWorker(const ParallelWorker&) = delete;
    ParallelWorker& operator=(const ParallelWorker&) = delete;
    ParallelWorker(ParallelWorker&&) = delete;
    ParallelWorker& operator=(ParallelWorker&&) = delete;

    ParallelWorker() = default;
    ~ParallelWorker() = default;

    void insert(HeapNode<T>* node, size_t size) {
        lock_guard<mutex> lock(worker_mutex);
        node->insertIntoList(first_root);
        worker_size += size;
    }

    // Only call when worker_mutex is locked and node is guaranteed to be in this worker's root list
    void remove(HeapNode<T>* node) {
        // !!! Debug
        // Check if worker_mutex is locked
        if (!worker_mutex.try_lock()) {
            throw std::runtime_error("Worker mutex is not locked when calling remove");
        }

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

        worker_size--;
    }

    // Only call when worker_mutex is locked
    void findMin(HeapNode<T>* &node, vector<HeapNode<T>*> &newly_consolidated, vector<size_t> &newly_consolidated_sizes) {
        // !!! Debug
        // Check if worker_mutex is locked
        if (!worker_mutex.try_lock()) {
            throw std::runtime_error("Worker mutex is not locked when calling findMin");
        } 

        if (first_root == nullptr) {
            node = nullptr;
            return;
        }

        for (int i = 0; i < newly_consolidated.size(); i++) {
            newly_consolidated[i] = nullptr;
            newly_consolidated_sizes[i] = 0;
        }
        
        // Consolidate while finding the minimum node

        int max_degree = worker_size;
        std::vector<HeapNode<T>*> table(max_degree + 1, nullptr);
        HeapNode<T>* min_node = first_root;
        HeapNode<T>* curr = first_root;
        int count = 0;
        do {
            if (*(curr->value) < *(min_node->value)) {
                min_node = curr;
            }
            if (table[curr->info.root_info.degree] == nullptr) {
                table[curr->info.root_info.degree] = curr;
            } else {
                // We have a duplicate degree, we need to link the two trees
                HeapNode<T>* x = curr;
                int d = x->info.root_info.degree;
                while (table[d] != nullptr) {
                    HeapNode<T>* y = table[d];
                    x->link(y);
                    newly_consolidated_sizes[x->info.root_info.worker_index]--; // We are linking two trees, so the total number of trees in the root list decreases by 1
                    newly_consolidated_sizes[y->info.root_info.worker_index]--;
                    table[d] = nullptr;
                    d++;
                    worker_size--; // We are linking two trees, so the total number of trees in the root list decreases by 1
                }
                table[d] = x;
            }

            HeapNode<T>* next = curr->right; // The original list

            curr->info.root_info.worker_index = count % newly_consolidated.size(); // Update the worker index for the node
            curr->insertIntoList(newly_consolidated[count % newly_consolidated.size()]); // Insert the node into the new consolidated list
            newly_consolidated_sizes[count % newly_consolidated.size()]++; // Increment the size of the consolidated list

            curr = next;
            count++;
        } while (curr != first_root);

        node = min_node;
    }
};

template<typename T>
class ParallelFibHeap
{
private:
    vector<unique_ptr<ParallelWorker<T>>> workers;
public:
    ParallelFibHeap(int num_workers) {
        for (int i = 0; i < num_workers; i++) {
            workers.push_back(make_unique<ParallelWorker<T>>());
        }
    }

    ~ParallelFibHeap() { }

    HeapNode<T>* insert(vector<T*> values) {
        // Find the worker with the smallest local heap size
        unsigned target_index = 0;
        size_t target_size = workers[0]->worker_size;
        for (unsigned i = 1; i < workers.size(); i++) {
            if (workers[i]->worker_size < target_size) {
                target_index = i;
                target_size = workers[i]->worker_size;
            }
        }
        cout << "Worker Selected: " << target_index << " with size: " << target_size << endl;

        // Create a whole list of new nodes for the input values
        HeapNode<T>* new_node = nullptr;
        for (const auto& val : values) {
            HeapNode<T>* node = new HeapNode<T>(val, target_index);
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

        // auto start = std::chrono::high_resolution_clock::now();

        // Add the new node to the root list
        workers[target_index]->insert(new_node, values.size());

        // auto end = std::chrono::high_resolution_clock::now();
        // auto duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

        // cout << "Insert time for " << values.size() << " values: " << duration_ms << " ms" << endl;

        return new_node;
    }

    int extractMin(T* & min_value) {
        // Acquire all worker mutexes to ensure a consistent view of the heap
        for (int i = 0; i < workers.size(); i++) {
            workers[i]->worker_mutex.lock();
        }

        // Find the minimum node among all workers
        vector<vector<HeapNode<T>*>> newly_consolidated(workers.size());
        vector<vector<size_t>> newly_consolidated_sizes(workers.size());
        vector<HeapNode<T>*> min_nodes(workers.size(), nullptr);

        #pragma omp parallel for num_threads(workers.size())
        for (int i = 0; i < workers.size(); i++) {
            newly_consolidated[i].resize(workers.size(), nullptr);
            newly_consolidated_sizes[i].resize(workers.size(), 0);
            workers[i]->findMin(min_nodes[i], newly_consolidated[i], newly_consolidated_sizes[i]);
        }

        HeapNode<T>* global_min_node = nullptr;
        for (int i = 0; i < workers.size(); i++) {
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

        // Rebuild the root list for each worker with the newly consolidated nodes
        #pragma omp parallel for num_threads(workers.size())
        for (int i = 0; i < workers.size(); i++) {
            workers[i]->first_root = nullptr; // Clear the current root list
            workers[i]->worker_size = 0; // Reset the worker size
            for (int j = 0; j < workers.size(); j++) {
                if (newly_consolidated[j][i] != nullptr) {
                    workers[i]->insert(newly_consolidated[j][i], newly_consolidated_sizes[j][i]); // Insert the newly consolidated node into the worker's root list, size is 0 because we are just reinserting existing nodes
                }
            }
        }

        // Reinsert the children of the minimum node into the root list of the worker that contained the minimum node
        if (global_min_node->child != nullptr) {
            HeapNode<T>* child = global_min_node->child;
            workers[global_min_node->info.root_info.worker_index]->insert(child, global_min_node->info.root_info.degree); // Insert the child into the worker's root list, size is 0 because we are just reinserting existing nodes
        }

        // Remove the minimum node
        int worker_index = global_min_node->info.root_info.worker_index;
        workers[worker_index]->remove(global_min_node);

        // Unlock all worker mutexes after the operation is done
        for (int i = workers.size() - 1; i >= 0; i--) {
            workers[i]->worker_mutex.unlock();
        }

        return 0;
    }
};
