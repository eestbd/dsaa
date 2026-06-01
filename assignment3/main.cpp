#include "Leaderboard.h"
#include "Manager.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#define UNIT_TEST 1000
#define DEBUG 1

// ============================================================
// Reference implementation for correctness checking
// ============================================================
// 정답 비교용 클래스
class ReferenceSolution {
private:
  std::map<int, int> scores; // player_id -> score

public:
  void submit(int player_id, int score) {
    auto it = scores.find(player_id);
    if (it == scores.end()) {
      scores[player_id] = score;
    } else {
      if (score > it->second) {
        it->second = score;
      }
    }
  }

  std::vector<int> top_k(int k) {
    std::vector<std::pair<int, int>> entries(scores.begin(), scores.end());
    int actual_k = std::min((int)entries.size(), k);
    std::partial_sort(entries.begin(), entries.begin() + actual_k,
                      entries.end(), [](const auto &a, const auto &b) {
                        if (a.second != b.second)
                          return a.second > b.second;
                        return a.first < b.first;
                      });
    std::vector<int> result;
    for (int i = 0; i < actual_k; i++) {
      result.push_back(entries[i].first);
    }
    return result;
  }

  int rank(int player_id) {
    auto it = scores.find(player_id);
    if (it == scores.end())
      return -1;
    int target_score = it->second;
    int r = 0;
    for (auto &p : scores) {
      if (p.second > target_score) {
        r++;
      } else if (p.second == target_score && p.first < player_id) {
        r++;
      }
    }
    return r;
  }

  void remove(int player_id) { scores.erase(player_id); }

  int size() const { return (int)scores.size(); }

  int get_score(int player_id) const {
    auto it = scores.find(player_id);
    return it == scores.end() ? -1 : it->second;
  }
};

// ============================================================
// Run operations from data file
// ============================================================
// 데이터 파일의 명령대로 Leaderboard를 실제로 실행하는 함수
bool run_data(const std::string &filename) {
  int num_players;
  std::vector<Operation> ops;

  if (!load_operations(filename, num_players, ops)) {
    return false;
  }

  std::cout << filename << " received with " << ops.size()
            << " operations, max " << num_players << " players" << std::endl;

  // Student's solution
  ScoreBoard board(num_players + 1);
  board.init_counters();
  init(board, num_players);

  // Reference solution
  ReferenceSolution ref;

  bool correct = true;
  int op_idx = 0;

  for (auto &op : ops) {
    op_idx++;
    switch (op.type) {
    case OP_SUBMIT: {
      submit(board, op.arg1, op.arg2);
      ref.submit(op.arg1, op.arg2);
      break;
    }
    case OP_TOP_K: {
      std::vector<int> student_result = top_k(board, op.arg1);
      std::vector<int> ref_result = ref.top_k(op.arg1);
#if DEBUG
      std::cout << "> ";
      for (size_t x = 0; x < student_result.size(); x++) {
        std::cout << student_result[x]
                  << (x + 1 == student_result.size() ? "" : ", ");
      }
      std::cout << std::endl;
#endif
      if (student_result != ref_result) {
        std::cerr << "MISMATCH at operation " << op_idx << ": top_k(" << op.arg1
                  << ")" << std::endl;
        std::cerr << "  Expected:";
        for (int x : ref_result)
          std::cerr << " " << x;
        std::cerr << std::endl;
        std::cerr << "  Got:     ";
        for (int x : student_result)
          std::cerr << " " << x;
        std::cerr << std::endl;
        correct = false;
      } else {
        if ((int)board.size() < (int)ref_result.size()) {
          std::cerr << "STATE MISMATCH at operation " << op_idx << ": top_k("
                    << op.arg1 << ")" << std::endl;
          std::cerr << "  ScoreBoard size (" << board.size()
                    << ") is smaller than expected (" << ref_result.size()
                    << ")." << std::endl;
          correct = false;
        } else {
          int check_k = ref_result.size();
          for (int idx = 0; idx < check_k; idx++) {
            ScoreEntry raw = board.get_raw(idx);
            if (raw.player_id != ref_result[idx] ||
                raw.score != ref.get_score(raw.player_id)) {
              std::cerr << "STATE MISMATCH at operation " << op_idx
                        << ": top_k(" << op.arg1 << ")" << std::endl;
              std::cerr << "  The elements in ScoreBoard[0..." << check_k - 1
                        << "] are not sorted properly or have wrong scores."
                        << std::endl;
              std::cerr << "  Expected player_id " << ref_result[idx]
                        << " with score " << ref.get_score(ref_result[idx])
                        << " at index " << idx << ", but found "
                        << raw.player_id << " with score " << raw.score
                        << std::endl;
              correct = false;
              break;
            }
          }
        }
      }
      break;
    }
    case OP_RANK: {
      int student_result = rank(board, op.arg1);
      int ref_result = ref.rank(op.arg1);
#if DEBUG
      std::cout << "> " << student_result << std::endl;
#endif
      if (student_result != ref_result) {
        std::cerr << "MISMATCH at operation " << op_idx << ": rank(" << op.arg1
                  << ")"
                  << " expected=" << ref_result << " got=" << student_result
                  << std::endl;
        correct = false;
      }
      break;
    }
    case OP_REMOVE: {
      remove(board, op.arg1);
      ref.remove(op.arg1);
      break;
    }
    }
    if (!correct)
      break;
  }

  if (correct) {
    std::cout << "Correct result!" << std::endl;
  } else {
    std::cout << "Incorrect result!" << std::endl;
  }

  std::cout << "[ Result for " << filename << " ]" << std::endl;
  board.print_stats();

  return correct;
}

// ============================================================
// Unit test with random operations
// ============================================================
// 랜덤 테스트를 실행하는 함수
bool run_unit_test(int n) {
  std::cout << "Unit test activated with " << n << " operations" << std::endl;

  srand(42);

  int num_players = n;
  ScoreBoard board(num_players + 1);
  board.init_counters();
  init(board, num_players);
  ReferenceSolution ref;

  bool correct = true;

  for (int i = 0; i < n; i++) {
    int op_type = rand() % 10;

    if (op_type < 5) {
      // submit (50%)
      int pid = rand() % (num_players / 2 + 1) + 1;
      int score = rand() % 1000;
      submit(board, pid, score);
      ref.submit(pid, score);
    } else if (op_type < 7) {
      // top_k (20%)
      if (ref.size() > 0) {
        int k = rand() % std::min(ref.size(), 5) + 1;
        auto sr = top_k(board, k);
        auto rr = ref.top_k(k);
#if DEBUG
        std::cout << "> ";
        for (size_t x = 0; x < sr.size(); x++) {
          std::cout << sr[x] << (x + 1 == sr.size() ? "" : ", ");
        }
        std::cout << std::endl;
#endif
        if (sr != rr) {
          std::cerr << "MISMATCH at unit test op " << i << ": top_k(" << k
                    << ")" << std::endl;
          std::cerr << "  Expected:";
          for (int x : rr)
            std::cerr << " " << x;
          std::cerr << std::endl;
          std::cerr << "  Got:     ";
          for (int x : sr)
            std::cerr << " " << x;
          std::cerr << std::endl;
          correct = false;
          break;
        } else {
          if ((int)board.size() < (int)rr.size()) {
            std::cerr << "STATE MISMATCH at unit test op " << i << ": top_k("
                      << k << ")" << std::endl;
            std::cerr << "  ScoreBoard size is smaller than expected."
                      << std::endl;
            correct = false;
          } else {
            int check_k = rr.size();
            for (int idx = 0; idx < check_k; idx++) {
              ScoreEntry raw = board.get_raw(idx);
              if (raw.player_id != rr[idx] ||
                  raw.score != ref.get_score(raw.player_id)) {
                std::cerr << "STATE MISMATCH at unit test op " << i
                          << ": top_k(" << k << ")" << std::endl;
                std::cerr << "  ScoreBoard[0..." << check_k - 1
                          << "] not sorted properly or have wrong scores."
                          << std::endl;
                std::cerr << "  Expected player_id " << rr[idx]
                          << " with score " << ref.get_score(rr[idx])
                          << " at index " << idx << ", but found "
                          << raw.player_id << " with score " << raw.score
                          << std::endl;
                correct = false;
                break;
              }
            }
          }
          if (!correct)
            break;
        }
      }
    } else if (op_type < 9) {
      // rank (20%)
      if (ref.size() > 0) {
        int pid = rand() % (num_players / 2 + 1) + 1;
        int sr = rank(board, pid);
        int rr = ref.rank(pid);
#if DEBUG
        std::cout << "> " << sr << std::endl;
#endif
        if (sr != rr) {
          std::cerr << "MISMATCH at unit test op " << i << ": rank(" << pid
                    << ")"
                    << " expected=" << rr << " got=" << sr << std::endl;
          correct = false;
          break;
        }
      }
    } else {
      // remove (10%)
      if (ref.size() > 0) {
        int pid = rand() % (num_players / 2 + 1) + 1;
        remove(board, pid);
        ref.remove(pid);
      }
    }
  }

  if (correct) {
    std::cout << "Correct result!" << std::endl;
  } else {
    std::cout << "Incorrect result!" << std::endl;
  }

  std::cout << "[ Result for unit_test ]" << std::endl;
  board.print_stats();

  return correct;
}

int main(int argc, char *argv[]) {
  // argc : 전달되는 문자열 개수
  // argv : 전달되는 문자열들
  // 예시 : ./Leaderboard data/test1.txt
  // argv[0] : "./Leaderboard", arvd[1] : "data/test1.txt"
  if (argc < 2) {
    std::cerr << "Usage: ./leaderboard data/<data_name>" << std::endl;
    std::cerr << "       ./leaderboard unit_test" << std::endl;
    return 1;
  }

  std::string arg(argv[1]);   // argv[1] 값을 string 형태로 저장
                              // 예시 : argv[1] = "unit_test"

  if (arg == "unit_test") {
    bool ok = run_unit_test(UNIT_TEST);   // 랜덤 테스트 실행
                                          // UNIT_TEST : 테스트 횟수
    return ok ? 0 : 1;
  } else {
    bool ok = run_data(arg);  // unit_test가 아니면 data 파일 실행 모드라고 판단
    return ok ? 0 : 1;
  }
}
