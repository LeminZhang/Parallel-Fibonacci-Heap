#include "FineGrainedFibHeap.h"

#include <algorithm>
#include <limits>
#include <random>
#include <shared_mutex>
#include <stdexcept>
#include <thread>

namespace {

FibNode* make_dummy_node() {
    FibNode* node = new FibNode(std::numeric_limits<int>::max(), -1);
    node->is_dummy = true;
    node->in_promising = false;
    return node;
}

}  // namespace

FineGrainedFibHeap::FineGrainedFibHeap(
    size_t section_count,
    size_t promising_list_size,
    size_t spill_sections_per_pass)
    : dummy_list_(section_count == 0 ? 1 : section_count, nullptr),
      dummy_list_locks_(dummy_list_.size()),
      promising_list_(),
      size_(0),
      extract_cursor_(0),
      extracts_since_consolidate_(0),
      spill_cursor_(0),
      promising_list_size_(std::max<size_t>(1, promising_list_size)),
      spill_sections_per_pass_(std::max<size_t>(1, spill_sections_per_pass)) {
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

// ===== helper functions =====

size_t FineGrainedFibHeap::get_random_index() {
    thread_local std::mt19937 rng([] {
        const auto seed = static_cast<unsigned int>(
            std::random_device{}() ^
            static_cast<unsigned int>(std::hash<std::thread::id>{}(std::this_thread::get_id())));
        return seed;
    }());
    std::uniform_int_distribution<size_t> dist(0, dummy_list_.size() - 1);
    return dist(rng);
}

void FineGrainedFibHeap::insert_after(FibNode* anchor, FibNode* node) {
    node->left = anchor;
    node->right = anchor->right;
    anchor->right->left = node;
    anchor->right = node;
}

void FineGrainedFibHeap::insert_into_section(size_t section_index, FibNode* node) {
    node->section_index = static_cast<int>(section_index);
    insert_after(dummy_list_[section_index], node);
}

void FineGrainedFibHeap::destroy_all() {
    if (dummy_list_.empty() || dummy_list_[0] == nullptr) {
        return;
    }

    promising_list_.clear();
    std::vector<FibNode*> nodes;
    FibNode* start = dummy_list_[0];
    FibNode* current = start;
    do {
        nodes.push_back(current);
        current = current->right;
    } while (current != start);

    for (FibNode* node : nodes) {
        if (node->child != nullptr) {
            std::vector<FibNode*> children = detach_children(node->child);
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

// ===== insert-related methods =====

void FineGrainedFibHeap::resize_handle_table(int handle_id) {
    if (handle_id < 0) {
        return;
    }
    const size_t slot = static_cast<size_t>(handle_id);
    if (slot >= current_generations_.size()) {
        size_t new_size = current_generations_.empty() ? 64 : current_generations_.size();
        while (slot >= new_size) {
            new_size *= 2;
        }
        current_generations_.resize(new_size, -1);
    }
}

/**
 * Update the version of this handle id
 */
void FineGrainedFibHeap::register_inserted_handle_node(FibNode* node) {
    if (node == nullptr || node->handle_id < 0) {
        return;
    }

    std::unique_lock<std::shared_mutex> generation_lock(generation_mutex_);
    resize_handle_table(node->handle_id);
    const size_t slot = static_cast<size_t>(node->handle_id);
    if (current_generations_[slot] < 0) {
        current_generations_[slot] = node->generation;
    }
}

FibNode* FineGrainedFibHeap::insert(FibNode* node) {
    const size_t section_index = get_random_index();
    std::lock_guard<std::mutex> section_lock(dummy_list_locks_[section_index]);
    insert_into_section(section_index, node);
    register_inserted_handle_node(node);
    size_.fetch_add(1);
    return node;
}

// ===== decreaseKey-related methods =====

void FineGrainedFibHeap::decreaseKey(int handle_id, int newVal) {
    int next_generation = 0;
    {
        std::unique_lock<std::shared_mutex> generation_lock(generation_mutex_);
        const size_t slot = static_cast<size_t>(handle_id);
        if (slot >= current_generations_.size() || current_generations_[slot] < 0) {
            throw std::runtime_error("decreaseKey handle has no active node");
        }
        // Update version
        next_generation = current_generations_[slot] + 1;
        current_generations_[slot] = next_generation;
    }
    // Generate a new node with new value and lastest version
    FibNode* replacement = new FibNode(newVal, handle_id);
    replacement->generation = next_generation;
    (void)insert(replacement);
}

// ===== deleteMin-related methods =====

void FineGrainedFibHeap::detach_from_list(FibNode* node) {
    node->left->right = node->right;
    node->right->left = node->left;
    node->left = node;
    node->right = node;
}

/**
 * Determine whether or not this node is stale (has smaller version num)
 */
bool FineGrainedFibHeap::is_current_version(FibNode* node) const {
    if (node == nullptr || node->handle_id < 0) {
        return true;
    }

    std::shared_lock<std::shared_mutex> generation_lock(generation_mutex_);
    const size_t slot = static_cast<size_t>(node->handle_id);
    if (slot >= current_generations_.size()) {
        return false;
    }
    return current_generations_[slot] >= 0 && node->generation == current_generations_[slot];
}

/**
 * Handle deletion of node in current_generations
 */
void FineGrainedFibHeap::retire_handle(FibNode* node) {
    if (node == nullptr || node->handle_id < 0) {
        return;
    }

    std::unique_lock<std::shared_mutex> generation_lock(generation_mutex_);
    const size_t slot = static_cast<size_t>(node->handle_id);
    if (slot >= current_generations_.size()) {
        return;
    }
    // Reset the version of this handle_id
    if (current_generations_[slot] == node->generation) {
        current_generations_[slot] = -1;
    }
}

/**
 * Detach an entire child list and reset the nodes to root-ready state.
 */
std::vector<FibNode*> FineGrainedFibHeap::detach_children(FibNode* child_start) {
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

std::vector<FibNode*> FineGrainedFibHeap::collect_section_roots(size_t section_index) {
    std::vector<FibNode*> roots;
    FibNode* section_dummy = dummy_list_[section_index];
    FibNode* section_end = dummy_list_[(section_index + 1) % dummy_list_.size()];
    FibNode* current = section_dummy->right;
    while (current != section_end) {
        FibNode* next = current->right;
        if (!current->is_dummy) {
            detach_from_list(current);
            current->section_index = -1;
            roots.push_back(current);
        }
        current = next;
    }
    return roots;
}

/**
 * Restore a section dummy node so it represents an empty section boundary.
 */
void FineGrainedFibHeap::clear_section(size_t section_index) {
    const size_t n = dummy_list_.size();
    FibNode* current = dummy_list_[section_index];
    current->left = dummy_list_[(section_index + n - 1) % n];
    current->right = dummy_list_[(section_index + 1) % n];
}

/**
 * Remove and return one valid candidate from the promising list.
 */
FibNode* FineGrainedFibHeap::try_take_promising_node() {
    std::lock_guard<std::mutex> promising_lock(promising_mutex_);
    if (promising_list_.empty()) {
        return nullptr;
    }

    for (size_t offset = 0; offset < promising_list_.size(); ++offset) {
        const size_t index = (extract_cursor_ + offset) % promising_list_.size();
        FibNode* candidate = promising_list_[index];
        if (candidate != nullptr && candidate->parent == nullptr && candidate->section_index >= 0) {
            promising_list_[index] = nullptr;
            candidate->in_promising = false;
            extract_cursor_ = (index + 1) % promising_list_.size();
            return candidate;
        }
        if (candidate != nullptr) {
            candidate->in_promising = false;
            promising_list_[index] = nullptr;
        }
    }
    return nullptr;
}

/**
 * Promising node clean up, remove an existing node from list
 */
void FineGrainedFibHeap::invalidate_promising_node(FibNode* node) {
    std::lock_guard<std::mutex> promising_lock(promising_mutex_);
    for (FibNode*& candidate : promising_list_) {
        if (candidate == node) {
            candidate->in_promising = false;
            candidate = nullptr;
        }
    }
}

/**
 * Fill up promising list with small nodes from random sections
 */
void FineGrainedFibHeap::spill_over() {
    std::lock_guard<std::mutex> promising_lock(promising_mutex_);
    // 3PLS, same as the paper
    if (promising_list_.size() < promising_list_size_ * 3) {
        promising_list_.assign(promising_list_size_ * 3, nullptr);
    }

    std::vector<size_t> empty_slots;
    empty_slots.reserve(promising_list_.size());
    for (size_t i = 0; i < promising_list_.size(); ++i) {
        FibNode* candidate = promising_list_[i];
        if (candidate == nullptr || candidate->parent != nullptr || candidate->section_index < 0) {
            if (candidate != nullptr) {
                candidate->in_promising = false;
            }
            promising_list_[i] = nullptr;
            empty_slots.push_back(i);
        }
    }

    if (empty_slots.empty()) {
        return;
    }

    std::vector<FibNode*> in_progress;
    in_progress.reserve(empty_slots.size());

    const size_t sections_to_scan =
        std::min(dummy_list_.size(), spill_sections_per_pass_);
    // Scan over some sections and get min nodes from there
    for (size_t offset = 0; offset < sections_to_scan; ++offset) {
        const size_t section_index = (spill_cursor_ + offset) % dummy_list_.size();
        std::lock_guard<std::mutex> section_lock(dummy_list_locks_[section_index]);

        FibNode* section_dummy = dummy_list_[section_index];
        FibNode* section_end = dummy_list_[(section_index + 1) % dummy_list_.size()];
        FibNode* current = section_dummy->right;

        while (current != section_end) {
            if (!current->is_dummy &&
                current->parent == nullptr &&
                current->section_index == static_cast<int>(section_index) &&
                !current->in_promising &&
                std::find(in_progress.begin(), in_progress.end(), current) == in_progress.end()) {
                auto pos = std::lower_bound(
                    in_progress.begin(), in_progress.end(), current,
                    [](const FibNode* a, const FibNode* b) {
                        return a->value < b->value;
                    });
                in_progress.insert(pos, current);
                if (in_progress.size() > empty_slots.size()) {
                    in_progress.pop_back();
                }
            }
            current = current->right;
        }
    }

    spill_cursor_ = (spill_cursor_ + sections_to_scan) % dummy_list_.size();

    const size_t fill_count = std::min(in_progress.size(), empty_slots.size());
    for (size_t i = 0; i < fill_count; ++i) {
        promising_list_[empty_slots[i]] = in_progress[i];
        in_progress[i]->in_promising = true;
    }
    if (!promising_list_.empty()) {
        extract_cursor_ %= promising_list_.size();
    }
}

void FineGrainedFibHeap::link(FibNode* y, FibNode* x) {
    detach_from_list(y);
    y->parent = x;
    y->section_index = -1;
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

/**
 * Merge roots of equal degree inside one section and rebuild that section.
 *
 * This is a local version of Fibonacci-heap consolidation: it only touches
 * one section, merges detached roots by degree, and puts the surviving roots
 * back into the same section.
 */
void FineGrainedFibHeap::consolidate_section(size_t section_index) {
    std::lock_guard<std::mutex> section_lock(dummy_list_locks_[section_index]);
    std::vector<FibNode*> roots = collect_section_roots(section_index);
    clear_section(section_index);

    if (roots.empty()) {
        return;
    }

    // Standard degree-table consolidation, but scoped to one section.
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

    for (FibNode* root : table) {
        if (root == nullptr) {
            continue;
        }
        // Reinsert the merged roots so later spill-over can see them.
        root->parent = nullptr;
        root->marked = false;
        insert_into_section(section_index, root);
    }
}

void FineGrainedFibHeap::consolidate_random_section() {
    if (dummy_list_.empty()) {
        return;
    }
    consolidate_section(get_random_index());
    extracts_since_consolidate_ = 0;
}

/**
 * Return one logical deleteMin result under the relaxed sectioned design.
 *
 * We do not maintain one exact global minimum. Instead, deleteMin first tries
 * the promising list, then refills it with spill-over, and finally performs a
 * local section consolidation if the candidate pool is still empty. Stale
 * versions created by reinsertion-based decreaseKey are removed internally and
 * are not returned as user-visible deletes.
 */
DeleteMinResult FineGrainedFibHeap::deleteMin() {
    while (true) {
        std::unique_lock<std::shared_mutex> coord_lock(coord_mutex_);
        if (size() == 0) {
            throw std::runtime_error("Heap is empty");
        }

        // Try the current candidate pool first, then progressively do more
        // maintenance if the pool cannot provide a root.
        FibNode* z = try_take_promising_node();
        if (z == nullptr) {
            // Refill if empty
            spill_over();
            z = try_take_promising_node();
            if (z == nullptr) {
                // Consolidate and refill
                consolidate_random_section();
                z = try_take_promising_node();
            }
        }

        if (z == nullptr) {
            continue;
        }

        invalidate_promising_node(z);
        // A structurally valid root can still be an obsolete version that
        // was superseded by a later decreaseKey reinsertion.
        const bool stale_version = !is_current_version(z);

        if (z->section_index < 0) {
            throw std::runtime_error("Promising node is no longer a root");
        }
        const size_t z_section_index = static_cast<size_t>(z->section_index);
        {
            // Remove z while holding its owning section lock so root-list
            // updates stay locally consistent.
            std::lock_guard<std::mutex> z_section_lock(dummy_list_locks_[z_section_index]);

            if (z->child != nullptr) {
                // Children become new roots and are redistributed across
                // sections instead of being appended to one global root list.
                std::vector<FibNode*> children = detach_children(z->child);
                z->child = nullptr;
                z->degree = 0;

                for (FibNode* child : children) {
                    const size_t section_index = get_random_index();
                    if (section_index == z_section_index) {
                        insert_into_section(section_index, child);
                    } else {
                        std::lock_guard<std::mutex> section_lock(dummy_list_locks_[section_index]);
                        insert_into_section(section_index, child);
                    }
                }
            }

            detach_from_list(z);
            z->section_index = -1;
        }

        DeleteMinResult result{z->value, z->handle_id};
        retire_handle(z);
        delete z;
        size_.fetch_sub(1);

        // Old versions created by decreaseKey are cleaned up lazily here.
        if (stale_version) {
            coord_lock.unlock();
            continue;
        }

        bool need_consolidate = false;
        bool need_spill = false;

        if (size() == 0) {
            // Once no physical nodes remain, the auxiliary candidate state can
            // be reset immediately.
            std::fill(promising_list_.begin(), promising_list_.end(), nullptr);
            extract_cursor_ = 0;
            extracts_since_consolidate_ = 0;
            return result;
        }

        // Periodic consolidation repairs local degree structure; spill-over
        // refills the candidate pool when too few live candidates remain.
        ++extracts_since_consolidate_;
        if (extracts_since_consolidate_ >= promising_list_size_) {
            need_consolidate = true;
        }

        size_t live_promising = 0;
        {
            std::lock_guard<std::mutex> promising_lock(promising_mutex_);
            live_promising =
                static_cast<size_t>(std::count_if(promising_list_.begin(), promising_list_.end(),
                    [](FibNode* node) { return node != nullptr; }));
        }
        const size_t refill_threshold = std::max<size_t>(1, promising_list_size_ / 3);
        if (live_promising < refill_threshold) {
            need_spill = true;
        }

        if (need_spill) {
            spill_over();
        } else if (need_consolidate) {
            consolidate_random_section();
            extracts_since_consolidate_ = 0;
        }

        return result;
    }
}
