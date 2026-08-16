# Binary Search — DSA Pattern

Binary search is one of the most misunderstood patterns in competitive programming.
The algorithm is trivial. Getting the boundaries exactly right is not.
Master binary search and you gain access to an entire class of problems that look unsolvable at first glance.

---

## Table of Contents

1. [Core Idea](#core-idea)
2. [Template 1: `left <= right` — Exact Match](#template-1-left--right--exact-match)
3. [Template 2: `left < right` — Bound Search](#template-2-left--right--bound-search)
4. [Finding Exact Value vs Lower Bound vs Upper Bound](#finding-exact-value-vs-lower-bound-vs-upper-bound)
5. [Binary Search on Answer — The Most Powerful Application](#binary-search-on-answer--the-most-powerful-application)
6. [Rotated Arrays](#rotated-arrays)
7. [Solved Problems](#solved-problems)
8. [Complexity Reference](#complexity-reference)

---

## Core Idea

Binary search works on any **sorted** or **monotone** structure. At each step, you eliminate half the remaining search space by comparing a midpoint against a condition.

The invariant you must maintain: **the answer is always inside `[left, right]`.**

Every iteration must strictly shrink the range. If you ever write code where `left` or `right` could stay the same after an iteration, you have an infinite loop bug.

**The one rule that prevents all overflow:**
```cpp
int mid = left + (right - left) / 2;   // CORRECT — never overflows
int mid = (left + right) / 2;          // WRONG — overflows when left+right > INT_MAX
```

---

## Template 1: `left <= right` — Exact Match

Use this when you are looking for a **specific value** and want to return its index, or confirm it does not exist.

```cpp
int binarySearch(vector<int>& nums, int target) {
    int left = 0, right = nums.size() - 1;  // both inclusive

    while (left <= right) {                  // loop while there is at least one element
        int mid = left + (right - left) / 2;

        if (nums[mid] == target) return mid;
        else if (nums[mid] < target) left = mid + 1;
        else right = mid - 1;
    }

    return -1;
}
```

**Why `left <= right`:**
- `right` is initialized to `size - 1` (last valid index).
- When `left == right`, we still have one unchecked element. The `<=` handles it.
- The loop exits when `left > right` — search space is exhausted.
- `mid` is always checked as a candidate and immediately discarded afterward (`mid + 1` or `mid - 1`). No element is ever reconsidered.

**When to use:**
- LeetCode: Binary Search (704)
- Any problem: "does this value exist at index X?"

---

## Template 2: `left < right` — Bound Search

Use this when you are looking for a **boundary** — the first index satisfying a condition.

```cpp
int lowerBound(vector<int>& nums, int target) {
    int left = 0, right = nums.size();      // right = size (one past end is valid answer)

    while (left < right) {                  // exit when left == right (that IS the answer)
        int mid = left + (right - left) / 2; // floor division — mid < right always

        if (nums[mid] < target) left = mid + 1;  // mid is too small, answer is after mid
        else right = mid;                         // mid might be the answer, keep it
    }

    return left;  // left == right == first index where nums[i] >= target
}
```

**Why `left < right`:**
- We never check `mid` as the final answer inside the loop. We narrow until `left == right`.
- `right = mid` (not `mid - 1`) because `mid` could itself be the answer.
- Floor division (`left + (right-left)/2`) guarantees `mid < right`, so `right = mid` always shrinks the range.
- If you used ceiling division with `right = mid`, you would get an infinite loop when `left + 1 == right`.

**Why `right = nums.size()` instead of `size - 1`:**
- If every element is smaller than the target, the answer is "insert at the end" — position `nums.size()`.
- Initializing `right = size - 1` would incorrectly miss this case.

**When to use:**
- Finding the first position where a condition becomes true.
- Lower bound / upper bound.
- Binary search on answer (Template 3 is a variant of this).

---

## Finding Exact Value vs Lower Bound vs Upper Bound

Given `nums = [1, 3, 3, 3, 5]` and `target = 3`:

| Operation | Result | Meaning |
|-----------|--------|---------|
| Exact search | 1, 2, or 3 (any) | Some index of value 3 |
| Lower bound | 1 | First index where `nums[i] >= 3` |
| Upper bound | 4 | First index where `nums[i] > 3` |
| Count of 3s | upper - lower = 3 | How many times 3 appears |

```cpp
// Lower bound: first index where nums[i] >= target
int lowerBound(vector<int>& nums, int target) {
    int l = 0, r = nums.size();
    while (l < r) {
        int m = l + (r - l) / 2;
        if (nums[m] < target) l = m + 1;
        else r = m;
    }
    return l;
}

// Upper bound: first index where nums[i] > target
int upperBound(vector<int>& nums, int target) {
    int l = 0, r = nums.size();
    while (l < r) {
        int m = l + (r - l) / 2;
        if (nums[m] <= target) l = m + 1;   // only difference: <= instead of <
        else r = m;
    }
    return l;
}

// Count occurrences of target
int countOccurrences(vector<int>& nums, int target) {
    return upperBound(nums, target) - lowerBound(nums, target);
}
```

The only difference between lower and upper bound is `<` vs `<=` in the condition.
Memorize this pair — it handles every range query on a sorted array.

---

## Binary Search on Answer — The Most Powerful Application

This is the insight that separates intermediate from advanced competitive programmers.

**You do not need an array to binary search.** You need:
1. A **monotone answer space** — a range `[lo, hi]` where every value is either valid or invalid.
2. A **feasibility function** — given a candidate answer, can you check in O(something) if it works?
3. The values in the range transition from infeasible to feasible exactly once (or vice versa).

```
Answer space:  [lo ........... hi]
Feasibility:   [F, F, F, T, T, T]
                          ^--- find this boundary
```

**Template:**
```cpp
// Find the MINIMUM feasible value in [lo, hi]
int binarySearchOnAnswer(int lo, int hi) {
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (isFeasible(mid)) hi = mid;    // mid works; answer is mid or smaller
        else lo = mid + 1;               // mid fails; answer must be larger
    }
    return lo;  // lo == hi == minimum feasible value
}

// Find the MAXIMUM feasible value in [lo, hi]
int binarySearchOnAnswerMax(int lo, int hi) {
    while (lo < hi) {
        int mid = lo + (hi - lo + 1) / 2; // CEILING division — prevents infinite loop
        if (isFeasible(mid)) lo = mid;    // mid works; answer is mid or larger
        else hi = mid - 1;               // mid fails; answer must be smaller
    }
    return lo;
}
```

**Why ceiling division for the maximum variant:**
When `lo + 1 == hi` and you do `mid = lo + (hi-lo)/2 = lo`, then `lo = mid = lo` — infinite loop.
Ceiling division gives `mid = hi`, and either `lo = hi` (converge) or `hi = hi - 1` (shrink). Both terminate.

**Recognizing "binary search on answer" problems:**
- "Find the **minimum** X such that [condition]"
- "Find the **maximum** X such that [condition]"
- "What is the **smallest** allocation / speed / capacity / time that works?"
- The condition must be **monotone**: if X works, then X+1 either also works (minimization) or doesn't (maximization)

**Classic examples:**
- Koko Eating Bananas — minimum eating speed
- Capacity to Ship Packages — minimum ship capacity
- Split Array Largest Sum — minimum possible largest subarray sum
- Aggressive Cows — maximum minimum distance between cows

---

## Rotated Arrays

A **rotated sorted array** is a sorted array that has been rotated at some pivot. For example:
```
Original:  [1, 2, 3, 4, 5, 6, 7]
Rotated:   [4, 5, 6, 7, 1, 2, 3]
                    ^--- pivot
```

**Key insight:** In a rotated array, when you pick any `mid`, **at least one half is always fully sorted.**

Check which half is sorted using `nums[left] <= nums[mid]`:
- If left half is sorted (`nums[left] <= nums[mid]`): check if target is in `[nums[left], nums[mid])`. If yes, search left. Otherwise search right.
- If right half is sorted: check if target is in `(nums[mid], nums[right]]`. If yes, search right. Otherwise search left.

```cpp
// Search in rotated array — O(log n)
int searchRotated(vector<int>& nums, int target) {
    int left = 0, right = nums.size() - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (nums[mid] == target) return mid;

        if (nums[left] <= nums[mid]) {          // left half is sorted
            if (nums[left] <= target && target < nums[mid])
                right = mid - 1;               // target in sorted left half
            else
                left = mid + 1;
        } else {                                // right half is sorted
            if (nums[mid] < target && target <= nums[right])
                left = mid + 1;                // target in sorted right half
            else
                right = mid - 1;
        }
    }
    return -1;
}
```

**Finding the minimum in a rotated array:**
The minimum is at the rotation pivot. The minimum is always in the **unsorted** half.
If `nums[mid] > nums[right]`, the minimum is in the right half. Otherwise it is in the left half (including `mid`).

```cpp
int findMin(vector<int>& nums) {
    int left = 0, right = nums.size() - 1;

    while (left < right) {
        int mid = left + (right - left) / 2;
        if (nums[mid] > nums[right]) left = mid + 1;   // min is in right half
        else right = mid;                               // mid could be min
    }
    return nums[left];
}
```

---

## Solved Problems

---

### Problem 1: Binary Search (LeetCode 704)

**Intuition:** The array is sorted. For any `mid`, elements to the left are smaller and elements to the right are larger. Exactly half the array is eliminated at each step.

**Why binary search applies:** Direct application. Sorted array, exact value search.

```cpp
class Solution {
public:
    int search(vector<int>& nums, int target) {
        int left = 0, right = (int)nums.size() - 1;

        while (left <= right) {
            int mid = left + (right - left) / 2;

            if (nums[mid] == target) return mid;
            else if (nums[mid] < target) left = mid + 1;
            else right = mid - 1;
        }

        return -1;
    }
};
```

**Complexity:**
- Time: O(log n)
- Space: O(1)

---

### Problem 2: Search in Rotated Sorted Array (LeetCode 33)

**Intuition:** Even after rotation, when you pick a midpoint, at least one of the two halves is guaranteed to be fully sorted. Use this property to decide which half to search.

**Why binary search applies:** By checking which half is sorted, you can determine with certainty whether the target is in that half or not — eliminating half the array each iteration.

```cpp
class Solution {
public:
    int search(vector<int>& nums, int target) {
        int left = 0, right = (int)nums.size() - 1;

        while (left <= right) {
            int mid = left + (right - left) / 2;

            if (nums[mid] == target) return mid;

            // Determine which half is sorted
            if (nums[left] <= nums[mid]) {
                // Left half [left, mid] is sorted
                if (nums[left] <= target && target < nums[mid]) {
                    right = mid - 1;    // target is in the sorted left half
                } else {
                    left = mid + 1;     // target is in the right half
                }
            } else {
                // Right half [mid, right] is sorted
                if (nums[mid] < target && target <= nums[right]) {
                    left = mid + 1;     // target is in the sorted right half
                } else {
                    right = mid - 1;    // target is in the left half
                }
            }
        }

        return -1;
    }
};
```

**Complexity:**
- Time: O(log n)
- Space: O(1)

**Edge case note:** `nums[left] <= nums[mid]` uses `<=` to handle the case where `left == mid` (single element — always "sorted").

---

### Problem 3: Find Minimum in Rotated Sorted Array (LeetCode 153)

**Intuition:** The minimum element is the inflection point — the one element where the value is less than its predecessor. The array has two segments: both sorted, with the minimum at the start of the second segment.

**Why binary search applies:** Compare `nums[mid]` with `nums[right]`. If `nums[mid] > nums[right]`, the rotation pivot (minimum) must be in the right half. Otherwise the minimum is in the left half (including `mid`).

```cpp
class Solution {
public:
    int findMin(vector<int>& nums) {
        int left = 0, right = (int)nums.size() - 1;

        while (left < right) {
            int mid = left + (right - left) / 2;

            if (nums[mid] > nums[right]) {
                // mid is in the larger left segment; min is in the right segment
                left = mid + 1;
            } else {
                // mid is in the smaller right segment (or is the min itself)
                right = mid;
            }
        }

        // left == right == index of minimum element
        return nums[left];
    }
};
```

**Complexity:**
- Time: O(log n)
- Space: O(1)

**Why compare against `nums[right]` and not `nums[left]`:**
If we compared against `nums[left]`, in the non-rotated case (`nums` is fully sorted), `nums[mid] >= nums[left]` is always true, so we would always go right and never find the minimum at index 0. Comparing against `nums[right]` correctly handles both rotated and non-rotated inputs.

---

### Problem 4: Koko Eating Bananas (LeetCode 875)

**Problem:** Koko has piles of bananas and `h` hours. She eats at speed `k` bananas/hour (one pile per hour, partially if the pile is smaller). Find the minimum integer `k` such that she can eat all bananas in `h` hours.

**Intuition:** If Koko can finish at speed `k`, she can also finish at speed `k+1`. The feasible speeds form a monotone increasing sequence `[false, false, ..., true, true, true]`. Binary search on the answer space.

**Why binary search applies:** The answer space is `[1, max(piles)]`. The feasibility function (can she finish in time?) is monotone in `k`. Binary search finds the minimum `k` in O(log(max_pile)) steps, each costing O(n) to evaluate.

```cpp
class Solution {
public:
    int minEatingSpeed(vector<int>& piles, int h) {
        // Answer space: [1, max(piles)]
        int lo = 1, hi = *max_element(piles.begin(), piles.end());

        while (lo < hi) {
            int mid = lo + (hi - lo) / 2;

            if (canFinish(piles, mid, h)) {
                hi = mid;       // mid works; try slower (smaller k)
            } else {
                lo = mid + 1;   // mid is too slow; must go faster
            }
        }

        return lo;  // minimum feasible speed
    }

private:
    bool canFinish(vector<int>& piles, int speed, int h) {
        int hoursNeeded = 0;
        for (int pile : piles) {
            // Ceiling division: ceil(pile / speed)
            hoursNeeded += (pile + speed - 1) / speed;
            if (hoursNeeded > h) return false;  // early exit
        }
        return hoursNeeded <= h;
    }
};
```

**Complexity:**
- Time: O(n log M) where M = max(piles), n = number of piles
- Space: O(1)

**The pattern generalized:**
Any problem of the form "find the minimum X such that [greedy check passes]" uses this exact structure. The key is writing `canFinish`/`isFeasible` correctly.

---

### Problem 5: Median of Two Sorted Arrays (LeetCode 4)

**Problem:** Find the median of two sorted arrays of total size `m + n` in O(log(m+n)) time.

**Intuition:** Partition both arrays such that the left halves together contain exactly `(m+n)/2` elements, and every element in the left halves is <= every element in the right halves. Binary search on the partition point in the smaller array.

**Why binary search applies:** We binary search on the partition index `i` in array A (size m). For each `i`, the corresponding partition `j` in array B is determined. We check whether the partition is valid (max of left halves <= min of right halves). This check is monotone in `i`.

```cpp
class Solution {
public:
    double findMedianSortedArrays(vector<int>& nums1, vector<int>& nums2) {
        // Ensure nums1 is the smaller array (binary search on the smaller one)
        if (nums1.size() > nums2.size()) return findMedianSortedArrays(nums2, nums1);

        int m = nums1.size(), n = nums2.size();
        int lo = 0, hi = m;
        int half = (m + n + 1) / 2;  // elements we want in the combined left partition

        while (lo <= hi) {
            int i = lo + (hi - lo) / 2;  // partition index in nums1 (0..m elements on the left)
            int j = half - i;             // partition index in nums2

            // Elements just left/right of each partition (handle out-of-bounds with sentinels)
            int maxLeft1  = (i == 0) ? INT_MIN : nums1[i - 1];
            int minRight1 = (i == m) ? INT_MAX : nums1[i];
            int maxLeft2  = (j == 0) ? INT_MIN : nums2[j - 1];
            int minRight2 = (j == n) ? INT_MAX : nums2[j];

            if (maxLeft1 <= minRight2 && maxLeft2 <= minRight1) {
                // Valid partition found
                if ((m + n) % 2 == 1) {
                    return max(maxLeft1, maxLeft2);     // odd total: median is max of left halves
                } else {
                    return (max(maxLeft1, maxLeft2) + min(minRight1, minRight2)) / 2.0;
                }
            } else if (maxLeft1 > minRight2) {
                hi = i - 1;     // i is too large; move partition left in nums1
            } else {
                lo = i + 1;     // i is too small; move partition right in nums1
            }
        }

        return 0.0; // unreachable for valid input
    }
};
```

**Complexity:**
- Time: O(log(min(m, n)))
- Space: O(1)

**Understanding the partition:**
- `i` elements from nums1 are on the left. `j = half - i` elements from nums2 are on the left.
- Valid partition: `maxLeft1 <= minRight2` AND `maxLeft2 <= minRight1`.
- If `maxLeft1 > minRight2`, we moved too many elements from nums1 to the left — decrease `i`.
- If `maxLeft2 > minRight1`, we moved too few elements from nums1 — increase `i`.

---

### Problem 6: Find Peak Element (LeetCode 162)

**Problem:** A peak element is one that is greater than its neighbors. Find any peak. The array is not globally sorted. `nums[-1] = nums[n] = -infinity`.

**Intuition:** If `nums[mid] < nums[mid+1]`, the right side is "going up" — a peak must exist to the right (either `mid+1` itself is a peak or the rising trend continues and must eventually fall). By always moving toward the higher neighbor, we are guaranteed to find a peak.

**Why binary search applies:** Even though the array is not sorted, we exploit a local monotone property. Moving toward the greater neighbor is a valid strategy because the boundary conditions (`-infinity` at both ends) guarantee a peak exists in whichever direction we move.

```cpp
class Solution {
public:
    int findPeakElement(vector<int>& nums) {
        int left = 0, right = (int)nums.size() - 1;

        while (left < right) {
            int mid = left + (right - left) / 2;

            if (nums[mid] < nums[mid + 1]) {
                // Ascending slope — peak is to the right of mid
                left = mid + 1;
            } else {
                // Descending slope (or nums[mid] is a peak) — peak is at mid or to the left
                right = mid;
            }
        }

        // left == right == a peak element index
        return left;
    }
};
```

**Complexity:**
- Time: O(log n)
- Space: O(1)

**Why this works (proof sketch):**
- When `nums[mid] < nums[mid+1]`: Consider the subarray `[mid+1, right]`. Its left boundary `nums[mid+1] > nums[mid]` and its right boundary is `-infinity`. Since it starts high and ends at `-infinity`, it must contain a peak.
- When `nums[mid] >= nums[mid+1]`: Consider the subarray `[left, mid]`. Its right boundary `nums[mid] >= nums[mid+1]` and its left boundary is `-infinity`. It must contain a peak.

---

## Complexity Reference

| Problem | Time | Space | Technique |
|---------|------|-------|-----------|
| Binary Search (704) | O(log n) | O(1) | Template 1 — exact match |
| Search in Rotated Array (33) | O(log n) | O(1) | Template 1 — sorted half property |
| Find Minimum in Rotated (153) | O(log n) | O(1) | Template 2 — boundary search |
| Koko Eating Bananas (875) | O(n log M) | O(1) | Binary search on answer |
| Median of Two Sorted Arrays (4) | O(log(min(m,n))) | O(1) | Binary search on partition |
| Find Peak Element (162) | O(log n) | O(1) | Template 2 — local monotone property |

---

## Summary: When to Apply Binary Search

Binary search is applicable whenever:

1. **The search space is sorted** — classic array binary search (Problems 1, 2, 3).
2. **You can eliminate half the space with a comparison** — the key is that mid's relationship to the target tells you definitively which half to keep.
3. **The answer is a value in a range, and feasibility is monotone** — binary search on answer (Problem 4). This is the most generalizable and powerful form.
4. **There is a local or global monotone property** — even without full sorting, if you can always move toward a guaranteed solution, binary search applies (Problem 6).

The question to always ask: **"If I double (or halve) my candidate answer, can I determine in one check whether I've gone too far?"** If yes, binary search is likely applicable.
