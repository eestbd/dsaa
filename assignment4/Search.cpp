#include "Search.h"

#include <algorithm>
#include <limits>
#include <random>
#include <utility>
#include <vector>

using namespace std;

namespace {

const int NO_EDGE = 0;
const long long NEG_INF = numeric_limits<long long>::min() / 4;

struct Solution {
  vector<int> nodes;
  vector<int> parent;
  vector<int> parent_edge;
  vector<char> in_selected;
  long long approx_score;
  long long audit_score;
  bool valid;
  bool audited;

  Solution()
      : approx_score(NEG_INF), audit_score(NEG_INF), valid(false),
        audited(false) {}
};

struct GraphProfile {
  bool dense;
  double density;
  int bad_edge_limit;
  vector<int> screened_seeds;
};

struct GreedyTuning {
  double soft_weight;
  double trap_weight;
  double bad_penalty;
  double degree_weight;
  int rcl_size;
};

struct CandidateScore {
  int node;
  double score;
};

struct SwapMove {
  int leaf_index;
  int outside_node;
  int connect_parent;
  int connect_edge;
  long long gain_hint;
};

int clamp_int(int value, int lo, int hi) {
  return max(lo, min(hi, value));
}

int dense_pool_limit(int n, int K) {
  if (n <= 100) {
    return min(n, max(K + 3, 8));
  }

  int limit = max(3 * K, 80);
  if (n >= 3000) {
    limit = max(limit, 120);
  }
  limit = min(limit, 360);
  if (limit > n * 3 / 4) {
    limit = max(K + 40, n / 2);
  }
  return min(n, max(K, limit));
}

int sparse_initial_pool_limit(int n, int K) {
  if (n <= 100) {
    return min(n, max(K + 6, 12));
  }

  int limit = max(3 * K, 80);
  if (n >= 3000) {
    limit = max(4 * K, 180);
  }
  return min(n, max(K, limit));
}

int pool_growth_step(int n, int K, bool dense) {
  if (dense) {
    return max(60, 2 * K);
  }
  if (n >= 3000) {
    return max(120, 2 * K);
  }
  return max(80, 2 * K);
}

GreedyTuning choose_tuning(int n, int K, bool dense) {
  if (n <= 100) {
    return {1.00, 0.42, 3.0, -0.15, 4};
  }

  if (dense) {
    if (K <= 80) {
      if (n < 2000) {
        return {1.45, 1.05, 9.0, -1.10, 4};
      }
      return {1.85, 1.40, 13.0, -1.80, 4};
    }
    return {1.70, 1.20, 12.0, -1.55, 3};
  }

  if (n >= 3000) {
    return {1.15, 0.55, 5.5, 0.25, 3};
  }
  return {1.20, 0.62, 5.0, 0.10, 4};
}

void add_unique_seed(vector<int> &seeds, int node) {
  if (node < 0) {
    return;
  }
  if (find(seeds.begin(), seeds.end(), node) == seeds.end()) {
    seeds.push_back(node);
  }
}

GraphProfile estimate_profile(Graph &graph, int n, int K,
                              const vector<int> &ranked_nodes) {
  GraphProfile profile;
  profile.dense = false;
  profile.density = 0.0;
  profile.bad_edge_limit = -65;

  if (n <= 100) {
    profile.dense = true;
    add_unique_seed(profile.screened_seeds, ranked_nodes[0]);
    return profile;
  }

  int seed_count = min(n, (n <= 100) ? 2 : 4);
  int sample_count = min(n, (n <= 100) ? max(8, K + 5) : 32);
  if (n >= 3000) {
    sample_count = min(n, 32);
  }
  sample_count = max(sample_count, min(n, max(24, K + 8)));

  int total_checks = 0;
  int total_edges = 0;
  vector<int> sampled_edges;
  vector<pair<double, int> > seed_scores;
  sampled_edges.reserve(seed_count * sample_count / 2);

  for (int s = 0; s < seed_count; ++s) {
    int seed = ranked_nodes[s];
    int edge_count = 0;
    int severe_count = 0;
    int soft = NO_EDGE;
    int trap = NO_EDGE;
    long long edge_sum = 0;

    for (int pos = 0; pos < sample_count; ++pos) {
      int target = ranked_nodes[pos];
      if (target == seed) {
        continue;
      }

      int edge = graph.read_map(seed, target);
      ++total_checks;
      if (edge < 0) {
        ++total_edges;
        ++edge_count;
        edge_sum += edge;
        sampled_edges.push_back(edge);
        if (soft == NO_EDGE || edge > soft) {
          soft = edge;
        }
        if (trap == NO_EDGE || edge < trap) {
          trap = edge;
        }
        if (edge <= -70) {
          ++severe_count;
        }
      }
    }

    double avg_edge = edge_count == 0
                          ? -120.0
                          : static_cast<double>(edge_sum) /
                                static_cast<double>(edge_count);
    double score = static_cast<double>(ranked_nodes.size() - s) * 0.01 +
                   static_cast<double>(edge_count) * 4.0 +
                   static_cast<double>(soft) * 0.9 +
                   static_cast<double>(trap) * 0.25 + avg_edge * 0.15 -
                   static_cast<double>(severe_count) * 6.0;
    seed_scores.push_back(make_pair(score, seed));
  }

  if (total_checks > 0) {
    profile.density =
        static_cast<double>(total_edges) / static_cast<double>(total_checks);
  }

  if (!sampled_edges.empty()) {
    sort(sampled_edges.begin(), sampled_edges.end());
    int index = static_cast<int>(sampled_edges.size()) / 4;
    profile.bad_edge_limit = sampled_edges[index];
    profile.bad_edge_limit = clamp_int(profile.bad_edge_limit, -85, -45);
  }

  profile.dense = profile.density >= 0.32;

  if (profile.dense && n >= 3000) {
    add_unique_seed(profile.screened_seeds, ranked_nodes[0]);
    return profile;
  }
  if (!profile.dense && n < 2000) {
    add_unique_seed(profile.screened_seeds, ranked_nodes[min(n - 1, 2)]);
    return profile;
  }

  sort(seed_scores.begin(), seed_scores.end(),
       [](const pair<double, int> &lhs, const pair<double, int> &rhs) {
         if (lhs.first != rhs.first) {
           return lhs.first > rhs.first;
         }
         return lhs.second < rhs.second;
       });

  for (size_t i = 0; i < seed_scores.size(); ++i) {
    add_unique_seed(profile.screened_seeds, seed_scores[i].second);
  }
  return profile;
}

void update_summary(int source, int target, int edge,
                    vector<int> &soft_edge,
                    vector<int> &trap_edge,
                    vector<int> &bad_count,
                    vector<int> &seen_degree,
                    vector<int> &attach_parent, int bad_edge_limit) {
  if (edge >= 0) {
    return;
  }

  ++seen_degree[target];
  if (soft_edge[target] == NO_EDGE || edge > soft_edge[target]) {
    soft_edge[target] = edge;
    attach_parent[target] = source;
  }
  if (trap_edge[target] == NO_EDGE || edge < trap_edge[target]) {
    trap_edge[target] = edge;
  }
  if (edge <= bad_edge_limit) {
    ++bad_count[target];
  }
}

void scan_from_selected_node(Graph &graph, int source,
                             const vector<int> &ranked_nodes, int limit,
                             const vector<char> &in_selected,
                             vector<int> &soft_edge,
                             vector<int> &trap_edge,
                             vector<int> &bad_count,
                             vector<int> &seen_degree,
                             vector<int> &attach_parent, int bad_edge_limit) {
  for (int pos = 0; pos < limit; ++pos) {
    int target = ranked_nodes[pos];
    if (in_selected[target]) {
      continue;
    }
    int edge = graph.read_map(source, target);
    update_summary(source, target, edge, soft_edge, trap_edge, bad_count,
                   seen_degree, attach_parent, bad_edge_limit);
  }
}

void expand_pool(Graph &graph, int old_limit, int new_limit,
                 const vector<int> &ranked_nodes,
                 const vector<int> &selected_nodes,
                 const vector<char> &in_selected,
                 vector<int> &soft_edge,
                 vector<int> &trap_edge,
                 vector<int> &bad_count,
                 vector<int> &seen_degree,
                 vector<int> &attach_parent,
                 int bad_edge_limit) {
  for (size_t i = 0; i < selected_nodes.size(); ++i) {
    int source = selected_nodes[i];
    for (int pos = old_limit; pos < new_limit; ++pos) {
      int target = ranked_nodes[pos];
      if (in_selected[target]) {
        continue;
      }
      int edge = graph.read_map(source, target);
      update_summary(source, target, edge, soft_edge, trap_edge, bad_count,
                     seen_degree, attach_parent, bad_edge_limit);
    }
  }
}

double evaluate_candidate(int node, const vector<int> &node_weight,
                          const vector<int> &soft_edge,
                          const vector<int> &trap_edge,
                          const vector<int> &bad_count,
                          const vector<int> &seen_degree,
                          const GreedyTuning &tuning) {
  double score = static_cast<double>(node_weight[node]);
  score += tuning.soft_weight * static_cast<double>(soft_edge[node]);
  score += tuning.trap_weight * static_cast<double>(trap_edge[node]);
  score -= tuning.bad_penalty * static_cast<double>(bad_count[node]);
  score += tuning.degree_weight *
           static_cast<double>(max(0, seen_degree[node] - 1));
  return score;
}

void insert_rcl_candidate(vector<CandidateScore> &rcl, int rcl_size, int node,
                          double score) {
  CandidateScore item;
  item.node = node;
  item.score = score;
  rcl.push_back(item);
  sort(rcl.begin(), rcl.end(), [](const CandidateScore &lhs,
                                  const CandidateScore &rhs) {
    if (lhs.score != rhs.score) {
      return lhs.score > rhs.score;
    }
    return lhs.node < rhs.node;
  });
  if (static_cast<int>(rcl.size()) > rcl_size) {
    rcl.pop_back();
  }
}

int choose_rcl_candidate(const vector<int> &ranked_nodes, int pool_limit,
                         const vector<char> &in_selected,
                         const vector<int> &node_weight,
                         const vector<int> &soft_edge,
                         const vector<int> &trap_edge,
                         const vector<int> &bad_count,
                         const vector<int> &seen_degree,
                         const vector<int> &attach_parent,
                         const GreedyTuning &tuning, mt19937 &rng,
                         int run_index) {
  vector<CandidateScore> rcl;
  rcl.reserve(tuning.rcl_size);

  for (int pos = 0; pos < pool_limit; ++pos) {
    int node = ranked_nodes[pos];
    if (in_selected[node] || attach_parent[node] < 0) {
      continue;
    }
    if (soft_edge[node] >= 0) {
      continue;
    }

    double score = evaluate_candidate(node, node_weight, soft_edge, trap_edge,
                                      bad_count, seen_degree, tuning);
    insert_rcl_candidate(rcl, tuning.rcl_size, node, score);
  }

  if (rcl.empty()) {
    return -1;
  }

  int choice_limit = static_cast<int>(rcl.size());
  if (run_index == 0) {
    return rcl[0].node;
  }

  uniform_int_distribution<int> distribution(0, choice_limit - 1);
  int pick = distribution(rng);
  if (pick > 0 && rcl[pick].score + 12.0 < rcl[0].score) {
    pick = 0;
  }
  return rcl[pick].node;
}

long long approximate_solution_score(const Solution &solution,
                                     const vector<int> &node_weight) {
  if (!solution.valid) {
    return NEG_INF;
  }

  long long score = 0;
  for (size_t i = 0; i < solution.nodes.size(); ++i) {
    score += node_weight[solution.nodes[i]];
    if (i > 0) {
      score += solution.parent_edge[i];
    }
  }
  return score;
}

Solution run_trap_aware_greedy(Graph &graph, int n, int K, int seed,
                               const vector<int> &node_weight,
                               const vector<int> &ranked_nodes,
                               const GraphProfile &profile, int run_index) {
  Solution result;
  if (seed < 0 || seed >= n || K <= 0 || K > n) {
    return result;
  }

  vector<int> soft_edge(n, NO_EDGE);
  vector<int> trap_edge(n, NO_EDGE);
  vector<int> bad_count(n, 0);
  vector<int> seen_degree(n, 0);
  vector<int> attach_parent(n, -1);
  vector<char> in_selected(n, 0);

  result.nodes.reserve(K);
  result.parent.reserve(K);
  result.parent_edge.reserve(K);
  result.in_selected.assign(n, 0);

  result.nodes.push_back(seed);
  result.parent.push_back(-1);
  result.parent_edge.push_back(0);
  result.in_selected[seed] = 1;
  in_selected[seed] = 1;

  if (K == 1) {
    result.approx_score = node_weight[seed];
    result.audit_score = result.approx_score;
    result.valid = true;
    result.audited = true;
    return result;
  }

  int pool_limit = profile.dense ? dense_pool_limit(n, K)
                                 : sparse_initial_pool_limit(n, K);
  pool_limit = min(n, max(K, pool_limit));
  GreedyTuning tuning = choose_tuning(n, K, profile.dense);
  mt19937 rng(1234567u + static_cast<unsigned int>(seed) * 97u +
              static_cast<unsigned int>(run_index) * 1009u);

  scan_from_selected_node(graph, seed, ranked_nodes, pool_limit, in_selected,
                          soft_edge, trap_edge, bad_count, seen_degree,
                          attach_parent, profile.bad_edge_limit);

  while (static_cast<int>(result.nodes.size()) < K) {
    int chosen = choose_rcl_candidate(ranked_nodes, pool_limit, in_selected,
                                      node_weight, soft_edge, trap_edge,
                                      bad_count, seen_degree, attach_parent,
                                      tuning, rng, run_index);

    while (chosen == -1 && pool_limit < n) {
      int next_limit = min(n, pool_limit + pool_growth_step(n, K, profile.dense));
      expand_pool(graph, pool_limit, next_limit, ranked_nodes, result.nodes,
                  in_selected, soft_edge, trap_edge, bad_count, seen_degree,
                  attach_parent, profile.bad_edge_limit);
      pool_limit = next_limit;
      chosen = choose_rcl_candidate(ranked_nodes, pool_limit, in_selected,
                                    node_weight, soft_edge, trap_edge,
                                    bad_count, seen_degree, attach_parent,
                                    tuning, rng, run_index);
    }

    if (chosen == -1) {
      result.valid = false;
      return result;
    }

    int parent = attach_parent[chosen];
    int edge = soft_edge[chosen];
    if (parent < 0 || edge >= 0 || !in_selected[parent]) {
      result.valid = false;
      return result;
    }

    result.nodes.push_back(chosen);
    result.parent.push_back(parent);
    result.parent_edge.push_back(edge);
    result.in_selected[chosen] = 1;
    in_selected[chosen] = 1;

    if (static_cast<int>(result.nodes.size()) < K) {
      scan_from_selected_node(graph, chosen, ranked_nodes, pool_limit,
                              in_selected, soft_edge, trap_edge, bad_count,
                              seen_degree, attach_parent,
                              profile.bad_edge_limit);
    }
  }

  result.valid = true;
  result.approx_score = approximate_solution_score(result, node_weight);
  return result;
}

long long exact_audit_score(Graph &graph, const vector<int> &nodes,
                            const vector<int> &node_weight) {
  int K = static_cast<int>(nodes.size());
  if (K == 0) {
    return NEG_INF;
  }

  long long node_sum = 0;
  for (int node : nodes) {
    node_sum += node_weight[node];
  }
  if (K == 1) {
    return node_sum;
  }

  vector<char> in_tree(K, 0);
  vector<int> best_edge(K, NO_EDGE);
  in_tree[0] = 1;

  int last_added = 0;
  int added = 1;
  long long edge_sum = 0;

  while (added < K) {
    for (int i = 0; i < K; ++i) {
      if (in_tree[i]) {
        continue;
      }
      int edge = graph.read_map(nodes[last_added], nodes[i]);
      if (edge < 0 && (best_edge[i] == NO_EDGE || edge < best_edge[i])) {
        best_edge[i] = edge;
      }
    }

    int chosen = -1;
    int chosen_edge = 0;
    for (int i = 0; i < K; ++i) {
      if (in_tree[i] || best_edge[i] >= 0) {
        continue;
      }
      if (chosen == -1 || best_edge[i] < chosen_edge) {
        chosen = i;
        chosen_edge = best_edge[i];
      }
    }

    if (chosen == -1) {
      return NEG_INF;
    }

    in_tree[chosen] = 1;
    edge_sum += chosen_edge;
    last_added = chosen;
    ++added;
  }

  return node_sum + edge_sum;
}

long long partial_audit_score(Graph &graph, const Solution &solution,
                              const vector<int> &node_weight,
                              int bad_edge_limit) {
  if (!solution.valid) {
    return NEG_INF;
  }

  int K = static_cast<int>(solution.nodes.size());
  long long score = approximate_solution_score(solution, node_weight);
  if (K <= 1) {
    return score;
  }

  int window = (K <= 160) ? 28 : 18;
  int checked = 0;
  int severe = 0;
  int trap = NO_EDGE;
  long long severe_sum = 0;

  for (int i = 1; i < K; ++i) {
    int begin = max(0, i - window);
    for (int j = begin; j < i; ++j) {
      if (solution.parent[i] == solution.nodes[j]) {
        continue;
      }
      int edge = graph.read_map(solution.nodes[i], solution.nodes[j]);
      ++checked;
      if (edge < 0 && edge <= bad_edge_limit) {
        ++severe;
        severe_sum += edge;
        if (trap == NO_EDGE || edge < trap) {
          trap = edge;
        }
      }
    }
  }

  if (checked == 0 || severe == 0) {
    return score;
  }

  long long penalty = severe_sum / 3;
  penalty += static_cast<long long>(trap) * min(severe, K / 10 + 1) / 5;
  return score + penalty;
}

void audit_solution(Graph &graph, Solution &solution,
                    const vector<int> &node_weight, int bad_edge_limit) {
  int K = static_cast<int>(solution.nodes.size());
  if (!solution.valid) {
    return;
  }

  if (K <= 80) {
    solution.audit_score = exact_audit_score(graph, solution.nodes, node_weight);
  } else {
    solution.audit_score =
        partial_audit_score(graph, solution, node_weight, bad_edge_limit);
  }
  solution.audited = true;
  if (solution.audit_score == NEG_INF) {
    solution.valid = false;
  }
}

vector<int>
make_seed_list(int n, int K, const vector<int> &ranked_nodes,
               const GraphProfile &profile) {
  int start_count = 3;
  if (n <= 100) {
    start_count = 1;
  } else if (n <= 1000) {
    start_count = 1;
  } else if (K <= 80) {
    start_count = 1;
  } else if (K <= 180) {
    start_count = profile.dense ? 3 : 3;
  } else {
    start_count = 2;
  }

  vector<int> seeds;
  seeds.reserve(start_count + 4);
  for (size_t i = 0; i < profile.screened_seeds.size(); ++i) {
    add_unique_seed(seeds, profile.screened_seeds[i]);
    if (static_cast<int>(seeds.size()) >= start_count) {
      return seeds;
    }
  }

  for (int i = 0; i < n; ++i) {
    add_unique_seed(seeds, ranked_nodes[i]);
    if (static_cast<int>(seeds.size()) >= start_count) {
      break;
    }
  }
  return seeds;
}

bool parent_tree_guard(const Solution &solution, int n, int K) {
  if (!solution.valid || static_cast<int>(solution.nodes.size()) != K) {
    return false;
  }
  if (static_cast<int>(solution.parent.size()) != K) {
    return false;
  }

  vector<char> seen(n, 0);
  vector<int> position(n, -1);
  for (int i = 0; i < K; ++i) {
    int node = solution.nodes[i];
    if (node < 0 || node >= n || seen[node]) {
      return false;
    }
    seen[node] = 1;
    position[node] = i;
  }

  int root_count = 0;
  for (int i = 0; i < K; ++i) {
    int parent = solution.parent[i];
    if (parent < 0) {
      ++root_count;
      if (i != 0) {
        return false;
      }
      continue;
    }
    int parent_index = position[parent];
    if (parent_index < 0 || parent_index == i) {
      return false;
    }
  }
  if (root_count != 1) {
    return false;
  }

  vector<int> degree_zero_check(K, 0);
  for (int i = 1; i < K; ++i) {
    int parent_index = position[solution.parent[i]];
    if (parent_index < 0) {
      return false;
    }
    ++degree_zero_check[i];
    ++degree_zero_check[parent_index];
  }
  return true;
}

Solution connected_fallback(Graph &graph, int n, int K,
                            const vector<int> &ranked_nodes) {
  Solution result;
  if (K <= 0 || K > n) {
    return result;
  }

  int seed_limit = min(n, max(24, K));
  for (int seed_pos = 0; seed_pos < seed_limit; ++seed_pos) {
    int seed = ranked_nodes[seed_pos];
    vector<char> in_selected(n, 0);
    Solution candidate;
    candidate.nodes.reserve(K);
    candidate.parent.reserve(K);
    candidate.parent_edge.reserve(K);
    candidate.in_selected.assign(n, 0);

    candidate.nodes.push_back(seed);
    candidate.parent.push_back(-1);
    candidate.parent_edge.push_back(0);
    candidate.in_selected[seed] = 1;
    in_selected[seed] = 1;

    int frontier = 0;
    while (static_cast<int>(candidate.nodes.size()) < K &&
           frontier < static_cast<int>(candidate.nodes.size())) {
      int source = candidate.nodes[frontier++];
      for (int target_pos = 0; target_pos < n; ++target_pos) {
        int target = ranked_nodes[target_pos];
        if (in_selected[target]) {
          continue;
        }
        int edge = graph.read_map(source, target);
        if (edge < 0) {
          candidate.nodes.push_back(target);
          candidate.parent.push_back(source);
          candidate.parent_edge.push_back(edge);
          candidate.in_selected[target] = 1;
          in_selected[target] = 1;
          if (static_cast<int>(candidate.nodes.size()) == K) {
            break;
          }
        }
      }
    }

    candidate.valid = static_cast<int>(candidate.nodes.size()) == K;
    candidate.approx_score = 0;
    if (candidate.valid) {
      return candidate;
    }
  }

  return result;
}

vector<int> tree_degree_by_index(const Solution &solution, int n) {
  int K = static_cast<int>(solution.nodes.size());
  vector<int> position(n, -1);
  for (int i = 0; i < K; ++i) {
    position[solution.nodes[i]] = i;
  }

  vector<int> degree(K, 0);
  for (int i = 1; i < K; ++i) {
    int parent_index = position[solution.parent[i]];
    if (parent_index >= 0) {
      ++degree[i];
      ++degree[parent_index];
    }
  }
  return degree;
}

long long score_for_swap_audit(Graph &graph, const Solution &solution,
                               const vector<int> &node_weight,
                               int bad_edge_limit) {
  int K = static_cast<int>(solution.nodes.size());
  if (K <= 80) {
    return exact_audit_score(graph, solution.nodes, node_weight);
  }
  return partial_audit_score(graph, solution, node_weight, bad_edge_limit);
}

void capped_leaf_swap(Graph &graph, Solution &best, int n,
                      const vector<int> &node_weight,
                      const vector<int> &ranked_nodes, int bad_edge_limit) {
  int K = static_cast<int>(best.nodes.size());
  if (!best.valid || K < 2 || n <= K || n <= 100) {
    return;
  }

  vector<int> initial_degree = tree_degree_by_index(best, n);
  int weakest_leaf_weight = numeric_limits<int>::max();
  for (int i = 1; i < K; ++i) {
    if (initial_degree[i] == 1) {
      weakest_leaf_weight = min(weakest_leaf_weight, node_weight[best.nodes[i]]);
    }
  }
  if (weakest_leaf_weight == numeric_limits<int>::max()) {
    return;
  }

  vector<char> initial_in_best(n, 0);
  for (int node : best.nodes) {
    initial_in_best[node] = 1;
  }

  int best_outside_weight = numeric_limits<int>::min();
  for (int node : ranked_nodes) {
    if (!initial_in_best[node]) {
      best_outside_weight = node_weight[node];
      break;
    }
  }
  int required_node_gain = (n >= 3000) ? 120 : 140;
  if (best_outside_weight - weakest_leaf_weight < required_node_gain) {
    return;
  }

  if (!best.audited) {
    audit_solution(graph, best, node_weight, bad_edge_limit);
    if (!best.valid) {
      return;
    }
  }

  int max_iterations = (n >= 3000) ? 1 : 2;
  int leaf_limit = (K <= 80) ? 3 : 2;
  int outside_limit = (n >= 3000) ? 18 : 20;
  int connect_scan_limit = (K <= 80) ? K : min(K, 48);
  long long min_gain = (K <= 80) ? 15 : 35;

  for (int iteration = 0; iteration < max_iterations; ++iteration) {
    vector<int> degree = tree_degree_by_index(best, n);
    vector<int> leaves;
    for (int i = 1; i < K; ++i) {
      if (degree[i] == 1) {
        leaves.push_back(i);
      }
    }
    sort(leaves.begin(), leaves.end(), [&](int lhs, int rhs) {
      return node_weight[best.nodes[lhs]] < node_weight[best.nodes[rhs]];
    });
    if (static_cast<int>(leaves.size()) > leaf_limit) {
      leaves.resize(leaf_limit);
    }
    if (leaves.empty()) {
      return;
    }

    vector<char> in_best(n, 0);
    for (int node : best.nodes) {
      in_best[node] = 1;
    }

    vector<int> outside;
    outside.reserve(outside_limit);
    for (int node : ranked_nodes) {
      if (!in_best[node]) {
        outside.push_back(node);
        if (static_cast<int>(outside.size()) >= outside_limit) {
          break;
        }
      }
    }
    if (outside.empty()) {
      return;
    }

    vector<SwapMove> moves;
    for (int leaf_index : leaves) {
      int leaf_node = best.nodes[leaf_index];
      for (int outside_node : outside) {
        if (node_weight[outside_node] <= node_weight[leaf_node]) {
          continue;
        }

        int parent = -1;
        int soft = NO_EDGE;
        int trap = NO_EDGE;
        int bad = 0;
        int scans = 0;
        for (int scan = 0; scan < K && scans < connect_scan_limit; ++scan) {
          int i = scan;
          if (K > connect_scan_limit) {
            i = (scan * K) / connect_scan_limit;
          }
          if (i == leaf_index) {
            continue;
          }
          ++scans;
          int edge = graph.read_map(best.nodes[i], outside_node);
          if (edge < 0) {
            if (soft == NO_EDGE || edge > soft) {
              soft = edge;
              parent = best.nodes[i];
            }
            if (trap == NO_EDGE || edge < trap) {
              trap = edge;
            }
            if (edge <= bad_edge_limit) {
              ++bad;
            }
          }
        }

        if (parent < 0) {
          continue;
        }

        long long gain_hint =
            static_cast<long long>(node_weight[outside_node]) -
            static_cast<long long>(node_weight[leaf_node]) + soft +
            static_cast<long long>(trap) / 3 -
            static_cast<long long>(bad) * 8;
        if (gain_hint <= 0) {
          continue;
        }

        SwapMove move;
        move.leaf_index = leaf_index;
        move.outside_node = outside_node;
        move.connect_parent = parent;
        move.connect_edge = soft;
        move.gain_hint = gain_hint;
        moves.push_back(move);
      }
    }

    if (moves.empty()) {
      return;
    }
    sort(moves.begin(), moves.end(), [](const SwapMove &lhs,
                                        const SwapMove &rhs) {
      if (lhs.gain_hint != rhs.gain_hint) {
        return lhs.gain_hint > rhs.gain_hint;
      }
      return lhs.outside_node < rhs.outside_node;
    });

    bool improved = false;
    int audit_count = min(3, static_cast<int>(moves.size()));
    for (int i = 0; i < audit_count; ++i) {
      Solution trial = best;
      const SwapMove &move = moves[i];
      trial.in_selected[trial.nodes[move.leaf_index]] = 0;
      trial.nodes[move.leaf_index] = move.outside_node;
      trial.parent[move.leaf_index] = move.connect_parent;
      trial.parent_edge[move.leaf_index] = move.connect_edge;
      trial.in_selected[move.outside_node] = 1;
      trial.valid = parent_tree_guard(trial, n, K);
      trial.audited = false;
      if (!trial.valid) {
        continue;
      }

      long long trial_score =
          score_for_swap_audit(graph, trial, node_weight, bad_edge_limit);
      if (trial_score >= best.audit_score + min_gain) {
        trial.audit_score = trial_score;
        trial.audited = true;
        best = trial;
        improved = true;
        break;
      }
    }

    if (!improved) {
      return;
    }
  }
}

} // namespace

void Search_MMST(Graph &graph, int K) {
  int n = graph.get_graph_size();
  if (K <= 0 || n <= 0 || K > n) {
    return;
  }

  vector<int> node_weight(n, 0);
  for (int i = 0; i < n; ++i) {
    node_weight[i] = graph.read_map(i, i);
  }

  vector<int> ranked_nodes(n, 0);
  for (int i = 0; i < n; ++i) {
    ranked_nodes[i] = i;
  }
  sort(ranked_nodes.begin(), ranked_nodes.end(), [&](int lhs, int rhs) {
    if (node_weight[lhs] != node_weight[rhs]) {
      return node_weight[lhs] > node_weight[rhs];
    }
    return lhs < rhs;
  });

  GraphProfile profile = estimate_profile(graph, n, K, ranked_nodes);
  vector<int> seeds = make_seed_list(n, K, ranked_nodes, profile);

  Solution best;
  for (size_t i = 0; i < seeds.size(); ++i) {
    Solution candidate = run_trap_aware_greedy(graph, n, K, seeds[i],
                                              node_weight, ranked_nodes,
                                              profile, static_cast<int>(i));
    if (!candidate.valid || !parent_tree_guard(candidate, n, K)) {
      continue;
    }
    if (seeds.size() > 1) {
      audit_solution(graph, candidate, node_weight, profile.bad_edge_limit);
      if (!candidate.valid) {
        continue;
      }
    } else {
      candidate.audit_score = candidate.approx_score;
      candidate.audited = false;
    }
    if (!best.valid || candidate.audit_score > best.audit_score) {
      best = candidate;
    }
  }

  if (!best.valid || !parent_tree_guard(best, n, K)) {
    best = connected_fallback(graph, n, K, ranked_nodes);
  }

  if (best.valid && parent_tree_guard(best, n, K)) {
    capped_leaf_swap(graph, best, n, node_weight, ranked_nodes,
                     profile.bad_edge_limit);
  }

  if (!best.valid || !parent_tree_guard(best, n, K)) {
    best = connected_fallback(graph, n, K, ranked_nodes);
  }

  if (!best.valid || !parent_tree_guard(best, n, K)) {
    return;
  }

  for (int node : best.nodes) {
    graph.submit_node(node);
  }
}
