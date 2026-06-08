#include "Search.h"

#include <algorithm>
#include <limits>
#include <vector>

using namespace std;

namespace {

const int NO_EDGE = 0;
const int BAD_EDGE_LIMIT = -65;

struct GreedyProfile {
  double soft_weight;
  double trap_weight;
  double bad_penalty;
  double degree_penalty;
};

struct SearchResult {
  vector<int> nodes;
  vector<int> tree_parent;
  long long audit_score;
  bool valid;
  bool audited;

  SearchResult()
      : audit_score(numeric_limits<long long>::min()), valid(false),
        audited(false) {}
};

struct Proposal {
  int leaf_index;
  int outside_node;
  int connect_parent;
  long long approximate_gain;
};

int initial_pool_limit(int n, int K) {
  if (n <= 80) {
    return min(n, max(K + 10, 20));
  }

  if (n < 2000) {
    return min(n, max(K + 60, 100));
  }

  int limit = max(K + 80, 120);
  if (K > 120) {
    limit = max(K + 120, 300);
  }
  return min(n, limit);
}

int next_pool_limit(int n, int current, int K) {
  if (n <= 80) {
    return min(n, current + max(5, K * 2));
  }

  int step = max(80, K * 2);
  if (current < 400) {
    step = max(step, 160);
  }
  return min(n, current + step);
}

double candidate_score(int node, const vector<int> &node_weight,
                       const vector<int> &soft_edge,
                       const vector<int> &trap_edge,
                       const vector<int> &bad_count,
                       const vector<int> &seen_degree,
                       const GreedyProfile &profile) {
  return static_cast<double>(node_weight[node]) +
         profile.soft_weight * static_cast<double>(soft_edge[node]) +
         profile.trap_weight * static_cast<double>(trap_edge[node]) -
         profile.bad_penalty * static_cast<double>(bad_count[node]) -
         profile.degree_penalty *
             static_cast<double>(max(0, seen_degree[node] - 1));
}

GreedyProfile choose_profile(int n, double observed_density) {
  if (n <= 80) {
    return {1.2, 0.3, 2.0, 0.5};
  }

  if (observed_density >= 0.45) {
    if (n < 2000) {
      return {1.2, 0.3, 2.0, 0.5};
    }
    return {2.0, 0.7, 5.5, 1.2};
  }

  if (n < 2000) {
    return {1.65, 0.55, 4.0, 1.0};
  }
  return {2.5, 1.0, 8.0, 2.0};
}

int choose_sparse_screened_seed(Graph &graph, int n, int K,
                                const vector<int> &ranked_nodes) {
  if (K <= 1 || n < 2000 || K > 80) {
    return ranked_nodes[0];
  }

  int sample_limit = min(n, 4);
  int sample_edges = 0;
  for (int pos = 0; pos < sample_limit; ++pos) {
    int target = ranked_nodes[pos];
    if (target == ranked_nodes[0]) {
      continue;
    }
    if (graph.read_map(ranked_nodes[0], target) < 0) {
      sample_edges++;
    }
  }

  double sample_density =
      static_cast<double>(sample_edges) / static_cast<double>(max(1, sample_limit - 1));
  if (sample_density >= 0.30) {
    return ranked_nodes[0];
  }

  int screen_count = min(n, 4);
  int screen_pool = min(n, max(K + 30, 60));
  int best_seed = ranked_nodes[0];
  double best_score = -numeric_limits<double>::infinity();
  double first_score = -numeric_limits<double>::infinity();

  for (int seed_index = 0; seed_index < screen_count; ++seed_index) {
    int seed = ranked_nodes[seed_index];
    int edge_count = 0;
    int bad_count = 0;
    long long edge_sum = 0;

    for (int pos = 0; pos < screen_pool; ++pos) {
      int target = ranked_nodes[pos];
      if (target == seed) {
        continue;
      }

      int edge = graph.read_map(seed, target);
      if (edge < 0) {
        edge_count++;
        edge_sum += edge;
        if (edge <= BAD_EDGE_LIMIT) {
          bad_count++;
        }
      }
    }

    if (edge_count == 0) {
      continue;
    }

    double average_edge =
        static_cast<double>(edge_sum) / static_cast<double>(edge_count);
    double score = static_cast<double>(edge_count) * 5.0 + average_edge -
                   static_cast<double>(bad_count) * 4.0 -
                   static_cast<double>(seed_index) * 0.1;

    if (seed_index == 0) {
      first_score = score;
    }
    if (score > best_score) {
      best_score = score;
      best_seed = seed;
    }
  }

  if (best_seed != ranked_nodes[0] && best_score >= first_score + 5.0) {
    return best_seed;
  }
  return ranked_nodes[0];
}

void update_summary(int source, int target, int edge,
                    vector<int> &soft_edge, vector<int> &trap_edge,
                    vector<int> &bad_count, vector<int> &seen_degree,
                    vector<int> &connect_parent, int bad_edge_limit) {
  if (edge >= 0) {
    return;
  }

  seen_degree[target]++;
  if (soft_edge[target] == NO_EDGE || edge > soft_edge[target]) {
    soft_edge[target] = edge;
    connect_parent[target] = source;
  }
  if (trap_edge[target] == NO_EDGE || edge < trap_edge[target]) {
    trap_edge[target] = edge;
  }
  if (edge <= bad_edge_limit) {
    bad_count[target]++;
  }
}

void scan_new_node(Graph &graph, int source, const vector<int> &ranked_nodes,
                   int pool_limit, const vector<char> &in_selected,
                   vector<int> &soft_edge, vector<int> &trap_edge,
                   vector<int> &bad_count, vector<int> &seen_degree,
                   vector<int> &connect_parent, int bad_edge_limit) {
  for (int pos = 0; pos < pool_limit; ++pos) {
    int target = ranked_nodes[pos];
    if (in_selected[target]) {
      continue;
    }
    int edge = graph.read_map(source, target);
    update_summary(source, target, edge, soft_edge, trap_edge, bad_count,
                   seen_degree, connect_parent, bad_edge_limit);
  }
}

void expand_pool(Graph &graph, int old_limit, int new_limit,
                 const vector<int> &ranked_nodes, const vector<int> &selected,
                 const vector<char> &in_selected, vector<int> &soft_edge,
                 vector<int> &trap_edge, vector<int> &bad_count,
                 vector<int> &seen_degree, vector<int> &connect_parent,
                 int bad_edge_limit) {
  for (int source : selected) {
    for (int pos = old_limit; pos < new_limit; ++pos) {
      int target = ranked_nodes[pos];
      if (in_selected[target]) {
        continue;
      }
      int edge = graph.read_map(source, target);
      update_summary(source, target, edge, soft_edge, trap_edge, bad_count,
                     seen_degree, connect_parent, bad_edge_limit);
    }
  }
}

int choose_candidate(const vector<int> &ranked_nodes, int pool_limit,
                     const vector<char> &in_selected,
                     const vector<int> &node_weight,
                     const vector<int> &soft_edge,
                     const vector<int> &trap_edge,
                     const vector<int> &bad_count,
                     const vector<int> &seen_degree,
                     const GreedyProfile &profile) {
  int best = -1;
  double best_score = -numeric_limits<double>::infinity();

  for (int pos = 0; pos < pool_limit; ++pos) {
    int node = ranked_nodes[pos];
    if (in_selected[node] || soft_edge[node] >= 0) {
      continue;
    }

    double score = candidate_score(node, node_weight, soft_edge, trap_edge,
                                   bad_count, seen_degree, profile);
    if (best == -1 || score > best_score ||
        (score == best_score && node_weight[node] > node_weight[best]) ||
        (score == best_score && node_weight[node] == node_weight[best] &&
         soft_edge[node] > soft_edge[best])) {
      best = node;
      best_score = score;
    }
  }

  return best;
}

SearchResult run_connected_greedy(Graph &graph, int n, int K, int seed,
                                  const vector<int> &node_weight,
                                  const vector<int> &ranked_nodes) {
  SearchResult result;
  if (K <= 0 || seed < 0 || seed >= n) {
    return result;
  }

  vector<int> soft_edge(n, NO_EDGE);
  vector<int> trap_edge(n, NO_EDGE);
  vector<int> bad_count(n, 0);
  vector<int> seen_degree(n, 0);
  vector<int> connect_parent(n, -1);
  vector<char> in_selected(n, 0);

  vector<int> selected;
  vector<int> tree_parent;
  selected.reserve(K);
  tree_parent.reserve(K);

  selected.push_back(seed);
  tree_parent.push_back(-1);
  in_selected[seed] = 1;

  if (K == 1) {
    result.nodes = selected;
    result.tree_parent = tree_parent;
    result.valid = true;
    return result;
  }

  int pool_limit = max(K, initial_pool_limit(n, K));
  scan_new_node(graph, seed, ranked_nodes, pool_limit, in_selected, soft_edge,
                trap_edge, bad_count, seen_degree, connect_parent,
                BAD_EDGE_LIMIT);

  int initial_edges = 0;
  vector<int> observed_edges;
  observed_edges.reserve(pool_limit);
  for (int pos = 0; pos < pool_limit; ++pos) {
    int node = ranked_nodes[pos];
    if (!in_selected[node] && seen_degree[node] > 0) {
      initial_edges++;
      observed_edges.push_back(trap_edge[node]);
    }
  }
  double observed_density =
      static_cast<double>(initial_edges) / static_cast<double>(max(1, pool_limit - 1));
  GreedyProfile profile = choose_profile(n, observed_density);

  int bad_edge_limit = BAD_EDGE_LIMIT;
  if (!observed_edges.empty()) {
    int bad_index = static_cast<int>(observed_edges.size()) / 3;
    nth_element(observed_edges.begin(), observed_edges.begin() + bad_index,
                observed_edges.end());
    bad_edge_limit = observed_edges[bad_index];
    bad_edge_limit = min(-55, max(-75, bad_edge_limit));

    for (int pos = 0; pos < pool_limit; ++pos) {
      int node = ranked_nodes[pos];
      if (!in_selected[node] && seen_degree[node] > 0) {
        bad_count[node] = (trap_edge[node] <= bad_edge_limit) ? 1 : 0;
      }
    }
  }

  if (observed_density < 0.45 && K <= 80 &&
      ((n < 2000 && pool_limit < 120) ||
       (n >= 2000 && pool_limit < 240))) {
    int target_limit = (n < 2000) ? 120 : 240;
    int expanded_limit = min(n, target_limit);
    expand_pool(graph, pool_limit, expanded_limit, ranked_nodes, selected,
                in_selected, soft_edge, trap_edge, bad_count, seen_degree,
                connect_parent, bad_edge_limit);
    pool_limit = expanded_limit;
  }

  while (static_cast<int>(selected.size()) < K) {
    int chosen = choose_candidate(ranked_nodes, pool_limit, in_selected,
                                  node_weight, soft_edge, trap_edge, bad_count,
                                  seen_degree, profile);

    while (chosen == -1 && pool_limit < n) {
      int expanded_limit = next_pool_limit(n, pool_limit, K);
      expand_pool(graph, pool_limit, expanded_limit, ranked_nodes, selected,
                  in_selected, soft_edge, trap_edge, bad_count, seen_degree,
                  connect_parent, bad_edge_limit);
      pool_limit = expanded_limit;
      chosen = choose_candidate(ranked_nodes, pool_limit, in_selected,
                                node_weight, soft_edge, trap_edge, bad_count,
                                seen_degree, profile);
    }

    if (chosen == -1) {
      result.nodes = selected;
      result.tree_parent = tree_parent;
      result.valid = false;
      return result;
    }

    selected.push_back(chosen);
    tree_parent.push_back(connect_parent[chosen]);
    in_selected[chosen] = 1;

    if (static_cast<int>(selected.size()) < K) {
      scan_new_node(graph, chosen, ranked_nodes, pool_limit, in_selected,
                    soft_edge, trap_edge, bad_count, seen_degree,
                    connect_parent, bad_edge_limit);
    }
  }

  result.nodes = selected;
  result.tree_parent = tree_parent;
  result.valid = true;
  return result;
}

SearchResult build_connected_fallback(Graph &graph, int n, int K,
                                      const vector<int> &ranked_nodes) {
  SearchResult result;
  if (K <= 0 || K > n) {
    return result;
  }

  for (int seed : ranked_nodes) {
    vector<char> in_selected(n, 0);
    vector<int> selected;
    vector<int> tree_parent;
    selected.reserve(K);
    tree_parent.reserve(K);

    selected.push_back(seed);
    tree_parent.push_back(-1);
    in_selected[seed] = 1;

    int frontier_index = 0;
    while (static_cast<int>(selected.size()) < K &&
           frontier_index < static_cast<int>(selected.size())) {
      int source = selected[frontier_index++];

      for (int target : ranked_nodes) {
        if (in_selected[target]) {
          continue;
        }

        int edge = graph.read_map(source, target);
        if (edge < 0) {
          selected.push_back(target);
          tree_parent.push_back(source);
          in_selected[target] = 1;

          if (static_cast<int>(selected.size()) == K) {
            break;
          }
        }
      }
    }

    if (static_cast<int>(selected.size()) == K) {
      result.nodes = selected;
      result.tree_parent = tree_parent;
      result.audit_score = 0;
      result.valid = true;
      result.audited = false;
      return result;
    }
  }

  return result;
}

long long audit_mmst_score(Graph &graph, const vector<int> &nodes,
                           const vector<int> &node_weight) {
  int K = static_cast<int>(nodes.size());
  if (K == 0) {
    return numeric_limits<long long>::min();
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
  int added_count = 1;
  long long edge_sum = 0;

  while (added_count < K) {
    for (int idx = 0; idx < K; ++idx) {
      if (in_tree[idx]) {
        continue;
      }
      int edge = graph.read_map(nodes[last_added], nodes[idx]);
      if (edge < 0 && (best_edge[idx] == NO_EDGE || edge < best_edge[idx])) {
        best_edge[idx] = edge;
      }
    }

    int chosen = -1;
    int chosen_edge = 0;
    for (int idx = 0; idx < K; ++idx) {
      if (in_tree[idx] || best_edge[idx] >= 0) {
        continue;
      }
      if (chosen == -1 || best_edge[idx] < chosen_edge) {
        chosen = idx;
        chosen_edge = best_edge[idx];
      }
    }

    if (chosen == -1) {
      return numeric_limits<long long>::min();
    }

    in_tree[chosen] = 1;
    edge_sum += chosen_edge;
    last_added = chosen;
    added_count++;
  }

  return node_sum + edge_sum;
}

vector<int> tree_degrees(const vector<int> &nodes,
                         const vector<int> &tree_parent, int n) {
  vector<int> node_position(n, -1);
  for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
    node_position[nodes[i]] = i;
  }

  vector<int> degree(nodes.size(), 0);
  for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
    int parent = tree_parent[i];
    if (parent < 0) {
      continue;
    }
    int parent_index = node_position[parent];
    if (parent_index >= 0) {
      degree[i]++;
      degree[parent_index]++;
    }
  }
  return degree;
}

void capped_leaf_swap(Graph &graph, SearchResult &result, int n,
                      const vector<int> &node_weight,
                      const vector<int> &ranked_nodes) {
  int K = static_cast<int>(result.nodes.size());
  if (!result.valid || K < 2 || n < 200) {
    return;
  }

  vector<char> initial_in_result(n, 0);
  for (int node : result.nodes) {
    initial_in_result[node] = 1;
  }

  vector<int> initial_degree = tree_degrees(result.nodes, result.tree_parent, n);
  int weakest_leaf_weight = numeric_limits<int>::max();
  for (int i = 1; i < K; ++i) {
    if (initial_degree[i] == 1) {
      weakest_leaf_weight =
          min(weakest_leaf_weight, node_weight[result.nodes[i]]);
    }
  }
  if (weakest_leaf_weight == numeric_limits<int>::max()) {
    return;
  }

  int best_outside_weight = numeric_limits<int>::min();
  for (int node : ranked_nodes) {
    if (!initial_in_result[node]) {
      best_outside_weight = node_weight[node];
      break;
    }
  }
  if (best_outside_weight == numeric_limits<int>::min()) {
    return;
  }

  int required_node_gain = (n >= 2000) ? 20 : 45;
  if (best_outside_weight - weakest_leaf_weight < required_node_gain) {
    return;
  }

  if (!result.audited) {
    result.audit_score = audit_mmst_score(graph, result.nodes, node_weight);
    result.audited = true;
    if (result.audit_score == numeric_limits<long long>::min()) {
      result.valid = false;
      return;
    }
  }

  const int max_iterations = (n >= 2000) ? 2 : 1;
  const int leaf_limit = min(K, 4);
  const int outside_limit = (n >= 2000) ? 28 : 18;
  const int proposal_audit_limit = 2;
  const long long min_gain = (n >= 2000) ? 40 : 100;

  long long current_score = result.audit_score;

  for (int iteration = 0; iteration < max_iterations; ++iteration) {
    vector<char> in_result(n, 0);
    for (int node : result.nodes) {
      in_result[node] = 1;
    }

    vector<int> degree = tree_degrees(result.nodes, result.tree_parent, n);
    vector<int> leaves;
    for (int i = 1; i < K; ++i) {
      if (degree[i] == 1) {
        leaves.push_back(i);
      }
    }

    sort(leaves.begin(), leaves.end(), [&](int lhs, int rhs) {
      return node_weight[result.nodes[lhs]] < node_weight[result.nodes[rhs]];
    });
    if (static_cast<int>(leaves.size()) > leaf_limit) {
      leaves.resize(leaf_limit);
    }

    vector<int> outside_nodes;
    for (int node : ranked_nodes) {
      if (!in_result[node]) {
        outside_nodes.push_back(node);
        if (static_cast<int>(outside_nodes.size()) >= outside_limit) {
          break;
        }
      }
    }

    vector<Proposal> proposals;
    for (int leaf_index : leaves) {
      int leaf_node = result.nodes[leaf_index];
      for (int outside_node : outside_nodes) {
        int best_parent = -1;
        int best_edge = NO_EDGE;

        for (int i = 0; i < K; ++i) {
          if (i == leaf_index) {
            continue;
          }
          int edge = graph.read_map(result.nodes[i], outside_node);
          if (edge < 0 && (best_parent == -1 || edge > best_edge)) {
            best_parent = result.nodes[i];
            best_edge = edge;
          }
        }

        if (best_parent == -1) {
          continue;
        }

        Proposal proposal;
        proposal.leaf_index = leaf_index;
        proposal.outside_node = outside_node;
        proposal.connect_parent = best_parent;
        proposal.approximate_gain =
            static_cast<long long>(node_weight[outside_node]) -
            static_cast<long long>(node_weight[leaf_node]) + best_edge;
        proposals.push_back(proposal);
      }
    }

    if (proposals.empty()) {
      return;
    }

    sort(proposals.begin(), proposals.end(),
         [](const Proposal &lhs, const Proposal &rhs) {
           return lhs.approximate_gain > rhs.approximate_gain;
         });

    bool improved = false;
    int audit_count = min(proposal_audit_limit, static_cast<int>(proposals.size()));
    for (int i = 0; i < audit_count; ++i) {
      const Proposal &proposal = proposals[i];
      vector<int> swapped_nodes = result.nodes;
      swapped_nodes[proposal.leaf_index] = proposal.outside_node;

      long long swapped_score =
          audit_mmst_score(graph, swapped_nodes, node_weight);
      if (swapped_score >= current_score + min_gain) {
        result.nodes = swapped_nodes;
        result.tree_parent[proposal.leaf_index] = proposal.connect_parent;
        result.audit_score = swapped_score;
        current_score = swapped_score;
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
  if (K <= 0 || n <= 0) {
    return;
  }

  vector<int> node_weight(n);
  for (int i = 0; i < n; ++i) {
    node_weight[i] = graph.read_map(i, i);
  }

  vector<int> ranked_nodes(n);
  for (int i = 0; i < n; ++i) {
    ranked_nodes[i] = i;
  }
  sort(ranked_nodes.begin(), ranked_nodes.end(), [&](int lhs, int rhs) {
    if (node_weight[lhs] != node_weight[rhs]) {
      return node_weight[lhs] > node_weight[rhs];
    }
    return lhs < rhs;
  });

  SearchResult best;
  int primary_seed = choose_sparse_screened_seed(graph, n, K, ranked_nodes);

  if (K == 1) {
    best.nodes.push_back(ranked_nodes[0]);
    best.tree_parent.push_back(-1);
    best.audit_score = node_weight[ranked_nodes[0]];
    best.valid = true;
    best.audited = true;
  } else {
    int start_count = 1;
    if (n <= 80) {
      start_count = 1;
    } else if (K <= 60) {
      start_count = 1;
    } else {
      start_count = min(n, 3);
    }

    vector<SearchResult> candidates;
    candidates.reserve(start_count);
    bool need_audit = (start_count > 1);
    for (int i = 0; i < start_count; ++i) {
      int seed = (i == 0) ? primary_seed : ranked_nodes[i];
      SearchResult candidate =
          run_connected_greedy(graph, n, K, seed, node_weight, ranked_nodes);
      if (candidate.valid) {
        candidate.audit_score = need_audit
                                    ? audit_mmst_score(graph, candidate.nodes,
                                                       node_weight)
                                    : 0;
        candidate.audited = need_audit;
        if (candidate.audit_score != numeric_limits<long long>::min()) {
          candidates.push_back(candidate);
        }
      }
    }

    int fallback_limit = min(n, max(start_count, 24));
    for (int i = start_count; candidates.empty() && i < fallback_limit; ++i) {
      SearchResult candidate =
          run_connected_greedy(graph, n, K, ranked_nodes[i], node_weight,
                               ranked_nodes);
      if (candidate.valid) {
        candidate.audit_score = need_audit
                                    ? audit_mmst_score(graph, candidate.nodes,
                                                       node_weight)
                                    : 0;
        candidate.audited = need_audit;
        if (candidate.audit_score != numeric_limits<long long>::min()) {
          candidates.push_back(candidate);
        }
      }
    }

    for (const SearchResult &candidate : candidates) {
      if (!best.valid || candidate.audit_score > best.audit_score) {
        best = candidate;
        best.valid = true;
      }
    }
  }

  if (!best.valid || static_cast<int>(best.nodes.size()) != K) {
    best = build_connected_fallback(graph, n, K, ranked_nodes);
  }

  if (best.valid && static_cast<int>(best.nodes.size()) == K) {
    SearchResult before_swap = best;
    capped_leaf_swap(graph, best, n, node_weight, ranked_nodes);

    if (!best.valid || static_cast<int>(best.nodes.size()) != K) {
      best = before_swap;
    }
  }

  if (!best.valid || static_cast<int>(best.nodes.size()) != K) {
    best = build_connected_fallback(graph, n, K, ranked_nodes);
  }

  if (best.valid && static_cast<int>(best.nodes.size()) == K) {
    for (int node : best.nodes) {
      graph.submit_node(node);
    }
  }
}
