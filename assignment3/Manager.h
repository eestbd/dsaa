// Manager.h

#ifndef MANAGER_H
#define MANAGER_H

#include "Leaderboard.h"
#include <vector>

// ============================================================
// Functions to be implemented by students in Manager.cpp
//
// board: ScoreBoard object that stores (player_id, score) pairs.
//        Use only the provided APIs (read_id, read_score, write,
//        write_score, swap, compare_score, compare_score_val,
//        compare_id, remove_at, remove_last, size, capacity).
//        ScoreBoard is the source of truth for valid entries and scores.
//
// [IMPORTANT RULES]
// 1. You MAY use STL data structures (std::vector, std::map,
//    std::unordered_map, etc.) for auxiliary caching/mapping.
// 2. You MUST NOT use any C++ standard sorting libraries
//    (e.g., std::sort, std::stable_sort, std::partial_sort,
//    std::nth_element, qsort) or any sorting-related functions
//    from <algorithm>. You must implement your own sorting logic.
// 3. All score comparisons and swaps for sorting MUST be done
//    inside the ScoreBoard using board.compare_score() and
//    board.swap() APIs.
// 4. When top_k(board, k) is called, not only should
//    it return the correct vector, but the ScoreBoard's internal
//    array from index 0 to k-1 MUST actually contain those top k
//    players in descending order. The grading script will verify
//    the internal state of the ScoreBoard.
// ============================================================

// Called once before processing operations.
// Use this to initialize any auxiliary data structures.
// Manager.cpp only resets metadata here; ScoreBoard itself is managed by the
// caller and by the provided ScoreBoard APIs.
void init(ScoreBoard &board, int num_players);

// Submit a score for a player.
// If the player already exists, update only if the new score is higher.
// If the player does not exist, add them.
// The actual (player_id, score) pair is always written to ScoreBoard.
void submit(ScoreBoard &board, int player_id, int score);

// Return the top k player_ids in descending order of score.
// If two players have the same score,
// the one with the smaller player_id comes first.
// The returned player_ids should be read from the repaired ScoreBoard prefix.
std::vector<int> top_k(ScoreBoard &board, int k);

// Return the 0-based rank of the given player.
// Rank 0 = highest score. Same-score ties broken by smaller player_id = higher
// rank. If the player does not exist, return -1.
// Auxiliary metadata may help locate a ScoreBoard index, but it does not store
// scores or independent rank/order data.
int rank(ScoreBoard &board, int player_id);

// Remove the player from the board.
// If the player does not exist, do nothing.
// Removal is physical: the entry is removed from ScoreBoard, not lazily marked.
void remove(ScoreBoard &board, int player_id);

#endif // MANAGER_H
