# 08 — C++ & Problem Solving

## C++ fundamentals

Know:

- stack vs heap
- pointers and references
- RAII
- constructors/destructors
- copy vs move semantics
- smart pointers
- `unique_ptr`
- `shared_ptr`
- `weak_ptr`
- STL containers
- templates
- virtual functions
- polymorphism
- memory management
- threads and mutexes

### Stack vs heap

Stack allocation is tied to scope/lifetime and is typically fast. Heap allocation provides dynamic lifetime but requires allocation/deallocation management.

RAII ties resource lifetime to object lifetime, making cleanup deterministic.

---

## DSA roadmap

### Arrays / Strings

- hash maps
- two pointers
- sliding window
- prefix sums
- binary search

### Linked Lists

- reverse list
- fast/slow pointers
- cycle detection

### Trees

- DFS
- BFS
- BST
- lowest common ancestor

### Graphs

- DFS/BFS
- topological sort
- union-find
- shortest path
- Dijkstra

### Heaps

- priority queues
- top K
- scheduling

### Dynamic Programming

Identify:

```text
State
Transition
Base case
Answer
```

### Backtracking

- subsets
- permutations
- combinations
- constraint search

---

## Problem-solving method

Before coding:

1. Restate the problem.
2. Identify input/output constraints.
3. Give a brute-force solution.
4. Find the bottleneck.
5. Choose the data structure/pattern.
6. Explain time and space complexity.
7. Code.
8. Test edge cases.

Do not jump straight to code in an interview.

---

## Backend-oriented DSA questions

Practice problems that resemble production work:

- rate limiter using token bucket
- LRU cache
- task scheduler
- top-K frequent events
- deduplicate events
- dependency resolution/topological sort
- shortest route
- log aggregation
- sliding-window metrics
- concurrent job scheduling

These connect algorithmic thinking to your backend experience.
