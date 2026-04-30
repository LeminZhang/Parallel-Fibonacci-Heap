#include "Dijkstra.h"
#include "ParallelFibHeap.h"
#define INF 1e9

Node::Node(int id) : id(id), distance(INF) {}

bool Node::operator<(const Node& other) const {
    return distance < other.distance;
}

Graph::Graph(int num_nodes, int num_threads) : num_threads(num_threads) {
    for (int i = 0; i < num_nodes; i++) {
        nodes.emplace_back(i);
    }

    for (int i = 0; i < num_nodes; i++) {
        for (int j = 0; j < num_nodes; j++) {
            if (i != j) {
                nodes[i].edges.push_back({&nodes[j], rand() % 10000 + 1}); // Random weight between 1 and 100
            }
        }
    }
}

void Graph::calculate_distances(int start_id) {
    ParallelFibHeap<Node> heap(num_threads); // Using num_threads threads for the heap
    vector<Node*> node_ptrs;
    for (auto& node : nodes) {
        node_ptrs.push_back(&node);
    }
    vector<HeapNode<Node>*> heap_nodes; 
    heap.insert(node_ptrs, heap_nodes);

    nodes[start_id].distance = 0; // Set the distance of the starting node to 0
    heap.decreaseKey(heap_nodes[start_id]); // Update the distance in the heap


    while (!heap.isEmpty()) {
        Node* nearest_node;
        heap.extractMin(nearest_node); 
        nearest_node->distance_calculated = true; // Mark the node as having its distance calculated

        #pragma omp parallel for num_threads(num_threads)
        for (const auto& edge : nearest_node->edges) {
            parallel_affinity::pin_current_thread_to_efficiency_core();
            if (edge.to->distance_calculated) {
                continue; // Skip if the distance to this node has already been calculated
            }
            long long new_distance = nearest_node->distance + edge.weight;
            if (new_distance < edge.to->distance) {
                // Copy node
                heap.obtainMutexesForDecreaseKey(heap_nodes[edge.to->id]); // Obtain necessary mutexes for decrease key operation
            }
        }
        
        #pragma omp parallel for num_threads(num_threads)
        for (const auto& edge : nearest_node->edges) {
            parallel_affinity::pin_current_thread_to_efficiency_core();
            if (edge.to->distance_calculated) {
                continue; // Skip if the distance to this node has already been calculated
            }
            long long new_distance = nearest_node->distance + edge.weight;
            if (new_distance < edge.to->distance) {
                // Copy node
                edge.to->distance = new_distance; // Update the distance of the neighboring node
                heap.decreaseKey(heap_nodes[edge.to->id]); // Update the distance in the heap
            }
        }
    }
}

long long Graph::get_distance(int node_id) const {
    return nodes[node_id].distance;
}