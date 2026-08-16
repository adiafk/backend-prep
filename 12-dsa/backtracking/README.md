# Backtracking

## 1. The Core Template — Choose, Explore, Unchoose

Backtracking is systematic trial-and-error on a decision tree. At each node you make a choice, recurse deeper, then undo the choice so you can try the next option. The key invariant: **the state before and after each recursive call must be identical**.

```
void backtrack(state, choices) {
    if (base_case(state)) {
        record(state);
        return;
    }
    for each choice in choices {
        if (!isValid(state, choice)) continue;  // pruning

        // CHOOSE
        apply(state, choice);

        // EXPLORE
        backtrack(state, remaining_choices);

        // UNCHOOSE (undo)
        undo(state, choice);
    }
}
```

The three steps are non-negotiable:
- **Choose** — modify shared state (push to path, mark visited, decrement target, etc.)
- **Explore** — recurse with updated state
- **Unchoose** — reverse the modification exactly (pop from path, unmark visited, restore target, etc.)

---

## 2. When to Use Backtracking

| Problem type | Signal in the problem statement |
|---|---|
| Permutations | "all orderings", "arrange" |
| Combinations | "choose k from n", "k-element subsets" |
| Subsets / Power Set | "all subsets", "generate all" |
| Constraint Satisfaction | "place queens", "valid sudoku", "word search" |
| Partition problems | "split array into groups satisfying X" |
| String generation | "generate valid parentheses", "phone number letters" |

Backtracking is appropriate when:
1. You need **all** solutions (not just one, not just count).
2. The search space is a tree or DAG of discrete choices.
3. You can prune invalid branches early.

It is **not** appropriate when the number of valid solutions is exponential and you only need one — use greedy or DP instead.

---

## 3. Pruning — Cutting the Search Space

Pruning is what makes backtracking practical. A solution that explores every branch before checking validity is just brute force.

### Pruning strategies

**Constraint pruning (most common)**
Check the validity condition before recursing, not after.
```cpp
// Bad — explores then rejects
explore();
if (!valid(state)) return;

// Good — rejects before exploring
if (!valid(state)) return;
explore();
```

**Feasibility pruning**
If the current partial solution can never lead to a complete solution, cut early.
Example in Combination Sum: if `remaining < 0`, no deeper sum can help.

**Symmetry / deduplication pruning**
Sort input + skip duplicates at the same recursion depth to avoid identical branches.
```cpp
for (int i = start; i < n; i++) {
    if (i > start && nums[i] == nums[i-1]) continue;  // skip duplicate branch
    // ...
}
```

**Bound pruning**
Compute a lower/upper bound on what is achievable from the current state. If the bound cannot reach the target, prune.

**Ordering**
Place the most constrained variable first (fail fast). In N-Queens, try the column with the fewest valid squares first.

---

## 4. Five Solved Problems

---

### Problem 1 — Subsets (LeetCode 78)

**Problem:** Given an integer array `nums` of unique elements, return all possible subsets.

**Approach:** At each index we make a binary choice — include or exclude. Alternatively, iterate forward and choose which subsequent elements to add. Because there are no duplicates, no deduplication is needed.

**Key insight:** We record the answer at every node of the tree (not just leaves), because every partial path is a valid subset.

```cpp
#include <vector>
using namespace std;

class Solution {
public:
    vector<vector<int>> result;
    vector<int> path;

    void backtrack(vector<int>& nums, int start) {
        // Record current subset at every node (including empty set)
        result.push_back(path);

        for (int i = start; i < (int)nums.size(); i++) {
            path.push_back(nums[i]);         // CHOOSE
            backtrack(nums, i + 1);          // EXPLORE (i+1: no reuse)
            path.pop_back();                 // UNCHOOSE
        }
    }

    vector<vector<int>> subsets(vector<int>& nums) {
        backtrack(nums, 0);
        return result;
    }
};
```

**Complexity:** Time O(2^n * n) — 2^n subsets, each copied in O(n). Space O(n) recursion depth.

---

### Problem 2 — Permutations (LeetCode 46)

**Problem:** Given an array `nums` of distinct integers, return all permutations.

**Approach:** At each step pick any unused number. Track which numbers are used with a boolean array (or by swapping in-place). Unlike subsets, we recurse into all positions, and the base case is a full-length path.

```cpp
#include <vector>
using namespace std;

class Solution {
public:
    vector<vector<int>> result;
    vector<int> path;

    void backtrack(vector<int>& nums, vector<bool>& used) {
        if ((int)path.size() == (int)nums.size()) {
            result.push_back(path);
            return;
        }
        for (int i = 0; i < (int)nums.size(); i++) {
            if (used[i]) continue;           // pruning: already in path

            used[i] = true;
            path.push_back(nums[i]);         // CHOOSE
            backtrack(nums, used);           // EXPLORE
            path.pop_back();                 // UNCHOOSE
            used[i] = false;
        }
    }

    vector<vector<int>> permute(vector<int>& nums) {
        vector<bool> used(nums.size(), false);
        backtrack(nums, used);
        return result;
    }
};
```

**Complexity:** Time O(n! * n). Space O(n).

**Variant with duplicates (LeetCode 47):** Sort first, then add:
```cpp
if (i > 0 && nums[i] == nums[i-1] && !used[i-1]) continue;
```

---

### Problem 3 — Combination Sum (LeetCode 39)

**Problem:** Given an array of distinct positive integers `candidates` and a target, return all combinations that sum to target. The same number may be used unlimited times.

**Approach:** At each step either reuse the current candidate (recurse with same `start`) or move to the next (recurse with `start+1`). Prune when `remaining < 0`.

```cpp
#include <vector>
#include <algorithm>
using namespace std;

class Solution {
public:
    vector<vector<int>> result;
    vector<int> path;

    void backtrack(vector<int>& candidates, int start, int remaining) {
        if (remaining == 0) {
            result.push_back(path);
            return;
        }
        for (int i = start; i < (int)candidates.size(); i++) {
            // Pruning: sorted array — if current > remaining, all larger ones too
            if (candidates[i] > remaining) break;

            path.push_back(candidates[i]);                 // CHOOSE
            backtrack(candidates, i, remaining - candidates[i]); // EXPLORE (i, not i+1: reuse allowed)
            path.pop_back();                               // UNCHOOSE
        }
    }

    vector<vector<int>> combinationSum(vector<int>& candidates, int target) {
        sort(candidates.begin(), candidates.end());  // enables break pruning
        backtrack(candidates, 0, target);
        return result;
    }
};
```

**Complexity:** Time O(n^(T/M)) where T = target, M = min candidate. Space O(T/M) recursion depth.

---

### Problem 4 — Word Search (LeetCode 79)

**Problem:** Given an m×n grid of characters and a string `word`, return true if the word exists in the grid following adjacent (up/down/left/right) cells without reusing a cell.

**Approach:** DFS/backtracking from every cell. Mark cells as visited by temporarily modifying the grid (`'#'`), then restore on backtrack.

```cpp
#include <vector>
#include <string>
using namespace std;

class Solution {
public:
    int rows, cols;

    bool backtrack(vector<vector<char>>& board, const string& word,
                   int r, int c, int idx) {
        if (idx == (int)word.size()) return true;   // base case: all chars matched

        // Bounds check + character match + visited check
        if (r < 0 || r >= rows || c < 0 || c >= cols) return false;
        if (board[r][c] != word[idx]) return false;

        char temp = board[r][c];
        board[r][c] = '#';                          // CHOOSE (mark visited)

        // EXPLORE all 4 directions
        bool found = backtrack(board, word, r+1, c, idx+1) ||
                     backtrack(board, word, r-1, c, idx+1) ||
                     backtrack(board, word, r, c+1, idx+1) ||
                     backtrack(board, word, r, c-1, idx+1);

        board[r][c] = temp;                         // UNCHOOSE (restore)
        return found;
    }

    bool exist(vector<vector<char>>& board, string word) {
        rows = board.size();
        cols = board[0].size();
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++)
                if (backtrack(board, word, r, c, 0)) return true;
        return false;
    }
};
```

**Complexity:** Time O(m*n * 4^L) where L = word length. Space O(L) recursion depth.

**Pruning opportunity:** Count character frequencies upfront. If the word requires more of some character than the grid contains, return false immediately.

---

### Problem 5 — N-Queens (LeetCode 51)

**Problem:** Place n queens on an n×n chessboard so no two queens attack each other. Return all valid configurations.

**Approach:** Place one queen per row (rows are already separated). Track which columns and diagonals are occupied. The two diagonal directions: `col - row` (top-left to bottom-right) and `col + row` (top-right to bottom-left).

```cpp
#include <vector>
#include <string>
#include <unordered_set>
using namespace std;

class Solution {
public:
    vector<vector<string>> result;
    unordered_set<int> cols, diag1, diag2;  // diag1: col-row, diag2: col+row

    void backtrack(int row, int n, vector<string>& board) {
        if (row == n) {
            result.push_back(board);
            return;
        }
        for (int col = 0; col < n; col++) {
            // Pruning: check column and both diagonals
            if (cols.count(col) || diag1.count(col - row) || diag2.count(col + row))
                continue;

            // CHOOSE
            board[row][col] = 'Q';
            cols.insert(col);
            diag1.insert(col - row);
            diag2.insert(col + row);

            // EXPLORE
            backtrack(row + 1, n, board);

            // UNCHOOSE
            board[row][col] = '.';
            cols.erase(col);
            diag1.erase(col - row);
            diag2.erase(col + row);
        }
    }

    vector<vector<string>> solveNQueens(int n) {
        vector<string> board(n, string(n, '.'));
        backtrack(0, n, board);
        return result;
    }
};
```

**Complexity:** Time O(n!) upper bound, much less in practice due to pruning. Space O(n).

**Using bitmasks for O(1) conflict check:**
```cpp
// cols, diag1, diag2 become integer bitmasks
// A bit set means that column/diagonal is occupied
void backtrack(int row, int n, int cols, int diag1, int diag2, vector<string>& board) {
    if (row == n) { result.push_back(board); return; }
    int available = ((1 << n) - 1) & ~(cols | diag1 | diag2);
    while (available) {
        int bit = available & (-available);  // lowest set bit
        int col = __builtin_ctz(bit);
        board[row][col] = 'Q';
        backtrack(row+1, n, cols|bit, (diag1|bit)<<1, (diag2|bit)>>1, board);
        board[row][col] = '.';
        available &= available - 1;
    }
}
```

---

## 5. Common Pitfalls

1. **Forgetting to undo** — the most common bug. If state is not restored, future branches see corrupted state.
2. **Pruning too late** — check constraints before recursing, not inside the base case.
3. **Off-by-one in `start`** — use `i+1` when elements cannot be reused; use `i` when they can.
4. **Not sorting for deduplication** — with duplicates, sort first, then skip `nums[i] == nums[i-1]` at the same depth.
5. **Recording reference vs. copy** — `result.push_back(path)` copies the vector; `result.push_back(&path)` would be a dangling reference bug.
