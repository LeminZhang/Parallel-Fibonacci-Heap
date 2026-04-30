#include <vector>

using namespace std;

class Node {
    struct Edge {
        Node * to;
        int weight;
    };
public:
    int id;
    vector<Edge> edges;
    bool distance_calculated = false;
    long long distance;
    Node(int id);
    bool operator<(const Node& other) const;
};

class Graph {
    vector<Node> nodes;
    int num_threads;
public:
    Graph(int num_nodes, int num_threads);
    Graph(const Graph&) = default;
    void calculate_distances(int start_id);
    long long get_distance(int node_id) const;
};
