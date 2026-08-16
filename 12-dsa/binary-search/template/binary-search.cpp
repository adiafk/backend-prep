// =============================================================================
// BINARY SEARCH CANONICAL TEMPLATES
// =============================================================================
// Binary search works by repeatedly halving the search space.
// The key invariant: the answer always lies within [left, right].
// Every iteration, we MUST shrink the search space — no infinite loops.

#include <bits/stdc++.h>
using namespace std;


// =============================================================================
// TEMPLATE 1: left <= right  (Exact match — answer is a specific index)
// =============================================================================
// Use when: you are looking for an exact value and want to return its index.
// Loop exits when left > right, meaning the element was not found.
// mid is always checked against the target, and we narrow around it.

int binarySearch_exact(vector<int>& nums, int target) {
    int left = 0;               // inclusive left boundary
    int right = nums.size() - 1; // inclusive right boundary

    while (left <= right) {     // '<=' because left == right is still a valid 1-element range
        // Avoid overflow: (left + right) / 2 can overflow for large values.
        int mid = left + (right - left) / 2;

        if (nums[mid] == target) {
            return mid;         // Found — return immediately
        } else if (nums[mid] < target) {
            left = mid + 1;     // mid is too small; discard mid and everything left of it
        } else {
            right = mid - 1;    // mid is too large; discard mid and everything right of it
        }
    }

    return -1; // not found — left has crossed right
}


// =============================================================================
// TEMPLATE 2: left < right  (Boundary / bound search — answer is a position)
// =============================================================================
// Use when: finding a lower/upper bound, or "binary search on answer".
// Loop exits when left == right, and that position IS the answer.
// mid is NEVER the answer — we only narrow around it.
// CRITICAL: when moving right = mid, mid must NOT equal right (use floor division).
//           when moving left = mid + 1, this is always safe.

int binarySearch_lowerBound(vector<int>& nums, int target) {
    int left = 0;
    int right = nums.size(); // NOTE: right can be nums.size() (one past end) to handle "insert at end"

    while (left < right) {   // '<' because left == right means we've converged on the answer
        int mid = left + (right - left) / 2; // floor division — guarantees mid < right, preventing infinite loop

        if (nums[mid] < target) {
            left = mid + 1;  // mid is strictly too small; answer is at mid+1 or later
        } else {
            right = mid;     // mid could be the answer; do NOT discard it (right = mid, not mid - 1)
        }
    }

    // Post-loop: left == right == the first position where nums[pos] >= target
    return left;
}

int binarySearch_upperBound(vector<int>& nums, int target) {
    int left = 0;
    int right = nums.size();

    while (left < right) {
        int mid = left + (right - left) / 2;

        if (nums[mid] <= target) {
            left = mid + 1;  // mid is <= target; upper bound is strictly after mid
        } else {
            right = mid;     // mid > target; upper bound could be here
        }
    }

    // Post-loop: left == right == first position where nums[pos] > target
    return left;
}


// =============================================================================
// TEMPLATE 3: Binary Search on Answer
// =============================================================================
// Use when: the answer is a value in a range [lo, hi], and you can write a
// feasibility function isFeasible(mid) that returns true/false.
// Pattern: find the MINIMUM value x such that isFeasible(x) == true.
//
// Shape of feasibility: [false, false, ..., true, true, true]
//                                           ^--- we want this boundary
//
// The key insight: you are not searching an array — you are searching the
// ANSWER SPACE. Any time you can define a monotone feasibility check,
// binary search finds the optimal answer in O(log(range)) * O(check).

bool isFeasible(/* problem state */, int candidate) {
    // Return true if 'candidate' satisfies the problem constraint.
    // This function must be MONOTONE: if candidate works, candidate+1 also works
    // (or the reverse for maximization problems).
    (void)candidate;
    return true; // placeholder
}

int binarySearchOnAnswer(int lo, int hi /* inclusive answer range */) {
    // Find minimum x in [lo, hi] such that isFeasible(x) is true.
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;

        if (isFeasible(mid)) {
            hi = mid;       // mid might be the answer; keep it in range
        } else {
            lo = mid + 1;   // mid definitely fails; move past it
        }
    }

    // lo == hi == the minimum feasible value
    return lo;
}


// =============================================================================
// DECIDING WHICH TEMPLATE TO USE — QUICK REFERENCE
// =============================================================================
//
//  Scenario                              Template    right init       Loop
//  ------------------------------------  ----------  ---------------  ----------
//  Find exact index of target            Template 1  nums.size()-1    left<=right
//  First index where nums[i] >= target   Template 2  nums.size()      left<right
//  First index where nums[i] > target    Template 2  nums.size()      left<right
//  Minimum feasible answer in [lo,hi]    Template 3  hi (inclusive)   left<right
//  Maximum feasible answer in [lo,hi]    Template 3  hi (inclusive)   left<right (flip logic)
//
// COMMON MISTAKES:
//  1. Using mid = (left + right) / 2  — overflows when left+right > INT_MAX
//  2. Using right = mid - 1 in Template 2 — may skip the answer
//  3. Forgetting that Template 2's loop exits at left == right (that IS the answer)
//  4. In Template 3, setting lo/hi incorrectly — always include the full valid range
//  5. Infinite loop: if right = mid and mid can equal left, use floor division (already done above)
// =============================================================================

int main() {
    // Demo: exact search
    vector<int> arr = {1, 3, 5, 7, 9, 11, 13};
    cout << "Index of 7: " << binarySearch_exact(arr, 7) << "\n";   // 3
    cout << "Index of 6: " << binarySearch_exact(arr, 6) << "\n";   // -1

    // Demo: lower bound (first position >= target)
    cout << "Lower bound of 6: " << binarySearch_lowerBound(arr, 6) << "\n"; // 3 (value 7)
    cout << "Lower bound of 7: " << binarySearch_lowerBound(arr, 7) << "\n"; // 3 (value 7)

    // Demo: upper bound (first position > target)
    cout << "Upper bound of 7: " << binarySearch_upperBound(arr, 7) << "\n"; // 4 (value 9)

    return 0;
}
