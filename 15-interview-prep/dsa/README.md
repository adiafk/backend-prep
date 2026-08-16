# DSA Interview Prep

## The Process

For every LeetCode problem in an interview:

1. **Read, restate** — repeat the problem in your own words. Catch edge cases early.
2. **Examples** — work through 2-3 examples by hand. Include edge cases (empty input, single element, all same).
3. **Brute force** — state the naive O(n²) or O(n³) approach. Don't code it.
4. **Optimize** — identify the bottleneck (usually a nested loop), apply pattern.
5. **Code** — clean, without syntax errors.
6. **Test** — trace through your examples again with the actual code.
7. **Complexity** — state time and space. Justify.

---

## Pattern Recognition

The single most valuable interview skill is mapping a problem to a pattern.

| Clue in the problem | Try this pattern |
|--------------------|-----------------|
| Sorted array, find target in O(log n) | Binary search |
| Contiguous subarray, max/min/count | Sliding window |
| Compare elements from both ends | Two pointers |
| Running sum/product, prefix | Prefix sum |
| Next greater/smaller element | Monotonic stack |
| K largest/smallest elements | Heap (priority queue) |
| Connected components, cycle detection | Union-Find |
| Graph traversal, shortest path | BFS (shortest), DFS (all paths) |
| Optimal substructure, overlapping subproblems | Dynamic programming |
| "At each step, pick the locally optimal" | Greedy |
| All combinations/permutations | Backtracking |
| Duplicate detection, O(1) lookup | Hash map / hash set |
| Merge sorted arrays/lists | Two pointer merge |
| Linked list cycle / middle | Fast/slow pointers |

---

## Complexity Cheat Sheet

```
O(1)       Hashmap get/set, array index access
O(log n)   Binary search, balanced BST, heap push/pop
O(n)       Single pass, two pointers
O(n log n) Sorting, heapify n elements
O(n²)      Nested loops (fix with two pointers / hashmap)
O(2ⁿ)      Subsets, combinations (backtracking)
O(n!)      Permutations (backtracking)
```

Interview target: usually O(n log n) or O(n). If you're at O(n²), think about sorting first, then two pointers, or a hashmap.

---

## Top 50 Problems by Pattern

### Arrays & Hashing
- Two Sum (hashmap)
- Contains Duplicate (set)
- Valid Anagram (freq count)
- Group Anagrams (sorted key map)
- Longest Consecutive Sequence (set + start detection)
- Product of Array Except Self (prefix product)
- Top K Frequent Elements (heap or bucket sort)

### Two Pointers
- Valid Palindrome
- Two Sum II (sorted array)
- 3Sum
- Container With Most Water
- Trapping Rain Water

### Sliding Window
- Best Time to Buy and Sell Stock
- Longest Substring Without Repeating Characters
- Minimum Window Substring
- Permutation in String

### Binary Search
- Binary Search
- Search a 2D Matrix
- Koko Eating Bananas
- Find Minimum in Rotated Sorted Array
- Search in Rotated Sorted Array

### Linked List
- Reverse Linked List
- Merge Two Sorted Lists
- Reorder List
- Remove Nth Node From End
- Linked List Cycle II

### Trees
- Invert Binary Tree
- Max Depth of Binary Tree
- Diameter of Binary Tree
- Same Tree
- Subtree of Another Tree
- LCA of BST
- Validate BST
- Level Order Traversal
- Serialize/Deserialize Binary Tree

### Graphs
- Number of Islands (DFS/BFS)
- Clone Graph
- Pacific Atlantic Water Flow
- Course Schedule (topological sort / cycle detect)
- Number of Connected Components (Union-Find)
- Longest Consecutive Sequence

### Heap
- Find Median from Data Stream
- Top K Frequent Elements
- K Closest Points to Origin
- Task Scheduler
- Design Twitter (k-way merge)

### DP
- Climbing Stairs (fibonacci)
- House Robber (1D DP)
- Coin Change (unbounded knapsack)
- Longest Increasing Subsequence
- Longest Common Subsequence
- Word Break
- Combination Sum IV
- House Robber II
- Decode Ways
- Unique Paths
- Jump Game

### Intervals
- Insert Interval
- Merge Intervals
- Non-overlapping Intervals
- Meeting Rooms II (min heap)

---

## Backtracking Template

```cpp
void backtrack(state, choices) {
    if (base_case) {
        result.push_back(state);
        return;
    }
    for (choice in choices) {
        make_choice(choice);
        backtrack(updated_state, updated_choices);
        undo_choice(choice);
    }
}
```

---

## DP Template

```cpp
// 1D DP (e.g., Climbing Stairs, House Robber)
int dp[n + 1];
dp[0] = base_0;
dp[1] = base_1;
for (int i = 2; i <= n; i++) {
    dp[i] = recurrence(dp[i-1], dp[i-2]);
}
return dp[n];

// 2D DP (e.g., LCS, Edit Distance)
int dp[m + 1][n + 1];
// initialize base cases: dp[i][0] and dp[0][j]
for (int i = 1; i <= m; i++) {
    for (int j = 1; j <= n; j++) {
        dp[i][j] = recurrence(dp[i-1][j], dp[i][j-1], dp[i-1][j-1]);
    }
}
return dp[m][n];
```

---

## Common Mistakes

- Off-by-one errors in binary search — use `left + (right - left) / 2` to avoid overflow
- Forgetting to handle empty input
- Modifying input array without asking if that's allowed
- Integer overflow: use `long long` when summing or multiplying large arrays in C++
- Not checking for `nullptr` in linked list problems
- DFS stack overflow on large inputs — prefer iterative BFS for graphs
- Returning the wrong thing (returning `count` instead of the actual path)

---

## Time Management in Interview (45 min)

```
0:00 - 0:05   Clarify requirements, edge cases
0:05 - 0:10   Brute force + optimize (discuss, don't code brute)
0:10 - 0:25   Code the solution
0:25 - 0:35   Test with examples, fix bugs
0:35 - 0:45   Complexity analysis, follow-up questions
```

If stuck after 5 min: ask for a hint. It's better to get a hint and solve it than to sit silent.
