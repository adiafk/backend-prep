# Stack — Fundamentals and Core Patterns

## 1. Stack Fundamentals

A stack is a Last-In First-Out (LIFO) linear data structure. Elements are inserted and removed from the same end, called the **top**.

### Core Operations — All O(1)

| Operation | Description                        | C++ (`std::stack`) | TS (`Array`) |
|-----------|------------------------------------|--------------------|--------------|
| push      | Insert element at top              | `s.push(x)`        | `s.push(x)`  |
| pop       | Remove top element                 | `s.pop()`          | `s.pop()`    |
| top/peek  | Read top without removing          | `s.top()`          | `s[s.length-1]` |
| empty     | Check if stack has no elements     | `s.empty()`        | `s.length === 0` |
| size      | Number of elements                 | `s.size()`         | `s.length`   |

### Memory Layout

```
push(1) → push(2) → push(3)

TOP →  [3]   ← most recently pushed
       [2]
       [1]   ← first pushed (bottom)
```

### C++ Declaration

```cpp
#include <stack>
stack<int> s;

// Common pattern: use vector as stack for index access
vector<int> stk;
stk.push_back(x);
stk.pop_back();
int top = stk.back();
```

### TypeScript Declaration

```ts
const stack: number[] = [];
stack.push(x);
stack.pop();
const top = stack[stack.length - 1];
```

---

## 2. When to Use a Stack

### Balanced / Matching Problems
Whenever you need to verify that an opening token is matched by a corresponding closing token in the correct order.
- Parentheses, brackets, braces validation
- XML / HTML tag matching
- Compiler expression parsing

Key insight: push opening tokens; on a closing token, check that `top` is the matching opener.

### Undo / Back Navigation
Any system where you need to reverse a sequence of operations in LIFO order.
- Browser back button (history of URLs)
- Text editor undo
- Git stash

### Depth-First Search (DFS) — Iterative
Recursive DFS uses the call stack implicitly. To make DFS iterative, maintain an explicit stack:

```cpp
stack<int> dfs;
dfs.push(start);
while (!dfs.empty()) {
    int node = dfs.top(); dfs.pop();
    for (int neighbor : graph[node]) {
        if (!visited[neighbor]) dfs.push(neighbor);
    }
}
```

### Monotonic Stack Problems (preview — covered in depth in `/monotonic-stack/README.md`)
- Next greater / next smaller element
- Largest rectangle in histogram
- Trapping rain water

### Expression Evaluation
- Reverse Polish Notation (RPN)
- Infix to postfix conversion

---

## 3. Solved Problems

---

### Problem 1 — Valid Parentheses (LC 20)

**Problem:** Given a string containing `(`, `)`, `{`, `}`, `[`, `]`, determine if the string is valid. Every opening bracket must be closed by the same type in correct order.

**Approach:**
- Push every opening bracket onto the stack.
- On every closing bracket, check if `top` is the matching opener. If not — invalid.
- At the end, stack must be empty.

**C++ Solution:**

```cpp
bool isValid(string s) {
    stack<char> stk;
    unordered_map<char, char> match = {{')', '('}, {']', '['}, {'}', '{'}};

    for (char c : s) {
        if (c == '(' || c == '[' || c == '{') {
            stk.push(c);
        } else {
            if (stk.empty() || stk.top() != match[c]) return false;
            stk.pop();
        }
    }
    return stk.empty();
}
```

**Complexity:** Time O(n), Space O(n)

**Walkthrough on `"({[]})"`:**
```
c='('  → push  → stk: ['(']
c='{'  → push  → stk: ['(', '{']
c='['  → push  → stk: ['(', '{', '[']
c=']'  → match['[']==top → pop → stk: ['(', '{']
c='}'  → match['{']==top → pop → stk: ['(']
c=')'  → match['(']==top → pop → stk: []
return stk.empty() → true
```

---

### Problem 2 — Min Stack (LC 155)

**Problem:** Design a stack that supports push, pop, top, and `getMin()` — all in O(1).

**Approach:**
Use two stacks: the main stack and an auxiliary `minStack`. The `minStack` stores the current minimum at each level. When you push `x`, push `min(x, minStack.top())` onto `minStack`.

**C++ Solution:**

```cpp
class MinStack {
    stack<int> stk;
    stack<int> minStk;

public:
    void push(int val) {
        stk.push(val);
        if (minStk.empty()) minStk.push(val);
        else minStk.push(min(val, minStk.top()));
    }

    void pop() {
        stk.pop();
        minStk.pop();
    }

    int top() {
        return stk.top();
    }

    int getMin() {
        return minStk.top();
    }
};
```

**Complexity:** Time O(1) all operations, Space O(n)

**Walkthrough on `push(5), push(3), push(7), push(1), pop()`:**
```
push(5): stk=[5],       minStk=[5]
push(3): stk=[5,3],     minStk=[5,3]
push(7): stk=[5,3,7],   minStk=[5,3,3]
push(1): stk=[5,3,7,1], minStk=[5,3,3,1]
getMin() → 1
pop():   stk=[5,3,7],   minStk=[5,3,3]
getMin() → 3   ← correctly restored
```

**Why this works:** `minStk` acts as a snapshot of what the minimum was at every stack depth, so popping always correctly restores the previous minimum.

---

### Problem 3 — Evaluate Reverse Polish Notation (LC 150)

**Problem:** Evaluate an arithmetic expression in RPN. Valid operators are `+`, `-`, `*`, `/`. Division truncates toward zero.

**Approach:**
- Scan tokens left to right.
- If a number, push it.
- If an operator, pop the top two numbers, apply the operator, push the result.

**C++ Solution:**

```cpp
int evalRPN(vector<string>& tokens) {
    stack<long long> stk;
    unordered_set<string> ops = {"+", "-", "*", "/"};

    for (const string& t : tokens) {
        if (ops.count(t)) {
            long long b = stk.top(); stk.pop();
            long long a = stk.top(); stk.pop();
            if (t == "+") stk.push(a + b);
            else if (t == "-") stk.push(a - b);
            else if (t == "*") stk.push(a * b);
            else stk.push((long long)(a / b)); // truncates toward zero in C++11+
        } else {
            stk.push(stoll(t));
        }
    }
    return (int)stk.top();
}
```

**Complexity:** Time O(n), Space O(n)

**Walkthrough on `["2","1","+","3","*"]` → (2+1)*3 = 9:**
```
"2"  → push 2    → stk: [2]
"1"  → push 1    → stk: [2, 1]
"+"  → pop 1, 2 → push 3  → stk: [3]
"3"  → push 3    → stk: [3, 3]
"*"  → pop 3, 3 → push 9  → stk: [9]
return 9
```

**Important:** Pop order matters. `b = stk.top()` first (righthand operand), then `a` (lefthand). For subtraction and division: result is `a OP b`.

---

### Problem 4 — Daily Temperatures (LC 739) — Introduction to Monotonic Stack

**Problem:** Given `temperatures[]`, for each day find how many days you must wait until a warmer temperature. Return a result array where `result[i]` is the number of days to wait for day `i`, or `0` if no warmer day exists.

**Naive approach:** O(n^2) — for each day, scan forward.

**Stack approach:** O(n) — maintain a stack of indices of days awaiting a warmer day. When the current day is warmer than the day at the stack's top index, that top index has found its answer.

**C++ Solution:**

```cpp
vector<int> dailyTemperatures(vector<int>& T) {
    int n = T.size();
    vector<int> result(n, 0);
    stack<int> stk; // indices of days awaiting a warmer day

    for (int i = 0; i < n; i++) {
        // current temperature is warmer than the temperature at stk.top()
        while (!stk.empty() && T[i] > T[stk.top()]) {
            int j = stk.top(); stk.pop();
            result[j] = i - j; // days waited = current index - day's index
        }
        stk.push(i);
    }
    return result;
}
```

**Complexity:** Time O(n) — each index pushed and popped at most once. Space O(n).

**Walkthrough on `[73, 74, 75, 71, 69, 72, 76, 73]`:**
```
i=0 T=73: stk empty → push 0        stk:[0]
i=1 T=74: T[1]>T[0]=73 → res[0]=1, pop; push 1  stk:[1]
i=2 T=75: T[2]>T[1]=74 → res[1]=1, pop; push 2  stk:[2]
i=3 T=71: 71 < 75, push 3           stk:[2,3]
i=4 T=69: 69 < 71, push 4           stk:[2,3,4]
i=5 T=72: 72>T[4]=69 → res[4]=1, pop
          72>T[3]=71 → res[3]=2, pop
          72<T[2]=75, push 5         stk:[2,5]
i=6 T=76: 76>T[5]=72 → res[5]=1, pop
          76>T[2]=75 → res[2]=4, pop; push 6  stk:[6]
i=7 T=73: 73 < 76, push 7           stk:[6,7]
remaining: res[6]=0, res[7]=0

result: [1, 1, 4, 2, 1, 1, 0, 0]
```

This pattern — maintaining a stack of "unresolved" indices and resolving them when a trigger condition is met — is the core of the **monotonic stack** technique. See `/monotonic-stack/README.md` for the full treatment.

---

## Summary Table

| Problem                  | Stack Role                              | Time | Space |
|--------------------------|-----------------------------------------|------|-------|
| Valid Parentheses        | Match opening to closing brackets       | O(n) | O(n)  |
| Min Stack                | Parallel min snapshot at each depth     | O(1) | O(n)  |
| Evaluate RPN             | Operand accumulator                     | O(n) | O(n)  |
| Daily Temperatures       | Monotonic stack — next greater index    | O(n) | O(n)  |

## Key Mental Models

1. **Push when deferring a decision** — push elements whose answers depend on future elements.
2. **Pop when the condition is satisfied** — the trigger event (e.g., a closing bracket, a higher temperature) resolves the deferred element.
3. **Stack holds open/unresolved state** — if the stack is empty at the end, everything was resolved; leftover elements have no answer.
