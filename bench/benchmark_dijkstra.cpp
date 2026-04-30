#include "../src/Dijkstra.h"
#include <iostream>
#include <chrono>

using namespace std;


int main() {
    int num_nodes[] = {5000, 10000, 15000, 20000}; // Adjust the number of nodes as needed
    int num_threads[] = {2, 4, 6, 8}; // Adjust the number of threads as needed
    for (int i = 0; i < 4; i++) {
        Graph graph(num_nodes[i], 1);
        auto start = chrono::high_resolution_clock::now();
        graph.calculate_distances(0);
        auto end = chrono::high_resolution_clock::now();
        auto baseline_duration_ms = chrono::duration<double, milli>(end - start).count();
        cout << "Dijkstra's algorithm with " << num_nodes[i] << " nodes and 1 thread took " << baseline_duration_ms << " ms." << endl;
        for (int j = 0; j < 4; j++) {
            Graph graph(num_nodes[i], num_threads[j]);
            auto start = chrono::high_resolution_clock::now();
            graph.calculate_distances(0);
            auto end = chrono::high_resolution_clock::now();
            auto duration_ms = chrono::duration<double, milli>(end - start).count();
            cout << "Dijkstra's algorithm with " << num_nodes[i] << " nodes and " << num_threads[j] << " threads took " << duration_ms << " ms." << "speedup: " << baseline_duration_ms / duration_ms << endl;
        }
    }

    return 0;
}