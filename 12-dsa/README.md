# DSA (Data Structures & Algorithms) — Section Overview

This section covers everything needed to pass DSA interviews at top tech companies. The focus is on recognizing patterns, not memorizing solutions.

---

## 1. How to Study DSA for Interviews

### Pattern-First vs Problem-First

**Problem-First** is how most people start: pick LeetCode, grind problems in order, memorize solutions. This fails because:
- You accumulate hundreds of isolated solutions with no transferable framework
- Novel problem variants break your memorized approach
- Under interview pressure, you cannot recall which of 400 problems this new one resembles

**Pattern-First** is how strong engineers approach DSA:
- Learn a pattern (e.g., sliding window) deeply — understand what it solves and why
- Solve 5–8 problems per pattern to internalize the mechanics
- When a new problem appears, you ask "which pattern does this fit?" not "have I seen this before?"

### The Pattern-First Method in Practice

1. **Understand the pattern's invariant** — what property does the pattern maintain? (e.g., sliding window maintains a valid window; two pointers maintain a sorted-array constraint)
2. **Implement the pattern template from scratch** before touching any problem
3. **Solve 2 easy, 3 medium, 1 hard** problems per pattern
4. **After each problem**, write one sentence describing what made this pattern the right choice
5. **Spaced repetition**: revisit problems after 3 days, 1 week, 2 weeks

### What Pattern-First Looks Like on the Clock

When you get a problem in an interview:
1. Identify the input type (array, string, graph, tree, etc.)
2. Identify the constraint or goal type (subarray, path, count, min/max, etc.)
3. Match to a pattern (see Pattern Identification Guide below)
4. Apply your memorized template
5. Adapt the template to the specific constraints

---

## 2. The 15 Most Important Patterns — Ranked by Interview Frequency

Rankings based on frequency across FAANG/Big-N coding interviews (LeetCode survey data, interview reports, Blind/Levels.fyi discussions).

| Rank | Pattern | Frequency | Core Idea |
|------|---------|-----------|-----------|
| 1 | Two Pointers | Very High | Shrink search space using sorted structure or converging pointers |
| 2 | Sliding Window | Very High | Maintain a variable or fixed-size window over a sequence |
| 3 | BFS / Level-Order Traversal | Very High | Shortest path, level-by-level graph/tree traversal |
| 4 | DFS / Backtracking | Very High | Exhaustive search with pruning; permutations, subsets, paths |
| 5 | Dynamic Programming (1D) | High | Overlapping subproblems with optimal substructure; memoization/tabulation |
| 6 | Dynamic Programming (2D) | High | Grid/string DP; LCS, edit distance, knapsack variants |
| 7 | Hash Map / Frequency Count | High | O(1) lookups to turn O(n^2) into O(n) |
| 8 | Binary Search | High | Eliminate half the search space each step; applies beyond sorted arrays |
| 9 | Tree DFS (recursive/iterative) | High | Preorder/inorder/postorder; path problems, serialization |
| 10 | Heap / Priority Queue | Medium-High | K-th element, merge K sorted, scheduling, top-K |
| 11 | Prefix Sum | Medium-High | Precompute cumulative sums for O(1) range queries |
| 12 | Monotonic Stack | Medium | Next greater element, histogram, span problems |
| 13 | Union-Find / Disjoint Set | Medium | Connected components, cycle detection in undirected graphs |
| 14 | Trie | Medium | Prefix matching, autocomplete, word search |
| 15 | Graph Topological Sort | Medium | Dependency ordering; cycle detection in directed graphs |

### Why These 15

Problems outside these 15 patterns appear rarely and can usually be decomposed into combinations of the above. Mastering these covers approximately 85–90% of what appears in screening rounds through final rounds at most companies.

---

## 3. Complexity Cheat Sheet

### Big-O Notation Fundamentals

| Complexity | Name | Example |
|-----------|------|---------|
| O(1) | Constant | Array index access, hash map get |
| O(log n) | Logarithmic | Binary search, heap push/pop |
| O(n) | Linear | Single loop, linear scan |
| O(n log n) | Linearithmic | Merge sort, heap sort, most practical sorts |
| O(n^2) | Quadratic | Nested loops, bubble sort |
| O(2^n) | Exponential | Brute-force subsets, naive recursion |
| O(n!) | Factorial | Brute-force permutations |

### Array Operations

| Operation | Average | Worst | Notes |
|-----------|---------|-------|-------|
| Access by index | O(1) | O(1) | Direct memory offset |
| Search (unsorted) | O(n) | O(n) | Linear scan |
| Search (sorted) | O(log n) | O(log n) | Binary search |
| Insert at end | O(1) amortized | O(n) | Resizing cost amortized |
| Insert at middle | O(n) | O(n) | Must shift elements |
| Delete at end | O(1) | O(1) | |
| Delete at middle | O(n) | O(n) | Must shift elements |

### Hash Map / Hash Set

| Operation | Average | Worst | Notes |
|-----------|---------|-------|-------|
| Insert | O(1) | O(n) | Worst case: all keys collide |
| Delete | O(1) | O(n) | |
| Search/Lookup | O(1) | O(n) | |
| Iteration | O(n) | O(n) | |

### Linked List

| Operation | Singly Linked | Doubly Linked | Notes |
|-----------|---------------|---------------|-------|
| Access by index | O(n) | O(n) | No random access |
| Search | O(n) | O(n) | |
| Insert at head | O(1) | O(1) | |
| Insert at tail | O(n) / O(1)* | O(1)* | *O(1) if tail pointer maintained |
| Insert at middle | O(n) | O(n) | Traversal cost |
| Delete at head | O(1) | O(1) | |
| Delete at middle | O(n) | O(n) | |

### Stack / Queue

| Operation | Stack | Queue (array-based) | Queue (linked list) |
|-----------|-------|---------------------|---------------------|
| Push/Enqueue | O(1) | O(1) amortized | O(1) |
| Pop/Dequeue | O(1) | O(n)* or O(1)** | O(1) |
| Peek | O(1) | O(1) | O(1) |
| Search | O(n) | O(n) | O(n) |

*Naive array queue shifts. **Circular buffer or deque.

### Binary Search Tree (BST)

| Operation | Average (balanced) | Worst (skewed) |
|-----------|-------------------|-----------------|
| Search | O(log n) | O(n) |
| Insert | O(log n) | O(n) |
| Delete | O(log n) | O(n) |
| Min/Max | O(log n) | O(n) |
| Inorder traversal | O(n) | O(n) |

### Heap (Binary Heap)

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| Build heap (heapify) | O(n) | Not O(n log n); proven via series |
| Insert | O(log n) | Bubble up |
| Delete max/min | O(log n) | Bubble down |
| Peek max/min | O(1) | Root element |
| Search arbitrary | O(n) | No ordering outside root |

### Graph (V = vertices, E = edges)

| Operation | Adjacency List | Adjacency Matrix |
|-----------|---------------|-----------------|
| Add vertex | O(1) | O(V^2) |
| Add edge | O(1) | O(1) |
| Remove edge | O(E) | O(1) |
| Check edge exists | O(V) | O(1) |
| BFS/DFS traversal | O(V + E) | O(V^2) |
| Space | O(V + E) | O(V^2) |

### Sorting Algorithms

| Algorithm | Best | Average | Worst | Space | Stable? |
|-----------|------|---------|-------|-------|---------|
| Bubble Sort | O(n) | O(n^2) | O(n^2) | O(1) | Yes |
| Selection Sort | O(n^2) | O(n^2) | O(n^2) | O(1) | No |
| Insertion Sort | O(n) | O(n^2) | O(n^2) | O(1) | Yes |
| Merge Sort | O(n log n) | O(n log n) | O(n log n) | O(n) | Yes |
| Quick Sort | O(n log n) | O(n log n) | O(n^2) | O(log n) | No |
| Heap Sort | O(n log n) | O(n log n) | O(n log n) | O(1) | No |
| Counting Sort | O(n+k) | O(n+k) | O(n+k) | O(k) | Yes |
| Radix Sort | O(nk) | O(nk) | O(nk) | O(n+k) | Yes |

### String Operations

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| Access character | O(1) | By index |
| Concatenation | O(n) | Creates new string |
| Substring | O(k) | k = length of substring |
| Search (naive) | O(nm) | n=text, m=pattern |
| Search (KMP) | O(n+m) | Knuth-Morris-Pratt |
| Search (Rabin-Karp) | O(n) avg | Rolling hash |

---

## 4. 8-Week DSA Study Plan

### Principles
- 1.5–2 hours per day minimum; 3–4 hours on weekends
- Do not skip fundamentals in favor of grinding problems
- Each week has a theme; treat it as a sprint
- Mock interviews start at Week 5

### Week 1 — Arrays and Hashing

**Goal**: Build the foundation. Master in-place manipulation, frequency counting, and two-pointer basics.

| Day | Focus | Problems |
|-----|-------|---------|
| Mon | Array basics, Two Sum (hash map) | Two Sum, Valid Anagram |
| Tue | Two pointers intro | Valid Palindrome, Two Sum II (sorted array) |
| Wed | Sliding window intro (fixed size) | Best Time to Buy/Sell Stock, Maximum Subarray |
| Thu | Prefix sum | Range Sum Query, Subarray Sum Equals K |
| Fri | Encoding/decoding, product | Product of Array Except Self, Encode/Decode Strings |
| Sat | Review + hard problem | Trapping Rain Water |
| Sun | Mock interview (1 problem, 45 min timed) | Self-assessment |

**Checkpoint**: Can you solve any easy array problem in under 15 minutes?

### Week 2 — Strings and More Arrays

**Goal**: Solidify string manipulation, sliding window, and sorting-based techniques.

| Day | Focus | Problems |
|-----|-------|---------|
| Mon | Sliding window (variable size) | Longest Substring Without Repeating, Permutation in String |
| Tue | Sorting techniques | Sort Colors, Merge Intervals |
| Wed | String manipulation | Longest Common Prefix, Group Anagrams |
| Thu | Kadane's and variants | Max Sum Subarray, Min Size Subarray Sum |
| Fri | String window hard | Minimum Window Substring |
| Sat | Review gaps | Pick 3 problems you struggled with |
| Sun | Mock interview | 2 problems in 60 min |

### Week 3 — Linked Lists

**Goal**: Master pointer manipulation, fast/slow pointers, reversal.

| Day | Focus | Problems |
|-----|-------|---------|
| Mon | Basics, reversal | Reverse Linked List, Merge Two Sorted Lists |
| Tue | Fast/slow pointers | Linked List Cycle, Middle of Linked List |
| Wed | In-place operations | Remove Nth Node From End, Reorder List |
| Thu | Complex pointer work | LRU Cache |
| Fri | Hard problems | Merge K Sorted Lists, Reverse Nodes in K-Group |
| Sat | Review | All week's problems without notes |
| Sun | Mock interview | |

### Week 4 — Trees (BFS + DFS)

**Goal**: Be fluent in recursive and iterative tree traversal. Handle all tree variants.

| Day | Focus | Problems |
|-----|-------|---------|
| Mon | DFS basics | Invert Binary Tree, Maximum Depth, Diameter |
| Tue | BST properties | Validate BST, Lowest Common Ancestor |
| Wed | Tree construction | Construct Tree from Inorder+Preorder, Serialize/Deserialize |
| Thu | BFS on trees | Level Order Traversal, Right Side View |
| Fri | Path problems | Binary Tree Max Path Sum, Path Sum III |
| Sat | Review | |
| Sun | Mock interview | |

### Week 5 — Graphs + Union-Find

**Goal**: Handle all graph representations, BFS/DFS on 2D grids, connectivity.

| Day | Focus | Problems |
|-----|-------|---------|
| Mon | Graph BFS | Number of Islands, Clone Graph |
| Tue | Graph DFS | Pacific Atlantic Water Flow, Surrounded Regions |
| Wed | Union-Find | Number of Connected Components, Redundant Connection |
| Thu | Shortest path / BFS | Rotting Oranges, Word Ladder |
| Fri | Topological sort | Course Schedule I & II |
| Sat | Advanced graphs | Dijkstra's (Cheapest Flights Within K Stops) |
| Sun | Mock interview (2 problems) | First mock with graph component |

### Week 6 — Dynamic Programming

**Goal**: Master DP pattern identification, bottom-up tabulation, and state definition.

| Day | Focus | Problems |
|-----|-------|---------|
| Mon | 1D DP fundamentals | Climbing Stairs, House Robber, Min Cost Climbing |
| Tue | Subsequence DP | Longest Increasing Subsequence, Longest Common Subsequence |
| Wed | 0/1 Knapsack | Partition Equal Subset Sum, Target Sum |
| Thu | 2D DP / Grid | Unique Paths, Minimum Path Sum, Edit Distance |
| Fri | String DP | Palindromic Substrings, Longest Palindromic Subsequence |
| Sat | DP hard | Burst Balloons, Coin Change II |
| Sun | Mock interview | |

### Week 7 — Heap, Binary Search, Monotonic Stack

**Goal**: Fill in high-frequency patterns not yet covered.

| Day | Focus | Problems |
|-----|-------|---------|
| Mon | Heap/Priority Queue | Kth Largest, K Closest Points, Top K Frequent |
| Tue | Merge patterns | Merge K Sorted Lists (heap approach), Find Median from Stream |
| Wed | Binary search fundamentals | Binary Search, Search in Rotated Sorted Array |
| Thu | Binary search on answer | Koko Eating Bananas, Minimum in Rotated Array |
| Fri | Monotonic stack | Daily Temperatures, Largest Rectangle in Histogram |
| Sat | Trie | Implement Trie, Word Search II |
| Sun | Mock interview | Full 2-problem session |

### Week 8 — Integration, Review, Mock Interviews

**Goal**: Simulate real interview conditions; identify and fix remaining weak spots.

| Day | Focus | Activity |
|-----|-------|---------|
| Mon | Weak pattern review | Pick your 2 weakest topics; do 4 problems each |
| Tue | System design + DSA combo | Practice explaining time/space tradeoffs out loud |
| Wed | Mock interview #1 | Timed 45 min, 2 problems, recorded or with a partner |
| Thu | Review mock + fix gaps | Analyze every mistake from Wednesday |
| Fri | Mock interview #2 | Different problem set |
| Sat | LeetCode company tag problems | Filter by target company, do 4–5 tagged problems |
| Sun | Final review | Go through all templates from memory |

### Daily Habits Throughout All 8 Weeks

- Before coding any problem, spend 3–5 minutes writing your approach in plain English
- After solving, ask: "What pattern is this? What was the key insight?"
- Maintain a notes file per pattern with your own words
- Time yourself after Week 3 — interviews have a clock

---

## 5. Pattern Identification Guide

### How to Identify the Right Pattern

Work through these questions in order when you see a new problem:

#### Step 1 — What is the input structure?

| Input | Likely Patterns |
|-------|----------------|
| Sorted array | Binary search, two pointers |
| Unsorted array | Hash map, prefix sum, sliding window, sorting |
| String | Sliding window, two pointers, trie, DP |
| Linked list | Fast/slow pointers, reversal, merge |
| Tree | DFS, BFS, recursive decomposition |
| Graph (adjacency list) | BFS, DFS, topological sort, union-find |
| Matrix/2D grid | BFS/DFS (treat as graph), DP |
| Stream / continuous input | Heap, sliding window |

#### Step 2 — What does the problem ask you to find?

| Goal | Likely Patterns |
|------|----------------|
| Shortest path / minimum steps | BFS |
| All paths / all combinations | DFS / backtracking |
| Maximum/minimum subarray/substring | Sliding window, Kadane's, DP |
| Count subarrays/substrings satisfying condition | Prefix sum with hash map |
| K-th largest/smallest | Heap, quickselect, binary search |
| Overlapping subproblems, optimize | Dynamic programming |
| Frequency / existence | Hash map / hash set |
| Pairs summing to target | Two pointers (sorted) or hash map |
| Connected components | Union-find or BFS/DFS |
| Ordering with dependencies | Topological sort |
| Prefix matching | Trie |
| Next greater/smaller element | Monotonic stack |

#### Step 3 — Are there any explicit constraints that narrow choices?

| Constraint | Implication |
|-----------|-------------|
| "In-place", O(1) space | Two pointers, in-place sort, avoid hash maps |
| O(n log n) time | Sorting-based, heap, divide & conquer |
| O(n) time | Linear scan, hash map, prefix sum, sliding window |
| Array is sorted or nearly sorted | Binary search, two pointers |
| K mentioned (top-K, K-th) | Heap or binary search on answer |
| Continuous subarray | Sliding window, prefix sum, Kadane's |
| Subsequence (not contiguous) | DP, greedy |

#### Quick Reference: Pattern Trigger Words

| Words/Phrases in Problem | Pattern to Try First |
|--------------------------|---------------------|
| "Subarray with sum equal to K" | Prefix sum + hash map |
| "Longest substring without repeating" | Sliding window |
| "Maximum sum subarray" | Kadane's algorithm |
| "Two numbers that add to target" | Hash map or two pointers |
| "Shortest path", "minimum hops" | BFS |
| "All permutations", "all subsets" | Backtracking |
| "Minimum/maximum of all subarrays" | Monotonic deque |
| "Overlapping intervals" | Sort by start, greedy merge |
| "Top K frequent", "K closest" | Heap |
| "Word starts with prefix" | Trie |
| "Clone", "copy" graph/tree | BFS/DFS with visited map |
| "Course prerequisites", "task order" | Topological sort |
| "Connected components", "union" | Union-find |
| "Search in sorted rotated array" | Binary search with pivot logic |
| "Buy and sell stock" | One-pass with running min |
| "Palindrome" | Two pointers expanding from center, DP |
| "Edit distance", "minimum operations" | 2D DP |
| "Coin change", "ways to make amount" | Unbounded knapsack DP |

---

## Directory Structure

```
12-dsa/
├── README.md                    (this file)
├── arrays/
│   ├── README.md                (arrays pattern guide)
│   └── interview-notes.md       (array interview traps)
├── strings/
├── linked-lists/
├── trees/
├── graphs/
├── dynamic-programming/
├── binary-search/
├── heaps/
├── sliding-window/
├── two-pointers/
├── backtracking/
├── monotonic-stack/
├── trie/
├── union-find/
└── top-k-and-sorting/
```

Each subdirectory follows the same structure: README.md with pattern guide, interview-notes.md, and solution files per problem.
