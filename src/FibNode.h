#pragma once

struct FibNode {
    int value;
    int degree;      // Number of children
    bool marked;     // Used in decrease-key (cascading cut)
    FibNode* parent;
    FibNode* child;
    FibNode* left;
    FibNode* right;

    explicit FibNode(int val)
        : value(val), degree(0), marked(false),
          parent(nullptr), child(nullptr),
          left(nullptr), right(nullptr) {}
};
