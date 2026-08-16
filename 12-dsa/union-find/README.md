# Union-Find (Disjoint Set Union)

## 1. Disjoint Set Union — Path Compression + Union by Rank

Union-Find is a data structure that tracks a collection of elements partitioned into disjoint (non-overlapping) sets. It supports two core operations efficiently:

- `find(x)` — which set does element x belong to? (returns the root/representative)
- `union(x, y)` — merge the sets containing x and y

### Without optimizations: O(n) per operation (degenerate tree)

### With two optimizations: O(alpha(n)) amortized — practically O(1)

**Path compression** (applied in `find`): After finding the root, make every node on the path point directly to the root. This flattens the tree.

```
Before find(5):   root <- 1 <- 3 <- 5
After find(5):    root <- 1
                  root <- 3
                  root <- 5
```

**Union by rank** (applied in `union`): Always attach the shorter tree under the taller tree. This keeps trees shallow. Rank is an upper bound on tree height.

Together, these give amortized time O(alpha(n)) where alpha is the inverse Ackermann function — effectively constant for any input size that fits in the universe.

---

## 2. Template Code

### C++

```cpp
#include <vector>
#include <numeric>
using namespace std;

class UnionFind {
public:
    vector<int> parent, rank_;
    int components;

    UnionFind(int n) : parent(n), rank_(n, 0), components(n) {
        iota(parent.begin(), parent.end(), 0);  // parent[i] = i
    }

    // Find with path compression
    int find(int x) {
        if (parent[x] != x)
            parent[x] = find(parent[x]);  // recursive path compression
        return parent[x];
    }

    // Path compression — iterative version (avoids stack overflow on large n)
    int findIter(int x) {
        int root = x;
        while (parent[root] != root) root = parent[root];
        while (parent[x] != root) {
            int next = parent[x];
            parent[x] = root;
            x = next;
        }
        return root;
    }

    // Union by rank — returns false if already in same set (cycle detected)
    bool unite(int x, int y) {
        int px = find(x), py = find(y);
        if (px == py) return false;  // already connected — adding edge = cycle

        if (rank_[px] < rank_[py]) swap(px, py);
        parent[py] = px;            // attach smaller tree under larger
        if (rank_[px] == rank_[py]) rank_[px]++;
        components--;
        return true;
    }

    bool connected(int x, int y) {
        return find(x) == find(y);
    }
};
```

### TypeScript

```typescript
class UnionFind {
    private parent: number[];
    private rank: number[];
    public components: number;

    constructor(n: number) {
        this.parent = Array.from({ length: n }, (_, i) => i);
        this.rank = new Array(n).fill(0);
        this.components = n;
    }

    find(x: number): number {
        if (this.parent[x] !== x) {
            this.parent[x] = this.find(this.parent[x]);  // path compression
        }
        return this.parent[x];
    }

    unite(x: number, y: number): boolean {
        const px = this.find(x);
        const py = this.find(y);
        if (px === py) return false;  // already same set

        if (this.rank[px] < this.rank[py]) {
            this.parent[px] = py;
        } else if (this.rank[px] > this.rank[py]) {
            this.parent[py] = px;
        } else {
            this.parent[py] = px;
            this.rank[px]++;
        }
        this.components--;
        return true;
    }

    connected(x: number, y: number): boolean {
        return this.find(x) === this.find(y);
    }
}
```

### Usage pattern

```cpp
UnionFind uf(n);
for (auto& edge : edges) {
    uf.unite(edge[0], edge[1]);
}
int numComponents = uf.components;
```

---

## 3. When to Use Union-Find

| Use case | Why UF fits |
|---|---|
| Count connected components | Each component is a disjoint set; answer is `uf.components` |
| Cycle detection (undirected graph) | If `find(u) == find(v)` before `unite(u,v)`, edge (u,v) creates a cycle |
| Minimum Spanning Tree (Kruskal's) | Add edges in sorted order; skip edge if endpoints already connected |
| Accounts merge / grouping | Union elements that share a property; find root to identify group |
| Percolation / grid connectivity | Flatten 2D grid to 1D index: `id = r * cols + c` |
| Dynamic connectivity | Edges added online; query connectivity at any time |

**Do NOT use Union-Find when:**
- You need to **delete/disconnect** edges (standard UF only supports union, not split)
- You need **directed** reachability (use DFS/BFS instead)
- You need the actual path between two nodes

**Union-Find vs. BFS/DFS for connected components:**
- BFS/DFS: O(V+E), single-pass, gives component membership directly
- Union-Find: O(E * alpha(V)), supports online edge additions without re-running
- Prefer UF when edges arrive incrementally or when you process edges repeatedly

---

## 4. Three Solved Problems

---

### Problem 1 — Number of Connected Components in an Undirected Graph (LeetCode 323)

**Problem:** Given n nodes (0 to n-1) and a list of undirected edges, return the number of connected components.

**Approach:** Initialize all nodes as separate components. For each edge, union the two endpoints. Each successful union (endpoints were in different sets) reduces the component count by 1.

```cpp
#include <vector>
#include <numeric>
using namespace std;

class Solution {
    vector<int> parent, rank_;

    int find(int x) {
        if (parent[x] != x) parent[x] = find(parent[x]);
        return parent[x];
    }

    bool unite(int x, int y) {
        int px = find(x), py = find(y);
        if (px == py) return false;
        if (rank_[px] < rank_[py]) swap(px, py);
        parent[py] = px;
        if (rank_[px] == rank_[py]) rank_[px]++;
        return true;
    }

public:
    int countComponents(int n, vector<vector<int>>& edges) {
        parent.resize(n);
        rank_.assign(n, 0);
        iota(parent.begin(), parent.end(), 0);

        int components = n;
        for (auto& e : edges) {
            if (unite(e[0], e[1])) components--;
        }
        return components;
    }
};
```

**Complexity:** Time O(E * alpha(V)). Space O(V).

---

### Problem 2 — Redundant Connection (LeetCode 684)

**Problem:** Given a tree with n nodes that has one extra edge added (making it have exactly one cycle), find and return the redundant edge. If multiple answers exist, return the last one in the input.

**Approach:** Process edges one by one. Try to union each edge's endpoints. If they are already connected (`find(u) == find(v)`), this edge is redundant — it creates a cycle. Return it.

```cpp
#include <vector>
#include <numeric>
using namespace std;

class Solution {
    vector<int> parent, rank_;

    int find(int x) {
        if (parent[x] != x) parent[x] = find(parent[x]);
        return parent[x];
    }

    bool unite(int x, int y) {
        int px = find(x), py = find(y);
        if (px == py) return false;          // CYCLE DETECTED
        if (rank_[px] < rank_[py]) swap(px, py);
        parent[py] = px;
        if (rank_[px] == rank_[py]) rank_[px]++;
        return true;
    }

public:
    vector<int> findRedundantConnection(vector<vector<int>>& edges) {
        int n = edges.size();
        parent.resize(n + 1);
        rank_.assign(n + 1, 0);
        iota(parent.begin(), parent.end(), 0);

        for (auto& e : edges) {
            if (!unite(e[0], e[1])) return e;  // unite failed = cycle = redundant edge
        }
        return {};
    }
};
```

**Why this works:** A tree with n nodes has exactly n-1 edges. Adding one more creates exactly one cycle. The first edge whose endpoints are already in the same component is the one that closes the cycle — it is redundant.

**Complexity:** Time O(E * alpha(V)). Space O(V).

---

### Problem 3 — Accounts Merge (LeetCode 721)

**Problem:** Given a list of accounts where `accounts[i][0]` is a name and `accounts[i][1:]` are email addresses, merge accounts that share at least one email. Return merged accounts sorted.

**Approach:**
1. Map every email to an index (its "node" in UF).
2. Within each account, union all emails together (they belong to the same person).
3. Group emails by their root representative.
4. For each group, look up the account owner's name via any email in that group.

```cpp
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <numeric>
using namespace std;

class Solution {
    vector<int> parent, rank_;

    int find(int x) {
        if (parent[x] != x) parent[x] = find(parent[x]);
        return parent[x];
    }

    void unite(int x, int y) {
        int px = find(x), py = find(y);
        if (px == py) return;
        if (rank_[px] < rank_[py]) swap(px, py);
        parent[py] = px;
        if (rank_[px] == rank_[py]) rank_[px]++;
    }

public:
    vector<vector<string>> accountsMerge(vector<vector<string>>& accounts) {
        unordered_map<string, int> emailIndex;  // email -> node id
        unordered_map<string, string> emailOwner; // email -> account name
        int idx = 0;

        for (auto& account : accounts) {
            string name = account[0];
            for (int i = 1; i < (int)account.size(); i++) {
                string email = account[i];
                if (!emailIndex.count(email)) {
                    emailIndex[email] = idx++;
                }
                emailOwner[email] = name;
            }
        }

        // Initialize UF with total number of unique emails
        parent.resize(idx);
        rank_.assign(idx, 0);
        iota(parent.begin(), parent.end(), 0);

        // Union all emails within each account
        for (auto& account : accounts) {
            int first = emailIndex[account[1]];
            for (int i = 2; i < (int)account.size(); i++) {
                unite(first, emailIndex[account[i]]);
            }
        }

        // Group emails by their root representative
        map<int, vector<string>> groups;  // root -> list of emails
        for (auto& [email, id] : emailIndex) {
            groups[find(id)].push_back(email);
        }

        // Build result
        vector<vector<string>> result;
        for (auto& [root, emails] : groups) {
            sort(emails.begin(), emails.end());
            string name = emailOwner[emails[0]];
            vector<string> merged = {name};
            merged.insert(merged.end(), emails.begin(), emails.end());
            result.push_back(merged);
        }
        return result;
    }
};
```

**Complexity:** Time O(N * alpha(N) + N log N) where N = total emails. The log N comes from sorting. Space O(N).

**Key insight:** The union-find does not need to know account names — it just groups email nodes. Names are recovered from the `emailOwner` map after grouping.

---

## 5. Quick Reference

```
Operation              | Time complexity
-----------------------|----------------
find(x)                | O(alpha(n)) amortized
unite(x, y)            | O(alpha(n)) amortized
connected(x, y)        | O(alpha(n)) amortized
Build UF on n nodes    | O(n)
Process E edges        | O(E * alpha(V)) total
```

alpha(n) < 5 for all practical n (inverse Ackermann — effectively constant).
