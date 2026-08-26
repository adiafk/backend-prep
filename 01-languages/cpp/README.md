# C++

Related: [C++/DSA deep-dive](../../08-cpp-dsa.md) · [DSA patterns](../../12-dsa/README.md) · [Concurrency fundamentals](../../00-foundations/concurrency/README.md)

---

## Stack vs Heap Allocation

Understanding where memory lives determines performance characteristics and lifetime rules.

```cpp
void example() {
  // Stack: automatic storage duration
  // - Allocated at function entry, freed at function exit (scope-tied lifetime)
  // - Allocation is O(1) — just decrement the stack pointer
  // - Limited size (typically 1–8 MB per thread)
  int x = 42;
  char buf[1024];  // 1 KB on the stack — fine
  // char buf[10000000]; // likely stack overflow

  // Heap: dynamic storage duration
  // - Allocated with new/malloc, freed with delete/free (manual — easy to forget)
  // - Allocation involves OS interaction and bookkeeping — slower
  // - Nearly unlimited size (limited by virtual memory)
  int* y = new int(42);
  delete y;  // must free explicitly, or leak

  // RAII replaces manual heap management (see below)
  // Stack objects with destructors give you heap safety
  auto vec = std::vector<int>(1000);  // vector manages its heap buffer internally
  // vec is destroyed at end of scope, freeing its heap buffer automatically
}
```

### Why manual new/delete fails at scale

```cpp
int* getData() {
  int* p = new int[100];
  if (someCondition()) {
    return nullptr;  // leak — forgot to delete[] p
  }
  // what if an exception is thrown here? also a leak
  return p;
}
// Every early return, exception path, and forgotten branch is a potential leak.
// RAII solves this by tying cleanup to object lifetime.
```

---

## RAII (Resource Acquisition Is Initialization)

The most important C++ idiom. Acquire a resource in a constructor; release it in a destructor. The destructor runs automatically when the object goes out of scope — even if an exception is thrown.

```cpp
// File handle wrapper: no way to forget to close
class File {
  FILE* handle_;
public:
  explicit File(const char* path, const char* mode) {
    handle_ = fopen(path, mode);
    if (!handle_) throw std::runtime_error(std::string("Cannot open: ") + path);
  }

  ~File() {
    if (handle_) fclose(handle_);  // guaranteed to run
  }

  // Prevent copying (would lead to double-close)
  File(const File&) = delete;
  File& operator=(const File&) = delete;

  FILE* get() { return handle_; }
};

void processFile() {
  File f("data.txt", "r");
  // ... use f.get() ...
  // f destroyed here — fclose called automatically
  // even if an exception is thrown above
}

// Mutex RAII — std::lock_guard is exactly this pattern
class LockGuard {
  std::mutex& mtx_;
public:
  explicit LockGuard(std::mutex& m) : mtx_(m) { mtx_.lock(); }
  ~LockGuard() { mtx_.unlock(); }  // unlocks even if exception thrown
  LockGuard(const LockGuard&) = delete;
};

void criticalSection(std::mutex& mu, std::vector<int>& data) {
  LockGuard guard(mu);      // lock acquired
  data.push_back(42);       // if this throws, guard destructor still unlocks
  // lock released at end of scope — no manual unlock needed
}
```

---

## Rule of Zero / Three / Five

When you write a destructor, copy constructor, or copy assignment operator, you almost certainly need all three — the "rule of three." Move semantics add two more — the "rule of five." The "rule of zero" says: prefer to not write any of these by using smart pointers and standard containers.

```cpp
// Rule of Zero: let standard types manage resources
class Config {
  std::string name_;       // string manages its memory
  std::vector<int> data_;  // vector manages its memory
  // No destructor, no copy/move needed — defaults are correct
};

// Rule of Five: when you manage raw resources directly
class Buffer {
  size_t size_;
  char* data_;

public:
  // Constructor
  explicit Buffer(size_t size) : size_(size), data_(new char[size]()) {}

  // Destructor — releases the resource
  ~Buffer() { delete[] data_; }

  // Copy constructor — deep copy
  Buffer(const Buffer& other) : size_(other.size_), data_(new char[other.size_]) {
    std::memcpy(data_, other.data_, size_);
  }

  // Copy assignment — deep copy with self-assignment guard
  Buffer& operator=(const Buffer& other) {
    if (this == &other) return *this;
    delete[] data_;
    size_ = other.size_;
    data_ = new char[size_];
    std::memcpy(data_, other.data_, size_);
    return *this;
  }

  // Move constructor — steal the resource, leave other in valid empty state
  Buffer(Buffer&& other) noexcept : size_(other.size_), data_(other.data_) {
    other.size_ = 0;
    other.data_ = nullptr;  // critical: prevent double-free in other's destructor
  }

  // Move assignment — steal and clean up old resource
  Buffer& operator=(Buffer&& other) noexcept {
    if (this == &other) return *this;
    delete[] data_;
    size_ = other.size_;
    data_ = other.data_;
    other.size_ = 0;
    other.data_ = nullptr;
    return *this;
  }
};
```

---

## Move Semantics

Moving transfers ownership of a resource instead of copying it. O(1) regardless of resource size.

```cpp
// rvalue reference (&&): binds to temporaries and explicitly moved objects
void take(std::string&& s) {
  // s is ours to pillage
  internalBuffer_ = std::move(s);  // move s into member — s is now "moved-from"
}

std::string greeting = "Hello, World!";
take(std::move(greeting));  // greeting is now in a valid but unspecified state
// Do NOT use greeting after moving from it — behavior is defined but value is unknown.
// The only safe operations on a moved-from object: assign a new value, or destroy it.

// std::move doesn't move anything — it's a cast to rvalue reference
// The actual move happens in the receiving function's move constructor/assignment
```

### Why this matters for containers

```cpp
std::vector<std::string> words;
std::string s = "hello";

words.push_back(s);             // copies s — s still valid, another string created
words.push_back(std::move(s))  // moves s — no copy, s is left in moved-from state
// For large strings or complex objects, move is dramatically cheaper than copy.

// Return value optimization (RVO/NRVO): compilers often eliminate moves entirely
std::string buildGreeting(const std::string& name) {
  return "Hello, " + name;  // compiler constructs directly in the caller's storage
}
// Don't write std::move on a return statement — it prevents RVO
```

---

## Smart Pointers

```cpp
#include <memory>

// unique_ptr: sole ownership — one owner at a time, no reference counting
// Overhead: exactly the same as a raw pointer
auto ptr = std::make_unique<int>(42);
*ptr = 100;
// ptr2 = ptr;   // Error: unique_ptr is not copyable
auto ptr2 = std::move(ptr);  // transfer ownership — ptr is now null
// ptr2 goes out of scope here — int is automatically deleted

// shared_ptr: shared ownership via atomic reference count
// Overhead: heap allocation for control block + atomic ops on copy/destroy
auto sp1 = std::make_shared<std::vector<int>>(std::initializer_list<int>{1, 2, 3});
{
  auto sp2 = sp1;  // ref count = 2
  auto sp3 = sp1;  // ref count = 3
}  // sp2, sp3 destroyed — ref count = 1
// sp1 destroyed — ref count = 0, vector is deleted

// weak_ptr: observe without owning — doesn't affect ref count
// Breaks cycles that would otherwise leak memory
std::weak_ptr<int> wp = sp1;
if (auto locked = wp.lock()) {  // returns shared_ptr if still alive, else null
  std::cout << *locked << '\n';
} else {
  std::cout << "Object was destroyed\n";
}

// Cycle that leaks without weak_ptr:
struct Node {
  std::shared_ptr<Node> next;   // forward link
  std::weak_ptr<Node> prev;     // use weak_ptr for backward links to break cycles
};
```

### When to use each

| Situation | Smart pointer |
|---|---|
| Single owner, no sharing | `unique_ptr` |
| Shared ownership needed | `shared_ptr` |
| Observer / cache / cycle-breaker | `weak_ptr` |
| Never | Raw owning pointer |

---

## Templates and Type Deduction

```cpp
// Function template: compiler deduces T from arguments
template<typename T>
T maxVal(T a, T b) { return a > b ? a : b; }

maxVal(3, 5)         // T = int, deduced
maxVal(3.0, 5.0)     // T = double, deduced
// maxVal(3, 5.0)    // Error: T can't be both int and double simultaneously
maxVal<double>(3, 5.0)  // explicit — forces T = double

// Class template
template<typename T>
class Stack {
  std::vector<T> data_;
public:
  void push(const T& val) { data_.push_back(val); }
  void push(T&& val) { data_.push_back(std::move(val)); }
  T& top() { return data_.back(); }
  bool empty() const { return data_.empty(); }
  void pop() { data_.pop_back(); }
};

Stack<int> intStack;
Stack<std::string> strStack;

// Constrained templates (C++20 concepts)
#include <concepts>

template<std::integral T>        // T must satisfy std::integral
T factorial(T n) {
  return n <= 1 ? 1 : n * factorial(n - 1);
}

template<typename T>
requires std::is_arithmetic_v<T>  // alternative syntax
T square(T x) { return x * x; }

// Template type deduction with auto
auto val = maxVal(3, 5);   // val: int
```

---

## STL Containers — Complexity Reference

| Container | Random access | Push back | Insert middle | Lookup | Notes |
|---|---|---|---|---|---|
| `vector<T>` | O(1) | O(1) amortized | O(n) | O(n) | Contiguous; cache-friendly |
| `deque<T>` | O(1) | O(1) amortized | O(n) | O(n) | Segmented; fast at both ends |
| `list<T>` | O(n) | O(1) | O(1) | O(n) | Doubly linked; iterator-stable |
| `unordered_map<K,V>` | — | — | O(1) avg | O(1) avg | Hash table; worst case O(n) |
| `map<K,V>` | — | — | O(log n) | O(log n) | Red-black tree; ordered |
| `unordered_set<T>` | — | — | O(1) avg | O(1) avg | Hash set |
| `set<T>` | — | — | O(log n) | O(log n) | Ordered set |
| `priority_queue<T>` | O(1) top | O(log n) | — | O(n) | Max-heap by default |

```cpp
#include <vector>
#include <unordered_map>
#include <queue>

// vector: amortized O(1) push_back because it doubles capacity
// Initial capacity: 0 or 1. When full: allocate 2x, copy all elements, free old.
// Amortized because most insertions are O(1); the occasional O(n) copy amortizes out.
std::vector<int> v;
v.reserve(1000);  // pre-allocate to avoid reallocations if size is known
v.push_back(1);   // O(1)

// unordered_map: O(1) average, O(n) worst case due to hash collisions
std::unordered_map<std::string, int> freq;
freq["apple"]++;   // operator[] default-constructs to 0, then increments
freq.count("apple");   // 0 or 1 (is key present?)
freq.find("apple");    // iterator or end() — prefer over count when you need the value

// priority_queue: max-heap by default
std::priority_queue<int> maxPQ;
maxPQ.push(3); maxPQ.push(1); maxPQ.push(4);
maxPQ.top();   // 4 — O(1)
maxPQ.pop();   // remove 4 — O(log n)

// min-heap
std::priority_queue<int, std::vector<int>, std::greater<int>> minPQ;
```

---

## Lambdas

```cpp
#include <algorithm>
#include <functional>

// Basic lambda
auto square = [](int x) { return x * x; };
square(5);  // 25

// Capture by value: copies at lambda creation time
int offset = 10;
auto addOffset = [offset](int x) { return x + offset; };
offset = 100;    // doesn't affect addOffset — it captured the value
addOffset(5);    // 15, not 105

// Capture by reference: live reference — dangerous if lambda outlives the variable
auto addOffsetRef = [&offset](int x) { return x + offset; };
offset = 100;
addOffsetRef(5);  // 105 — sees current value

// Capture all by value or reference (avoid in production — unclear what's captured)
auto byVal = [=](int x) { return x + offset; };
auto byRef = [&](int x) { return x + offset; };

// mutable: allows modifying captured-by-value copies
int count = 0;
auto counter = [count]() mutable { return ++count; };
counter();  // 1 (local copy of count)
counter();  // 2
count;      // still 0 — original unchanged

// Practical: sort with custom comparator
std::vector<std::pair<int,int>> intervals = {{1,3},{2,6},{8,10}};
std::sort(intervals.begin(), intervals.end(),
  [](const auto& a, const auto& b) { return a.first < b.first; });

// Store in std::function (with type erasure overhead)
std::function<int(int)> fn = [offset](int x) { return x + offset; };
```

---

## const Correctness

```cpp
// const on a variable: value cannot change after initialization
const int MAX_SIZE = 1024;

// const reference: can bind to temporaries, avoids copying, prevents modification
void print(const std::string& s) {
  // s.push_back('!');  // Error: const reference
  std::cout << s << '\n';
}
print("hello");  // temporary can bind to const&

// const member function: does not modify the object — can be called on const objects
class Rectangle {
  double width_, height_;
public:
  Rectangle(double w, double h) : width_(w), height_(h) {}

  double area() const { return width_ * height_; }  // const — safe to call on const Rect
  void scale(double factor) { width_ *= factor; height_ *= factor; }  // non-const

  // mutable: can be modified even in const member functions (e.g., caching)
  mutable double cachedArea_ = -1;
  double cachedAreaFn() const {
    if (cachedArea_ < 0) cachedArea_ = width_ * height_;  // OK: mutable
    return cachedArea_;
  }
};

const Rectangle r(3, 4);
r.area();    // OK — const member function
r.scale(2);  // Error — non-const member function on const object

// const pointer distinctions
int x = 5;
const int* p1 = &x;   // pointer to const int — can't change *p1
int* const p2 = &x;   // const pointer to int — can't change p2 (the address)
const int* const p3 = &x;  // both const
```

---

## Virtual Dispatch and vtable

```cpp
// Virtual dispatch: runtime polymorphism via vtable
class Animal {
public:
  virtual std::string sound() const { return "..."; }   // virtual
  virtual ~Animal() = default;  // MUST be virtual if you delete via base pointer
};

class Dog : public Animal {
public:
  std::string sound() const override { return "Woof"; }
};

class Cat : public Animal {
public:
  std::string sound() const override { return "Meow"; }
};

// How it works: each class with virtual functions has a vtable (table of function pointers)
// Each object has a hidden vptr (vtable pointer) as its first member
// animal->sound() compiles to: animal->vptr[sound_index]()
// sizeof(Dog) > sizeof a plain struct by one pointer (the vptr)

// Without virtual destructor: UNDEFINED BEHAVIOR when deleting via base pointer
Animal* a = new Dog();
a->sound();   // "Woof" — dynamic dispatch works
delete a;     // if ~Animal not virtual: only ~Animal runs, ~Dog doesn't → leak/UB

// Pure virtual: must be overridden in derived class (abstract class)
class Shape {
public:
  virtual double area() const = 0;  // pure virtual — Shape cannot be instantiated
  virtual ~Shape() = default;
};

// Non-virtual function: no dispatch, called based on static type
class Base {
public:
  void nonVirtual() { std::cout << "Base\n"; }
};
class Derived : public Base {
public:
  void nonVirtual() { std::cout << "Derived\n"; }
};
Base* b = new Derived();
b->nonVirtual();  // "Base" — not virtual, no dispatch
```

---

## Memory Layout and Alignment

```cpp
#include <cstddef>

struct Compact {
  char a;    // 1 byte
  char b;    // 1 byte
  short c;   // 2 bytes
  int d;     // 4 bytes
};
// sizeof(Compact) == 8 — natural alignment, no padding needed here

struct Padded {
  char a;    // 1 byte
  // 3 bytes padding (int must align to 4-byte boundary)
  int b;     // 4 bytes
  char c;    // 1 byte
  // 3 bytes padding (struct size must be multiple of largest alignment)
};
// sizeof(Padded) == 12 — 6 bytes of data, 6 bytes of padding

struct Optimized {
  int b;     // 4 bytes — put largest first
  char a;    // 1 byte
  char c;    // 1 byte
  // 2 bytes padding (to align to 4-byte boundary)
};
// sizeof(Optimized) == 8

// Rule: sort fields from largest to smallest alignment to minimize padding
// offsetof(Type, member) gives the byte offset of a field
static_assert(offsetof(Padded, b) == 4, "unexpected offset");
```

---

## Common Undefined Behavior

UB means the compiler is allowed to assume it never happens. Code that "works" in debug builds may break in optimized builds.

```cpp
// 1. Null pointer dereference
int* p = nullptr;
*p = 42;   // UB — typical result: SIGSEGV, but compiler may optimize around it

// 2. Use-after-free
int* x = new int(5);
delete x;
*x = 10;   // UB — memory may have been reused; behavior is unpredictable

// 3. Out-of-bounds array access (no automatic bounds checking)
int arr[5] = {};
arr[10] = 1;           // UB — no exception, just memory corruption
std::vector<int> v(5);
v[10] = 1;             // UB — use v.at(10) for bounds-checked access (throws)

// 4. Signed integer overflow (unsigned overflow is defined — it wraps)
int big = INT_MAX;
big + 1;     // UB — compiler assumes this doesn't happen and may optimize accordingly
unsigned u = UINT_MAX;
u + 1;       // defined: 0 (wraps around)

// 5. Reading an uninitialized variable
int x;
int y = x;  // UB — x has indeterminate value

// 6. Data race (two threads accessing same memory, at least one writes, no synchronization)
int counter = 0;  // shared
// thread 1: counter++   // UB if not protected by mutex or atomic
// thread 2: counter++

// Detection: compile with -fsanitize=address,undefined (ASan + UBSan)
// Use -fsanitize=thread (TSan) for data races
```

---

## std::thread, std::mutex, std::lock_guard

```cpp
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

// Basic thread creation
void worker(int id) {
  std::cout << "Thread " << id << " running\n";
}

std::thread t(worker, 42);
t.join();  // wait for t to finish — must call join() or detach() before destructor

// Protecting shared state with mutex
std::mutex mu;
std::vector<int> shared_data;

void addItem(int val) {
  std::lock_guard<std::mutex> lock(mu);  // RAII: locks mu, unlocks when lock goes out of scope
  shared_data.push_back(val);
  // lock released here — even if push_back throws
}

// lock_guard vs unique_lock
// lock_guard: simpler, cannot be unlocked manually
// unique_lock: can be unlocked/re-locked manually, required for condition_variable

// Atomic operations: for simple counters, no mutex needed
std::atomic<int> counter{0};

void increment() {
  counter.fetch_add(1, std::memory_order_relaxed);  // atomic increment
}

std::vector<std::thread> threads;
for (int i = 0; i < 10; ++i) {
  threads.emplace_back(increment);
}
for (auto& t : threads) t.join();
std::cout << counter.load() << '\n';  // 10 — guaranteed

// Thread-safe singleton (C++11 guarantees static local initialization is thread-safe)
class Config {
public:
  static Config& instance() {
    static Config inst;  // initialized exactly once, thread-safe
    return inst;
  }
};
```

---

## Common Interview Questions

**Q: What is RAII and why does C++ use it instead of garbage collection?**
RAII ties resource lifetime to object scope — acquisition in the constructor, release in the destructor. The destructor runs deterministically when the object goes out of scope (or is explicitly destroyed), including when exceptions are thrown. GC is non-deterministic: you don't know when the collector runs, so resources like file handles, mutexes, and sockets can't be reliably managed. RAII gives deterministic, low-overhead resource management without GC pauses.

**Q: What is the difference between `unique_ptr`, `shared_ptr`, and `weak_ptr`?**
`unique_ptr` expresses sole ownership — cannot be copied, only moved. Zero overhead over a raw pointer. `shared_ptr` allows multiple owners via atomic reference counting — the resource is deleted when the last owner is destroyed. Has overhead (control block allocation, atomic operations). `weak_ptr` holds a non-owning reference to a `shared_ptr`-managed object — doesn't increment the ref count, used to break ownership cycles and to observe objects without extending their lifetime.

**Q: What does `std::move` actually do?**
`std::move` is a cast — it converts its argument to an rvalue reference (`T&&`). It doesn't move anything by itself. The actual resource transfer happens in whichever move constructor or move assignment operator receives the rvalue reference. After moving from an object, it's in a valid but unspecified state — safe to destroy or reassign, but don't read from it.

**Q: Why must a virtual destructor be declared in a polymorphic base class?**
If you delete a derived object through a base class pointer and the destructor is not virtual, only the base class destructor runs — the derived class destructor is never called. This leaks any resources the derived class manages. With `virtual ~Base()`, the vtable dispatches to the most-derived destructor, which chains up through the hierarchy correctly.

**Q: What is undefined behavior and why is it dangerous in optimized builds?**
The C++ standard declares certain operations (null dereference, signed overflow, out-of-bounds access, use-after-free, data races) as undefined behavior. The compiler is free to assume these never happen, and uses that assumption to optimize. Code that appears to work in debug builds may be completely wrong in optimized builds because the compiler legally eliminates or transforms the code around the UB. Use AddressSanitizer (`-fsanitize=address`) and UBSanitizer (`-fsanitize=undefined`) during development to catch UB before it reaches production.

**Q: What is the rule of five?**
When a class manages a raw resource (raw pointer, file handle, socket), you must explicitly define all five special member functions to handle both copying and moving correctly: destructor, copy constructor, copy assignment operator, move constructor, move assignment operator. If you define a destructor, the compiler won't generate move operations, and the default copy operations do a shallow copy — which causes double-free bugs. Prefer the rule of zero: use `unique_ptr`, `vector`, `string`, and other RAII types so the compiler-generated defaults are correct.
