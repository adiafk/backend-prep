# Array Interview Notes — Traps, Tips, and Common Mistakes

These notes document the mistakes that cost candidates the most in interviews. Read this before any array problem session.

---

## Common Traps

### Trap 1: Off-by-One in Two-Pointer Problems

The most frequent bug. The confusion comes from whether the loop condition is `left < right` or `left <= right`.

**Rule**: Use `left < right` when the two pointers should never overlap (pair-finding, palindrome check). Use `left <= right` when processing each element individually (binary search).

**Example**: In Valid Palindrome with `left` and `right` converging:
- You want `left < right` so you do not compare an element with itself
- Using `left <= right` checks `arr[i] == arr[i]` which is always true and gives wrong answers on odd-length inputs

**Check yourself**: Before submitting, trace through a single-element array and a two-element array.

---

### Trap 2: Index Initialization for Maximum/Minimum Problems

Initializing `maxVal = 0` or `minVal = 0` silently fails when all values are negative (or all positive, respectively).

**Wrong**:
```cpp
int maxVal = 0; // fails if all nums are negative
for (int n : nums) maxVal = max(maxVal, n);
```

**Correct**:
```cpp
int maxVal = nums[0]; // or INT_MIN
for (int i = 1; i < nums.size(); i++) maxVal = max(maxVal, nums[i]);
```

This applies to: Maximum Subarray (Kadane's), Best Time to Buy Stock, finding peak elements. Always initialize to the first element or to INT_MIN/INT_MAX.

---

### Trap 3: Modifying the Array While Iterating

Removing elements from the front of a vector while iterating causes index shifts. Elements get skipped.

**Wrong**:
```cpp
for (int i = 0; i < nums.size(); i++) {
    if (shouldRemove(nums[i])) {
        nums.erase(nums.begin() + i); // i is now pointing to the next element, not the current
    }
}
```

**Correct options**:
1. Iterate backward when erasing
2. Use two-pointer in-place overwrite: copy "keepers" forward with a write pointer
3. Collect indices to remove, then erase in a separate pass

The two-pointer in-place approach is the standard interview answer for "remove elements in-place":
```cpp
int writePtr = 0;
for (int readPtr = 0; readPtr < nums.size(); readPtr++) {
    if (!shouldRemove(nums[readPtr])) {
        nums[writePtr++] = nums[readPtr];
    }
}
nums.resize(writePtr);
```

---

### Trap 4: Integer Overflow in Prefix Sum / Product Problems

Summing n=10^5 elements each up to 10^9 gives a sum up to 10^14, which overflows `int` (max ~2.1 * 10^9).

**Always use `long long` in C++ for sum/product problems unless the problem explicitly bounds the result to int range.**

```cpp
// Wrong
int prefixSum = 0;

// Correct
long long prefixSum = 0;
```

In TypeScript, JavaScript numbers are 64-bit floats. They can represent integers exactly up to 2^53 - 1 (~9 * 10^15). For larger values, use `BigInt`.

---

### Trap 5: Sliding Window on Arrays with Negative Numbers

Sliding window assumes that expanding the window always makes it "worse" (or "better") in a monotone way. With negative numbers, adding an element can improve a sum-based constraint — which breaks the shrinking logic.

**Wrong approach**: Using sliding window for "subarray sum equals K" on an array with negatives.

**Correct approach**: Prefix sum + hash map. This is unconditionally correct and handles negatives.

```
// At each index i:
// count subarrays ending at i with sum == K
// <=> count j where prefix[i] - prefix[j] == K
//    <=> count j where prefix[j] == prefix[i] - K
```

**Mental check**: Before applying sliding window to a sum problem, ask: "Does this array have negative numbers?" If yes, use prefix sum + hash map instead.

---

### Trap 6: Confusing "Subarray" with "Subsequence"

- **Subarray**: contiguous elements. `[1,2,3]` has subarrays `[1]`, `[2]`, `[3]`, `[1,2]`, `[2,3]`, `[1,2,3]`.
- **Subsequence**: elements in order but not necessarily contiguous. `[1,3]` is a subsequence of `[1,2,3]`.

**Subarray problems** → sliding window, Kadane's, prefix sum.
**Subsequence problems** → dynamic programming (LIS, LCS), two pointers on sorted arrays.

Misidentifying this collapses your entire approach.

---

### Trap 7: Assuming `unordered_map` Is Always Faster Than `map`

`unordered_map` has O(1) average but O(n) worst case per operation. In competitive programming, adversarial test cases can trigger worst-case behavior in `unordered_map` (hash flooding).

For interviews, `unordered_map` is fine — but be aware:
- `map` is O(log n) per operation but deterministic
- If an interviewer questions your hash map's complexity, acknowledge the worst case
- For integer keys, `unordered_map<int, int>` with a good hash function is safe in practice

---

### Trap 8: Not Handling Empty Array Input

Many solutions crash or return wrong values on empty input because they access `nums[0]` unconditionally.

**Always add an early return**:
```cpp
if (nums.empty()) return 0; // or -1, or {}, depending on return type
```

Interviewers notice when you handle edge cases without prompting. It signals maturity.

---

### Trap 9: Forgetting That `size()` Returns Unsigned in C++

`vector::size()` returns `size_t`, which is an unsigned type. Expressions like `nums.size() - 1` when `nums` is empty return a huge positive number, not -1.

```cpp
// Dangerous if nums is empty
for (int i = 0; i < nums.size() - 1; i++) { ... }
// nums.size() - 1 wraps around to SIZE_MAX if size is 0

// Safe
for (int i = 0; i + 1 < (int)nums.size(); i++) { ... }
// or
int n = (int)nums.size();
for (int i = 0; i < n - 1; i++) { ... }
```

**Habit**: Cast `nums.size()` to `int` immediately and store as `n`.

---

### Trap 10: Two Sum — Same Index Used Twice

Problem says you cannot use the same element twice. The hash map approach naturally avoids this because you check for the complement before inserting the current element. But confirm this when explaining your solution.

If you insert before checking:
```cpp
// Wrong: allows using same element twice
seen[nums[i]] = i;
if (seen.count(target - nums[i])) { ... } // could find nums[i] itself
```

```cpp
// Correct: check first, then insert
if (seen.count(target - nums[i])) { return {seen[target - nums[i]], i}; }
seen[nums[i]] = i;
```

---

## Tips for Interviews

### Tip 1: Say What You're Checking Before You Code

Before writing any line of code, say out loud:
- "What is my time complexity goal?"
- "Does this array have any special properties (sorted, all positive, bounded range)?"
- "What should I return on empty input?"

This delays the clock slightly but signals strong problem decomposition. Interviewers value this far more than fast-but-wrong code.

---

### Tip 2: Use a Two-Pass Approach to Break Deadlocks

When you are stuck on how to do something in one pass, solve it in two passes first. Two-pass O(n) is almost always accepted, and the two-pass version often reveals the one-pass insight.

Product of Array Except Self is the canonical example: building separate left and right arrays first makes the O(1) space optimization obvious.

---

### Tip 3: "What Would Brute Force Look Like?" Is a Valid Starting Point

State the brute force O(n^2) or O(n^3) approach immediately, then say: "I want to eliminate the inner loop. What information am I recomputing each time?" This reasoning aloud demonstrates that you understand the complexity bottleneck.

The answer is almost always: "I can precompute [prefix sum / hash map / sorted structure] to answer each inner-loop query in O(1)."

---

### Tip 4: For "In-Place" Problems, Think About Invariants

When forced to work in-place, define your invariant clearly:
- "Everything to the left of `writePtr` satisfies the condition."
- "I will never read from `writePtr` again."

This prevents you from clobbering data you still need. Write this invariant as a comment in your code — it shows structured thinking.

---

### Tip 5: Trace Through Your Solution on the Example Before Submitting

Spend 60 seconds tracing your final code on the provided example. This catches:
- Off-by-one errors
- Wrong loop boundary
- Uninitialized variable
- Missing return for empty input

For every problem that has an all-negative or all-zero input, mentally trace that too.

---

### Tip 6: Know When Hash Map Beats Two Pointers

Two pointers require sorting or a pre-existing monotone property. Hash map does not.

| Situation | Use |
|-----------|-----|
| Array is sorted | Two pointers |
| Array is unsorted, cannot sort | Hash map |
| Need original indices | Hash map (sorting destroys indices) |
| O(1) space required and sorting allowed | Two pointers after sorting |

Two Sum asks for indices → cannot sort → hash map.
Two Sum II (sorted input) → two pointers.

---

### Tip 7: Prefix Sum + Hash Map Is the Swiss Army Knife for Subarray Problems

Memorize this pattern cold. It solves:
- Subarray sum equals K (count or existence)
- Subarray sum divisible by K
- Binary array: longest subarray with equal 0s and 1s (convert 0 to -1, find subarray sum = 0)
- Submatrix sum equals target (extend prefix sum to 2D, reduce each row pair to 1D problem)

The general form:
```
running_sum -> (initialize with {0: 1} for "empty prefix" base case)
for each element:
    update running_sum
    answer += map[running_sum - target]
    map[running_sum]++
```

The `{0: 1}` initialization handles subarrays starting from index 0.

---

### Tip 8: On Hard Array Problems, Think "Can I Reduce to a Simpler Problem?"

- 2D grid problems often reduce to a 1D array problem applied per row or per column
- Hard subarray problems often reduce to hash map + prefix sum
- K-element window problems reduce to basic sliding window
- "Best path" in a grid reduces to DP on rows

The reduction insight is usually worth more points in an interview than the implementation detail.

---

## Red Flags to Avoid in Your Own Code

- `arr[-1]` access: always bounds check
- Nested loop that is actually O(n^2) where you told the interviewer O(n)
- Returning early before processing all edge cases
- Using `int` for sums/products without checking overflow bounds
- Not resetting variables between test cases (matters in competitive programming; less so in interview but still a habit issue)
- Writing code before explaining your approach

---

## Quick Reference: Complexity Targets

Know these target complexities before you start coding. If your solution cannot hit the target, say so and discuss why.

| Problem Type | Expected Time | Expected Space |
|-------------|---------------|----------------|
| Simple scan / count | O(n) | O(1) |
| Pair finding (sorted) | O(n) | O(1) |
| Pair finding (unsorted) | O(n) | O(n) |
| Subarray sum problems | O(n) | O(n) |
| Sliding window | O(n) | O(1) to O(k) |
| Sort then process | O(n log n) | O(1) or O(n) |
| Range queries | O(n) build, O(1) query | O(n) |
