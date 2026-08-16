# Prefix Sum Pattern

## What Prefix Sums Enable

A prefix sum array transforms a sequence so that any **range sum query** over the original array can be answered in **O(1)** instead of O(n).

### Construction

Given array `arr` of length n, define:

```
prefix[0] = 0
prefix[i] = arr[0] + arr[1] + ... + arr[i-1]   (1-indexed prefix, 0-padded)
```

Then the sum of elements from index `l` to `r` (inclusive, 0-indexed) is:

```
rangeSum(l, r) = prefix[r + 1] - prefix[l]
```

**C++ Construction:**
```cpp
vector<int> buildPrefix(vector<int>& arr) {
    int n = arr.size();
    vector<int> prefix(n + 1, 0);
    for (int i = 0; i < n; i++)
        prefix[i + 1] = prefix[i] + arr[i];
    return prefix;
}

int rangeSum(vector<int>& prefix, int l, int r) {
    return prefix[r + 1] - prefix[l]; // O(1)
}
```

**TypeScript Construction:**
```typescript
function buildPrefix(arr: number[]): number[] {
    const prefix = new Array(arr.length + 1).fill(0);
    for (let i = 0; i < arr.length; i++)
        prefix[i + 1] = prefix[i] + arr[i];
    return prefix;
}

function rangeSum(prefix: number[], l: number, r: number): number {
    return prefix[r + 1] - prefix[l];
}
```

### When prefix sums apply:
- Multiple range sum queries on a static array
- Counting subarrays whose sum equals / is divisible by k
- Finding subarrays with specific sum properties using a hash map on prefix values
- Difference arrays (inverse of prefix sums) for range update queries

---

## 2D Prefix Sums

Extend the idea to a 2D grid for O(1) rectangle sum queries.

### Construction

For an m x n matrix `mat`:

```
prefix[i][j] = sum of all elements in rectangle from (0,0) to (i-1, j-1)
```

Recurrence (inclusion-exclusion):
```
prefix[i][j] = mat[i-1][j-1]
             + prefix[i-1][j]
             + prefix[i][j-1]
             - prefix[i-1][j-1]
```

Rectangle sum from top-left (r1,c1) to bottom-right (r2,c2) (0-indexed):
```
sum = prefix[r2+1][c2+1]
    - prefix[r1][c2+1]
    - prefix[r2+1][c1]
    + prefix[r1][c1]
```

**C++ Implementation:**
```cpp
vector<vector<int>> build2DPrefix(vector<vector<int>>& mat) {
    int m = mat.size(), n = mat[0].size();
    vector<vector<int>> p(m + 1, vector<int>(n + 1, 0));

    for (int i = 1; i <= m; i++)
        for (int j = 1; j <= n; j++)
            p[i][j] = mat[i-1][j-1] + p[i-1][j] + p[i][j-1] - p[i-1][j-1];

    return p;
}

int rectSum(vector<vector<int>>& p, int r1, int c1, int r2, int c2) {
    return p[r2+1][c2+1] - p[r1][c2+1] - p[r2+1][c1] + p[r1][c1];
}
```

**Complexity:**
- Build: O(m * n)
- Query: O(1)

---

## Solved Problems

---

### 1. Range Sum Query — Immutable
**LeetCode 303**

**Problem:** Given an integer array, handle multiple queries each asking for the sum of elements between indices left and right (inclusive).

**Intuition:**
Brute force would be O(n) per query. If queries are numerous, this is expensive. Precompute a prefix sum array once in O(n). Every subsequent query is answered in O(1) using `prefix[right+1] - prefix[left]`.

The trade-off: O(n) upfront work, O(1) per query. For q queries, total complexity drops from O(n*q) to O(n + q).

**C++ Solution:**
```cpp
class NumArray {
    vector<int> prefix;
public:
    NumArray(vector<int>& nums) {
        int n = nums.size();
        prefix.resize(n + 1, 0);
        for (int i = 0; i < n; i++)
            prefix[i + 1] = prefix[i] + nums[i];
    }

    int sumRange(int left, int right) {
        return prefix[right + 1] - prefix[left];
    }
};
```

**Complexity:**
- Constructor: O(n)
- sumRange: O(1)
- Space: O(n) for prefix array

---

### 2. Subarray Sum Equals K
**LeetCode 560**

**Problem:** Given an integer array and an integer k, return the number of contiguous subarrays whose sum equals k.

**Intuition:**
The sum of subarray [l, r] = prefix[r+1] - prefix[l]. We want this to equal k, so we need prefix[r+1] - k = prefix[l] for some l <= r.

As we compute prefix sums left to right, store each prefix value in a hash map counting how many times it has appeared. At each position r+1 with current prefix sum `curr`, look up how many times `curr - k` has appeared in the map — each occurrence corresponds to a valid starting index l.

Initialize the map with `{0: 1}` to handle subarrays starting at index 0.

**C++ Solution:**
```cpp
int subarraySum(vector<int>& nums, int k) {
    unordered_map<int, int> prefixCount;
    prefixCount[0] = 1; // empty prefix

    int runningSum = 0, count = 0;
    for (int num : nums) {
        runningSum += num;
        // How many previous prefix sums equal runningSum - k?
        count += prefixCount[runningSum - k];
        prefixCount[runningSum]++;
    }
    return count;
}
```

**Why this works:** If there exists index l such that prefix[l] = runningSum - k, then sum(l..current) = runningSum - prefix[l] = k. Each such l gives one valid subarray.

**Complexity:**
- Time: O(n) — single pass with O(1) hash map operations
- Space: O(n) — at most n+1 distinct prefix sums

---

### 3. Product of Array Except Self
**LeetCode 238**

**Problem:** Return an array output where output[i] is the product of all elements of nums except nums[i]. No division allowed, O(n) time, O(1) extra space.

**Intuition:**
output[i] = (product of all elements to the left of i) * (product of all elements to the right of i).

This is a "prefix product" and "suffix product" problem. Compute the left prefix products in one left-to-right pass. Then multiply in the right suffix products in a right-to-left pass, accumulating the suffix product in a single variable rather than an array (to achieve O(1) extra space).

**C++ Solution:**
```cpp
vector<int> productExceptSelf(vector<int>& nums) {
    int n = nums.size();
    vector<int> output(n, 1);

    // Pass 1: output[i] = product of nums[0..i-1]
    int leftProduct = 1;
    for (int i = 0; i < n; i++) {
        output[i] = leftProduct;
        leftProduct *= nums[i];
    }

    // Pass 2: multiply in product of nums[i+1..n-1]
    int rightProduct = 1;
    for (int i = n - 1; i >= 0; i--) {
        output[i] *= rightProduct;
        rightProduct *= nums[i];
    }

    return output;
}
```

**Trace for [1, 2, 3, 4]:**
- After pass 1: output = [1, 1, 2, 6]   (left products)
- After pass 2: output = [24, 12, 8, 6] (multiplied by right products 24, 12, 4, 1)

**Complexity:**
- Time: O(n) — two passes
- Space: O(1) extra (output array is required by the problem, not counted as extra)

---

## Pattern Summary

| Technique | Use Case | Query Time | Build Time |
|---|---|---|---|
| 1D Prefix Sum | Range sum in static array | O(1) | O(n) |
| 2D Prefix Sum | Rectangle sum in static matrix | O(1) | O(m*n) |
| Prefix Sum + Hash Map | Count subarrays with sum = k | O(n) total | O(n) |
| Prefix/Suffix Product | Product except self (no division) | O(n) total | O(n) |

## Key Insight: Hash Map on Prefix Values

The pattern `prefixCount[runningSum - target]` generalizes to many problems:

- Subarray sum divisible by k: look up `(runningSum % k + k) % k` in the map
- Binary array with equal 0s and 1s: map 0 -> -1, then find subarrays with sum = 0
- Longest subarray with sum k: store first occurrence index of each prefix sum, compute max length

The idea is always the same: prefix[r] - prefix[l] = target rearranges to prefix[l] = prefix[r] - target. Process right to left greedily using the map to look up how many (or which) valid left endpoints exist.
