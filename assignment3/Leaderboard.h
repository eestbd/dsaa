#ifndef LEADERBOARD_H
#define LEADERBOARD_H

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ============================================================
// Operation types for the leaderboard
// ============================================================
enum OpType {
  OP_SUBMIT, // submit(player_id, score)
  OP_TOP_K,  // top_k(k) -> returns top k player_ids by score descending
  OP_RANK,   // rank(player_id) -> returns 1-based rank
  OP_REMOVE  // remove(player_id)
};

struct Operation {
  OpType type;
  int arg1; // player_id for SUBMIT/RANK/REMOVE, k for TOP_K
  int arg2; // score for SUBMIT, unused otherwise
};

// ============================================================
// ScoreEntry: a (player_id, score) pair stored in internal array
// ============================================================
struct ScoreEntry {
  int player_id;
  int score;
};

// ============================================================
// ScoreBoard class
// - Wraps an internal array of ScoreEntry
// - All access must go through provided APIs
// - Each API call increments the access counter
// ============================================================
bool run_data(const std::string &filename);
bool run_unit_test(int n);

class ScoreBoard {
private:
  friend bool ::run_data(const std::string &filename);
  friend bool ::run_unit_test(int n);

  ScoreEntry *m_data;
  int m_size;     // current number of entries
  int m_capacity; // max capacity

  long long m_read_count;
  long long m_write_count;
  long long m_compare_count;
  long long m_swap_count;

public:
  ScoreBoard(int capacity) {
    m_capacity = capacity;
    m_size = 0;
    m_data = new ScoreEntry[capacity];
    m_read_count = 0;
    m_write_count = 0;
    m_compare_count = 0;
    m_swap_count = 0;
  }

  ~ScoreBoard() { delete[] m_data; }

  // Returns the number of entries currently stored
  int size() const { return m_size; }

  // Returns the max capacity
  int capacity() const { return m_capacity; }

  // Read player_id at index i
  int read_id(int i) {
    assert(i >= 0 && i < m_size);
    m_read_count++;
    return m_data[i].player_id;
  }

  // Read score at index i
  int read_score(int i) {
    assert(i >= 0 && i < m_size);
    m_read_count++;
    return m_data[i].score;
  }

  // Write (player_id, score) at index i
  // Can write at index == m_size to append (increases size by 1)
  void write(int i, int player_id, int score) {
    assert(i >= 0 && i <= m_size);
    if (i == m_size) {
      assert(m_size < m_capacity);
      m_size++;
    }
    m_write_count++;
    m_data[i].player_id = player_id;
    m_data[i].score = score;
  }

  // Overwrite only the score at index i
  void write_score(int i, int score) {
    assert(i >= 0 && i < m_size);
    m_write_count++;
    m_data[i].score = score;
  }

  // Swap entries at index i and j
  void swap(int i, int j) {
    assert(i >= 0 && i < m_size);
    assert(j >= 0 && j < m_size);
    m_swap_count++;
    ScoreEntry tmp = m_data[i];
    m_data[i] = m_data[j];
    m_data[j] = tmp;
  }

  // Compare scores at index i and j
  // Returns negative if score[i] < score[j],
  // 0 if equal,
  // positive if score[i] > score[j]
  int compare_score(int i, int j) {
    assert(i >= 0 && i < m_size);
    assert(j >= 0 && j < m_size);
    m_compare_count++;
    return m_data[i].score - m_data[j].score;
  }

  // Compare score at index i with a given value
  // Returns negative if score[i] < val, 0 if equal, positive if score[i] > val
  int compare_score_val(int i, int val) {
    assert(i >= 0 && i < m_size);
    m_compare_count++;
    return m_data[i].score - val;
  }

  // Compare player_id at index i with a given value
  // Returns negative if id[i] < val, 0 if equal, positive if id[i] > val
  int compare_id(int i, int val) {
    assert(i >= 0 && i < m_size);
    m_compare_count++;
    return m_data[i].player_id - val;
  }

  // Remove entry at index i by shifting everything after it left
  // This physically shifts elements, incurring Read/Write costs per shift!
  void remove_at(int i) {
    assert(i >= 0 && i < m_size);
    for (int j = i; j < m_size - 1; j++) {
      m_data[j] = m_data[j + 1];
      m_read_count++;
      m_write_count++;
    }
    m_size--;
  }

  // Remove the last entry (O(1))
  void remove_last() {
    assert(m_size > 0);
    m_write_count++;
    m_size--;
  }

private:
  void init_counters() {
    m_read_count = 0;
    m_write_count = 0;
    m_compare_count = 0;
    m_swap_count = 0;
  }

  long long get_read_count() const { return m_read_count; }
  long long get_write_count() const { return m_write_count; }
  long long get_compare_count() const { return m_compare_count; }
  long long get_swap_count() const { return m_swap_count; }

  long long get_total_cost() const {
    return m_read_count + m_write_count + m_compare_count + m_swap_count;
  }

  void print_stats() const {
    std::cout << " => Read Count: " << m_read_count
              << " Write Count: " << m_write_count
              << " Compare Count: " << m_compare_count
              << " Swap Count: " << m_swap_count << std::endl;
    std::cout << " => Total Access Count: " << get_total_cost() << std::endl;
  }

  // For verification: get raw data
  ScoreEntry get_raw(int i) const { return m_data[i]; }
};

// ============================================================
// Parse operations from data file
// ============================================================
inline bool load_operations(const std::string &filename, int &num_players,
                            std::vector<Operation> &ops) {
  std::ifstream fin(filename);
  if (!fin.is_open()) {
    std::cerr << "Error: cannot open " << filename << std::endl;
    return false;
  }

  int num_ops;
  fin >> num_players >> num_ops;

  ops.resize(num_ops);
  for (int i = 0; i < num_ops; i++) {
    std::string op_str;
    fin >> op_str;
    if (op_str == "submit") {
      ops[i].type = OP_SUBMIT;
      fin >> ops[i].arg1 >> ops[i].arg2;
    } else if (op_str == "top_k") {
      ops[i].type = OP_TOP_K;
      fin >> ops[i].arg1;
      ops[i].arg2 = 0;
    } else if (op_str == "rank") {
      ops[i].type = OP_RANK;
      fin >> ops[i].arg1;
      ops[i].arg2 = 0;
    } else if (op_str == "remove") {
      ops[i].type = OP_REMOVE;
      fin >> ops[i].arg1;
      ops[i].arg2 = 0;
    } else {
      std::cerr << "Error: unknown operation " << op_str << std::endl;
      return false;
    }
  }
  fin.close();
  return true;
}

#endif // LEADERBOARD_H