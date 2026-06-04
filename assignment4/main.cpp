#include "Search.h"
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

long read_proc_status_kb(const std::string& target_key) {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.find(target_key) == 0) {
            size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) {
                return std::stol(line.substr(pos));
            }
        }
    }
    return -1;
}

/**
 * Use this macro for debugging!
 * 0 prints only results while 1 prints map (only for map50.data)
 */
#define CHECK 0

int main(int argc, char *argv[]) {
  /**
   * main.cpp: Main code that actually calls and execute Search_MMST
   * Do not modify codes under this annotation when submitting your code!!
   * Only CHECK definition modification is available for the students.
   */
  assert(argv[1] &&
         "Program should run with 1 arguement!(data name or unit_test)");
  string file_name(argv[1]);
  Graph new_graph;
  new_graph.readFile(file_name);
#if CHECK
  if (new_graph.get_graph_size() < 101) {
    new_graph.printGraph();
  }
#endif
  // This initiates your search algorithm
  int K = new_graph.get_subgraph_size();
  Search_MMST(new_graph, K);
  if (!new_graph.verify_subgraph()) {
    cerr << "Given subgraph nodes are invalid" << endl;
    return -1;
  }

  // Grading code: DO NOT MODIFY
  int total_read_count = new_graph.get_read_count();
  int score_mmst = new_graph.calc_subgraph();
  cout << "\033[1;36m"
       << "Graph map access count: " << total_read_count << endl;
  cout << "\033[1;36m"
       << "Score of MMST: " << score_mmst << endl;
  float total_score;
  int data_graph_size = new_graph.get_graph_size();

  total_score =
      (float)score_mmst - new_graph.get_alpha_d() * (float)total_read_count;

  cout << "\033[1;36m"
       << "Total Score: " << total_score << endl;

  long vm_peak_kb = read_proc_status_kb("VmPeak:");
  cout << "\033[1;36m"
       << "Peak memory usage: " << vm_peak_kb << " KB\033[0m\n";

  return 0;
}
