# Heap and Priority Queue — Patterns and Problems

## 1. Min Heap vs Max Heap

A **heap** is a complete binary tree stored as an array that satisfies the **heap property**:

- **Min Heap:** Every parent node is less than or equal to its children. The minimum element is always at the root.
- **Max Heap:** Every parent node is greater than or equal to its children. The maximum element is always at the root.

### Array Representation

For a node at index `i` (0-indexed):
- Left child: `2i + 1`
- Right child: `2i + 2`
- Parent: `(i - 1) / 2`

```
Min Heap example (array: [1, 3, 2, 7, 6, 5, 4]):

           1            ← root = minimum
         /   \
        3     2
       / \   / \
      7   6 5   4
```

### Core Operations

| Operation  | Time Complexity | Description                              |
|------------|-----------------|------------------------------------------|
| push       | O(log n)        | Insert and sift up to restore heap order |
| pop        | O(log n)        | Remove root, replace with last, sift down|
| top/peek   | O(1)            | Read root without removing               |
| heapify    | O(n)            | Build heap from unsorted array           |
| size       | O(1)            | Number of elements                       |

### When to Use Which

| Use Case                                    | Heap Type       |
|---------------------------------------------|-----------------|
| Get the minimum element quickly             | Min Heap        |
| Get the maximum element quickly             | Max Heap        |
| K largest elements in a stream              | Min Heap of size K |
| K smallest elements in a stream             | Max Heap of size K |
| Merge K sorted lists                        | Min Heap        |
| Median of a stream                          | Min Heap + Max Heap |
| Dijkstra's shortest path                    | Min Heap        |
| Prim's MST                                  | Min Heap        |

**The counterintuitive rule:** To maintain the K largest elements, use a **min heap** of size K. The min heap's root is the smallest of the K largest — it acts as the eviction gate. If a new element exceeds the root, it belongs in the top-K set, so replace the root.

---

## 2. Priority Queue in C++ and TypeScript

### C++ — `std::priority_queue`

```cpp
#include <queue>
using namespace std;

// Max Heap (default) — largest element at top
priority_queue<int> maxHeap;
maxHeap.push(3);
maxHeap.push(1);
maxHeap.push(4);
cout << maxHeap.top(); // 4

// Min Heap — use greater<int> comparator
priority_queue<int, vector<int>, greater<int>> minHeap;
minHeap.push(3);
minHeap.push(1);
minHeap.push(4);
cout << minHeap.top(); // 1

// Custom comparator (e.g., min heap by second element of pair)
auto cmp = [](pair<int,int>& a, pair<int,int>& b) {
    return a.second > b.second; // min heap on second value
};
priority_queue<pair<int,int>, vector<pair<int,int>>, decltype(cmp)> pq(cmp);

// Common operations
pq.push({1, 5});
pq.top();         // peek (does NOT remove)
pq.pop();         // remove top (does NOT return value)
pq.empty();
pq.size();
```

**Critical C++ gotcha:** `pq.pop()` does not return the element. Always do `top()` first, then `pop()`:
```cpp
int val = pq.top();
pq.pop();
```

### TypeScript — No Built-in Heap

JavaScript/TypeScript does not have a built-in priority queue. In interviews, implement a minimal one or use a sorted approach.

**Minimal Binary Min Heap:**

```ts
class MinHeap {
    private heap: number[] = [];

    push(val: number): void {
        this.heap.push(val);
        this._siftUp(this.heap.length - 1);
    }

    pop(): number | undefined {
        if (this.heap.length === 0) return undefined;
        const top = this.heap[0];
        const last = this.heap.pop()!;
        if (this.heap.length > 0) {
            this.heap[0] = last;
            this._siftDown(0);
        }
        return top;
    }

    peek(): number | undefined {
        return this.heap[0];
    }

    size(): number {
        return this.heap.length;
    }

    private _siftUp(i: number): void {
        while (i > 0) {
            const parent = Math.floor((i - 1) / 2);
            if (this.heap[parent] <= this.heap[i]) break;
            [this.heap[parent], this.heap[i]] = [this.heap[i], this.heap[parent]];
            i = parent;
        }
    }

    private _siftDown(i: number): void {
        const n = this.heap.length;
        while (true) {
            let smallest = i;
            const left = 2 * i + 1;
            const right = 2 * i + 2;
            if (left < n && this.heap[left] < this.heap[smallest]) smallest = left;
            if (right < n && this.heap[right] < this.heap[smallest]) smallest = right;
            if (smallest === i) break;
            [this.heap[smallest], this.heap[i]] = [this.heap[i], this.heap[smallest]];
            i = smallest;
        }
    }
}
```

**For Max Heap in TS:** negate values when pushing and negate when reading, or flip the comparison in sift operations.

```ts
// Quick max heap trick using negation
const maxHeap = new MinHeap();
maxHeap.push(-5);  // store negated
maxHeap.push(-3);
maxHeap.push(-8);
const max = -maxHeap.pop()!;  // negate back → 8
```

---

## 3. K-th Element Problems — The Pattern

### Core Insight

The "K-th" or "top K" family of problems all share the same heap strategy:

```
To find K largest:
  → Maintain a MIN heap of exactly K elements
  → Invariant: heap contains the K largest seen so far
  → Root = K-th largest (smallest of the top K)

To find K smallest:
  → Maintain a MAX heap of exactly K elements
  → Invariant: heap contains the K smallest seen so far
  → Root = K-th smallest (largest of the bottom K)
```

### Template — K Largest in a Stream

```cpp
// Online: process elements one at a time
priority_queue<int, vector<int>, greater<int>> minHeap; // min heap, size K

for (int x : stream) {
    minHeap.push(x);
    if ((int)minHeap.size() > k) {
        minHeap.pop(); // evict the smallest of top-K if we exceed K
    }
}
// minHeap.top() = K-th largest
```

### Template — Top K Frequent (offline, full array known)

```cpp
// Count frequencies, then use a min heap of size K on (freq, element) pairs
unordered_map<int, int> freq;
for (int x : nums) freq[x]++;

priority_queue<pair<int,int>, vector<pair<int,int>>, greater<>> minHeap;
for (auto& [val, cnt] : freq) {
    minHeap.push({cnt, val});
    if ((int)minHeap.size() > k) minHeap.pop();
}
```

### Alternative — Quick Select for Exact K-th

For the exact K-th largest in an unsorted array (not stream), QuickSelect gives O(n) average. Heap gives O(n log k) always. Choose:
- **Heap** when K is small, data is a stream, or you need top-K not just K-th.
- **QuickSelect** when you need the exact K-th element with best average performance.

---

## 4. Solved Problems

---

### Problem 1 — Kth Largest Element in an Array (LC 215)

**Problem:** Find the k-th largest element in an unsorted array. Note: k-th largest in sorted order, not k-th distinct.

**Approach:** Min heap of size k. Process every element; evict the minimum if size exceeds k. The root is always the k-th largest.

**C++ Solution:**

```cpp
int findKthLargest(vector<int>& nums, int k) {
    // Min heap of size k
    priority_queue<int, vector<int>, greater<int>> minHeap;

    for (int x : nums) {
        minHeap.push(x);
        if ((int)minHeap.size() > k) {
            minHeap.pop(); // remove the smallest element, maintaining top-K
        }
    }
    return minHeap.top(); // k-th largest = smallest of top-K
}
```

**Complexity:** Time O(n log k), Space O(k)

**Walkthrough on `nums = [3,2,1,5,6,4]`, `k = 2`:**
```
x=3: heap=[3]
x=2: heap=[2,3]
x=1: heap=[1,2,3] → size=3>2, pop min=1 → heap=[2,3]
x=5: heap=[2,3,5] → size=3>2, pop min=2 → heap=[3,5]
x=6: heap=[3,5,6] → size=3>2, pop min=3 → heap=[5,6]
x=4: heap=[4,5,6] → size=3>2, pop min=4 → heap=[5,6]
top = 5 (2nd largest) ✓
```

---

### Problem 2 — Top K Frequent Elements (LC 347)

**Problem:** Given an integer array `nums` and integer `k`, return the `k` most frequent elements. Answer is guaranteed unique; order does not matter.

**Approach:**
1. Count frequencies with a hash map — O(n).
2. Use a min heap of size k sorted by frequency. When size > k, pop the least frequent.
3. Result is all elements remaining in the heap.

**C++ Solution:**

```cpp
vector<int> topKFrequent(vector<int>& nums, int k) {
    // Step 1: count frequencies
    unordered_map<int, int> freq;
    for (int x : nums) freq[x]++;

    // Step 2: min heap keyed by frequency
    // pair: {frequency, value}
    priority_queue<pair<int,int>, vector<pair<int,int>>, greater<>> minHeap;

    for (auto& [val, cnt] : freq) {
        minHeap.push({cnt, val});
        if ((int)minHeap.size() > k) {
            minHeap.pop(); // remove least frequent
        }
    }

    // Step 3: extract results
    vector<int> result;
    while (!minHeap.empty()) {
        result.push_back(minHeap.top().second);
        minHeap.pop();
    }
    return result;
}
```

**Complexity:** Time O(n log k), Space O(n) for the frequency map

**Walkthrough on `nums = [1,1,1,2,2,3]`, `k = 2`:**
```
freq: {1→3, 2→2, 3→1}
Process {3,1}: heap=[{3,1}]
Process {2,2}: heap=[{2,2},{3,1}]
Process {1,3}: heap=[{1,3},{3,1},{2,2}] → size=3>2, pop {1,3} → heap=[{2,2},{3,1}]
Result: [2, 1]
```

**Alternative — Bucket Sort: O(n)**
Since frequencies are bounded by `n`, use a bucket array of size `n+1` where `bucket[f]` contains all elements with frequency `f`. Then scan from right to collect top K.

```cpp
vector<int> topKFrequent(vector<int>& nums, int k) {
    unordered_map<int, int> freq;
    for (int x : nums) freq[x]++;

    int n = nums.size();
    vector<vector<int>> bucket(n + 1);
    for (auto& [val, cnt] : freq) bucket[cnt].push_back(val);

    vector<int> result;
    for (int f = n; f >= 1 && (int)result.size() < k; f--) {
        for (int val : bucket[f]) {
            result.push_back(val);
            if ((int)result.size() == k) break;
        }
    }
    return result;
}
```

---

### Problem 3 — Find Median from Data Stream (LC 295)

**Problem:** Design a data structure that supports adding integers and finding the median at any time.

**Approach:** Two heaps:
- `maxHeap` (left half) — max heap containing the smaller half of numbers.
- `minHeap` (right half) — min heap containing the larger half of numbers.

**Invariants to maintain:**
1. `maxHeap.size()` equals `minHeap.size()` OR `maxHeap.size()` = `minHeap.size() + 1` (left half can have one extra).
2. Every element in `maxHeap` ≤ every element in `minHeap`.

**Finding median:**
- If both same size: `(maxHeap.top() + minHeap.top()) / 2.0`
- If left is larger: `maxHeap.top()`

**C++ Solution:**

```cpp
class MedianFinder {
    priority_queue<int> maxHeap;                            // left half, max heap
    priority_queue<int, vector<int>, greater<int>> minHeap; // right half, min heap

public:
    void addNum(int num) {
        // Step 1: push to max heap (left)
        maxHeap.push(num);

        // Step 2: balance — ensure max of left ≤ min of right
        if (!minHeap.empty() && maxHeap.top() > minHeap.top()) {
            minHeap.push(maxHeap.top());
            maxHeap.pop();
        }

        // Step 3: rebalance sizes — left can have at most one more than right
        if ((int)maxHeap.size() > (int)minHeap.size() + 1) {
            minHeap.push(maxHeap.top());
            maxHeap.pop();
        } else if ((int)minHeap.size() > (int)maxHeap.size()) {
            maxHeap.push(minHeap.top());
            minHeap.pop();
        }
    }

    double findMedian() {
        if (maxHeap.size() == minHeap.size()) {
            return (maxHeap.top() + minHeap.top()) / 2.0;
        }
        return maxHeap.top(); // left always has one more when odd total
    }
};
```

**Complexity:** addNum O(log n), findMedian O(1), Space O(n)

**Walkthrough on `addNum(1), addNum(2), addNum(3)`:**
```
addNum(1):
  maxHeap=[1], minHeap=[]
  No balance needed (minHeap empty)
  Sizes: 1 vs 0, left=right+1 ✓
  findMedian() → maxHeap.top()=1

addNum(2):
  push to maxHeap: maxHeap=[2,1]
  max(maxHeap)=2 > min(minHeap) is vacuous (minHeap empty) — skip balance
  Sizes: 2 vs 0, left > right+1 → move 2 to minHeap
  maxHeap=[1], minHeap=[2]
  findMedian() → (1+2)/2.0 = 1.5

addNum(3):
  push to maxHeap: maxHeap=[3,1]
  max(maxHeap)=3 > min(minHeap)=2 → move 3 to minHeap
  maxHeap=[1], minHeap=[2,3]
  Sizes: 1 vs 2, right > left → move 2 to maxHeap
  maxHeap=[2,1], minHeap=[3]
  findMedian() → maxHeap.top()=2
```

**Why this works:** The two-heap structure always keeps the boundary between "smaller half" and "larger half" at the tops of each heap, accessible in O(1).

---

### Problem 4 — Merge K Sorted Lists (LC 23)

**Problem:** Given an array of `k` linked-list heads, each sorted in ascending order, merge them into one sorted list.

**Approach:** Min heap keyed by node value. Start by pushing all k list heads. Repeatedly extract the minimum, add to result, and push its next node (if any).

**C++ Solution:**

```cpp
struct ListNode {
    int val;
    ListNode* next;
    ListNode(int x) : val(x), next(nullptr) {}
};

ListNode* mergeKLists(vector<ListNode*>& lists) {
    // Min heap: {value, node pointer}
    // Custom comparator: min heap by node value
    auto cmp = [](ListNode* a, ListNode* b) {
        return a->val > b->val; // greater → min heap
    };
    priority_queue<ListNode*, vector<ListNode*>, decltype(cmp)> minHeap(cmp);

    // Initialize: push first node of each list
    for (ListNode* head : lists) {
        if (head) minHeap.push(head);
    }

    ListNode dummy(0);
    ListNode* tail = &dummy;

    while (!minHeap.empty()) {
        ListNode* node = minHeap.top();
        minHeap.pop();

        tail->next = node;
        tail = tail->next;

        if (node->next) minHeap.push(node->next);
    }

    return dummy.next;
}
```

**Complexity:** Time O(N log k) where N = total nodes, k = number of lists. Each node is pushed and popped once from the heap of size k. Space O(k) for the heap.

**Walkthrough on `[[1→4→5], [1→3→4], [2→6]]`:**
```
Initial heap: {1(L1), 1(L2), 2(L3)}
              (min heap, both 1s present — order between them is arbitrary)

Step 1: pop 1(L1) → result: 1 → push 4(L1)   heap:{1(L2), 2(L3), 4(L1)}
Step 2: pop 1(L2) → result: 1→1 → push 3(L2)  heap:{2(L3), 3(L2), 4(L1)}
Step 3: pop 2(L3) → result: 1→1→2 → push 6(L3) heap:{3(L2), 4(L1), 6(L3)}
Step 4: pop 3(L2) → result: 1→1→2→3 → push 4(L2) heap:{4(L1), 4(L2), 6(L3)}
Step 5: pop 4(L1) → result: 1→1→2→3→4 → push 5(L1) heap:{4(L2), 5(L1), 6(L3)}
Step 6: pop 4(L2) → result: ...→4 → no next     heap:{5(L1), 6(L3)}
Step 7: pop 5(L1) → result: ...→5 → no next     heap:{6(L3)}
Step 8: pop 6(L3) → result: ...→6 → no next     heap:{}

Final: 1→1→2→3→4→4→5→6
```

---

## 5. Summary

### Complexity Reference

| Problem                    | Approach             | Time         | Space  |
|----------------------------|----------------------|--------------|--------|
| Kth Largest Element        | Min heap size K      | O(n log k)   | O(k)   |
| Top K Frequent Elements    | Min heap size K      | O(n log k)   | O(n)   |
| Top K Frequent (optimal)   | Bucket sort          | O(n)         | O(n)   |
| Median from Data Stream    | Two heaps            | O(log n) add | O(n)   |
| Merge K Sorted Lists       | Min heap size K      | O(N log k)   | O(k)   |

### Key Patterns to Memorize

1. **K largest → min heap of size K** (root = K-th largest, evict minimum)
2. **K smallest → max heap of size K** (root = K-th smallest, evict maximum)
3. **Median → max heap (left) + min heap (right)**, sizes differ by at most 1
4. **Merge sorted sources → min heap seeded with one node per source**

### Common Mistakes

1. **C++ `pop()` returns void** — always `top()` then `pop()` separately.
2. **Forgetting to push the next node** in merge K lists — when you consume a node, push its `.next`.
3. **Off-by-one in median** — left heap (maxHeap) holds the median when total count is odd; don't mix up which heap gets the extra element.
4. **K largest uses min heap, not max heap** — instinct says "max heap for largest" but you want to evict the smallest of your K candidates, which requires a min heap at the boundary.
