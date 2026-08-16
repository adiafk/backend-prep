# Monotonic Stack — Pattern and Problems

## 1. What is a Monotonic Stack?

A **monotonic stack** is a stack that maintains a strict ordering invariant on its elements at all times — either always increasing (bottom to top) or always decreasing.

### Monotonic Increasing Stack
Elements from bottom to top are in strictly increasing order. Before pushing a new element, **pop all elements that are greater than or equal to the new element**.

```
State after processing [3, 1, 4, 2]:
Pushes and pops:
  push 3 → [3]
  1 < 3  → pop 3, push 1 → [1]
  4 > 1  → push 4        → [1, 4]
  2 < 4  → pop 4, push 2 → [1, 2]
Stack (bottom→top): [1, 2]  ← strictly increasing
```

### Monotonic Decreasing Stack
Elements from bottom to top are in strictly decreasing order. Before pushing, pop all elements that are less than or equal to the new element.

```
Stack (bottom→top): [5, 3, 1]  ← strictly decreasing
```

### The Invariant in One Line

> An element is popped when a new element violates the monotonic property. At the moment of popping, the new element is the answer for the popped element.

This is the key insight that makes monotonic stacks useful: **the pop event encodes a relationship between two elements**.

---

## 2. When to Use a Monotonic Stack

### Signature Problems

| Problem Pattern                        | Stack Type            | Pop Trigger                         |
|----------------------------------------|-----------------------|-------------------------------------|
| Next Greater Element                   | Monotonic Decreasing  | Current element > stack top         |
| Next Smaller Element                   | Monotonic Increasing  | Current element < stack top         |
| Previous Greater Element               | Monotonic Decreasing  | Scan right-to-left, same pop rule   |
| Previous Smaller Element               | Monotonic Increasing  | Scan right-to-left, same pop rule   |
| Largest Rectangle in Histogram         | Monotonic Increasing  | Current bar shorter than top        |
| Trapping Rain Water                    | Monotonic Decreasing  | Current bar taller than top         |
| Remove K Digits (lexicographically min)| Monotonic Increasing  | Current digit < top and k > 0       |

### How to Identify in an Interview

Ask yourself:
- "For each element, do I need the **next/previous** element that is **larger/smaller**?"
- "Does solving for one element require comparing it against a range of other elements?"
- "Is O(n^2) brute force obvious, suggesting an O(n) trick exists?"

If yes to any — monotonic stack is a strong candidate.

---

## 3. The Template

```cpp
// Template: Monotonic Increasing Stack (finds next smaller element)
// Swap the condition for next greater / decreasing variant

vector<int> nextSmaller(vector<int>& arr) {
    int n = arr.size();
    vector<int> result(n, -1);  // default: no smaller element found
    vector<int> stk;             // store indices, not values

    for (int i = 0; i < n; i++) {
        // While stack is non-empty AND current element violates invariant:
        while (!stk.empty() && arr[i] < arr[stk.back()]) {
            int idx = stk.back();
            stk.pop_back();
            result[idx] = arr[i]; // current element is the "next smaller" for idx
        }
        stk.push_back(i);
    }
    // Elements remaining in stack have no smaller element to their right
    return result;
}
```

### Template Variants

```cpp
// Next Greater Element (monotonic decreasing stack)
while (!stk.empty() && arr[i] > arr[stk.back()])

// Previous Greater Element (same stack, result set at push time or scan RTL)
// Scan left to right; when about to push i, stk.back() is previous greater:
result[i] = stk.empty() ? -1 : arr[stk.back()];
while (!stk.empty() && arr[i] >= arr[stk.back()]) stk.pop_back();
stk.push_back(i);

// Non-strict: use >= or <= instead of > or < in condition
```

### Storing Indices vs Values

Almost always store **indices** in the stack, not values. This lets you compute:
- Distance: `i - stk.back()` (days waited, width of rectangle)
- Look up the value: `arr[stk.back()]`

---

## 4. Solved Problems

---

### Problem 1 — Next Greater Element I (LC 496)

**Problem:** Given `nums1` (subset of `nums2`), for each element in `nums1` find the next greater element in `nums2`. Return -1 if none exists.

**Approach:**
- Run a monotonic decreasing stack over `nums2`.
- When `nums2[i]` pops `nums2[j]`, `nums2[i]` is the next greater element for `nums2[j]`.
- Store results in a hash map, then answer queries from `nums1`.

```cpp
vector<int> nextGreaterElement(vector<int>& nums1, vector<int>& nums2) {
    unordered_map<int, int> nge; // value → next greater value
    stack<int> stk;              // monotonic decreasing (values)

    for (int x : nums2) {
        // x is greater than top → top has found its next greater element
        while (!stk.empty() && x > stk.top()) {
            nge[stk.top()] = x;
            stk.pop();
        }
        stk.push(x);
    }
    // Remaining elements in stack have no greater element
    while (!stk.empty()) {
        nge[stk.top()] = -1;
        stk.pop();
    }

    vector<int> result;
    for (int x : nums1) result.push_back(nge[x]);
    return result;
}
```

**Complexity:** Time O(m + n), Space O(n)

**Walkthrough on `nums2 = [1, 3, 4, 2]`:**
```
x=1: stk empty → push 1     stk:[1]
x=3: 3>1 → nge[1]=3, pop; push 3  stk:[3]
x=4: 4>3 → nge[3]=4, pop; push 4  stk:[4]
x=2: 2<4 → push 2           stk:[4,2]
cleanup: nge[4]=-1, nge[2]=-1
nge: {1→3, 3→4, 4→-1, 2→-1}
```

---

### Problem 2 — Daily Temperatures (LC 739)

**Problem:** For each day, find the number of days until a warmer temperature. Return 0 if no warmer day exists.

**Approach:** Monotonic decreasing stack of indices. When `T[i] > T[stk.top()]`, resolve the top's waiting period.

```cpp
vector<int> dailyTemperatures(vector<int>& T) {
    int n = T.size();
    vector<int> result(n, 0);
    vector<int> stk; // indices, monotonic decreasing by temperature

    for (int i = 0; i < n; i++) {
        while (!stk.empty() && T[i] > T[stk.back()]) {
            int j = stk.back();
            stk.pop_back();
            result[j] = i - j;
        }
        stk.push_back(i);
    }
    return result;
}
```

**Complexity:** Time O(n), Space O(n)

**Walkthrough on `[73, 74, 75, 71, 69, 72, 76, 73]`:**
```
i=0 T=73: push 0              stk:[0]
i=1 T=74: 74>73 → res[0]=1, pop; push 1   stk:[1]
i=2 T=75: 75>74 → res[1]=1, pop; push 2   stk:[2]
i=3 T=71: push 3              stk:[2,3]
i=4 T=69: push 4              stk:[2,3,4]
i=5 T=72: 72>69→res[4]=1; 72>71→res[3]=2; 72<75 → push 5   stk:[2,5]
i=6 T=76: 76>72→res[5]=1; 76>75→res[2]=4; push 6            stk:[6]
i=7 T=73: push 7              stk:[6,7]
→ result: [1,1,4,2,1,1,0,0]
```

Each index is pushed once and popped at most once → O(n) total operations.

---

### Problem 3 — Largest Rectangle in Histogram (LC 84)

**Problem:** Given heights of bars in a histogram, find the largest rectangular area.

**Key Observation:** For each bar `i`, the largest rectangle with height `heights[i]` extends left until a shorter bar and right until a shorter bar. We need "previous smaller" and "next smaller" for each bar.

**Approach:** Monotonic increasing stack of indices. When bar `i` is shorter than `stk.top()`, bar at top gets resolved: its right boundary is `i`, its left boundary is the new top.

```cpp
int largestRectangleArea(vector<int>& heights) {
    int n = heights.size();
    vector<int> stk;   // monotonic increasing by height
    int maxArea = 0;

    // Append sentinel 0 to flush the stack at the end
    heights.push_back(0);

    for (int i = 0; i <= n; i++) {
        while (!stk.empty() && heights[i] < heights[stk.back()]) {
            int h = heights[stk.back()];
            stk.pop_back();

            // Width: from stk.back()+1 to i-1
            // If stack is empty, the bar extends all the way to index 0
            int width = stk.empty() ? i : i - stk.back() - 1;
            maxArea = max(maxArea, h * width);
        }
        stk.push_back(i);
    }
    return maxArea;
}
```

**Complexity:** Time O(n), Space O(n)

**Walkthrough on `[2, 1, 5, 6, 2, 3]` (appended 0):**
```
i=0 h=2: push 0          stk:[0]
i=1 h=1: 1<2 → pop 0, h=2, width=1 (stk empty → width=i=1), area=2; push 1  stk:[1]
i=2 h=5: push 2          stk:[1,2]
i=3 h=6: push 3          stk:[1,2,3]
i=4 h=2: 2<6 → pop 3, h=6, width=4-2-1=1, area=6
         2<5 → pop 2, h=5, width=4-1-1=2, area=10
         2>=1, push 4    stk:[1,4]
i=5 h=3: push 5          stk:[1,4,5]
i=6 h=0: 0<3 → pop 5, h=3, width=6-4-1=1, area=3
         0<2 → pop 4, h=2, width=6-1-1=4, area=8
         0<1 → pop 1, h=1, width=6 (stk empty), area=6
maxArea = 10
```

**Width formula:** When popping index `j` with trigger `i`:
- Right boundary is `i` (exclusive)
- Left boundary is `stk.back() + 1` after pop (or `0` if stack is empty)
- Width = `i - stk.back() - 1` (or just `i` if stack is empty)

---

### Problem 4 — Trapping Rain Water (LC 42) — Stack Approach

**Problem:** Given an elevation map `height[]`, compute how much water it can trap after raining.

**Approach (stack-based):** Maintain a monotonic decreasing stack. When `height[i] > height[stk.top()]`, a "valley" is found. The water trapped is bounded by the current bar and the bar beneath the top.

```cpp
int trap(vector<int>& height) {
    int n = height.size();
    vector<int> stk;  // monotonic decreasing, stores indices
    int water = 0;

    for (int i = 0; i < n; i++) {
        while (!stk.empty() && height[i] > height[stk.back()]) {
            int bottom = stk.back();
            stk.pop_back();

            if (stk.empty()) break; // no left wall

            int left = stk.back();
            int width = i - left - 1;
            int boundedHeight = min(height[left], height[i]) - height[bottom];
            water += width * boundedHeight;
        }
        stk.push_back(i);
    }
    return water;
}
```

**Complexity:** Time O(n), Space O(n)

**Walkthrough on `[0, 1, 0, 2, 1, 0, 1, 3, 2, 1, 2, 1]`:**

The stack approach processes water **layer by layer** (horizontal slices):

```
i=0 h=0: push 0           stk:[0]
i=1 h=1: 1>0 → pop 0 (bottom=0), left stk empty → break; push 1  stk:[1]
i=2 h=0: push 2           stk:[1,2]
i=3 h=2: 2>0 → pop 2 (bottom=h=0), left=1 (h=1)
              width=3-1-1=1, boundedH=min(1,2)-0=1, water+=1
         2>1 → pop 1 (bottom=h=1), stk empty → break; push 3  stk:[3]
i=4 h=1: push 4           stk:[3,4]
i=5 h=0: push 5           stk:[3,4,5]
i=6 h=1: 1>0 → pop 5 (bottom=0), left=4 (h=1)
              width=6-4-1=1, boundedH=min(1,1)-0=1, water+=1
         1>=1, push 6     stk:[3,4,6]
i=7 h=3: 3>1 → pop 6 (bottom=1), left=4 (h=1)
              width=7-4-1=2, boundedH=min(1,3)-1=0, water+=0
         3>1 → pop 4 (bottom=1), left=3 (h=2)
              width=7-3-1=3, boundedH=min(2,3)-1=1, water+=3
         3>=2, push 7     stk:[3,7]
... continues
Total water = 6
```

**Alternative (two-pointer approach)** is often cleaner for this specific problem, but the stack approach generalizes and connects directly to the histogram pattern.

---

### Problem 5 — Remove K Digits (LC 402)

**Problem:** Given a string `num` representing a non-negative integer and an integer `k`, remove `k` digits to make the result the smallest possible number.

**Approach:** Monotonic increasing stack. To get the smallest number, whenever the current digit is smaller than the top, we should remove the top (it's better to have a smaller digit earlier). Each removal uses one of our `k` removals.

```cpp
string removeKdigits(string num, int k) {
    string stk; // acts as a monotonic increasing stack (using string for easy output)

    for (char c : num) {
        // While we still have removals left and current digit < top digit:
        // removing top gives a smaller number
        while (k > 0 && !stk.empty() && c < stk.back()) {
            stk.pop_back();
            k--;
        }
        stk.push_back(c);
    }

    // If k > 0, we haven't removed enough — remove from the end (largest digits)
    // e.g., "1234" with k=2 → remove '3','4' → "12"
    stk.resize(stk.size() - k);

    // Remove leading zeros
    int start = 0;
    while (start < (int)stk.size() - 1 && stk[start] == '0') start++;

    return stk.empty() ? "0" : stk.substr(start);
}
```

**Complexity:** Time O(n), Space O(n)

**Walkthrough on `num = "1432219"`, `k = 3`:**
```
c='1': stk empty → push '1'         stk:"1"
c='4': 4>1, push '4'                stk:"14"
c='3': 3<4 → pop '4' (k=2); 3>1 → push '3'  stk:"13"
c='2': 2<3 → pop '3' (k=1); 2>1 → push '2'  stk:"12"
c='2': 2>=2, push '2'               stk:"122"
c='1': 1<2 → pop '2' (k=0); k=0 stop → push '1'  stk:"121"
c='9': k=0, just push → stk:"1219"
k=0, no trailing removal needed
No leading zeros
Result: "1219"
```

**Walkthrough on `num = "10200"`, `k = 1`:**
```
c='1': push → stk:"1"
c='0': 0<1 → pop '1' (k=0); push '0' → stk:"0"
c='2': push → stk:"02"
c='0': push → stk:"020"
c='0': push → stk:"0200"
k=0
Leading zero removal: start=1 → "200"
Result: "200"
```

---

## 5. Pattern Summary

### Decision Tree

```
Need next/previous greater/smaller for each element?
    └─ Monotonic stack — O(n)

Histogram / rectangle area?
    └─ Monotonic increasing stack, compute width on pop

Rain water / valley detection?
    └─ Monotonic decreasing stack, compute trapped water on pop

Lexicographically smallest after k removals?
    └─ Monotonic increasing stack, pop greedily when condition met
```

### Complexity

All monotonic stack solutions share the same complexity profile:
- **Time: O(n)** — each element is pushed exactly once and popped at most once → 2n operations total.
- **Space: O(n)** — stack holds at most n elements.

### Common Pitfalls

1. **Forgetting to handle elements left in the stack** — they are elements with no answer (default -1 or 0).
2. **Width calculation in histogram** — after popping, the new `stk.back()` is the left boundary (not `j-1`), because all elements between `stk.back()` and `j` were already popped (they were taller and later resolved).
3. **Storing indices vs values** — store indices when you need distance or want to look up neighboring elements; store values only when you need just the value relationship.
4. **Strict vs non-strict inequality** — for problems with duplicate values, decide whether equal elements should cause a pop or not. Usually non-strict (`>=` or `<=`) is safer to avoid counting duplicates.
