# C++

## Smart Pointers

Raw pointers require manual memory management — easy to leak or double-free. Smart pointers automate this.

```cpp
#include <memory>

// unique_ptr: sole ownership — one owner, cleaned up when it goes out of scope
std::unique_ptr<int> p = std::make_unique<int>(42);
std::cout << *p << "\n";   // 42
// p is deleted automatically when it leaves scope

// Move ownership (unique_ptr cannot be copied)
std::unique_ptr<int> p2 = std::move(p);
// p is now nullptr; p2 owns the int

// shared_ptr: shared ownership — reference counted
auto sp1 = std::make_shared<std::vector<int>>(std::initializer_list<int>{1, 2, 3});
auto sp2 = sp1;  // reference count = 2
sp1.reset();     // reference count = 1; data still alive
sp2.reset();     // reference count = 0; data deleted

// weak_ptr: observe without owning — breaks circular references
std::weak_ptr<int> wp = sp1;
if (auto locked = wp.lock()) {  // safely access if still alive
    std::cout << *locked << "\n";
}
```

---

## Move Semantics

Avoid unnecessary copies of expensive resources (heap-allocated memory, file handles).

```cpp
class Buffer {
    size_t size_;
    char* data_;
public:
    Buffer(size_t size) : size_(size), data_(new char[size]) {}

    // Copy constructor: duplicate the data
    Buffer(const Buffer& other) : size_(other.size_), data_(new char[other.size_]) {
        std::memcpy(data_, other.data_, size_);
    }

    // Move constructor: steal the data, leave other in valid-but-empty state
    Buffer(Buffer&& other) noexcept : size_(other.size_), data_(other.data_) {
        other.size_ = 0;
        other.data_ = nullptr;  // prevent double-free
    }

    ~Buffer() { delete[] data_; }
};

Buffer a(1024);
Buffer b = std::move(a);  // move — O(1), no copy
```

Use `std::move` when: passing a local variable to a function that takes `&&`, returning a local variable (often unnecessary — NRVO applies), storing in a container.

---

## STL Containers — Complexity Cheat Sheet

| Container | Access | Insert (end) | Insert (middle) | Search |
|-----------|--------|-------------|----------------|--------|
| `vector` | O(1) | O(1) amortized | O(n) | O(n) |
| `deque` | O(1) | O(1) | O(n) | O(n) |
| `list` | O(n) | O(1) | O(1) | O(n) |
| `unordered_map` | O(1) avg | O(1) avg | — | O(1) avg |
| `map` | O(log n) | O(log n) | — | O(log n) |
| `unordered_set` | — | O(1) avg | — | O(1) avg |
| `set` | — | O(log n) | — | O(log n) |
| `priority_queue` | O(1) top | O(log n) | — | O(n) |
| `stack` / `queue` | O(1) top | O(1) | — | O(n) |

```cpp
#include <bits/stdc++.h>
using namespace std;

// Common competitive programming setup
int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    // Max heap (default)
    priority_queue<int> maxPQ;
    maxPQ.push(3); maxPQ.push(1); maxPQ.push(4);
    cout << maxPQ.top() << "\n";  // 4

    // Min heap
    priority_queue<int, vector<int>, greater<int>> minPQ;
    minPQ.push(3); minPQ.push(1); minPQ.push(4);
    cout << minPQ.top() << "\n";  // 1

    // unordered_map with default value
    unordered_map<string, int> freq;
    freq["apple"]++;  // default-constructs to 0, then increments

    return 0;
}
```

---

## Templates

```cpp
// Function template
template<typename T>
T max(T a, T b) { return a > b ? a : b; }

// Class template
template<typename T>
class MinStack {
    stack<T> data, mins;
public:
    void push(T val) {
        data.push(val);
        mins.push(mins.empty() ? val : min(val, mins.top()));
    }
    void pop() { data.pop(); mins.pop(); }
    T top() { return data.top(); }
    T getMin() { return mins.top(); }
};

// Template with constraint (C++20 concepts)
template<std::integral T>
T factorial(T n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}
```

---

## Competitive Programming Idioms

```cpp
#include <bits/stdc++.h>
using namespace std;
typedef long long ll;
typedef vector<int> vi;
typedef pair<int,int> pii;
#define all(x) x.begin(), x.end()
#define pb push_back

// Sorting with custom comparator
vector<pii> intervals = {{1,3},{2,6},{8,10}};
sort(all(intervals), [](const pii& a, const pii& b) {
    return a.first < b.first;  // sort by start time
});

// Binary search
int idx = lower_bound(all(sorted_vec), target) - sorted_vec.begin();

// GCD, LCM
int g = __gcd(12, 8);       // 4
int l = 12 / g * 8;         // 24 (avoid overflow: divide first)

// Bit tricks
bool isPowerOf2 = n > 0 && !(n & (n - 1));
int countBits = __builtin_popcount(n);
int lsb = n & (-n);         // isolate lowest set bit

// 2D grid BFS directions
int dx[] = {0, 0, 1, -1};
int dy[] = {1, -1, 0, 0};
```
