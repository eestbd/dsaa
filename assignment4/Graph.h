#ifndef __GRAPH_H__
#define __GRAPH_H__

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>
using namespace std;

class Graph {
public:
  Graph();  // Class Constructor
  ~Graph(); // Class Destructor

  void readFile(string &filename);
  void printGraph();
  /*
   * verify_subgraph: Verifies given subgraph
   * Student cannot use this function, but may give intuition on how to use the
   * graph class
   */
  bool verify_subgraph();
  /*
   * Following functions are APIs students may use
   * read_map, get_graph_size
   */
  int read_map(int row, int col);
  int get_graph_size();
  /*
   * submit_node: Add a node to your subgraph
   * Use this function to submit your chosen nodes.
   */
  void submit_node(int node);

  // These are for grading
  int calc_subgraph();
  int get_subgraph_size();
  static uint64_t get_read_count();
  float get_alpha_d();

private:
  vector<int> subgraph_nodes;
  static uint64_t read_count;
  int graph_size;
  int subgraph_size;
  float alpha_d;
  int **map;
};

#endif
