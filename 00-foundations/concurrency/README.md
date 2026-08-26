# Concurrency

Concurrency is the source of the hardest bugs in production systems: they are nondeterministic, timing-dependent, and often unreproducible. This file covers the full mental model — from race conditions at the hardware level to high-level patterns like producer-consumer and async/await.

**Prerequisites**: [Operating Systems](../operating-systems/README.md) — process, thread, and scheduler concepts are assumed.

---

## Table of Contents

1. [Race Condition and Data Race](#1-race-condition-and-data-race)
2. [Critical Section and Mutual Exclusion](#2-critical-section-and-mutual-exclusion)
3. [Mutex](#3-mutex)
4. [Semaphore](#4-semaphore)
5. [Spinlock](#5-spinlock)
6. [Deadlock](#6-deadlock)
7. [Livelock and Starvation](#7-livelock-and-starvation)
8. [Condition Variable](#8-condition-variable)
9. [Producer-Consumer Problem](#9-producer-consumer-problem)
10. [Readers-Writers Problem](#10-readers-writers-problem)
11. [Memory Ordering](#11-memory-ordering)
12. [Atomic Operations](#12-atomic-operations)
13. [Thread Pools](#13-thread-pools)
14. [Async/Await vs Threads](#14-asyncawait-vs-threads)
15. [Runtime Comparison: Node.js vs Go vs Java](#15-runtime-comparison-nodejs-vs-go-vs-java)
16. [Interview Questions](#16-interview-questions)

---

## 1. Race Condition and Data Race

A **race condition** is when the correctness of a program depends on the relative timing or ordering of events in concurrent execution. A **data race** is the specific case where two threads access the same memory location concurrently, at least one access is a write, and there is no synchronization.

All data races are race conditions. Not all race conditions are data races (e.g., a TOCTOU bug on a file).

### Concrete example: two threads incrementing a counter

```cpp
#include <iostream>
#include <thread>

int counter = 0;  // shared, no synchronization

void increment(int iterations) {
    for (int i = 0; i < iterations; ++i) {
        counter++;  // NOT atomic — three machine instructions
    }
}

int main() {
    std::thread t1(increment, 1'000'000);
    std::thread t2(increment, 1'000'000);
    t1.join();
    t2.join();
    std::cout << counter << "\n";  // expected 2000000, actual: anywhere from ~1000000 to 2000000
}
```

Why does `counter++` lose updates? It compiles to three instructions:

```asm
mov eax, [counter]   ; (1) load counter into register
add eax, 1           ; (2) increment register
mov [counter], eax   ; (3) store back to memory
```

If thread 1 executes (1) and reads `counter = 1000`, then is preempted, and thread 2 runs (1)–(3) ten thousand times bringing `counter` to `11000`, then thread 1 resumes from (2) and stores `1001` — ten thousand increments are lost.

This is undefined behavior in C++ (two threads write a non-atomic variable). The compiler and CPU are permitted to assume no data races exist and can transform the code in ways that amplify the damage (e.g., hoisting the load out of the loop entirely).

---

## 2. Critical Section and Mutual Exclusion

A **critical section** is a block of code that accesses shared state and must not be executed by more than one thread at a time.

**Mutual exclusion** (mutex as a concept, before the data structure) is the property that only one thread is in the critical section at any time.

Properties a correct mutual exclusion mechanism must provide:
1. **Safety (mutex property)**: at most one thread in the critical section at any time
2. **Liveness (progress)**: if no thread is in the critical section, a thread waiting to enter will eventually enter
3. **Bounded waiting (fairness)**: a thread waiting to enter will not wait forever while others repeatedly enter

Software-only solutions (Peterson's algorithm) exist but are impractical on modern hardware because the CPU reorders memory accesses. Real implementations use hardware atomic instructions.

---

## 3. Mutex

A mutex (mutual exclusion lock) is the primary synchronization primitive. It has two operations: `lock()` and `unlock()`. A thread that calls `lock()` when the mutex is held by another thread blocks (goes to sleep) until the holder calls `unlock()`.

**Ownership**: a mutex has an owner — the thread that locked it. Only the owner can unlock it. This is the key difference from a semaphore.

```cpp
#include <iostream>
#include <thread>
#include <mutex>

int counter = 0;
std::mutex mtx;

void increment(int iterations) {
    for (int i = 0; i < iterations; ++i) {
        std::lock_guard<std::mutex> lock(mtx);  // RAII: locks on construction, unlocks on destruction
        counter++;
    }
}

int main() {
    std::thread t1(increment, 1'000'000);
    std::thread t2(increment, 1'000'000);
    t1.join();
    t2.join();
    std::cout << counter << "\n";  // always exactly 2000000
}
```

`std::lock_guard` is the correct pattern: it unlocks automatically when the guard goes out of scope, even if an exception is thrown. Never call `mtx.lock()` and `mtx.unlock()` manually — a thrown exception between them leaves the mutex locked forever.

### Mutex implementation internals

At the hardware level, `lock()` uses an atomic compare-and-swap to try to change the mutex state from "unlocked" to "locked by thread X." If that fails (another thread owns it), the thread calls `futex_wait()` (Linux) to put itself to sleep in the kernel. `unlock()` stores "unlocked" and calls `futex_wake()` to wake one waiter. The futex (fast userspace mutex) design means the common uncontended case (lock is free) needs only one atomic instruction with no syscall.

### Types of mutexes

- **`std::mutex`**: non-recursive. If the owning thread tries to lock it again → undefined behavior (deadlock in practice).
- **`std::recursive_mutex`**: the owning thread can lock it multiple times; must unlock the same number of times. Slower — avoid if possible, usually indicates a design issue.
- **`std::timed_mutex`**: adds `try_lock_for()` and `try_lock_until()` — useful for avoiding deadlock by failing fast.
- **`std::shared_mutex`** (C++17): supports shared (read) and exclusive (write) locking. Multiple readers can hold the shared lock simultaneously; a writer takes exclusive ownership.

---

## 4. Semaphore

A semaphore is an integer counter with two atomic operations:
- **`wait()` (P, down, acquire)**: if counter > 0, decrement and return; else block until counter > 0
- **`signal()` (V, up, release)**: increment counter, wake one waiter if any

**No ownership**: any thread can call `signal()`, not just the thread that called `wait()`. This is the fundamental difference from a mutex.

### Binary semaphore vs counting semaphore

**Binary semaphore**: initialized to 1, counter is always 0 or 1. Behaves like a mutex but without ownership — can be used for signaling between threads (thread A waits, thread B signals when work is ready).

**Counting semaphore**: initialized to N, allows up to N threads to be in the critical section simultaneously. Classic use: limit the number of concurrent database connections.

```cpp
#include <semaphore>  // C++20

std::counting_semaphore<10> db_slots(10);  // max 10 concurrent queries

void query_database() {
    db_slots.acquire();          // blocks if 10 others are already querying
    // ... do the query ...
    db_slots.release();
}
```

### Semaphore vs Mutex

| Property | Mutex | Semaphore |
|---|---|---|
| Ownership | Yes — only owner unlocks | No — any thread can signal |
| Use case | Mutual exclusion | Resource counting, signaling |
| Initial value | "unlocked" | Any non-negative integer |
| Recursive lock | Possible (recursive_mutex) | Not applicable |
| Priority inheritance | Often supported | Usually not |

Mutexes should be your default. Use semaphores when you explicitly need the "any thread can release" or "N slots" semantics.

---

## 5. Spinlock

A spinlock is a mutex that, instead of sleeping when the lock is contended, **busy-loops (spins)** trying to acquire it.

```cpp
#include <atomic>

class Spinlock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;  // false = unlocked

public:
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) {
            // spin — keep retrying
            // modern CPUs: add _mm_pause() to reduce power and improve fairness
        }
    }

    void unlock() {
        flag.clear(std::memory_order_release);
    }
};
```

### When a spinlock beats a mutex

A mutex blocks the thread by making a syscall (`futex_wait`), which involves a context switch (~5 µs). If the critical section completes in under ~1 µs, the overhead of sleeping and being woken exceeds the time saved by not spinning.

**Use a spinlock when:**
- The critical section is extremely short (a few instructions, < 1 µs)
- You are on a multicore machine (spinning on a single core means no one else can run on that core — pointless)
- The lock is rarely contended
- You cannot afford sleep/wake latency (real-time systems, kernel interrupt handlers)

**Do not use a spinlock when:**
- The critical section is long (you waste CPU burning cycles while waiting)
- You are in user space on a uniprocessor (the thread holding the lock can never run to release it while you're spinning)
- The lock can be highly contended (excessive cache-line bouncing between cores)

---

## 6. Deadlock

A deadlock is a state where a set of threads are all blocked waiting for resources held by other threads in the set — a circular wait with no exit.

### The four necessary conditions (Coffman conditions)

All four must hold simultaneously for deadlock to occur. Breaking any one prevents it.

1. **Mutual exclusion**: resources are held in a non-shareable mode (only one thread can hold a mutex at a time)
2. **Hold and wait**: a thread holding at least one resource is waiting to acquire additional resources held by other threads
3. **No preemption**: resources cannot be forcibly taken from a thread; they must be released voluntarily
4. **Circular wait**: T1 waits for a resource held by T2, T2 waits for a resource held by T3, ..., Tn waits for a resource held by T1

### Canonical deadlock example

```cpp
std::mutex mtx_a, mtx_b;

void thread1() {
    std::lock_guard<std::mutex> la(mtx_a);  // acquires A
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::lock_guard<std::mutex> lb(mtx_b);  // waits for B (held by thread2)
}

void thread2() {
    std::lock_guard<std::mutex> lb(mtx_b);  // acquires B
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::lock_guard<std::mutex> la(mtx_a);  // waits for A (held by thread1)
}
// Both threads wait forever.
```

### Prevention strategies

**1. Lock ordering (eliminates circular wait)**: define a global ordering on all mutexes (e.g., by memory address). Every thread must acquire mutexes in that order. If all threads acquire A before B, the circular wait becomes impossible.

```cpp
// Always lock the lower-addressed mutex first
void transfer(Account& from, Account& to) {
    auto [first, second] = std::minmax(&from, &to,
        [](Account* a, Account* b) { return a < b; });
    std::lock_guard<std::mutex> l1(first->mtx);
    std::lock_guard<std::mutex> l2(second->mtx);
    // safe — deterministic lock order regardless of argument order
}
```

**2. `std::lock()` / `std::scoped_lock` (C++17)**: acquires multiple mutexes simultaneously using a deadlock-avoidance algorithm (try-lock with backoff).

```cpp
// C++17: acquires both or neither, no deadlock
std::scoped_lock lock(mtx_a, mtx_b);
```

**3. Timed locking (detection + recovery)**: use `try_lock_for()`. If you can't acquire within a timeout, release what you hold and retry. This breaks deadlocks but can cause livelock if threads keep timing out and retrying simultaneously.

**4. Lock-free data structures**: eliminate mutexes entirely using atomics. Deadlock becomes impossible. Harder to implement correctly.

### Detection

Represent threads and resources as a directed graph (resource allocation graph). A cycle in the graph indicates deadlock. Some databases (PostgreSQL, MySQL) periodically run deadlock detection on their transaction lock graphs and kill one transaction (the victim) to break the cycle.

---

## 7. Livelock and Starvation

**Livelock**: threads are actively running but making no progress. Classic example: two threads each try to acquire locks A and B, see a conflict, each backs off and retries, and they perfectly synchronize in their retries forever. Neither is blocked, but neither completes.

Prevented by randomized backoff (Ethernet CSMA/CD) or by designating one thread as the "priority" holder.

**Starvation**: a thread is perpetually denied access to a resource because other threads keep getting priority. A high-priority thread that continuously holds a lock starves low-priority waiters. Fixed by fairness guarantees in the scheduler (CFS handles CPU starvation) or fair mutex implementations (ticket locks grant access in FIFO order).

---

## 8. Condition Variable

A condition variable allows threads to wait for a condition to become true without busy-waiting, while atomically releasing a mutex.

**The problem it solves**: you want to sleep until shared state changes. You can't just check state under a mutex and then sleep — between releasing the mutex and sleeping, another thread could change the state and send the wakeup signal you would miss.

```cpp
std::mutex mtx;
std::condition_variable cv;
bool ready = false;

// Thread 1 — waiter
void waiter() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return ready; });
    // Atomically: releases lock, sleeps, and when woken, reacquires lock before returning.
    // The predicate lambda re-checks the condition — protects against spurious wakeups.
    // ... proceed with work ...
}

// Thread 2 — notifier
void notifier() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        ready = true;
    }  // release lock before notifying — avoids immediate recontention
    cv.notify_one();  // wake one waiter; use notify_all() to wake all
}
```

**Spurious wakeups**: `cv.wait()` can return even when no one called `notify_one()`. This is permitted by POSIX and happens due to signal delivery and internal OS mechanisms. Always use the predicate form (second argument to `wait()`) which re-checks the condition and goes back to sleep if false.

**`notify_one()` vs `notify_all()`**: `notify_one()` wakes one arbitrary waiter — correct when any one waiter can handle the condition. `notify_all()` wakes every waiter — use when all waiters need to re-check (e.g., a broadcast "shutdown" event).

---

## 9. Producer-Consumer Problem

One or more producer threads generate work items; one or more consumer threads process them. The shared buffer must be accessed safely.

```cpp
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>

template<typename T>
class BoundedQueue {
    std::queue<T> q;
    std::mutex mtx;
    std::condition_variable not_full;
    std::condition_variable not_empty;
    const size_t capacity;
    bool done = false;

public:
    explicit BoundedQueue(size_t cap) : capacity(cap) {}

    void push(T item) {
        std::unique_lock<std::mutex> lock(mtx);
        not_full.wait(lock, [this] { return q.size() < capacity || done; });
        if (done) return;
        q.push(std::move(item));
        not_empty.notify_one();
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mtx);
        not_empty.wait(lock, [this] { return !q.empty() || done; });
        if (q.empty()) return std::nullopt;  // done and no items left
        T item = std::move(q.front());
        q.pop();
        not_full.notify_one();
        return item;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mtx);
        done = true;
        not_full.notify_all();
        not_empty.notify_all();
    }
};

// Usage
BoundedQueue<int> queue(100);  // buffer of 100 items

std::thread producer([&] {
    for (int i = 0; i < 1000; ++i) queue.push(i);
    queue.shutdown();
});

std::thread consumer([&] {
    while (auto item = queue.pop()) {
        process(*item);
    }
});
```

Key points:
- **Two condition variables**: `not_full` (producers wait here when buffer is at capacity) and `not_empty` (consumers wait here when buffer is empty). Using one condition variable would require `notify_all()` which wakes both producers and consumers unnecessarily.
- **Backpressure**: the bounded buffer is essential for production systems — an unbounded queue lets a slow consumer cause unlimited memory growth.
- **Shutdown**: producers signal completion via `shutdown()`, which broadcasts on both CVs so consumers drain remaining items and then exit.

---

## 10. Readers-Writers Problem

Many readers can safely read shared data simultaneously, but a writer needs exclusive access. Allowing concurrent readers while blocking them only for writes improves throughput for read-heavy workloads.

```cpp
#include <shared_mutex>

class ReadWriteCache {
    std::unordered_map<std::string, std::string> data;
    mutable std::shared_mutex rw_mtx;

public:
    // Multiple threads can read simultaneously
    std::optional<std::string> get(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(rw_mtx);  // shared (read) lock
        auto it = data.find(key);
        return it != data.end() ? std::optional{it->second} : std::nullopt;
    }

    // Writers get exclusive access — no readers or other writers during write
    void set(const std::string& key, const std::string& value) {
        std::unique_lock<std::shared_mutex> lock(rw_mtx);  // exclusive (write) lock
        data[key] = value;
    }
};
```

**Writer starvation problem**: if readers arrive continuously, a writer waiting for exclusive access can wait indefinitely because there is always at least one reader present. Solutions:
1. **Writer preference**: once a writer is waiting, no new readers are admitted. Starves readers instead.
2. **Fair FIFO queue**: requests are served in arrival order. The standard approach for database read-write locks.
3. **Retry with timeout**: writer takes a timed lock; readers back off if writer priority is signaled.

`std::shared_mutex` in C++ does not guarantee writer preference. For production use with write-heavy loads, measure whether a plain `std::mutex` outperforms due to simpler implementation.

---

## 11. Memory Ordering

The most misunderstood aspect of concurrent programming. CPUs and compilers reorder memory operations — and they are allowed to.

### CPU reordering

Modern out-of-order CPUs execute instructions in an order that may differ from the program order, provided the result is the same **as observed from the current core**. Other cores may observe a different order.

Example (x86 allows store-load reordering under certain conditions; ARM/POWER are even more aggressive):

```
Thread 1:            Thread 2:
x = 1;               y = 1;
r1 = y;              r2 = x;

// Possible outcome on ARM: r1 == 0 and r2 == 0
// Both threads' store (x=1, y=1) were reordered after their own load
```

This is valid because each CPU has a store buffer — stores go into the buffer before hitting the cache, so loads from other addresses can complete first.

### Compiler reordering

The compiler also reorders, eliminates, or hoists memory operations when it can prove (given the single-thread assumption) that the result is the same. `volatile` prevents the compiler from caching a variable in a register but does **not** prevent compiler or CPU reordering — it is not sufficient for synchronization.

```cpp
// WRONG: volatile does not prevent reordering
volatile bool ready = false;
volatile int data = 0;

// Thread 1:
data = 42;     // compiler/CPU may reorder this AFTER setting ready
ready = true;

// Thread 2:
while (!ready) {}
// data might still be 0 here even though ready is true
```

### Memory fences/barriers

A memory fence is an instruction that constrains the order in which memory operations become visible:

- **`std::atomic_thread_fence(std::memory_order_acquire)`**: no memory operation after this fence can be reordered before it
- **`std::atomic_thread_fence(std::memory_order_release)`**: no memory operation before this fence can be reordered after it
- **`std::atomic_thread_fence(std::memory_order_seq_cst)`**: full barrier — nothing crosses in either direction

```cpp
// Correct with explicit fences (usually prefer acquire/release on atomics instead)
std::atomic<bool> ready(false);
int data = 0;

// Thread 1 (writer):
data = 42;
std::atomic_thread_fence(std::memory_order_release);  // data write stays before this
ready.store(true, std::memory_order_relaxed);

// Thread 2 (reader):
while (!ready.load(std::memory_order_relaxed)) {}
std::atomic_thread_fence(std::memory_order_acquire);  // data read stays after this
assert(data == 42);  // guaranteed
```

### C++ memory order for atomics

| Memory order | Guarantees |
|---|---|
| `relaxed` | Atomicity only — no ordering relative to other variables |
| `acquire` | No subsequent reads/writes can move before this load |
| `release` | No preceding reads/writes can move after this store |
| `acq_rel` | Both acquire and release (for read-modify-write like `fetch_add`) |
| `seq_cst` | Full sequential consistency — most expensive, default |

A `release` store **synchronizes-with** an `acquire` load that sees the stored value. This is the fundamental inter-thread happens-before relationship.

---

## 12. Atomic Operations

An atomic operation executes as a single, indivisible unit — no other thread can observe it in a partially-complete state.

### Test-and-Set

Sets a flag to 1 and returns the old value atomically. The primitive behind most spinlocks. `std::atomic_flag::test_and_set()` in C++.

### Compare-and-Swap (CAS)

The most powerful primitive. Atomically: if `*ptr == expected`, set `*ptr = desired` and return `true`; else update `expected` to `*ptr` and return `false`.

```cpp
std::atomic<int> counter(0);

void increment_with_cas() {
    int expected = counter.load(std::memory_order_relaxed);
    while (!counter.compare_exchange_weak(
        expected,               // in/out: expected value (updated on failure)
        expected + 1,           // desired value
        std::memory_order_release,  // success order
        std::memory_order_relaxed   // failure order
    )) {
        // expected was updated to the current value — retry
    }
}
```

`compare_exchange_weak` can spuriously fail (returns false even when `*ptr == expected`) — acceptable in a loop. `compare_exchange_strong` never spuriously fails but is slower on some architectures (LL/SC-based like ARM).

### ABA problem

CAS checks value equality, not identity. If a value changes from A to B and back to A between your load and your CAS, the CAS succeeds but the state has changed. Solutions:
- **Tagged pointers**: pack a version counter into unused bits of a pointer (common in lock-free stacks)
- **Hazard pointers**: for memory reclamation in lock-free data structures
- **`std::atomic<std::shared_ptr>`** (C++20): handles memory lifetime automatically

### Using `std::atomic` correctly

```cpp
std::atomic<int> counter(0);

// Correct: atomic increment
counter.fetch_add(1, std::memory_order_relaxed);  // returns old value

// WRONG: this is a race condition — load and store are separate atomic operations
// but the read-modify-write is not
counter = counter.load() + 1;  // data race if another thread writes between load and store
```

---

## 13. Thread Pools

Creating a new OS thread for each incoming request is wrong at scale.

**Thread creation cost**: ~10–100 µs per thread (kernel TCB allocation, stack mapping, scheduler registration). At 10k requests/second, thread creation alone consumes ~1 second of CPU time per second.

**Thread memory cost**: each thread's stack defaults to 8 MB virtual (1 MB physical typically). 10k threads = 80 GB virtual address space, ~10 GB physical. On a 32 GB machine, that's a problem.

**Context switch overhead**: with 10k active threads, the scheduler churns through context switches that pollute caches and add ~5–10 µs latency per switch.

A thread pool pre-creates N threads (where N ≈ number of CPU cores for CPU-bound work, or 2–10x cores for I/O-bound work) and feeds them work via a shared queue.

```cpp
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>

class ThreadPool {
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mtx;
    std::condition_variable cv;
    bool stop = false;

public:
    explicit ThreadPool(size_t n_threads) {
        workers.reserve(n_threads);
        for (size_t i = 0; i < n_threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mtx);
                        cv.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();  // execute outside the lock
                }
            });
        }
    }

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<R> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(queue_mtx);
            tasks.emplace([task] { (*task)(); });
        }
        cv.notify_one();
        return result;
    }

    ~ThreadPool() {
        { std::lock_guard<std::mutex> lock(queue_mtx); stop = true; }
        cv.notify_all();
        for (auto& t : workers) t.join();
    }
};
```

**Sizing the pool**:
- **CPU-bound tasks**: N = number of logical cores (`std::thread::hardware_concurrency()`). More threads than cores means context-switching overhead with no throughput gain.
- **I/O-bound tasks**: N = cores × (1 + wait_time / cpu_time). If threads spend 90% of their time waiting on I/O, you need 10× cores threads to keep the CPUs busy. In practice, for pure I/O-bound work, async I/O with a small thread pool (equal to cores) outperforms a large thread pool by eliminating context switching entirely.

---

## 14. Async/Await vs Threads

### Cooperative vs preemptive scheduling

**Threads (preemptive)**: the OS can interrupt a thread at any time (timer interrupt → preemption). The thread does not need to cooperate. This means CPU-bound threads get fair shares automatically, but it also means the scheduler is involved in every context switch.

**Async/coroutines (cooperative)**: a coroutine runs until it explicitly yields (at an `await` point, typically at I/O). The runtime's event loop picks the next runnable coroutine. No kernel involvement per switch — just a user-space context switch (~100 ns vs ~5 µs for a thread switch).

### Green threads

Go goroutines, Erlang processes, and Java virtual threads (Project Loom) are **green threads**: they look like preemptive threads to the programmer but are multiplexed onto a smaller pool of OS threads by the runtime. The runtime has its own scheduler. Goroutines are preemptable by the Go runtime scheduler (since Go 1.14, via signal-based preemption at safe points).

### When to use async vs threads

| Workload | Best approach | Reason |
|---|---|---|
| I/O-bound (network, disk) | Async / coroutines | Thread-per-connection doesn't scale; async handles 100k concurrent I/Os with a few threads |
| CPU-bound computation | OS threads | Coroutines can't parallelize CPU work; you need real parallelism across cores |
| Mixed (I/O wait + CPU burst) | Green threads (Go, Loom) or async + thread pool | Best of both: coroutine scheduling for I/O + thread pool for CPU offload |

### Python asyncio gotcha

Python coroutines (`async def`) are cooperative. A CPU-bound `await` point that doesn't yield blocks the entire event loop. The correct pattern: run CPU-bound work in a thread pool using `loop.run_in_executor()`, which offloads to a `ThreadPoolExecutor` and returns an awaitable.

---

## 15. Runtime Comparison: Node.js vs Go vs Java

### Node.js — single-threaded event loop

Node.js runs JavaScript on a **single OS thread** with an event loop (libuv). All JavaScript is single-threaded — no two JavaScript callbacks run concurrently. I/O operations are dispatched to the OS (epoll/kqueue) and their callbacks fire when the OS signals completion.

```
┌─────────────────────────────────────────────┐
│                 Node.js Process             │
│                                             │
│  ┌──────────────────────────────────────┐   │
│  │         Event Loop (single thread)   │   │
│  │  1. timers (setTimeout/setInterval)  │   │
│  │  2. pending I/O callbacks           │   │
│  │  3. idle/prepare                    │   │
│  │  4. poll (block for new I/O events) │   │
│  │  5. check (setImmediate)            │   │
│  │  6. close callbacks                 │   │
│  └──────────────────────────────────────┘   │
│                                             │
│  ┌──────────────────────────────────────┐   │
│  │  libuv thread pool (default: 4 threads)  │
│  │  handles: fs, DNS, crypto, zlib      │   │
│  └──────────────────────────────────────┘   │
└─────────────────────────────────────────────┘
```

**No race conditions on JS state** — because only one callback runs at a time, you never need a mutex for in-memory JS objects.

**CPU-bound work blocks the event loop** — a 100 ms synchronous computation delays all other requests by 100 ms. Mitigation: `worker_threads` (separate V8 instances with shared `SharedArrayBuffer`).

**Throughput for I/O**: excellent. A Node.js server can handle tens of thousands of concurrent connections with one thread because open connections don't consume a thread while waiting for data.

### Go — goroutines and M:N scheduling

Go's runtime maintains a pool of OS threads (M) and schedules goroutines (G) onto them via a work-stealing scheduler with per-thread run queues (P = logical processor = one per core by default).

```
┌──────────────────────────────────────────────────────┐
│                     Go Runtime                       │
│                                                      │
│  P0 (core 0)          P1 (core 1)                    │
│  ┌──────────┐         ┌──────────┐                   │
│  │ M0 (OS   │         │ M1 (OS   │                   │
│  │ thread)  │         │ thread)  │                   │
│  │  G1 ←───│──steal──│── G7     │                   │
│  │  G2      │         │  G8      │                   │
│  │  G3      │         │  G9      │                   │
│  └──────────┘         └──────────┘                   │
│                                                      │
│  Global run queue: [G10, G11, G12, ...]              │
│  Goroutines blocked on I/O: [G4, G5, G6]            │
│  (parked on netpoller — OS thread not consumed)      │
└──────────────────────────────────────────────────────┘
```

Creating a goroutine: `go func() { ... }()` — ~1 µs, 2 KB initial stack. Run 1 million goroutines without issue.

When a goroutine blocks on I/O (network socket), the Go runtime parks it on the netpoller (using epoll/kqueue under the hood) and the OS thread is freed to run another goroutine. This is how Go achieves concurrency comparable to async without async syntax.

When a goroutine blocks on a **syscall** (e.g., `file.Read()`), the runtime detaches the OS thread (P is handed to a new or existing M) so CPU-bound goroutines don't stall.

### Java — OS threads → virtual threads (Project Loom, Java 21)

**Traditional Java threads**: each `Thread` is a 1:1 OS thread. Creating thousands is expensive. Java's answer has historically been reactive frameworks (Reactor, RxJava) or async callbacks — which make code hard to read and debug.

**Virtual threads (Java 21+)**: lightweight user-mode threads scheduled by the JVM, similar to goroutines. `Thread.ofVirtual().start(() -> { ... })`. Can create millions. Blocking operations (JDBC, file I/O, `Socket`) are automatically made non-blocking at the JVM level — the virtual thread is parked and the carrier OS thread is freed.

```java
// Old approach: ExecutorService with bounded thread pool
ExecutorService pool = Executors.newFixedThreadPool(200);
pool.submit(() -> { handleRequest(); });  // max 200 concurrent

// New approach: virtual threads (Java 21)
ExecutorService pool = Executors.newVirtualThreadPerTaskExecutor();
pool.submit(() -> { handleRequest(); });  // millions concurrent, no rewrite needed
```

**The key difference from async**: blocking code stays blocking in appearance. You don't rewrite with `CompletableFuture` chains or reactive types. The JVM handles the multiplexing. This is identical in philosophy to Go.

| Runtime | Concurrency model | Parallelism | Stack per unit | Good for |
|---|---|---|---|---|
| Node.js | Event loop (cooperative) | Single thread (JS) | N/A | I/O-heavy APIs |
| Go | M:N goroutines | All cores | 2–8 KB | I/O + CPU, simplicity |
| Java (threads) | OS threads | All cores | 1 MB | CPU-bound, established codebases |
| Java (virtual) | M:N (JVM) | All cores | ~KB | I/O-heavy, existing Java ecosystem |

---

## 16. Interview Questions

### What is a race condition vs a data race?

A **data race** has a precise definition: two threads access the same memory location concurrently, at least one access is a write, and the accesses are not synchronized by any primitive. Data races are undefined behavior in C++ and Java — the compiler and CPU can do anything.

A **race condition** is a broader, semantic concept: the program's correctness depends on the interleaving of thread operations. A race condition can exist without a data race (using a mutex to protect a counter but reading it at the wrong time) and can cause bugs that are not UB.

Example of race condition without data race:
```cpp
std::mutex mtx;
int balance = 100;

// Thread 1 (withdrawal):
{
    std::lock_guard lock(mtx);
    if (balance >= 50) {
        // preempted here by thread 2 — balance is still 100 because we hold the lock
        // Thread 2 cannot withdraw because it cannot acquire the lock.
        // This specific example is actually safe.
    }
}
```

TOCTOU (time-of-check to time-of-use) is a race condition without a data race: check that a file exists, then open it — between check and open, another process deletes the file. No shared memory is accessed unsafely, but the behavior is still incorrect.

---

### How do you prevent deadlock?

Four strategies, in order of preference:

1. **Lock ordering**: establish a total order on all mutexes. Every thread acquires them in that order. Eliminates circular wait. Works well when the set of mutexes is fixed.

2. **`std::scoped_lock` (C++17)**: acquires multiple mutexes atomically (with internal try-lock and backoff). No need to know the ordering at the call site.

3. **Lock-free design**: replace mutexes with `std::atomic` operations. Deadlock is impossible without locks. Correct lock-free code is hard to write; use well-tested concurrent data structures from libraries.

4. **Timeout with retry**: use `try_lock_for()`. If acquisition fails within a deadline, release held locks and retry. Requires handling retry logic; can cause livelock without random backoff.

Detection (for databases): periodically compute the lock-wait graph. A cycle means deadlock. Kill one transaction (the cheapest to abort) and retry it. PostgreSQL runs this check every `deadlock_timeout` (default 1 second).

---

### When to use a mutex vs an atomic?

**Use `std::atomic` when:**
- The operation is a single read, write, or read-modify-write on one variable
- You need a counter, flag, or pointer that threads read and write concurrently
- The operation fits naturally into CAS (fetch_add, compare_exchange)
- You need maximum performance and can reason about memory ordering

**Use `std::mutex` when:**
- You need to protect a compound operation across multiple variables that must be consistent with each other (e.g., update a map and increment a count as one unit)
- The critical section involves non-atomic operations (heap allocation, I/O, calling external code)
- Correctness is more important than peak performance in this section
- You need a condition variable (requires a mutex)

Rule of thumb: one variable → atomic. Multiple variables → mutex.

```cpp
// Correct with atomic: single counter
std::atomic<int> request_count(0);
request_count.fetch_add(1, std::memory_order_relaxed);

// Requires mutex: two variables that must stay consistent
std::mutex mtx;
int total_bytes = 0;
int total_requests = 0;

void record_request(int bytes) {
    std::lock_guard lock(mtx);
    total_bytes += bytes;      // must update both atomically
    total_requests += 1;       // can't use two atomics here without extra logic
}
```

---

### Why is Node.js single-threaded?

JavaScript was designed for browsers where concurrent access to the DOM would require locks around every property access — impractical. The single-threaded model eliminates an entire class of concurrency bugs: no mutex needed for JS state because only one callback runs at a time.

For I/O-bound server workloads (which cover most web APIs), single-threaded event-driven concurrency outperforms thread-per-request at high connection counts because there is no context-switching overhead and no thread memory overhead. nginx uses the same model.

The cost: CPU-bound work blocks the event loop and degrades latency for all concurrent users. Node.js addresses this with `worker_threads` (true OS threads with shared memory via `SharedArrayBuffer`) or by offloading CPU work to child processes.

The single-threaded model is also why Node.js's `async/await` is safe to use on plain JS objects without any synchronization — there is no interleaving within the event loop tick.

---

### Related topics

- [Operating Systems](../operating-systems/README.md) — threads, scheduling, context switching, and address space layout underpin everything here
- [JavaScript](../../01-languages/javascript/README.md) — Node.js event loop, Promise mechanics, async/await implementation
