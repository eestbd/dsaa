#include "Graph.h"
#include <iomanip>
#include <queue>
#include <tuple>

using namespace std;

uint64_t Graph::read_count = 0;
Graph::Graph() : map(nullptr) {}

Graph::~Graph() {
  if (map != nullptr) {
    for (int i = 0; i < graph_size; ++i) {
      if (map[i] != nullptr) {
        delete[] map[i];
      }
    }
    delete[] map;
    map = nullptr;
  }
}

/*
 * readFile: Reads data from file and parse them into size and map
 */

void Graph::submit_node(int node) { subgraph_nodes.push_back(node); }

void Graph::readFile(string &filename) {
  ifstream file;
  file.open(filename);
  assert(file.is_open() && "Given file does not exist!\n");
  file >> graph_size;
  file >> subgraph_size;
  file >> alpha_d;
  cout << filename << " contains " << graph_size
       << " nodes with target subgraph size " << subgraph_size << endl;
  string test;
  map = new int *[graph_size];
  for (int i = 0; i < graph_size; i++) {
    map[i] = new int[graph_size];
    for (int j = 0; j < graph_size; j++) {
      file >> map[i][j];
    }
  }
  file.close();
}

/*
 * printGraph: Prints out the entire map.
 * For big data, it may incurr huge burden on the shell
 */
void Graph::printGraph() {
  for (int i = 0; i < graph_size; i++) {
    for (int j = 0; j < graph_size; j++) {
      cout << setw(3) << map[i][j] << " ";
    }
    cout << endl;
  }
}

/*
 * read_map: Map reading API that can be used by students
 * Since map variable is protected, students should use this API to read the
 * data from graph map
 */
int Graph::read_map(int row, int col) {
  if (row < 0 || row >= graph_size || col < 0 || col >= graph_size) {
    cerr << "Error: map index out of bounds (" << row << ", " << col << ")"
         << endl;
    return 0; // Return 0 to prevent crash but indicate no edge
  }
  read_count++;
  return map[row][col];
}

// get_graph_size: Returns the size of whole graph
int Graph::get_graph_size() { return graph_size; }

// helper for disjoint set
struct UnionFind {
  vector<int> parent;
  UnionFind(int n) {
    parent.resize(n);
    for (int i = 0; i < n; ++i)
      parent[i] = i;
  }
  int find(int i) {
    if (parent[i] == i)
      return i;
    return parent[i] = find(parent[i]);
  }
  void unite(int i, int j) {
    int root_i = find(i);
    int root_j = find(j);
    if (root_i != root_j) {
      parent[root_i] = root_j;
    }
  }
};

// calc_subgraph: Calculates the Score of MMST
int Graph::calc_subgraph() {
  int accumulated_node = 0;
  for (int node : subgraph_nodes) {
    accumulated_node += map[node][node];
  }

  // Find MST edges using Kruskal's
  vector<tuple<int, int, int>> edges; // weight, u_idx, v_idx
  for (size_t i = 0; i < subgraph_nodes.size(); ++i) {
    for (size_t j = i + 1; j < subgraph_nodes.size(); ++j) {
      int u = subgraph_nodes[i];
      int v = subgraph_nodes[j];
      if (map[u][v] != 0) {
        edges.push_back({map[u][v], i, j});
      }
    }
  }

  sort(edges.begin(), edges.end()); // sorts by weight ascending

  UnionFind uf(subgraph_nodes.size());
  int edge_weight_sum = 0;
  int edge_count = 0;

  for (auto &edge : edges) {
    int w = get<0>(edge);
    int u = get<1>(edge);
    int v = get<2>(edge);

    if (uf.find(u) != uf.find(v)) {
      uf.unite(u, v);
      edge_weight_sum += w;
      edge_count++;
      if (edge_count == subgraph_nodes.size() - 1) {
        break;
      }
    }
  }

  return accumulated_node + edge_weight_sum;
}

// get_subgraph_size: Returns the subgraph size
int Graph::get_subgraph_size() { return subgraph_size; }

// get_read_count: Returns the read count of the map
uint64_t Graph::get_read_count() { return read_count; }

/*
 * verify_subgraph: Verifies given subgraph
 * The function checks 3 things:
 * 1. Number of nodes in subgraph
 * 2. Check for duplicated node index
 * 3. Connectivity of each nodes in subgraph using BFS
 */
bool Graph::verify_subgraph() {
  // Check subgraph node number
  if (subgraph_nodes.size() != subgraph_size) {
    cerr << "Subgraph node number(" << subgraph_nodes.size()
         << ") does not match the given size(" << subgraph_size << ")!" << endl;
    return false;
  }

  // Boundary Check
  for (int node : subgraph_nodes) {
    if (node < 0 || node >= graph_size) {
      cerr << "Invalid node index out of bounds: " << node << endl;
      return false;
    }
  }
  set<int> node_set(subgraph_nodes.begin(), subgraph_nodes.end());
  if (node_set.size() != subgraph_size) {
    cerr << "Subgraph node includes duplicated node!" << endl;
    return false;
  }

  if (subgraph_nodes.empty())
    return true;

  // BFS to check connectivity within the subgraph
  queue<int> q;
  set<int> visited;

  q.push(subgraph_nodes[0]);
  visited.insert(subgraph_nodes[0]);

  while (!q.empty()) {
    int curr = q.front();
    q.pop();

    for (int next_node : subgraph_nodes) {
      if (curr != next_node && map[curr][next_node] != 0 &&
          visited.find(next_node) == visited.end()) {
        visited.insert(next_node);
        q.push(next_node);
      }
    }
  }

  if (visited.size() == subgraph_size) {
    return true;
  }

  cerr << "The graph is not connected!!" << endl;
  return false;
}

// get_alpha_d: Returns alpha_d
float Graph::get_alpha_d() { return alpha_d; }
