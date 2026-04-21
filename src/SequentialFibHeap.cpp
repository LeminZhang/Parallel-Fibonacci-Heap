#include "SequentialFibHeap.h"
#include <vector>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <list>

SequentialFibHeap::SequentialFibHeap()
    : min_node_(nullptr), size_(0) {}

SequentialFibHeap::~SequentialFibHeap() {
    if (min_node_) destroy_all(min_node_);
}

bool   SequentialFibHeap::isEmpty() const { return size_ == 0; }
size_t SequentialFibHeap::size()    const { return size_; }
FibNode* SequentialFibHeap::min() const { return min_node_; }

/**
 * insert: O(1) amortized
 * Allocate space for a new Node, insert it after min
 */
FibNode* SequentialFibHeap::insert(int handle_id, int value) {
    // FibNode* node = new FibNode(value, handle_id);

    // // 线程局部池（每个线程一份）
    thread_local std::list<FibNode> tls_pool;

    // 在线程局部池中就地构造一个节点
    tls_pool.emplace_back(value, handle_id);
    FibNode* node = &tls_pool.back(); // 地址稳定

    if (min_node_ == nullptr) {
        node->left = node;
        node->right = node;
        min_node_ = node;
    } else {
        // Add to the right of min node
        node->left = min_node_;
        node->right = min_node_->right;
        min_node_->right->left = node;
        min_node_->right = node;
        if (node->value < min_node_->value)
            min_node_ = node;
    }
    size_++;
    return node;
}

/**
 * deleteMin: O(log n) amortized
 * Delete the min node, move its children to root list and consolidate
 */
DeleteMinResult SequentialFibHeap::deleteMin() {
    if (!min_node_) throw std::runtime_error("Heap is empty");
    FibNode* z = min_node_;
    DeleteMinResult result{z->value, z->handle_id};

    // Promote all children of z into the root list
    if (z->child) {
        std::vector<FibNode*> children;
        FibNode* child = z->child;
        do {
            children.push_back(child);
            child = child->right;
        } while (child != z->child);

        for (FibNode* current : children) {
            current->left->right = current->right;
            current->right->left = current->left;
            current->left = min_node_;
            current->right = min_node_->right;
            min_node_->right->left = current;
            min_node_->right = current;
            current->parent = nullptr;
        }
    }

    // Remove z from root list
    z->left->right = z->right;
    z->right->left = z->left;

    if (z == z->right) {
        min_node_ = nullptr;
    } else {
        min_node_ = z->right;
        consolidate();
    }

    size_--;
    delete z;
    return result;
}

/**
 * decreaseKey: O(1) amortized
 * Decrease key of given node, cut node if necessary
 */
void SequentialFibHeap::decreaseKey(FibNode* node, int newVal) {
    node->value = newVal;
    FibNode* parent = node->parent;
    if (parent != nullptr && node->value < parent->value) {
        cut(node, parent);
        cascading_cut(parent);
    }
    if (node->value < min_node_->value)
        min_node_ = node;
}

// ─── Private ─────────────────────────────────────────────────────────────────

void SequentialFibHeap::consolidate() {
    int max_deg = static_cast<int>(std::log2(static_cast<double>(size_))) + 2;
    std::vector<FibNode*> table(max_deg + 1, nullptr);

    std::vector<FibNode*> roots;
    FibNode* curr = min_node_;
    do {
        roots.push_back(curr);
        curr = curr->right;
    } while (curr != min_node_);

    for (FibNode* w : roots) {
        FibNode* x = w;
        int d = x->degree;
        while (d < (int)table.size() && table[d] != nullptr) {
            FibNode* y = table[d];
            if (y->value < x->value) std::swap(x, y);
            link(y, x);
            table[d] = nullptr;
            d++;
            if (d >= (int)table.size())
                table.resize(d + 1, nullptr);
        }
        table[d] = x;
    }

    min_node_ = nullptr;
    for (FibNode* node : table) {
        if (!node) continue;
        if (min_node_ == nullptr) {
            node->left  = node;
            node->right = node;
            min_node_   = node;
        } else {
            node->left = min_node_;
            node->right = min_node_->right;
            min_node_->right->left = node;
            min_node_->right = node;
            if (node->value < min_node_->value)
                min_node_ = node;
        }
    }
}

void SequentialFibHeap::link(FibNode* y, FibNode* x) {
    y->left->right = y->right;
    y->right->left = y->left;

    y->parent = x;
    if (x->child == nullptr) {
        x->child = y;
        y->left = y;
        y->right = y;
    } else {
        y->left = x->child;
        y->right = x->child->right;
        x->child->right->left = y;
        x->child->right = y;
    }
    x->degree++;
    y->marked = false;
}

// Cut node from its parent and move it to the root list
void SequentialFibHeap::cut(FibNode* node, FibNode* parent) {
    // Remove node from parent's child list
    if (node->right == node) {
        parent->child = nullptr;
    } else {
        node->left->right = node->right;
        node->right->left = node->left;
        if (parent->child == node)
            parent->child = node->right;
    }
    parent->degree--;

    // Add node to root list
    node->left = min_node_;
    node->right = min_node_->right;
    min_node_->right->left = node;
    min_node_->right = node;
    node->parent = nullptr;
    node->marked = false;
}

// Cascading cut: if parent is marked, cut it too and recurse upward
void SequentialFibHeap::cascading_cut(FibNode* node) {
    FibNode* parent = node->parent;
    if (parent != nullptr) {
        if (!node->marked) {
            node->marked = true;
        } else {
            cut(node, parent);
            cascading_cut(parent);
        }
    }
}

void SequentialFibHeap::destroy_all(FibNode* start) {
    std::vector<FibNode*> nodes;
    FibNode* curr = start;
    do {
        nodes.push_back(curr);
        curr = curr->right;
    } while (curr != start);

    for (FibNode* node : nodes) {
        if (node->child) destroy_all(node->child);
        delete node;
    }
}
