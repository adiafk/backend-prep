# Dynamic Programming — Mindset, Templates, and Problems

## 1. The DP Mindset

Dynamic programming is applicable when a problem has two properties:

### Overlapping Subproblems

The same subproblem is solved multiple times in a naive recursive solution.

```
fib(5)
├── fib(4)
│   ├── fib(3)
│   │   ├── fib(2)  <-- computed multiple times
│   │   └── fib(1)
│   └── fib(2)      <-- duplicate
└── fib(3)          <-- duplicate subtree
    ├── fib(2)      <-- duplicate
    └── fib(1)
```

Without memoization, fib(5) makes 15 recursive calls. With memoization: 9 unique subproblems.

### Optimal Substructure

The optimal solution to the problem can be built from optimal solutions to its subproblems.

Example: The shortest path from A to C through B equals shortest(A, B) + shortest(B, C). If the middle path were not optimal, you could replace it with a better one.

---

## 2. Top-Down (Memoization) vs Bottom-Up (Tabulation)

### Top-Down — Memoization

Start with the original problem. Recurse into subproblems. Cache results to avoid recomputation.

```
Direction: large problem → smaller subproblems
Data structure: hash map or array memo[]
Call order: determined by recursion (lazy evaluation)
```

```cpp
// Fibonacci — top-down
unordered_map<int, long long> memo;

long long fib(int n) {
    if (n <= 1) return n;
    if (memo.count(n)) return memo[n];
    memo[n] = fib(n - 1) + fib(n - 2);
    return memo[n];
}
```

**Pros:** Easier to reason about (follows natural problem decomposition). Only computes needed subproblems.
**Cons:** Recursive call stack overhead. Risk of stack overflow for large n.

---

### Bottom-Up — Tabulation

Start with the smallest subproblems (base cases). Iteratively build up to the answer.

```
Direction: smallest subproblems → large problem
Data structure: array dp[]
Call order: explicit iteration (eager evaluation)
```

```cpp
// Fibonacci — bottom-up
long long fib(int n) {
    if (n <= 1) return n;
    vector<long long> dp(n + 1);
    dp[0] = 0; dp[1] = 1;
    for (int i = 2; i <= n; i++) {
        dp[i] = dp[i-1] + dp[i-2];
    }
    return dp[n];
}

// Space-optimized (only need last two values)
long long fibOpt(int n) {
    if (n <= 1) return n;
    long long a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        long long c = a + b;
        a = b;
        b = c;
    }
    return b;
}
```

**Pros:** No recursion overhead. Easier to optimize space. Cache-friendly access patterns.
**Cons:** Requires knowing the order of computation upfront. May compute unnecessary subproblems.

---

### Choosing Between Them

| Factor | Top-Down | Bottom-Up |
|---|---|---|
| Ease of coding | Easier (follows problem statement) | Requires figuring out iteration order |
| Space optimization | Harder | Easier (sliding window) |
| Only some subproblems needed | Computes only what's needed | Computes everything |
| Stack overflow risk | Yes (deep recursion) | No |
| Interview default | Usually fine | Slightly preferred for production |

---

## 3. Templates

### 1D DP Template

```cpp
// dp[i] = answer for subproblem of size i
// Transition: dp[i] = f(dp[i-1], dp[i-2], ...)

int solve(int n) {
    vector<int> dp(n + 1, 0);

    // Base case(s)
    dp[0] = base0;
    dp[1] = base1;

    // Fill table
    for (int i = 2; i <= n; i++) {
        dp[i] = /* transition using dp[i-1], dp[i-2], etc. */;
    }

    return dp[n];
}
```

**Space optimization when only last k values are needed:**
```cpp
// Instead of full array, keep only what you need
int prev2 = base0, prev1 = base1;
for (int i = 2; i <= n; i++) {
    int curr = prev1 + prev2;   // or whatever the transition is
    prev2 = prev1;
    prev1 = curr;
}
return prev1;
```

---

### 2D DP Template

```cpp
// dp[i][j] = answer considering first i items with capacity/length j
// Common for: LCS, knapsack, edit distance, grid paths

int solve(vector<int>& A, vector<int>& B) {
    int m = A.size(), n = B.size();
    // (m+1) x (n+1) to handle base cases elegantly
    vector<vector<int>> dp(m + 1, vector<int>(n + 1, 0));

    // Base cases often handled by initialization (zeros, or specific values)

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (/* match condition */) {
                dp[i][j] = /* use dp[i-1][j-1] */;
            } else {
                dp[i][j] = /* use dp[i-1][j] or dp[i][j-1] */;
            }
        }
    }

    return dp[m][n];
}
```

---

## 4. Solved Problems

---

### Problem 1 — Climbing Stairs

**LeetCode 70**

You can climb 1 or 2 steps at a time. How many distinct ways to reach the top (n steps)?

**Recurrence:** `ways(n) = ways(n-1) + ways(n-2)` — you arrived from step n-1 (took 1 step) or from step n-2 (took 2 steps). This is Fibonacci.

```cpp
int climbStairs(int n) {
    if (n <= 2) return n;

    int prev2 = 1, prev1 = 2;
    for (int i = 3; i <= n; i++) {
        int curr = prev1 + prev2;
        prev2 = prev1;
        prev1 = curr;
    }
    return prev1;
}
```

Time: O(n). Space: O(1).

**Verification:**
- n=1: 1 way (1)
- n=2: 2 ways (1+1, 2)
- n=3: 3 ways (1+1+1, 1+2, 2+1)
- n=4: 5 ways — follows Fibonacci sequence exactly.

---

### Problem 2 — House Robber

**LeetCode 198**

Rob houses in a line. Cannot rob two adjacent houses. Maximize total amount robbed.

**Recurrence:** At each house i, either:
- Rob it: `nums[i] + dp[i-2]` (can't have robbed i-1)
- Skip it: `dp[i-1]`

`dp[i] = max(nums[i] + dp[i-2], dp[i-1])`

```cpp
int rob(vector<int>& nums) {
    int n = nums.size();
    if (n == 1) return nums[0];

    int prev2 = nums[0];
    int prev1 = max(nums[0], nums[1]);

    for (int i = 2; i < n; i++) {
        int curr = max(nums[i] + prev2, prev1);
        prev2 = prev1;
        prev1 = curr;
    }
    return prev1;
}
```

Time: O(n). Space: O(1).

**Trace with [2, 7, 9, 3, 1]:**
```
i=0: prev2=2
i=1: prev1=max(2,7)=7
i=2: curr=max(9+2, 7)=11, prev2=7, prev1=11
i=3: curr=max(3+7, 11)=11, prev2=11, prev1=11
i=4: curr=max(1+11, 11)=12, prev2=11, prev1=12
Answer: 12 (rob houses 0, 2, 4: 2+9+1=12)
```

---

### Problem 3 — Longest Common Subsequence

**LeetCode 1143**

Find the length of the longest common subsequence (LCS) of two strings.

```
text1 = "abcde", text2 = "ace"
LCS = "ace", length = 3
```

**Recurrence:**
- If `text1[i-1] == text2[j-1]`: `dp[i][j] = dp[i-1][j-1] + 1`
- Else: `dp[i][j] = max(dp[i-1][j], dp[i][j-1])`

```cpp
int longestCommonSubsequence(string text1, string text2) {
    int m = text1.size(), n = text2.size();
    vector<vector<int>> dp(m + 1, vector<int>(n + 1, 0));

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (text1[i-1] == text2[j-1]) {
                dp[i][j] = dp[i-1][j-1] + 1;        // Characters match — extend
            } else {
                dp[i][j] = max(dp[i-1][j], dp[i][j-1]);  // Take best without one char
            }
        }
    }
    return dp[m][n];
}
```

Time: O(m * n). Space: O(m * n). Can be optimized to O(min(m,n)) using rolling array.

**DP table for "abcde" vs "ace":**
```
    ""  a  c  e
""   0  0  0  0
a    0  1  1  1
b    0  1  1  1
c    0  1  2  2
d    0  1  2  2
e    0  1  2  3   <-- answer
```

---

### Problem 4 — 0/1 Knapsack

**Classic problem** (appears as LeetCode 416 Partition Equal Subset Sum, etc.)

Given items with weights and values, and a knapsack of capacity W, maximize total value without exceeding weight.

Each item can be used at most once (0/1 = either take or don't take).

**Recurrence:**
- Don't take item i: `dp[i][w] = dp[i-1][w]`
- Take item i (if weight[i] <= w): `dp[i][w] = dp[i-1][w - weight[i]] + value[i]`
- `dp[i][w] = max(don't take, take)`

```cpp
int knapsack(int W, vector<int>& weights, vector<int>& values) {
    int n = weights.size();
    vector<vector<int>> dp(n + 1, vector<int>(W + 1, 0));

    for (int i = 1; i <= n; i++) {
        for (int w = 0; w <= W; w++) {
            dp[i][w] = dp[i-1][w];   // Don't take item i
            if (weights[i-1] <= w) {
                dp[i][w] = max(dp[i][w], dp[i-1][w - weights[i-1]] + values[i-1]);
            }
        }
    }
    return dp[n][W];
}
```

**Space-optimized (1D, iterate w in reverse):**
```cpp
int knapsack1D(int W, vector<int>& weights, vector<int>& values) {
    int n = weights.size();
    vector<int> dp(W + 1, 0);

    for (int i = 0; i < n; i++) {
        // Iterate BACKWARDS to ensure each item is used at most once
        for (int w = W; w >= weights[i]; w--) {
            dp[w] = max(dp[w], dp[w - weights[i]] + values[i]);
        }
    }
    return dp[W];
}
```

Time: O(n * W). Space: O(n * W) or O(W) optimized.

**Why iterate backwards in 1D?** If you go forward, when computing dp[w] you might use the updated dp[w - weights[i]] which already includes item i — allowing it to be used twice (unbounded knapsack). Going backward ensures you use dp values from before item i was considered.

---

### Problem 5 — Coin Change

**LeetCode 322**

Given coins of various denominations and a target amount, find the minimum number of coins needed to make the amount. Return -1 if impossible.

This is an unbounded knapsack variant — each coin can be used unlimited times.

**Recurrence:** `dp[amount] = min over all coins c: dp[amount - c] + 1`

```cpp
int coinChange(vector<int>& coins, int amount) {
    // dp[i] = min coins to make amount i
    // Initialize to amount+1 (impossible sentinel, larger than any valid answer)
    vector<int> dp(amount + 1, amount + 1);
    dp[0] = 0;   // Base case: 0 coins to make amount 0

    for (int i = 1; i <= amount; i++) {
        for (int coin : coins) {
            if (coin <= i) {
                dp[i] = min(dp[i], dp[i - coin] + 1);
            }
        }
    }

    return dp[amount] > amount ? -1 : dp[amount];
}
```

Time: O(amount * coins.size()). Space: O(amount).

**Trace with coins=[1,5,6,9], amount=11:**
```
dp[0]=0
dp[1]=1  (1)
dp[5]=1  (5)
dp[6]=1  (6)
dp[9]=1  (9)
dp[10]=2 (5+5 or 1+9)
dp[11]=2 (5+6)
```

**Why `amount + 1` as sentinel?** It's larger than any valid answer (you'd never need more than `amount` coins of denomination 1). Using INT_MAX risks overflow when you do `dp[i-coin] + 1`.

**Contrast with 0/1 knapsack:** Here the inner loop iterates forward (we can reuse coins). In 0/1 knapsack the inner loop iterates backward (each item used at most once).

---

### Problem 6 — Longest Increasing Subsequence

**LeetCode 300**

Find the length of the longest strictly increasing subsequence.

```
Input:  [10, 9, 2, 5, 3, 7, 101, 18]
Output: 4 (the LIS is [2, 3, 7, 101])
```

**Approach 1 — O(n^2) DP:**

`dp[i]` = length of LIS ending at index i.

```cpp
int lengthOfLIS(vector<int>& nums) {
    int n = nums.size();
    vector<int> dp(n, 1);   // Every element is an LIS of length 1 by itself

    int maxLen = 1;
    for (int i = 1; i < n; i++) {
        for (int j = 0; j < i; j++) {
            if (nums[j] < nums[i]) {
                dp[i] = max(dp[i], dp[j] + 1);
            }
        }
        maxLen = max(maxLen, dp[i]);
    }
    return maxLen;
}
```

**Approach 2 — O(n log n) with patience sorting:**

Maintain a `tails` array where `tails[i]` = smallest tail element of all increasing subsequences of length i+1.

```cpp
int lengthOfLIS(vector<int>& nums) {
    vector<int> tails;   // tails[i] = smallest tail for LIS of length i+1

    for (int num : nums) {
        // Find first element in tails >= num (lower_bound for strictly increasing)
        auto it = lower_bound(tails.begin(), tails.end(), num);

        if (it == tails.end()) {
            tails.push_back(num);    // Extend the LIS
        } else {
            *it = num;               // Replace — maintains the invariant
        }
    }
    return tails.size();
}
```

Time: O(n log n). Space: O(n).

**Trace with [10, 9, 2, 5, 3, 7, 101, 18]:**
```
num=10: tails=[10]
num= 9: replace 10 → tails=[9]
num= 2: replace 9  → tails=[2]
num= 5: extend     → tails=[2, 5]
num= 3: replace 5  → tails=[2, 3]
num= 7: extend     → tails=[2, 3, 7]
num=101:extend     → tails=[2, 3, 7, 101]
num=18: replace 101→ tails=[2, 3, 7, 18]
Length = 4
```

Note: `tails` is not the actual LIS — it's a virtual array used only to track the length. To reconstruct the actual subsequence, you need extra bookkeeping.

**Why does replacing work?** Replacing `tails[i]` with a smaller value doesn't destroy any existing LIS — it only creates the possibility of extending sequences further in the future with numbers that fit between the new smaller value and the next tail.

---

## 5. Common DP Problem Categories

| Category | Signature | Examples |
|---|---|---|
| Linear 1D | dp[i] depends on dp[i-1], dp[i-2] | Climbing Stairs, House Robber, Fibonacci |
| Interval | dp[i][j] = answer for subarray i..j | Matrix Chain, Palindromic Substrings |
| Two-sequence | dp[i][j] = answer for prefix of each | LCS, Edit Distance |
| Knapsack | dp[i][w] = best using i items with capacity w | 0/1 Knapsack, Coin Change, Subset Sum |
| Grid paths | dp[r][c] = ways/cost to reach cell | Unique Paths, Minimum Path Sum |

---

## 6. Complexity Reference

| Problem | Time | Space | Optimized Space |
|---|---|---|---|
| Climbing Stairs | O(n) | O(n) | O(1) |
| House Robber | O(n) | O(n) | O(1) |
| Longest Common Subsequence | O(m*n) | O(m*n) | O(min(m,n)) |
| 0/1 Knapsack | O(n*W) | O(n*W) | O(W) |
| Coin Change | O(amount * coins) | O(amount) | — |
| LIS (DP) | O(n^2) | O(n) | — |
| LIS (binary search) | O(n log n) | O(n) | — |

---

## 7. Debugging DP

1. **Wrong base case:** Check dp[0] and dp[1] manually.
2. **Off-by-one:** Using `i` vs `i-1` as index into input array — be consistent about whether dp[i] means "first i elements" or "element at index i".
3. **Wrong iteration order:** 0/1 knapsack needs backward loop in 1D; unbounded needs forward.
4. **Sentinel value overflow:** If you initialize to INT_MAX and do `dp[...] + 1`, you get overflow. Use `amount + 1` or `n + 1` instead.
5. **Not reading the whole DP table:** For some problems the answer is `max(dp[0..n])`, not just `dp[n]`.
