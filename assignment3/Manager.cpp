// Manager.cpp

#include "Manager.h"
#include <unordered_map>

// ============================================================
// Assignment 3: Online Leaderboard
//
// Strategy:
//   Adaptive prefix repair + player_id -> ScoreBoard index cache.
//
// Invariant:
//   board[0..prefix_len-1] is the true top-prefix, sorted by:
//     1. higher score first
//     2. if score ties, smaller player_id first
//
// The suffix is kept valid but not globally sorted. submit/remove update the
// ScoreBoard immediately, and top_k/rank repair only when needed.
//
// Note:
//   The ScoreBoard remains the source of truth for all valid entries.
//   Manager keeps only physical-index metadata; it does not keep score values
//   or an independently maintained leaderboard order.
// ============================================================

static std::unordered_map<int, int> pos; // player_id -> current index

static int prefix_len = 0;
static bool full_sorted = false;
static long long rank_scan_work = 0;

// Thresholds used to choose between local prefix repair, partial repair, and
// full repair. They affect when ScoreBoard is rearranged, not what the answer is.
static const int PREFIX_KEEP_AFTER_FULL = 2980;
static const int PARTIAL_REPAIR_TARGET = 3100;
static const int REMOVE_PRESERVE_LIMIT = 256;
static const int TOPK_FULL_SORT_LIMIT = 48;
static const int QUICKSELECT_MIN_K = 8;
static const int QUICKSELECT_MIN_MISSING = 5;
static const int QUICKSELECT_MIN_SUFFIX = 32;
static const long long RANK_SCAN_REPAIR_LIMIT = 855000;

// Compare two ScoreBoard indices in leaderboard order.
// Score comparison and tie-breaking both go through ScoreBoard APIs.
static bool idx_better(ScoreBoard &board, int a, int b) {
  if (a == b) return false;

  int cmp_score = board.compare_score(a, b);
  if (cmp_score > 0) return true;
  if (cmp_score < 0) return false;

  int b_id = board.read_id(b);
  return board.compare_id(a, b_id) < 0;
}

// Compare a ScoreBoard entry against a submitted key.
// The key is temporary input; stored scores are still read from ScoreBoard.
static bool entry_better_than_key(ScoreBoard &board,
                                  int idx,
                                  int score,
                                  int player_id) {
  int cmp_score = board.compare_score_val(idx, score);
  if (cmp_score > 0) return true;
  if (cmp_score < 0) return false;

  return board.compare_id(idx, player_id) < 0;
}

// Reverse comparison used when deciding whether a new or updated player can
// enter the maintained prefix.
static bool key_better_than_entry(ScoreBoard &board,
                                  int score,
                                  int player_id,
                                  int idx) {
  int cmp_score = board.compare_score_val(idx, score);
  if (cmp_score < 0) return true;
  if (cmp_score > 0) return false;

  return board.compare_id(idx, player_id) > 0;
}

// A submit only changes ScoreBoard when the new score improves the old one.
static bool is_improved_score(ScoreBoard &board, int idx, int score) {
  return board.compare_score_val(idx, score) < 0;
}

// Swap two physical ScoreBoard entries and keep the index cache consistent.
// The actual data movement is performed by board.swap().
static void swap_entries(ScoreBoard &board, int a, int b) {
  if (a == b) return;

  int a_id = board.read_id(a);
  int b_id = board.read_id(b);

  board.swap(a, b);

  pos[a_id] = b;
  pos[b_id] = a;
}

// Same as swap_entries(), but the caller already knows board[a]'s player_id.
// This is only a read-count optimization for the physical-index metadata.
static void swap_entries_known_a(ScoreBoard &board, int a, int b, int a_id) {
  if (a == b) return;

  int b_id = board.read_id(b);

  board.swap(a, b);

  pos[a_id] = b;
  pos[b_id] = a;
}

// Used inside sorting/selection routines. pos is rebuilt afterward from
// ScoreBoard, so it is not updated for every raw swap.
static void raw_swap_entries(ScoreBoard &board, int a, int b) {
  if (a == b) return;
  board.swap(a, b);
}

// Move one known player left inside the maintained prefix using adjacent swaps.
// This preserves the prefix order while updating only physical indices.
static void move_left_by_swaps(ScoreBoard &board,
                               int from,
                               int to,
                               int moving_pid) {
  while (from > to) {
    int other_pid = board.read_id(from - 1);

    board.swap(from, from - 1);

    pos[moving_pid] = from - 1;
    pos[other_pid] = from;

    --from;
  }
}

// Move one known player right, mainly to push a removed prefix entry outward.
// ScoreBoard still performs every swap.
static void move_right_by_swaps(ScoreBoard &board,
                                int from,
                                int to,
                                int moving_pid) {
  while (from < to) {
    int other_pid = board.read_id(from + 1);

    board.swap(from, from + 1);

    pos[moving_pid] = from + 1;
    pos[other_pid] = from;

    ++from;
  }
}

// Find where a temporary key would belong in the already sorted prefix [0, hi).
// This uses binary search over ScoreBoard comparisons.
static int find_insert_pos_prefix(ScoreBoard &board,
                                  int hi,
                                  int score,
                                  int player_id) {
  int lo = 0;

  while (lo < hi) {
    int mid = lo + (hi - lo) / 2;

    if (entry_better_than_key(board, mid, score, player_id)) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }

  return lo;
}

// Rebuild the physical index cache by reading player ids from ScoreBoard.
// No score or rank information is copied into Manager.
static void rebuild_pos(ScoreBoard &board) {
  pos.clear();

  int n = board.size();
  if (n > 0) {
    pos.reserve(n * 2 + 1);
  }

  for (int i = 0; i < n; ++i) {
    pos[board.read_id(i)] = i;
  }
}

// Small ranges are cheaper to finish with insertion sort.
// Ordering checks and swaps still use ScoreBoard APIs.
static void raw_insertion_sort_range(ScoreBoard &board, int lo, int hi) {
  for (int i = lo + 1; i <= hi; ++i) {
    int j = i;
    while (j > lo && idx_better(board, j, j - 1)) {
      raw_swap_entries(board, j, j - 1);
      --j;
    }
  }
}

// Pick a pivot with a few ScoreBoard comparisons to avoid very poor partitions.
static int raw_median_of_three(ScoreBoard &board, int a, int b, int c) {
  if (idx_better(board, a, b)) {
    if (idx_better(board, b, c)) return b;
    if (idx_better(board, a, c)) return c;
    return a;
  }

  if (idx_better(board, a, c)) return a;
  if (idx_better(board, b, c)) return c;
  return b;
}

// Partition [lo, hi] by leaderboard order around a ScoreBoard pivot.
// The pivot id is only read when a score tie actually occurs.
static int raw_partition_range(ScoreBoard &board, int lo, int hi) {
  int mid = lo + (hi - lo) / 2;
  int pivot = raw_median_of_three(board, lo, mid, hi);

  raw_swap_entries(board, pivot, hi);

  // The pivot id is needed only for score ties, so read it lazily.
  int pivot_id = 0;
  bool has_pivot_id = false;
  int store = lo;
  for (int i = lo; i < hi; ++i) {
    int cmp_score = board.compare_score(i, hi);
    bool before = cmp_score > 0;
    if (cmp_score == 0) {
      if (!has_pivot_id) {
        pivot_id = board.read_id(hi);
        has_pivot_id = true;
      }
      before = board.compare_id(i, pivot_id) < 0;
    }
    if (before) {
      raw_swap_entries(board, store, i);
      ++store;
    }
  }

  raw_swap_entries(board, store, hi);
  return store;
}

// Custom quicksort over ScoreBoard entries. The smaller side is handled first
// to keep recursion depth modest.
static void raw_quick_sort_range(ScoreBoard &board, int lo, int hi) {
  while (lo < hi) {
    if (hi - lo <= 5) {
      raw_insertion_sort_range(board, lo, hi);
      return;
    }

    int p = raw_partition_range(board, lo, hi);

    if (p - lo < hi - p) {
      if (lo < p - 1) raw_quick_sort_range(board, lo, p - 1);
      lo = p + 1;
    } else {
      if (p + 1 < hi) raw_quick_sort_range(board, p + 1, hi);
      hi = p - 1;
    }
  }
}

// Custom quickselect used to bring the top portion of a range forward without
// fully sorting the suffix.
static void raw_quick_select_top_range(ScoreBoard &board,
                                       int lo,
                                       int hi,
                                       int target) {
  while (lo < hi) {
    if (hi - lo <= 5) {
      raw_insertion_sort_range(board, lo, hi);
      return;
    }

    int p = raw_partition_range(board, lo, hi);

    if (p == target) {
      return;
    }

    if (target < p) {
      hi = p - 1;
    } else {
      lo = p + 1;
    }
  }
}

// Restore the whole ScoreBoard into exact leaderboard order.
// After raw swaps, pos is rebuilt by reading ids from ScoreBoard.
static void full_repair_sort(ScoreBoard &board) {
  int n = board.size();

  if (n > 1) {
    raw_quick_sort_range(board, 0, n - 1);
  }

  rebuild_pos(board);
  prefix_len = n;
  full_sorted = true;
  rank_scan_work = 0;
}

static void drop_full_sorted_for_mutation(ScoreBoard &board) {
  // After a mutation, the whole board may no longer be sorted. Still, keeping
  // a large prefix is useful because top_k() often asks for nearby k values.
  if (!full_sorted) return;

  int n = board.size();

  if (prefix_len > PREFIX_KEEP_AFTER_FULL) {
    prefix_len = PREFIX_KEEP_AFTER_FULL;
  }
  if (prefix_len > n) {
    prefix_len = n;
  }

  full_sorted = false;
}

// Normalize metadata after submit/remove. If the maintained prefix covers all
// entries, ScoreBoard is fully sorted again.
static void finish_after_mutation(ScoreBoard &board) {
  int n = board.size();

  if (prefix_len > n) {
    prefix_len = n;
  }

  full_sorted = (prefix_len == n);
}

static void insert_candidate_into_prefix(ScoreBoard &board,
                                         int idx,
                                         int score,
                                         int player_id) {
  // If the changed entry can enter the maintained top-prefix, put it near the
  // end of the prefix and bubble it left to its correct prefix position.
  if (prefix_len <= 0) return;
  if (prefix_len >= board.size()) return;

  if (!key_better_than_entry(board, score, player_id, prefix_len - 1)) {
    return;
  }

  if (idx != prefix_len) {
    swap_entries_known_a(board, idx, prefix_len, player_id);
  }

  int dest = find_insert_pos_prefix(board, prefix_len, score, player_id);
  move_left_by_swaps(board, prefix_len, dest, player_id);
}

// Find the best entry in the unsorted tail using ScoreBoard comparisons.
static int find_best_index(ScoreBoard &board, int start, int n) {
  int best = start;

  for (int i = start + 1; i < n; ++i) {
    if (idx_better(board, i, best)) {
      best = i;
    }
  }

  return best;
}

// Extend the maintained prefix one entry at a time. This is useful for small k,
// where selection is cheaper than repairing a large range.
static void ensure_prefix(ScoreBoard &board, int need) {
  int n = board.size();
  if (need > n) {
    need = n;
  }

  while (prefix_len < need) {
    int best = find_best_index(board, prefix_len, n);
    swap_entries(board, prefix_len, best);
    ++prefix_len;
  }

  full_sorted = (prefix_len == n);
}

static void quickselect_extend_prefix(ScoreBoard &board, int need) {
  // Extends the sorted prefix without sorting the whole board. The old prefix
  // is already correct, so only the suffix part is selected/sorted.
  int n = board.size();
  if (need > n) {
    need = n;
  }
  if (need <= prefix_len) {
    return;
  }

  int old_prefix = prefix_len;

  raw_quick_select_top_range(board, old_prefix, n - 1, need - 1);
  if (old_prefix < need - 1) {
    raw_quick_sort_range(board, old_prefix, need - 1);
  }

  rebuild_pos(board);
  prefix_len = need;
  full_sorted = (prefix_len == n);
}

static void partial_repair_prefix(ScoreBoard &board, int need) {
  // For large top_k(), a full sort is often more than necessary. This makes
  // only the first 'need' entries the true sorted top-prefix.
  int n = board.size();
  if (need > n) {
    need = n;
  }
  if (need <= 0) {
    prefix_len = 0;
    full_sorted = (n == 0);
    rank_scan_work = 0;
    return;
  }

  if (need < n) {
    raw_quick_select_top_range(board, 0, n - 1, need - 1);
  }
  if (need > 1) {
    raw_quick_sort_range(board, 0, need - 1);
  }

  rebuild_pos(board);
  prefix_len = need;
  full_sorted = (prefix_len == n);
  rank_scan_work = 0;
}

// Initialize Manager-side metadata before the operation stream starts.
// ScoreBoard contents are not mirrored outside ScoreBoard.
void init(ScoreBoard &board, int num_players) {
  (void)board;

  // ScoreBoard starts empty. Only Manager-side metadata is reset here.
  pos.clear();
  if (num_players > 0) {
    pos.reserve(num_players * 2 + 1);
  }

  prefix_len = 0;
  full_sorted = false;
  rank_scan_work = 0;
}

// Insert a new player or improve an existing player's score.
// The stored score is written only through ScoreBoard APIs.
void submit(ScoreBoard &board, int player_id, int score) {
  auto it = pos.find(player_id);

  if (it == pos.end()) {
    // New players are always appended to ScoreBoard first. The prefix is fixed
    // only if this new entry can affect the maintained top region.
    drop_full_sorted_for_mutation(board);

    int idx = board.size();
    board.write(idx, player_id, score);
    pos[player_id] = idx;

    if (idx == 0) {
      prefix_len = 1;
      full_sorted = true;
      return;
    }

    insert_candidate_into_prefix(board, idx, score, player_id);
    finish_after_mutation(board);
    return;
  }

  int idx = it->second;

  if (!is_improved_score(board, idx, score)) {
    return;
  }

  // Existing score is updated inside ScoreBoard. Since scores only improve, an
  // entry can only move toward the front of the leaderboard.
  drop_full_sorted_for_mutation(board);

  idx = pos[player_id];
  board.write_score(idx, score);

  if (idx < prefix_len) {
    int dest = find_insert_pos_prefix(board, idx, score, player_id);
    move_left_by_swaps(board, idx, dest, player_id);
  } else {
    insert_candidate_into_prefix(board, idx, score, player_id);
  }

  finish_after_mutation(board);
}

// Return the top k player ids after physically repairing the ScoreBoard prefix.
// The result vector is filled by reading player ids from ScoreBoard.
std::vector<int> top_k(ScoreBoard &board, int k) {
  std::vector<int> result;

  if (k <= 0) return result;

  int n = board.size();
  if (k > n) {
    k = n;
  }

  if (k > prefix_len) {
    // Before returning, physically repair ScoreBoard[0..k-1]. The returned
    // vector below is then built by reading those positions from ScoreBoard.
    if (k == n || k > TOPK_FULL_SORT_LIMIT || (k > 64 && k * 4 > n * 3)) {
      int target = PARTIAL_REPAIR_TARGET;
      if (target < k) {
        target = k;
      }
      if (target >= n) {
        full_repair_sort(board);
      } else {
        partial_repair_prefix(board, target);
      }
    } else if (k >= QUICKSELECT_MIN_K &&
               k - prefix_len >= QUICKSELECT_MIN_MISSING &&
               n - prefix_len >= QUICKSELECT_MIN_SUFFIX) {
      quickselect_extend_prefix(board, k);
    } else {
      ensure_prefix(board, k);
    }
  }

  result.reserve(k);
  for (int i = 0; i < k; ++i) {
    result.push_back(board.read_id(i));
  }

  return result;
}

// Return the 0-based rank of a player based on the current ScoreBoard state.
// pos only helps locate the physical entry; it is not independent rank data.
int rank(ScoreBoard &board, int player_id) {
  auto it = pos.find(player_id);
  if (it == pos.end()) return -1;

  int idx = it->second;

  if (full_sorted || idx < prefix_len) {
    // In these cases the physical index is the rank. read_id() is used as a
    // final check that the index cache still points to the requested player.
    if (idx >= 0 && idx < board.size() && board.read_id(idx) == player_id) {
      return idx;
    }
    return -1;
  }

  int n = board.size();
  int scan_len = n - prefix_len;
  rank_scan_work += scan_len;

  if (rank_scan_work > RANK_SCAN_REPAIR_LIMIT && n > 1) {
    full_repair_sort(board);
    idx = pos[player_id];
    if (idx >= 0 && idx < board.size() && board.read_id(idx) == player_id) {
      return idx;
    }
    return -1;
  }

  int result = prefix_len;

  // The target is outside the maintained prefix. Count only suffix entries
  // that rank before it; all prefix entries are already before it.
  for (int i = prefix_len; i < n; ++i) {
    if (i == idx) continue;

    int cmp_score = board.compare_score(i, idx);

    if (cmp_score > 0) {
      ++result;
    } else if (cmp_score == 0 && board.compare_id(i, player_id) < 0) {
      ++result;
    }
  }

  return result;
}

// Physically remove a player from ScoreBoard and update physical-index metadata.
// No lazy deletion or separate validity table is used.
void remove(ScoreBoard &board, int player_id) {
  auto it = pos.find(player_id);
  if (it == pos.end()) return;

  // The entry is physically removed from ScoreBoard. If it was inside the
  // maintained prefix, shrink the prefix rather than immediately refilling it.
  drop_full_sorted_for_mutation(board);

  int idx = it->second;
  int last = board.size() - 1;

  if (idx < prefix_len && prefix_len <= REMOVE_PRESERVE_LIMIT) {
    move_right_by_swaps(board, idx, prefix_len - 1, player_id);

    int remove_idx = prefix_len - 1;
    --prefix_len;

    if (remove_idx != last) {
      swap_entries_known_a(board, remove_idx, last, player_id);
    }

    board.remove_last();
    pos.erase(player_id);
  } else {
    if (idx < prefix_len) {
      prefix_len = idx;
    }

    if (idx != last) {
      swap_entries_known_a(board, idx, last, player_id);
    }

    board.remove_last();
    pos.erase(player_id);
  }

  finish_after_mutation(board);
}
