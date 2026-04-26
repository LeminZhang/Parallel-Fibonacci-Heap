#pragma once

struct FibNode {
    int value;
    int handle_id;   // For testing purpose, give each node ID
    int generation;  // Version used by reinsertion-based decreaseKey
    int degree;      // Number of children
    int section_index;  // Fine-grained root-list section ownership, -1 if not on a root section
    bool in_promising;  // Whether the node is currently tracked in the promising list
    bool marked;     // Used in decrease-key (cascading cut)
    bool is_dummy;   // Used by fine-grained root-list sections
    FibNode* parent;
    FibNode* child;
    FibNode* left;
    FibNode* right;

    explicit FibNode(int val, int handle = -1)
        : value(val), handle_id(handle), generation(0), degree(0), section_index(-1), in_promising(false),
          marked(false), is_dummy(false),
          parent(nullptr), child(nullptr),
          left(nullptr), right(nullptr) {}
};
