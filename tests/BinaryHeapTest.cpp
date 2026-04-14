#include "../src/BinaryHeap.h"

#include <cassert>
#include <iostream>
#include <stdexcept>

namespace {

void test_insert_and_delete_min_orders_values() {
    BinaryHeap heap;
    heap.insert(0, 7);
    heap.insert(1, 3);
    heap.insert(2, 10);
    heap.insert(3, 1);

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
    BinaryHeap heap;

    BinaryHeapNode* a = heap.insert(0, 20);
    BinaryHeapNode* b = heap.insert(1, 8);
    BinaryHeapNode* c = heap.insert(2, 15);

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
