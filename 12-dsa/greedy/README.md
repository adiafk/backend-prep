# Greedy Algorithms

## When Greedy Works

A greedy algorithm makes the locally optimal choice at each step and hopes it leads to a global optimum. This works when the problem has the **greedy choice property**: a locally optimal choice leads to a globally optimal solution.

**Verify before applying**: if greedy doesn't work, you need DP. When in doubt, prove by exchange argument (assume an optimal solution differs from greedy, show you can swap to match greedy without losing optimality).

---

## Interval Scheduling

### Activity Selection / Non-Overlapping Intervals
Greedy: sort by **end time**, always pick the interval that ends earliest. This leaves the most room for future intervals.

```cpp
// Count minimum intervals to remove to make the rest non-overlapping
int eraseOverlapIntervals(vector<vector<int>>& intervals) {
    sort(intervals.begin(), intervals.end(), [](const auto& a, const auto& b) {
        return a[1] < b[1];  // sort by end time
    });
    
    int removals = 0;
    int lastEnd = INT_MIN;
    
    for (auto& interval : intervals) {
        if (interval[0] >= lastEnd) {
            lastEnd = interval[1];  // compatible, take it
        } else {
            removals++;             // overlap, remove current
        }
    }
    return removals;
}
// Time: O(n log n) | Space: O(1)
```

### Merge Intervals
```cpp
vector<vector<int>> merge(vector<vector<int>>& intervals) {
    sort(intervals.begin(), intervals.end());
    vector<vector<int>> result;
    
    for (auto& interval : intervals) {
        if (result.empty() || result.back()[1] < interval[0]) {
            result.push_back(interval);  // no overlap
        } else {
            result.back()[1] = max(result.back()[1], interval[1]);  // merge
        }
    }
    return result;
}
```

---

## Jump Game

```cpp
// Can you reach the last index?
bool canJump(vector<int>& nums) {
    int maxReach = 0;
    for (int i = 0; i < nums.size(); i++) {
        if (i > maxReach) return false;   // stuck — can't reach i
        maxReach = max(maxReach, i + nums[i]);
    }
    return true;
}
// Time: O(n) | Space: O(1)

// Minimum jumps to reach end
int jump(vector<int>& nums) {
    int jumps = 0, curEnd = 0, farthest = 0;
    for (int i = 0; i < (int)nums.size() - 1; i++) {
        farthest = max(farthest, i + nums[i]);
        if (i == curEnd) {
            jumps++;
            curEnd = farthest;
        }
    }
    return jumps;
}
```

---

## Scheduling / Profit Maximization

### Task Scheduler
Given tasks with frequencies and cooldown n, find minimum time to complete all tasks.

```cpp
int leastInterval(vector<char>& tasks, int n) {
    vector<int> freq(26, 0);
    for (char t : tasks) freq[t - 'A']++;
    
    int maxFreq = *max_element(freq.begin(), freq.end());
    int maxCount = count(freq.begin(), freq.end(), maxFreq);
    
    // Arrange as: (maxFreq-1) chunks of (n+1), plus final chunk of maxCount tasks
    int result = (maxFreq - 1) * (n + 1) + maxCount;
    return max((int)tasks.size(), result);  // can't do fewer than total tasks
}
```

---

## Huffman / Coding Problems

### Assign Cookies (easy greedy)
```cpp
int findContentChildren(vector<int>& g, vector<int>& s) {
    sort(g.begin(), g.end());  // greed factors
    sort(s.begin(), s.end());  // cookie sizes
    
    int child = 0, cookie = 0;
    while (child < g.size() && cookie < s.size()) {
        if (s[cookie] >= g[child]) child++;  // satisfied
        cookie++;
    }
    return child;
}
```

### Gas Station (circular route)
```cpp
int canCompleteCircuit(vector<int>& gas, vector<int>& cost) {
    int totalGas = 0, currentGas = 0, startStation = 0;
    
    for (int i = 0; i < gas.size(); i++) {
        int net = gas[i] - cost[i];
        totalGas += net;
        currentGas += net;
        
        if (currentGas < 0) {
            startStation = i + 1;  // can't start here or before
            currentGas = 0;
        }
    }
    
    return totalGas >= 0 ? startStation : -1;
}
```

---

## Greedy vs DP

| Problem shape | Use |
|-------------|-----|
| Make one global optimal choice at each step, no future decisions affected | Greedy |
| Each choice depends on the result of future choices (overlapping subproblems) | DP |
| Need to try all combinations | Backtracking |

Classic greedy problems: interval scheduling, Dijkstra, Prim's MST, Huffman coding, job sequencing, fractional knapsack.

Classic DP problems: 0/1 knapsack, longest common subsequence, edit distance, coin change (counting), matrix chain.
