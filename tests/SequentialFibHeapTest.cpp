#include "../src/SequentialFibHeap.h"

#include <cassert>
#include <iostream>
#include <stdexcept>

namespace {

void test_insert_and_delete_min_orders_values() {
    SequentialFibHeap heap;
    heap.insert(new FibNode(7, 0));
    heap.insert(new FibNode(3, 1));
    heap.insert(new FibNode(10, 2));
    heap.insert(new FibNode(1, 3));

    assert(!heap.isEmpty());
    assert(heap.size() == 4);
    assert(heap.min() != nullptr);
    assert(heap.min()->value == 1);

    assert(heap.deleteMin().value == 1);
    assert(heap.deleteMin().value == 3);
    assert(heap.deleteMin().value == 7);
    assert(heap.deleteMin().value == 10);
    assert(heap.isEmpty());
    assert(heap.size() == 0);
}

void test_decrease_key_updates_minimum() {
    SequentialFibHeap heap;

    FibNode* a = heap.insert(new FibNode(20, 0));
    FibNode* b = heap.insert(new FibNode(8, 1));
    FibNode* c = heap.insert(new FibNode(15, 2));

    assert(heap.min() == b);

    heap.decreaseKey(a, 2);
    assert(heap.min() == a);
    assert(heap.min()->value == 2);

    assert(heap.deleteMin().value == 2);
    assert(heap.deleteMin().value == 8);
    assert(heap.deleteMin().value == 15);
    assert(heap.isEmpty());

    (void)c;
}

}

int main() {
    test_insert_and_delete_min_orders_values();
    test_decrease_key_updates_minimum();

    std::cout << "SequentialFibHeap tests passed.\n";
    return 0;
}
