#include "../src/SequentialFibHeap.h"

#include <cassert>
#include <iostream>
#include <stdexcept>

namespace {

void test_insert_and_delete_min_orders_values() {
    SequentialFibHeap heap;
    heap.insert(7);
    heap.insert(3);
    heap.insert(10);
    heap.insert(1);

    assert(!heap.isEmpty());
    assert(heap.size() == 4);
    assert(heap.min() != nullptr);
    assert(heap.min()->value == 1);

    assert(heap.deleteMin() == 1);
    assert(heap.deleteMin() == 3);
    assert(heap.deleteMin() == 7);
    assert(heap.deleteMin() == 10);
    assert(heap.isEmpty());
    assert(heap.size() == 0);
}

void test_decrease_key_updates_minimum() {
    SequentialFibHeap heap;

    FibNode* a = heap.insert(20);
    FibNode* b = heap.insert(8);
    FibNode* c = heap.insert(15);

    assert(heap.min() == b);

    heap.decreaseKey(a, 2);
    assert(heap.min() == a);
    assert(heap.min()->value == 2);

    assert(heap.deleteMin() == 2);
    assert(heap.deleteMin() == 8);
    assert(heap.deleteMin() == 15);
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
