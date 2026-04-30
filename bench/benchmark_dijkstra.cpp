#include "../src/Dijkstra.h"
#include <iostream>
#include <chrono>

using namespace std;


int main() {
    int num_nodes = 20000; // Adjust the number of nodes as needed
    int num_threads = 4; // Adjust the number of threads as needed
    Graph graph(num_nodes, num_threads);
    auto start = chrono::high_resolution_clock::now();
    graph.calculate_distances(0); // Calculate distances from node 0
    auto end = chrono::high_resolution_clock::now();
    auto duration_ms = chrono::duration<double, milli>(end - start).count();
    cout << "Dijkstra's algorithm completed in " << duration_ms << " ms" << endl;


    // Print distances to all nodes
    for (int i = 0; i < num_nodes; i++) {
        // cout << "Distance from node 0 to node " << i << ": " << graph.get_distance(i) << endl;
    }

    return 0;
}