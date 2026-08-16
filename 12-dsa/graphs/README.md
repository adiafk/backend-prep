# Graphs — Representations, Traversals, and Problems

## 1. Graph Representations

### Adjacency List

```cpp
// Unweighted undirected graph
vector<vector<int>> adj(n);   // adj[u] = list of neighbors of u

// Add edge u — v
adj[u].push_back(v);
adj[v].push_back(u);   // Omit for directed graph

// Example: 4 nodes, edges 0-1, 0-2, 1-3
// adj[0] = {1, 2}
// adj[1] = {0, 3}
// adj[2] = {0}
// adj[3] = {1}
```

**Weighted graph:**
```cpp
vector<vector<pair<int,int>>> adj(n);   // adj[u] = {(v, weight), ...}
adj[u].push_back({v, w});
```

**Using unordered_map (arbitrary node labels):**
```cpp
unordered_map<int, vector<int>> adj;
adj[u].push_back(v);
```

---

### Adjacency Matrix

```cpp
vector<vector<int>> matrix(n, vector<int>(n, 0));

// Add edge u — v (unweighted)
matrix[u][v] = 1;
matrix[v][u] = 1;   // Omit for directed

// Weighted
matrix[u][v] = weight;

// Check if edge exists: O(1)
if (matrix[u][v]) { /* edge exists */ }
```

---

### Comparison

| Property | Adjacency List | Adjacency Matrix |
|---|---|---|
| Space | O(V + E) | O(V^2) |
| Check edge (u,v) | O(degree(u)) | O(1) |
| Iterate neighbors of u | O(degree(u)) | O(V) |
| Best for | Sparse graphs | Dense graphs, O(1) edge lookup |
| Typical interviews | Almost always this | Floyd-Warshall, certain DP |

**Rule of thumb:** Use adjacency list by default. Use matrix only when the graph is dense (E close to V^2) or you need O(1) edge existence checks.

---

## 2. BFS vs DFS — When Each Is Better

### BFS (Breadth-First Search)

Explores level by level using a queue.

```cpp
void bfs(int start, vector<vector<int>>& adj, int n) {
    vector<bool> visited(n, false);
    queue<int> q;

    visited[start] = true;
    q.push(start);

    while (!q.empty()) {
        int node = q.front(); q.pop();
        // Process node

        for (int neighbor : adj[node]) {
            if (!visited[neighbor]) {
                visited[neighbor] = true;
                q.push(neighbor);
            }
        }
    }
}
```

**Use BFS when:**
- Finding the shortest path in an unweighted graph
- Level-by-level exploration is required
- Finding the minimum number of steps/moves
- The solution is likely near the source

---

### DFS (Depth-First Search)

Explores as deep as possible before backtracking.

```cpp
// Recursive
void dfs(int node, vector<vector<int>>& adj, vector<bool>& visited) {
    visited[node] = true;
    // Process node

    for (int neighbor : adj[node]) {
        if (!visited[neighbor]) {
            dfs(neighbor, adj, visited);
        }
    }
}

// Iterative
void dfsIterative(int start, vector<vector<int>>& adj, int n) {
    vector<bool> visited(n, false);
    stack<int> stk;

    stk.push(start);

    while (!stk.empty()) {
        int node = stk.top(); stk.pop();
        if (visited[node]) continue;
        visited[node] = true;
        // Process node

        for (int neighbor : adj[node]) {
            if (!visited[neighbor]) {
                stk.push(neighbor);
            }
        }
    }
}
```

**Use DFS when:**
- Detecting cycles
- Topological sorting
- Finding all paths
- Connected components
- Backtracking problems
- The graph is deep and solutions are far from source

---

### Quick Decision Reference

| Goal | Algorithm |
|---|---|
| Shortest path (unweighted) | BFS |
| Minimum steps/moves | BFS |
| Cycle detection | DFS |
| Topological sort | DFS |
| Connected components | Either |
| All paths from source | DFS |
| Bipartite check | BFS |

---

## 3. The Visited Set Pattern

**Why it matters:** Without tracking visited nodes, you will loop infinitely in graphs with cycles.

### Array (when nodes are 0..n-1)
```cpp
vector<bool> visited(n, false);
// Mark before or immediately when enqueuing/pushing, NOT after popping
visited[node] = true;
```

**Critical rule for BFS:** Mark visited when you ENQUEUE, not when you dequeue. Marking on dequeue allows the same node to be enqueued multiple times.

```cpp
// CORRECT — mark on enqueue
if (!visited[neighbor]) {
    visited[neighbor] = true;   // Mark here
    q.push(neighbor);
}

// WRONG — mark on dequeue (can process same node multiple times)
int node = q.front(); q.pop();
visited[node] = true;   // Too late
```

### Set (for arbitrary node types)
```cpp
unordered_set<int> visited;
visited.insert(start);
```

### Grid traversal (2D)
```cpp
vector<vector<bool>> visited(rows, vector<bool>(cols, false));
// Or modify the grid in-place by marking cells as '#' or 0
```

---

## 4. Solved Problems

---

### Problem 1 — Number of Islands

**LeetCode 200**

Given an m x n grid of '1's (land) and '0's (water), count the number of islands.

```
Input:
11110
11010
11000
00000
Output: 1

Input:
11000
11000
00100
00011
Output: 3
```

**Approach — DFS (sink island):**
```cpp
class Solution {
    void dfs(vector<vector<char>>& grid, int r, int c) {
        int rows = grid.size(), cols = grid[0].size();
        // Boundary check and water check
        if (r < 0 || r >= rows || c < 0 || c >= cols || grid[r][c] != '1')
            return;

        grid[r][c] = '0';   // Sink the land cell (mark visited in-place)

        dfs(grid, r + 1, c);
        dfs(grid, r - 1, c);
        dfs(grid, r, c + 1);
        dfs(grid, r, c - 1);
    }

public:
    int numIslands(vector<vector<char>>& grid) {
        int count = 0;
        for (int r = 0; r < grid.size(); r++) {
            for (int c = 0; c < grid[0].size(); c++) {
                if (grid[r][c] == '1') {
                    count++;
                    dfs(grid, r, c);   // Sink entire island
                }
            }
        }
        return count;
    }
};
```

**Approach — BFS:**
```cpp
int numIslands(vector<vector<char>>& grid) {
    int rows = grid.size(), cols = grid[0].size();
    int count = 0;
    vector<pair<int,int>> dirs = {{0,1},{0,-1},{1,0},{-1,0}};

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (grid[r][c] == '1') {
                count++;
                queue<pair<int,int>> q;
                q.push({r, c});
                grid[r][c] = '0';   // Mark visited immediately

                while (!q.empty()) {
                    auto [row, col] = q.front(); q.pop();
                    for (auto [dr, dc] : dirs) {
                        int nr = row + dr, nc = col + dc;
                        if (nr >= 0 && nr < rows && nc >= 0 && nc < cols
                            && grid[nr][nc] == '1') {
                            grid[nr][nc] = '0';   // Mark on enqueue
                            q.push({nr, nc});
                        }
                    }
                }
            }
        }
    }
    return count;
}
```

Time: O(m * n). Space: O(min(m,n)) for BFS queue in worst case, O(m*n) for DFS stack in worst case.

**When to use BFS vs DFS here:** Both work. BFS avoids deep recursion for large grids (stack overflow risk). DFS is simpler to write.

---

### Problem 2 — Clone Graph

**LeetCode 133**

Given a node in a connected undirected graph, return a deep copy (clone) of the graph.

```cpp
class Node {
public:
    int val;
    vector<Node*> neighbors;
    Node(int v) : val(v) {}
};

class Solution {
    unordered_map<Node*, Node*> cloned;   // original -> clone mapping

    Node* dfs(Node* node) {
        if (!node) return nullptr;

        // Already cloned — return the clone to avoid infinite loop
        if (cloned.count(node)) return cloned[node];

        Node* clone = new Node(node->val);
        cloned[node] = clone;   // Register BEFORE recursing (handles cycles)

        for (Node* neighbor : node->neighbors) {
            clone->neighbors.push_back(dfs(neighbor));
        }
        return clone;
    }

public:
    Node* cloneGraph(Node* node) {
        return dfs(node);
    }
};
```

Time: O(V + E). Space: O(V) for the hash map.

**Critical detail:** Register the clone in the map BEFORE recursing into neighbors. If you register after, a cycle (A → B → A) will cause infinite recursion because A hasn't been recorded when B tries to recurse back to A.

**BFS variant:**
```cpp
Node* cloneGraph(Node* node) {
    if (!node) return nullptr;

    unordered_map<Node*, Node*> cloned;
    queue<Node*> q;

    cloned[node] = new Node(node->val);
    q.push(node);

    while (!q.empty()) {
        Node* curr = q.front(); q.pop();
        for (Node* neighbor : curr->neighbors) {
            if (!cloned.count(neighbor)) {
                cloned[neighbor] = new Node(neighbor->val);
                q.push(neighbor);
            }
            cloned[curr]->neighbors.push_back(cloned[neighbor]);
        }
    }
    return cloned[node];
}
```

---

### Problem 3 — Course Schedule (Cycle Detection / Topological Sort)

**LeetCode 207**

There are n courses (0 to n-1). Given prerequisites[i] = [a, b] meaning "b must be taken before a", determine if it is possible to finish all courses.

This is equivalent to: does the directed graph have a cycle?

**Approach — DFS with 3-color marking:**

```
State 0 = unvisited
State 1 = visiting (in current DFS path — cycle if we see this again)
State 2 = visited (fully processed — safe)
```

```cpp
class Solution {
    bool hasCycle(int node, vector<vector<int>>& adj, vector<int>& state) {
        if (state[node] == 1) return true;    // Back edge — cycle detected
        if (state[node] == 2) return false;   // Already fully processed

        state[node] = 1;   // Mark as visiting

        for (int neighbor : adj[node]) {
            if (hasCycle(neighbor, adj, state)) return true;
        }

        state[node] = 2;   // Mark as fully visited
        return false;
    }

public:
    bool canFinish(int numCourses, vector<vector<int>>& prerequisites) {
        vector<vector<int>> adj(numCourses);
        for (auto& pre : prerequisites) {
            adj[pre[1]].push_back(pre[0]);   // pre[1] must come before pre[0]
        }

        vector<int> state(numCourses, 0);

        for (int i = 0; i < numCourses; i++) {
            if (hasCycle(i, adj, state)) return false;
        }
        return true;
    }
};
```

**Approach — BFS (Kahn's algorithm / topological sort):**

```cpp
bool canFinish(int numCourses, vector<vector<int>>& prerequisites) {
    vector<vector<int>> adj(numCourses);
    vector<int> inDegree(numCourses, 0);

    for (auto& pre : prerequisites) {
        adj[pre[1]].push_back(pre[0]);
        inDegree[pre[0]]++;
    }

    // Start with all courses that have no prerequisites
    queue<int> q;
    for (int i = 0; i < numCourses; i++) {
        if (inDegree[i] == 0) q.push(i);
    }

    int processed = 0;
    while (!q.empty()) {
        int course = q.front(); q.pop();
        processed++;

        for (int next : adj[course]) {
            inDegree[next]--;
            if (inDegree[next] == 0) q.push(next);
        }
    }

    // If we processed all courses, no cycle exists
    return processed == numCourses;
}
```

Time: O(V + E). Space: O(V + E).

**DFS vs BFS for cycle detection:**
- DFS (3-color): more natural for cycle detection, easier to understand intuitively
- Kahn's BFS: also gives topological order, easy to check if all nodes processed

---

### Problem 4 — Pacific Atlantic Water Flow

**LeetCode 417**

Given an m x n matrix of heights, water flows to neighbors with equal or lower height. Find all cells from which water can flow to both the Pacific ocean (top/left border) and the Atlantic ocean (bottom/right border).

```
Pacific ~   ~   ~   ~   ~
       ~ 1   2   2   3  (5) *
       ~ 3   2   3  (4) (4) *
       ~ 2   4  (5)  3   1  *
       ~ (6)(7)  1   4   5  *
       ~ (5)  1   1   2   4  *
          *   *   *   *   * Atlantic

() = cells that can reach both oceans
```

**Key insight (reverse BFS):** Instead of simulating water flowing down from every cell (expensive), reverse the flow — start BFS/DFS from the ocean borders and find which cells can reach each ocean. The answer is the intersection.

```cpp
class Solution {
    int rows, cols;
    vector<pair<int,int>> dirs = {{0,1},{0,-1},{1,0},{-1,0}};

    void bfs(vector<vector<int>>& heights,
             queue<pair<int,int>>& q,
             vector<vector<bool>>& visited) {
        while (!q.empty()) {
            auto [r, c] = q.front(); q.pop();
            for (auto [dr, dc] : dirs) {
                int nr = r + dr, nc = c + dc;
                if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
                if (visited[nr][nc]) continue;
                // Reverse flow: neighbor must be >= current (water flows down TO ocean)
                if (heights[nr][nc] < heights[r][c]) continue;
                visited[nr][nc] = true;
                q.push({nr, nc});
            }
        }
    }

public:
    vector<vector<int>> pacificAtlantic(vector<vector<int>>& heights) {
        rows = heights.size();
        cols = heights[0].size();

        vector<vector<bool>> pacific (rows, vector<bool>(cols, false));
        vector<vector<bool>> atlantic(rows, vector<bool>(cols, false));

        queue<pair<int,int>> pq, aq;

        // Pacific: top row and left column
        for (int c = 0; c < cols; c++) { pacific[0][c]        = true; pq.push({0, c}); }
        for (int r = 0; r < rows; r++) { pacific[r][0]        = true; pq.push({r, 0}); }

        // Atlantic: bottom row and right column
        for (int c = 0; c < cols; c++) { atlantic[rows-1][c]  = true; aq.push({rows-1, c}); }
        for (int r = 0; r < rows; r++) { atlantic[r][cols-1]  = true; aq.push({r, cols-1}); }

        bfs(heights, pq, pacific);
        bfs(heights, aq, atlantic);

        vector<vector<int>> result;
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                if (pacific[r][c] && atlantic[r][c]) {
                    result.push_back({r, c});
                }
            }
        }
        return result;
    }
};
```

Time: O(m * n). Space: O(m * n) for visited arrays and queue.

**Why reverse flow works:** Cell X can flow to the Pacific iff the Pacific can "flow up" to X. We BFS from ocean borders going uphill (>= height), which is exactly the reverse of downhill water flow.

---

## 5. Pattern Summary

| Pattern | Core Idea | Problems |
|---|---|---|
| BFS flood fill | Mark on enqueue, process level by level | Number of Islands |
| DFS with clone map | Register before recursing to handle cycles | Clone Graph |
| 3-color DFS | white/gray/black to detect back edges | Course Schedule |
| Kahn's algorithm | Remove nodes with in-degree 0 iteratively | Course Schedule |
| Reverse BFS from border | Avoid redundant computation by inverting the flow direction | Pacific Atlantic |

---

## 6. Complexity Reference

| Problem | Time | Space |
|---|---|---|
| Number of Islands | O(m * n) | O(m * n) |
| Clone Graph | O(V + E) | O(V) |
| Course Schedule | O(V + E) | O(V + E) |
| Pacific Atlantic | O(m * n) | O(m * n) |
