#include "FineGrainedFibHeap.h"

#include <algorithm>
#include <limits>
#include <shared_mutex>
#include <stdexcept>

namespace {

FibNode* make_dummy_node() {
    FibNode* node = new FibNode(std::numeric_limits<int>::max(), -1);
    node->is_dummy = true;
    return node;
}

}  // namespace

FineGrainedFibHeap::FineGrainedFibHeap(size_t section_count)
    : dummy_list_(section_count == 0 ? 1 : section_count, nullptr),
      dummy_list_locks_(dummy_list_.size()),
      size_(0),
      min_node_(nullptr),
      rng_(std::random_device{}()) {
    for (size_t i = 0; i < dummy_list_.size(); ++i) {
        dummy_list_[i] = make_dummy_node();
    }

    const size_t n = dummy_list_.size();
    for (size_t i = 0; i < n; ++i) {
        FibNode* current = dummy_list_[i];
        current->left = dummy_list_[(i + n - 1) % n];
        current->right = dummy_list_[(i + 1) % n];
    }
}

FineGrainedFibHeap::~FineGrainedFibHeap() {
    destroy_all();
}

size_t FineGrainedFibHeap::size() const {
    return size_.load();
}

bool FineGrainedFibHeap::isEmpty() const {
    return size() == 0;
}

FibNode* FineGrainedFibHeap::min() const {
    std::shared_lock<std::shared_mutex> coord_lock(coord_mutex_);
    std::lock_guard<std::mutex> min_lock(min_mutex_);
    return min_node_;
}

size_t FineGrainedFibHeap::get_random_index() {
    std::lock_guard<std::mutex> lock(rng_mutex_);
    std::uniform_int_distribution<size_t> dist(0, dummy_list_.size() - 1);
    return dist(rng_);
}

void FineGrainedFibHeap::insert_after(FibNode* anchor, FibNode* node) {
    node->left = anchor;
    node->right = anchor->right;
    anchor->right->left = node;
    anchor->right = node;
}

void FineGrainedFibHeap::detach_from_list(FibNode* node) {
    node->left->right = node->right;
    node->right->left = node->left;
    node->left = node;
    node->right = node;
}

std::vector<FibNode*> FineGrainedFibHeap::collect_children(FibNode* child_start) {
    std::vector<FibNode*> children;
    if (child_start == nullptr) {
        return children;
    }

    FibNode* current = child_start;
    do {
        children.push_back(current);
        current = current->right;
    } while (current != child_start);

    for (FibNode* child : children) {
        detach_from_list(child);
        child->parent = nullptr;
        child->marked = false;
    }
    return children;
}

std::vector<FibNode*> FineGrainedFibHeap::collect_all_roots() {
    std::vector<FibNode*> roots;
    for (size_t i = 0; i < dummy_list_.size(); ++i) {
        FibNode* section_dummy = dummy_list_[i];
        FibNode* section_end = dummy_list_[(i + 1) % dummy_list_.size()];
        FibNode* current = section_dummy->right;
        while (current != section_end) {
            FibNode* next = current->right;
            if (!current->is_dummy) {
                detach_from_list(current);
                roots.push_back(current);
            }
            current = next;
        }
    }
    return roots;
}

void FineGrainedFibHeap::clear_root_sections() {
    const size_t n = dummy_list_.size();
    for (size_t i = 0; i < n; ++i) {
        FibNode* current = dummy_list_[i];
        current->left = dummy_list_[(i + n - 1) % n];
        current->right = dummy_list_[(i + 1) % n];
    }
}

FibNode* FineGrainedFibHeap::insert(int handle_id, int value) {
    std::shared_lock<std::shared_mutex> coord_lock(coord_mutex_);

    FibNode* node = new FibNode(value, handle_id);
    size_t section_index = 0;
    do {
        section_index = get_random_index();
    } while (!dummy_list_locks_[section_index].try_lock());

    FibNode* section_dummy = dummy_list_[section_index];
    insert_after(section_dummy, node);
    dummy_list_locks_[section_index].unlock();

    {
        std::lock_guard<std::mutex> min_lock(min_mutex_);
        if (min_node_ == nullptr || node->value < min_node_->value) {
            min_node_ = node;
        }
    }

    size_.fetch_add(1);
    return node;
}

void FineGrainedFibHeap::decreaseKey(FibNode* node, int newVal) {
    std::unique_lock<std::shared_mutex> coord_lock(coord_mutex_);
    if (node == nullptr) {
        return;
    }

    node->value = newVal;
    FibNode* parent = node->parent;
    if (parent != nullptr && node->value < parent->value) {
        cut(node, parent);
        cascading_cut(parent);
    }

    std::lock_guard<std::mutex> min_lock(min_mutex_);
    if (min_node_ == nullptr || node->value < min_node_->value) {
        min_node_ = node;
    }
}

DeleteMinResult FineGrainedFibHeap::deleteMin() {
    std::unique_lock<std::shared_mutex> coord_lock(coord_mutex_);
    if (size() == 0 || min_node_ == nullptr) {
        throw std::runtime_error("Heap is empty");
    }

    FibNode* z = min_node_;
    DeleteMinResult result{z->value, z->handle_id};

    if (z->child != nullptr) {
        std::vector<FibNode*> children = collect_children(z->child);
        z->child = nullptr;
        z->degree = 0;

        size_t section_index = 0;
        for (FibNode* child : children) {
            std::lock_guard<std::mutex> section_lock(dummy_list_locks_[section_index]);
            insert_after(dummy_list_[section_index], child);
            section_index = (section_index + 1) % dummy_list_.size();
        }
    }

    detach_from_list(z);
    delete z;
    size_.fetch_sub(1);

    if (size() == 0) {
        std::lock_guard<std::mutex> min_lock(min_mutex_);
        min_node_ = nullptr;
        return result;
    }

    consolidate_all();
    rebuild_min_from_roots();
    return result;
}

void FineGrainedFibHeap::rebuild_min_from_roots() {
    FibNode* next_min = nullptr;
    for (size_t i = 0; i < dummy_list_.size(); ++i) {
        FibNode* section_dummy = dummy_list_[i];
        FibNode* section_end = dummy_list_[(i + 1) % dummy_list_.size()];
        FibNode* current = section_dummy->right;
        while (current != section_end) {
            if (!current->is_dummy &&
                (next_min == nullptr || current->value < next_min->value)) {
                next_min = current;
            }
            current = current->right;
        }
    }

    std::lock_guard<std::mutex> min_lock(min_mutex_);
    min_node_ = next_min;
}

void FineGrainedFibHeap::consolidate_all() {
    std::vector<FibNode*> roots = collect_all_roots();
    clear_root_sections();

    std::vector<FibNode*> table(16, nullptr);
    for (FibNode* root : roots) {
        FibNode* x = root;
        int degree = x->degree;
        while (degree >= static_cast<int>(table.size())) {
            table.resize(table.size() * 2, nullptr);
        }
        while (table[degree] != nullptr) {
            FibNode* y = table[degree];
            if (y->value < x->value) {
                std::swap(x, y);
            }
            link(y, x);
            table[degree] = nullptr;
            ++degree;
            while (degree >= static_cast<int>(table.size())) {
                table.resize(table.size() * 2, nullptr);
            }
        }
        table[degree] = x;
    }

    size_t section_index = 0;
    for (FibNode* root : table) {
        if (root == nullptr) {
            continue;
        }
        std::lock_guard<std::mutex> section_lock(dummy_list_locks_[section_index]);
        insert_after(dummy_list_[section_index], root);
        root->parent = nullptr;
        root->marked = false;
        section_index = (section_index + 1) % dummy_list_.size();
    }
}

void FineGrainedFibHeap::link(FibNode* y, FibNode* x) {
    detach_from_list(y);
    y->parent = x;
    if (x->child == nullptr) {
        x->child = y;
        y->left = y;
        y->right = y;
    } else {
        insert_after(x->child, y);
    }
    x->degree++;
    y->marked = false;
}

void FineGrainedFibHeap::cut(FibNode* node, FibNode* parent) {
    if (node->right == node) {
        parent->child = nullptr;
    } else {
        if (parent->child == node) {
            parent->child = node->right;
        }
        node->left->right = node->right;
        node->right->left = node->left;
    }
    parent->degree--;

    size_t section_index = get_random_index();
    std::lock_guard<std::mutex> section_lock(dummy_list_locks_[section_index]);
    insert_after(dummy_list_[section_index], node);
    node->parent = nullptr;
    node->marked = false;
}

void FineGrainedFibHeap::cascading_cut(FibNode* node) {
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

void FineGrainedFibHeap::destroy_all() {
    if (dummy_list_.empty() || dummy_list_[0] == nullptr) {
        return;
    }

    std::vector<FibNode*> nodes;
    FibNode* start = dummy_list_[0];
    FibNode* current = start;
    do {
        nodes.push_back(current);
        current = current->right;
    } while (current != start);

    for (FibNode* node : nodes) {
        if (node->child != nullptr) {
            std::vector<FibNode*> children = collect_children(node->child);
            for (FibNode* child : children) {
                delete child;
            }
        }
        delete node;
    }

    for (FibNode*& dummy : dummy_list_) {
        dummy = nullptr;
    }
}
