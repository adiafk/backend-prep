# Trees — Patterns, Traversals, and Problems

## 1. Binary Tree Node Definition

```cpp
struct TreeNode {
    int val;
    TreeNode* left;
    TreeNode* right;
    TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
};
```

---

## 2. Binary Tree Traversals

### Conceptual Order

```
        1
       / \
      2   3
     / \
    4   5

Inorder  (L, Root, R): 4 2 5 1 3
Preorder (Root, L, R): 1 2 4 5 3
Postorder(L, R, Root): 4 5 2 3 1
```

---

### Inorder Traversal (Left → Root → Right)

**Recursive:**
```cpp
void inorder(TreeNode* root, vector<int>& result) {
    if (!root) return;
    inorder(root->left, result);
    result.push_back(root->val);
    inorder(root->right, result);
}
```

**Iterative (explicit stack):**
```cpp
vector<int> inorderIterative(TreeNode* root) {
    vector<int> result;
    stack<TreeNode*> stk;
    TreeNode* curr = root;

    while (curr || !stk.empty()) {
        // Go as far left as possible
        while (curr) {
            stk.push(curr);
            curr = curr->left;
        }
        // Process node
        curr = stk.top(); stk.pop();
        result.push_back(curr->val);
        // Move to right subtree
        curr = curr->right;
    }
    return result;
}
```

**Key insight:** Iterative inorder is a classic interview question. The loop structure "go left, pop, go right" mirrors the recursive call stack.

---

### Preorder Traversal (Root → Left → Right)

**Recursive:**
```cpp
void preorder(TreeNode* root, vector<int>& result) {
    if (!root) return;
    result.push_back(root->val);
    preorder(root->left, result);
    preorder(root->right, result);
}
```

**Iterative:**
```cpp
vector<int> preorderIterative(TreeNode* root) {
    vector<int> result;
    if (!root) return result;

    stack<TreeNode*> stk;
    stk.push(root);

    while (!stk.empty()) {
        TreeNode* node = stk.top(); stk.pop();
        result.push_back(node->val);
        // Push right first so left is processed first
        if (node->right) stk.push(node->right);
        if (node->left)  stk.push(node->left);
    }
    return result;
}
```

---

### Postorder Traversal (Left → Right → Root)

**Recursive:**
```cpp
void postorder(TreeNode* root, vector<int>& result) {
    if (!root) return;
    postorder(root->left, result);
    postorder(root->right, result);
    result.push_back(root->val);
}
```

**Iterative (two-stack trick):**
```cpp
vector<int> postorderIterative(TreeNode* root) {
    vector<int> result;
    if (!root) return result;

    stack<TreeNode*> stk;
    stk.push(root);

    while (!stk.empty()) {
        TreeNode* node = stk.top(); stk.pop();
        result.push_back(node->val);
        // Push left first so right is processed first (reverse of preorder)
        if (node->left)  stk.push(node->left);
        if (node->right) stk.push(node->right);
    }
    // Postorder = reverse of (Root, Right, Left)
    reverse(result.begin(), result.end());
    return result;
}
```

**Alternative iterative (single stack):**
```cpp
vector<int> postorderSingleStack(TreeNode* root) {
    vector<int> result;
    stack<TreeNode*> stk;
    TreeNode* curr = root;
    TreeNode* lastVisited = nullptr;

    while (curr || !stk.empty()) {
        while (curr) {
            stk.push(curr);
            curr = curr->left;
        }
        TreeNode* peekNode = stk.top();
        // If right child exists and not yet processed
        if (peekNode->right && lastVisited != peekNode->right) {
            curr = peekNode->right;
        } else {
            result.push_back(peekNode->val);
            lastVisited = stk.top(); stk.pop();
        }
    }
    return result;
}
```

---

## 3. BFS on Trees — Level Order Traversal

BFS uses a queue. Process nodes level by level.

```
        3
       / \
      9  20
         / \
        15   7

Level order: [[3], [9, 20], [15, 7]]
```

**Template:**
```cpp
vector<vector<int>> levelOrder(TreeNode* root) {
    vector<vector<int>> result;
    if (!root) return result;

    queue<TreeNode*> q;
    q.push(root);

    while (!q.empty()) {
        int levelSize = q.size();          // Snapshot size BEFORE processing
        vector<int> level;

        for (int i = 0; i < levelSize; i++) {
            TreeNode* node = q.front(); q.pop();
            level.push_back(node->val);

            if (node->left)  q.push(node->left);
            if (node->right) q.push(node->right);
        }
        result.push_back(level);
    }
    return result;
}
```

**Why snapshot the size?** At the start of each iteration, `q.size()` tells you exactly how many nodes are on the current level. Children added during that iteration belong to the next level.

---

## 4. Key Tree Properties

### Height (Max Depth)

Height = number of edges on the longest path from root to a leaf.
Depth = number of nodes on that path (height + 1, or interchangeable depending on convention).

```cpp
int height(TreeNode* root) {
    if (!root) return 0;
    return 1 + max(height(root->left), height(root->right));
}
```

**Pattern:** Post-order style — compute left and right first, combine at the root.

---

### Diameter

Diameter = longest path between any two nodes (does not have to pass through root).

```
        1
       / \
      2   3
     / \
    4   5

Diameter = 3 (path: 4 → 2 → 5, or 4 → 2 → 1 → 3)
```

```cpp
int diameterOfBinaryTree(TreeNode* root) {
    int diameter = 0;

    function<int(TreeNode*)> depth = [&](TreeNode* node) -> int {
        if (!node) return 0;
        int left  = depth(node->left);
        int right = depth(node->right);
        diameter = max(diameter, left + right);   // Update global max
        return 1 + max(left, right);              // Return height to parent
    };

    depth(root);
    return diameter;
}
```

**Key insight:** At each node, the diameter through that node = left_height + right_height. Track the max across all nodes.

---

### Balanced Tree

A tree is height-balanced if the heights of left and right subtrees differ by at most 1 at every node.

```cpp
// Returns -1 if unbalanced, otherwise returns height
int checkBalanced(TreeNode* root) {
    if (!root) return 0;

    int left  = checkBalanced(root->left);
    int right = checkBalanced(root->right);

    if (left == -1 || right == -1) return -1;       // Propagate failure
    if (abs(left - right) > 1)    return -1;        // Unbalanced here

    return 1 + max(left, right);
}

bool isBalanced(TreeNode* root) {
    return checkBalanced(root) != -1;
}
```

**Pattern:** Return a sentinel value (-1) to short-circuit the recursion when imbalance is detected. Avoids O(n log n) recomputation.

---

## 5. Solved Problems

---

### Problem 1 — Maximum Depth of Binary Tree

**LeetCode 104**

Given the root of a binary tree, return its maximum depth (number of nodes along the longest path from root to a leaf).

**Approach — DFS (recursive):**
```cpp
int maxDepth(TreeNode* root) {
    if (!root) return 0;
    return 1 + max(maxDepth(root->left), maxDepth(root->right));
}
```
Time: O(n) — visit every node once.
Space: O(h) — recursion stack, where h is height.

**Approach — BFS (iterative):**
```cpp
int maxDepth(TreeNode* root) {
    if (!root) return 0;

    queue<TreeNode*> q;
    q.push(root);
    int depth = 0;

    while (!q.empty()) {
        depth++;
        int levelSize = q.size();
        for (int i = 0; i < levelSize; i++) {
            TreeNode* node = q.front(); q.pop();
            if (node->left)  q.push(node->left);
            if (node->right) q.push(node->right);
        }
    }
    return depth;
}
```

**When to use BFS variant:** When the tree is very deep and you want to avoid stack overflow. Also gives level-by-level processing as a bonus.

---

### Problem 2 — Invert Binary Tree

**LeetCode 226**

Invert (mirror) a binary tree.

```
Input:      Output:
    4           4
   / \         / \
  2   7       7   2
 / \ / \     / \ / \
1  3 6  9   9  6 3  1
```

**Recursive:**
```cpp
TreeNode* invertTree(TreeNode* root) {
    if (!root) return nullptr;

    swap(root->left, root->right);
    invertTree(root->left);
    invertTree(root->right);

    return root;
}
```

**Iterative (BFS):**
```cpp
TreeNode* invertTree(TreeNode* root) {
    if (!root) return nullptr;

    queue<TreeNode*> q;
    q.push(root);

    while (!q.empty()) {
        TreeNode* node = q.front(); q.pop();
        swap(node->left, node->right);

        if (node->left)  q.push(node->left);
        if (node->right) q.push(node->right);
    }
    return root;
}
```

Time: O(n). Space: O(n) for queue (BFS) or O(h) for recursion stack.

**Pattern:** Swap children at each node. The swap happens at the current node; recursion handles the rest.

---

### Problem 3 — Diameter of Binary Tree

**LeetCode 543**

Return the length of the diameter (longest path between any two nodes, measured in number of edges).

```cpp
int diameterOfBinaryTree(TreeNode* root) {
    int diameter = 0;

    function<int(TreeNode*)> dfs = [&](TreeNode* node) -> int {
        if (!node) return 0;

        int leftDepth  = dfs(node->left);
        int rightDepth = dfs(node->right);

        // Path through this node has leftDepth + rightDepth edges
        diameter = max(diameter, leftDepth + rightDepth);

        // Return depth of this subtree to parent
        return 1 + max(leftDepth, rightDepth);
    };

    dfs(root);
    return diameter;
}
```

Time: O(n). Space: O(h).

**Common mistake:** Returning early without computing both subtrees, missing the global max update.

**Trace through example:**
```
Node 4: left=0, right=0 → diameter candidate=0, return 1
Node 5: left=0, right=0 → diameter candidate=0, return 1
Node 2: left=1, right=1 → diameter candidate=2, return 2
Node 3: left=0, right=0 → diameter candidate=0, return 1
Node 1: left=2, right=1 → diameter candidate=3, return 3
Final diameter: 3
```

---

### Problem 4 — Lowest Common Ancestor

**LeetCode 236**

Given a binary tree and two nodes p and q, find their lowest common ancestor (LCA).

**LCA definition:** The deepest node that has both p and q as descendants (a node can be a descendant of itself).

```cpp
TreeNode* lowestCommonAncestor(TreeNode* root, TreeNode* p, TreeNode* q) {
    // Base cases: empty tree, or found p or q
    if (!root || root == p || root == q) return root;

    TreeNode* left  = lowestCommonAncestor(root->left,  p, q);
    TreeNode* right = lowestCommonAncestor(root->right, p, q);

    // p and q found in different subtrees — current node is LCA
    if (left && right) return root;

    // Both in same subtree — return whichever subtree found them
    return left ? left : right;
}
```

Time: O(n). Space: O(h).

**Reasoning:**
- If root is p or q, root is the LCA (since we're guaranteed both exist).
- If left returns non-null and right returns non-null, p and q are in different subtrees — root is the LCA.
- If only one side returns non-null, both nodes are in that subtree — propagate the result upward.

**Trace:**
```
Find LCA(4, 5) in the tree above:
- At node 1: recurse left (finds 2) and right (finds null)
- At node 2: recurse left (finds 4) and right (finds 5)
- At node 2: left=4, right=5 → both non-null → return node 2
- LCA = 2
```

---

### Problem 5 — Binary Tree Level Order Traversal

**LeetCode 102**

Return the level-order traversal as a list of lists.

```cpp
vector<vector<int>> levelOrder(TreeNode* root) {
    vector<vector<int>> result;
    if (!root) return result;

    queue<TreeNode*> q;
    q.push(root);

    while (!q.empty()) {
        int levelSize = q.size();
        vector<int> level;

        for (int i = 0; i < levelSize; i++) {
            TreeNode* node = q.front(); q.pop();
            level.push_back(node->val);

            if (node->left)  q.push(node->left);
            if (node->right) q.push(node->right);
        }
        result.push_back(level);
    }
    return result;
}
```

Time: O(n). Space: O(n) — queue holds at most one full level.

**Variants built from this pattern:**
- Level order bottom-up (LeetCode 107): reverse result at the end.
- Zigzag level order (LeetCode 103): alternate between push_back and push_front using a deque.
- Right side view (LeetCode 199): take the last element of each level.
- Average of levels (LeetCode 637): sum each level, divide by size.

---

## 6. Pattern Summary

| Pattern | Use When | Traversal |
|---|---|---|
| DFS (recursive) | Height, diameter, path sums, LCA | Post-order usually |
| DFS (iterative) | Avoid stack overflow, explicit stack needed | Pre/In/Post |
| BFS | Level-by-level, shortest path to leaf, right side view | Level order |
| Return sentinel | Detect invalid state (unbalanced, no path) | Post-order |
| Global variable + DFS | Diameter, max path sum — answer is not the return value | Post-order |

---

## 7. Complexity Reference

| Problem | Time | Space |
|---|---|---|
| Any traversal | O(n) | O(h) DFS, O(n) BFS |
| Max Depth | O(n) | O(h) |
| Invert Tree | O(n) | O(h) |
| Diameter | O(n) | O(h) |
| LCA | O(n) | O(h) |
| Level Order | O(n) | O(n) |

h = height of tree. For balanced tree h = O(log n). For skewed tree h = O(n).
