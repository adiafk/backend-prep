# Arrays — Pattern Guide

Arrays are the most fundamental data structure in DSA interviews. Nearly every other pattern (sliding window, two pointers, prefix sum) is first introduced on arrays. Mastering arrays is not optional.

---

## 1. Core Operations and Complexities

### Memory Model

Arrays store elements in contiguous memory. Index access is O(1) because the CPU computes the memory address directly:
```
address(i) = base_address + i * element_size
```

This is the root of every array complexity property.

### Operation Complexity Table

| Operation | Time | Space | Notes |
|-----------|------|-------|-------|
| Access `arr[i]` | O(1) | O(1) | Direct address computation |
| Update `arr[i] = x` | O(1) | O(1) | |
| Search (unsorted) | O(n) | O(1) | Linear scan |
| Search (sorted) | O(log n) | O(1) | Binary search |
| Insert at end | O(1) amortized | O(1) | Dynamic arrays resize by doubling |
| Insert at index i | O(n) | O(1) | Must shift n-i elements right |
| Delete at end | O(1) | O(1) | |
| Delete at index i | O(n) | O(1) | Must shift n-i-1 elements left |
| Traverse all | O(n) | O(1) | |
| Copy / clone | O(n) | O(n) | |
| Sort | O(n log n) | O(log n) | Introsort (most implementations) |

### Dynamic Array Resize Behavior

`std::vector` (C++) and `Array` (JavaScript/TypeScript) are dynamic arrays. When capacity is exceeded:
- A new array of size 2x is allocated
- All elements are copied — O(n) cost
- Amortized across n insertions: each element is copied at most log(n) times, so total cost is O(n), meaning O(1) amortized per insert

### Important: In-Place vs Extra Space

Many problems ask for in-place operations. This means O(1) extra space — you may only use the input array itself (plus a constant number of variables). This constrains your approach significantly:
- Cannot use a second array as scratch space
- Two pointers and index manipulation become your main tools
- Overwriting elements you no longer need is a common trick

---

## 2. Key Techniques

### Technique 1: Two Pointers

**When to use**: Problems involving pairs, sorted arrays, palindrome checking, partitioning.

**Core idea**: Maintain two indices (`left` and `right`) that move toward each other or in the same direction, eliminating the need for a nested loop.

**Variants**:
- **Converging**: `left` starts at 0, `right` at end, they move toward each other (Two Sum II, Valid Palindrome)
- **Same direction (fast/slow)**: Both pointers start at 0, one moves faster (Remove Duplicates, Move Zeroes)
- **Partition**: One pointer marks the boundary of the "processed" region (Dutch National Flag, Partition)

**Template (converging)**:
```
left = 0, right = n - 1
while left < right:
    evaluate arr[left] and arr[right]
    if condition to move left: left++
    else if condition to move right: right--
    else: process pair, move both
```

**Complexity**: O(n) time, O(1) space — this is the point. You replaced O(n^2) brute force.

**Key insight**: Two pointers only works correctly when you can argue that moving one pointer cannot miss a valid answer. This relies on some ordering or monotonicity property.

---

### Technique 2: Prefix Sum

**When to use**: Range sum queries, subarray sum equals target, counting subarrays with a property.

**Core idea**: Precompute `prefix[i] = sum of arr[0..i-1]`. Then the sum of any subarray `arr[l..r]` is `prefix[r+1] - prefix[l]`, computed in O(1).

**Construction**:
```
prefix[0] = 0
prefix[i] = prefix[i-1] + arr[i-1]   (1-indexed prefix for convenience)
sum(l, r) = prefix[r+1] - prefix[l]  (0-indexed l, r inclusive)
```

**Extended use — subarray sum equals K**:
- Maintain a running sum and a hash map of `{running_sum: count}`
- At each index, check if `running_sum - K` exists in the map
- This handles negative numbers, which sliding window cannot

**Prefix sum does NOT require non-negative values.** Sliding window does. This distinction matters.

**When to prefer prefix sum over sliding window**:
- Array contains negative numbers
- You need to count subarrays (not just find one)
- You need sum equality (not inequality)

---

### Technique 3: Sliding Window (Introduction)

**When to use**: Contiguous subarray/substring problems with a size or sum constraint.

**Core idea**: Maintain a window `[left, right]`. Expand by moving `right` forward. When the window violates a constraint, shrink by moving `left` forward. The window never restarts from scratch — it slides.

**Two types**:
1. **Fixed-size window**: Window size is given as K. Move both pointers at the same rate.
2. **Variable-size window**: Expand until constraint violated, shrink until valid again. Track max/min window size seen.

**Why it works**: For problems where expanding the window always degrades toward constraint violation (e.g., sum exceeds target, too many distinct characters), we know:
- Once we shrink to restore validity, we don't need to re-examine positions before `left`
- This gives O(n) — each element enters and exits the window at most once

**Constraint**: The "validity" of the window must be monotone with respect to size. If adding an element can both help and hurt validity arbitrarily, sliding window breaks. Use prefix sum + hash map instead.

Full sliding window treatment is in `../sliding-window/README.md`. Arrays uses it at the intro level (Kadane's, best stock price).

---

## 3. Template Code

### C++ Templates

#### Two Pointers — Converging
```cpp
#include <vector>
using namespace std;

// Generic two-pointer converging template
// Precondition: array is sorted (or problem has equivalent monotone property)
void twoPointers(vector<int>& arr) {
    int left = 0, right = (int)arr.size() - 1;
    
    while (left < right) {
        int current = arr[left] + arr[right]; // or whatever comparison
        
        if (current == target) {
            // process answer
            left++;
            right--;
        } else if (current < target) {
            left++;  // need larger sum
        } else {
            right--; // need smaller sum
        }
    }
}
```

#### Prefix Sum
```cpp
#include <vector>
using namespace std;

vector<int> buildPrefix(const vector<int>& arr) {
    int n = arr.size();
    vector<int> prefix(n + 1, 0);
    for (int i = 0; i < n; i++) {
        prefix[i + 1] = prefix[i] + arr[i];
    }
    return prefix;
}

// Query sum of arr[l..r] (inclusive, 0-indexed)
int rangeSum(const vector<int>& prefix, int l, int r) {
    return prefix[r + 1] - prefix[l];
}
```

#### Sliding Window — Variable Size
```cpp
#include <vector>
#include <unordered_map>
using namespace std;

int slidingWindowMax(const vector<int>& arr, int k) {
    // Example: max sum of any subarray of length <= k with all unique elements
    unordered_map<int, int> window; // tracks element frequencies in window
    int left = 0;
    int maxResult = 0;
    int currentSum = 0;
    
    for (int right = 0; right < (int)arr.size(); right++) {
        // Expand window
        window[arr[right]]++;
        currentSum += arr[right];
        
        // Shrink while constraint violated
        while (window[arr[right]] > 1) { // example constraint: no duplicates
            window[arr[left]]--;
            if (window[arr[left]] == 0) window.erase(arr[left]);
            currentSum -= arr[left];
            left++;
        }
        
        // Window [left, right] is valid; update answer
        maxResult = max(maxResult, currentSum);
    }
    
    return maxResult;
}
```

#### Kadane's Algorithm (Maximum Subarray)
```cpp
int maxSubarray(const vector<int>& arr) {
    int maxSoFar = arr[0];
    int currentMax = arr[0];
    
    for (int i = 1; i < (int)arr.size(); i++) {
        // Either extend previous subarray or start fresh
        currentMax = max(arr[i], currentMax + arr[i]);
        maxSoFar = max(maxSoFar, currentMax);
    }
    
    return maxSoFar;
}
```

---

### TypeScript Templates

#### Two Pointers — Converging
```typescript
function twoPointers(arr: number[], target: number): [number, number] | null {
    // Precondition: arr is sorted
    let left = 0;
    let right = arr.length - 1;
    
    while (left < right) {
        const sum = arr[left] + arr[right];
        
        if (sum === target) {
            return [left, right];
        } else if (sum < target) {
            left++;
        } else {
            right--;
        }
    }
    
    return null;
}
```

#### Prefix Sum
```typescript
function buildPrefix(arr: number[]): number[] {
    const prefix: number[] = new Array(arr.length + 1).fill(0);
    for (let i = 0; i < arr.length; i++) {
        prefix[i + 1] = prefix[i] + arr[i];
    }
    return prefix;
}

// Sum of arr[l..r] inclusive, 0-indexed
function rangeSum(prefix: number[], l: number, r: number): number {
    return prefix[r + 1] - prefix[l];
}
```

#### Sliding Window — Variable Size
```typescript
function longestValidWindow(arr: number[]): number {
    const freq = new Map<number, number>();
    let left = 0;
    let maxLen = 0;
    
    for (let right = 0; right < arr.length; right++) {
        // Expand
        freq.set(arr[right], (freq.get(arr[right]) ?? 0) + 1);
        
        // Shrink while invalid
        while ((freq.get(arr[right]) ?? 0) > 1) {
            const leftVal = arr[left];
            freq.set(leftVal, (freq.get(leftVal) ?? 0) - 1);
            if (freq.get(leftVal) === 0) freq.delete(leftVal);
            left++;
        }
        
        maxLen = Math.max(maxLen, right - left + 1);
    }
    
    return maxLen;
}
```

#### Kadane's Algorithm
```typescript
function maxSubarray(arr: number[]): number {
    let maxSoFar = arr[0];
    let currentMax = arr[0];
    
    for (let i = 1; i < arr.length; i++) {
        currentMax = Math.max(arr[i], currentMax + arr[i]);
        maxSoFar = Math.max(maxSoFar, currentMax);
    }
    
    return maxSoFar;
}
```

---

## 4 & 5. Problems with Full Solutions

---

### Problem 1: Two Sum

**Problem Statement (paraphrased)**:
Given an integer array `nums` and a target integer, return the indices of the two numbers that add up to the target. Exactly one valid answer exists. You may not use the same element twice.

**Intuition**:
For each element `nums[i]`, we need to know if `target - nums[i]` exists in the array at some other index. A hash map lets us answer this in O(1) per element, reducing total time from O(n^2) to O(n).

**Approach**:
Iterate through the array. For each element, compute its complement (`target - nums[i]`). Check if the complement is already in the hash map (meaning we have seen it before). If yes, return the current index and the stored index. If no, store `nums[i] -> i` in the map and continue.

The single-pass approach works because: if indices i < j form the answer, when we reach j we will find i in the map. We never need to look backward again.

**C++ Code**:
```cpp
#include <vector>
#include <unordered_map>
using namespace std;

class Solution {
public:
    vector<int> twoSum(vector<int>& nums, int target) {
        unordered_map<int, int> seen; // value -> index
        
        for (int i = 0; i < (int)nums.size(); i++) {
            int complement = target - nums[i];
            
            if (seen.count(complement)) {
                return {seen[complement], i};
            }
            
            seen[nums[i]] = i;
        }
        
        return {}; // guaranteed answer exists per problem statement
    }
};
```

**TypeScript Code**:
```typescript
function twoSum(nums: number[], target: number): number[] {
    const seen = new Map<number, number>(); // value -> index
    
    for (let i = 0; i < nums.length; i++) {
        const complement = target - nums[i];
        
        if (seen.has(complement)) {
            return [seen.get(complement)!, i];
        }
        
        seen.set(nums[i], i);
    }
    
    return []; // unreachable per problem guarantee
}
```

**Complexity**:
- Time: O(n) — single pass, O(1) hash map operations
- Space: O(n) — hash map stores up to n entries

**Edge Cases**:
- Duplicate values: `nums = [3, 3]`, `target = 6` → answer is `[0, 1]`. Works correctly because we check the map before inserting; when we reach index 1, index 0's value (3) is already in the map.
- Negative numbers: handled transparently by hash map.
- Single-element array: impossible by problem constraint (need two elements).
- What if no solution: problem guarantees one, but defensive code returns `[]`.

---

### Problem 2: Best Time to Buy and Sell Stock

**Problem Statement (paraphrased)**:
Given an array `prices` where `prices[i]` is the stock price on day i, find the maximum profit from one buy and one sell transaction. You must buy before you sell. Return 0 if no profit is possible.

**Intuition**:
To maximize profit, you want to buy at the minimum price seen so far and sell at some future day. A single pass works: track the running minimum price. At each day, the best profit if you sell today is `prices[i] - minSoFar`. Update the global maximum.

This is a sliding window / greedy approach. You do not need to enumerate all pairs.

**Approach**:
Initialize `minPrice = INT_MAX`, `maxProfit = 0`. For each price:
1. Update `maxProfit = max(maxProfit, price - minPrice)`
2. Update `minPrice = min(minPrice, price)`

The order matters: check profit before updating minPrice, otherwise you'd allow buying and selling on the same day at zero profit (which is harmless here but conceptually wrong).

**C++ Code**:
```cpp
#include <vector>
#include <climits>
using namespace std;

class Solution {
public:
    int maxProfit(vector<int>& prices) {
        int minPrice = INT_MAX;
        int maxProfit = 0;
        
        for (int price : prices) {
            maxProfit = max(maxProfit, price - minPrice);
            minPrice = min(minPrice, price);
        }
        
        return maxProfit;
    }
};
```

**TypeScript Code**:
```typescript
function maxProfit(prices: number[]): number {
    let minPrice = Infinity;
    let maxProfit = 0;
    
    for (const price of prices) {
        maxProfit = Math.max(maxProfit, price - minPrice);
        minPrice = Math.min(minPrice, price);
    }
    
    return maxProfit;
}
```

**Complexity**:
- Time: O(n) — single pass
- Space: O(1) — only two variables

**Edge Cases**:
- Strictly decreasing prices: `[5, 4, 3, 2, 1]` → profit is 0 (never buy). Handled because `price - minPrice` is always negative or zero in decreasing sequence, and `maxProfit` starts at 0.
- Single element: returns 0 (cannot buy and sell).
- All same price: returns 0.
- Large price swings: works with standard int arithmetic; for very large inputs, use `long long` in C++.

---

### Problem 3: Contains Duplicate

**Problem Statement (paraphrased)**:
Given an integer array `nums`, return `true` if any value appears at least twice. Return `false` if all elements are distinct.

**Intuition**:
We need to detect repeated values. A hash set allows O(1) insertion and lookup. As we scan, if the current element already exists in the set, we found a duplicate.

**Approach**:
Maintain a hash set of seen values. For each element, check if it is in the set. If yes, return true. If no, insert it. If we finish the loop, return false.

Alternative O(n log n) approach: sort the array, then scan for adjacent duplicates. This is O(1) extra space (excluding sort stack) but slower. Use hash set unless space is constrained.

**C++ Code**:
```cpp
#include <vector>
#include <unordered_set>
using namespace std;

class Solution {
public:
    bool containsDuplicate(vector<int>& nums) {
        unordered_set<int> seen;
        
        for (int num : nums) {
            if (seen.count(num)) {
                return true;
            }
            seen.insert(num);
        }
        
        return false;
    }
};

// Alternative: one-liner using set size comparison
// return unordered_set<int>(nums.begin(), nums.end()).size() != nums.size();
```

**TypeScript Code**:
```typescript
function containsDuplicate(nums: number[]): boolean {
    const seen = new Set<number>();
    
    for (const num of nums) {
        if (seen.has(num)) {
            return true;
        }
        seen.add(num);
    }
    
    return false;
}

// Alternative one-liner:
// return new Set(nums).size !== nums.length;
```

**Complexity**:
- Time: O(n) average — n hash set operations
- Space: O(n) — hash set stores up to n elements

**Edge Cases**:
- Empty array: returns false (no elements, no duplicate).
- Single element: returns false.
- All same: returns true on second element.
- Hash collision (worst case): hash set degrades to O(n) per lookup in truly adversarial input, giving O(n^2) total. In practice, this does not happen with integers in modern implementations.
- If O(1) space required: sort in place then check adjacent pairs — O(n log n), O(1) extra space.

---

### Problem 4: Maximum Subarray

**Problem Statement (paraphrased)**:
Given an integer array `nums` (may contain negative numbers), find the contiguous subarray with the largest sum and return that sum.

**Intuition**:
Kadane's algorithm is a classic dynamic programming approach that runs in O(n). The key insight: at each position i, the maximum subarray ending at i is either:
1. Just `nums[i]` alone (start a fresh subarray), or
2. `nums[i]` appended to the best subarray ending at i-1

If the best subarray ending at i-1 has a negative sum, it hurts us to extend it — we are better off starting fresh.

**Approach**:
- `currentMax` = max sum of any subarray ending at current index
- `maxSoFar` = global maximum seen so far
- Recurrence: `currentMax = max(nums[i], currentMax + nums[i])`
- This is equivalent to: extend previous subarray if it helps, otherwise reset

**C++ Code**:
```cpp
#include <vector>
#include <algorithm>
using namespace std;

class Solution {
public:
    int maxSubArray(vector<int>& nums) {
        int currentMax = nums[0];
        int maxSoFar = nums[0];
        
        for (int i = 1; i < (int)nums.size(); i++) {
            currentMax = max(nums[i], currentMax + nums[i]);
            maxSoFar = max(maxSoFar, currentMax);
        }
        
        return maxSoFar;
    }
};

// If you also need to return the subarray indices:
// Track start, end, tempStart indices alongside.
```

**TypeScript Code**:
```typescript
function maxSubArray(nums: number[]): number {
    let currentMax = nums[0];
    let maxSoFar = nums[0];
    
    for (let i = 1; i < nums.length; i++) {
        currentMax = Math.max(nums[i], currentMax + nums[i]);
        maxSoFar = Math.max(maxSoFar, currentMax);
    }
    
    return maxSoFar;
}
```

**Complexity**:
- Time: O(n) — single pass
- Space: O(1) — two variables

**Edge Cases**:
- All negative numbers: `[-5, -3, -1]` → returns `-1`. Correct — we cannot avoid all negative numbers, so we pick the least negative. This works because we initialize `maxSoFar = nums[0]` (not 0) and update on the first iteration.
- Single element: returns that element.
- All positive: returns sum of entire array.
- Large array with alternating positive/negative: Kadane's handles correctly by dynamically deciding whether to extend.
- Common mistake: initializing `maxSoFar = 0` fails for all-negative inputs. Always initialize to `nums[0]`.

---

### Problem 5: Product of Array Except Self

**Problem Statement (paraphrased)**:
Given an integer array `nums`, return an array `answer` where `answer[i]` is the product of all elements of `nums` except `nums[i]`. Solve in O(n) time without using division. Follow-up: O(1) extra space (output array does not count).

**Intuition**:
Division would be trivial: compute total product, divide by each element. The constraint forbids division (and breaks on zeros).

Instead, observe that `answer[i]` = (product of all elements to the left of i) * (product of all elements to the right of i).

Compute left products in one pass left-to-right. Compute right products in one pass right-to-left. Multiply them.

**Approach** (O(n) space, then optimized to O(1)):

Two-array approach:
1. `left[i]` = product of `nums[0..i-1]`, with `left[0] = 1`
2. `right[i]` = product of `nums[i+1..n-1]`, with `right[n-1] = 1`
3. `answer[i] = left[i] * right[i]`

O(1) space optimization:
1. Build `answer` as the left products array in a forward pass
2. Traverse right-to-left, maintaining a running right product (`R`) and multiplying into `answer[i]`

**C++ Code**:
```cpp
#include <vector>
using namespace std;

class Solution {
public:
    vector<int> productExceptSelf(vector<int>& nums) {
        int n = nums.size();
        vector<int> answer(n, 1);
        
        // Forward pass: answer[i] = product of nums[0..i-1]
        int leftProduct = 1;
        for (int i = 0; i < n; i++) {
            answer[i] = leftProduct;
            leftProduct *= nums[i];
        }
        
        // Backward pass: multiply in product of nums[i+1..n-1]
        int rightProduct = 1;
        for (int i = n - 1; i >= 0; i--) {
            answer[i] *= rightProduct;
            rightProduct *= nums[i];
        }
        
        return answer;
    }
};
```

**TypeScript Code**:
```typescript
function productExceptSelf(nums: number[]): number[] {
    const n = nums.length;
    const answer: number[] = new Array(n).fill(1);
    
    // Forward pass: answer[i] = product of nums[0..i-1]
    let leftProduct = 1;
    for (let i = 0; i < n; i++) {
        answer[i] = leftProduct;
        leftProduct *= nums[i];
    }
    
    // Backward pass: multiply in product of nums[i+1..n-1]
    let rightProduct = 1;
    for (let i = n - 1; i >= 0; i--) {
        answer[i] *= rightProduct;
        rightProduct *= nums[i];
    }
    
    return answer;
}
```

**Complexity**:
- Time: O(n) — two passes
- Space: O(1) extra (output array excluded from space complexity per problem statement)

**Edge Cases**:
- Array contains a single zero: all products are 0 except at the zero's index, where the product is the product of all non-zero elements. The algorithm handles this naturally without special casing.
- Array contains two or more zeros: all products are 0. Also handled naturally.
- Negative numbers: handled correctly since we only multiply, no sign manipulation.
- Array of length 2: `[a, b]` → `[b, a]`. Works correctly.
- Overflow: not guarded in the above code. For production code, use `long long` in C++ or `BigInt` in TypeScript, or note that problem constraints bound the product.
- Common mistake: forgetting to initialize `leftProduct = 1` before the loop (not `leftProduct = nums[0]`).

---

## Pattern Summary

| Problem | Pattern Used | Key Insight |
|---------|-------------|-------------|
| Two Sum | Hash map | Trade space for O(1) complement lookup |
| Best Time to Buy Stock | Greedy / running min | Track best buy point as we scan |
| Contains Duplicate | Hash set | O(1) membership test replaces O(n) scan |
| Maximum Subarray | Kadane's (1D DP) | Extend or reset based on running sum sign |
| Product Except Self | Prefix product (left + right) | Decompose into two prefix passes |
