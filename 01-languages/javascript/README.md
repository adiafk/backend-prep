# JavaScript

Related: [TypeScript](../typescript/README.md) · [TypeScript/Node.js deep-dive](../../07-typescript-nodejs.md) · [Concurrency fundamentals](../../00-foundations/concurrency/README.md)

---

## Event Loop — In Depth

JavaScript is single-threaded. Async operations are handled by handing work off to the host environment (browser/Node.js), which puts callbacks back on a queue when ready. The event loop's job is to decide which callback runs next.

### The queues

| Queue | What goes here | Priority |
|---|---|---|
| Call stack | Synchronous code executing right now | — (runs to completion) |
| Microtask queue | Promise `.then`/`.catch`, `queueMicrotask`, `MutationObserver` | Highest — drained completely before next macrotask |
| Macrotask queue (task queue) | `setTimeout`, `setInterval`, `setImmediate` (Node), I/O callbacks | One task per loop iteration |

### The algorithm (per iteration)

1. Execute the current macrotask to completion (or run the initial script).
2. Drain the microtask queue completely — every item, including newly queued microtasks.
3. If needed: render (browser only).
4. Pick the next macrotask from the queue and go to step 1.

### Execution order puzzle — memorize this pattern

```javascript
console.log('A')                            // sync

setTimeout(() => console.log('B'), 0)       // macrotask

Promise.resolve()
  .then(() => {
    console.log('C')                        // microtask 1
    return Promise.resolve()
  })
  .then(() => console.log('D'))            // microtask 2 (queued after C runs)

queueMicrotask(() => console.log('E'))      // microtask 3

console.log('F')                            // sync

// Output: A F C E D B
// Explanation:
//   Sync runs: A, F
//   Microtask queue drained: C (queued first), E (queued second),
//     then D (queued when the inner Promise.resolve resolved)
//   Macrotask: B
```

### Harder puzzle — nested Promises

```javascript
Promise.resolve()
  .then(() => {
    console.log('1')
    Promise.resolve().then(() => console.log('2'))  // inner microtask
  })
  .then(() => console.log('3'))

// Output: 1 2 3
// The inner Promise.resolve().then queues '2' during the first microtask.
// '2' runs before '3' because the microtask queue is fully drained at each step.
// If this were setTimeout instead of Promise.resolve, '3' would come before '2'.
```

### Node.js specifics

Node has additional phases (libuv event loop): timers → I/O callbacks → idle/prepare → poll → check (`setImmediate`) → close callbacks. `process.nextTick` drains before the microtask queue, making it even higher priority than Promises — a common gotcha.

```javascript
// Node.js priority order:
process.nextTick(() => console.log('nextTick'))   // runs before any Promise
Promise.resolve().then(() => console.log('Promise'))
setTimeout(() => console.log('setTimeout'), 0)
// Output: nextTick, Promise, setTimeout
```

---

## Closures and Lexical Scope

A closure is created when a function captures variables from its enclosing scope. The function retains a live reference to those variables — not a copy.

### Module pattern (encapsulation)

```javascript
function createCache(maxSize = 100) {
  const store = new Map()  // private — not accessible from outside

  return {
    get(key) {
      return store.get(key)
    },
    set(key, value) {
      if (store.size >= maxSize) {
        // evict oldest entry
        store.delete(store.keys().next().value)
      }
      store.set(key, value)
    },
    size() {
      return store.size
    },
  }
}

const cache = createCache(50)
cache.set('user:1', { name: 'Alice' })
cache.get('user:1')   // { name: 'Alice' }
cache.store           // undefined — truly private
```

### Memoization

```javascript
function memoize(fn) {
  const cache = new Map()  // closed over by the returned function
  return function(...args) {
    const key = JSON.stringify(args)
    if (cache.has(key)) return cache.get(key)
    const result = fn.apply(this, args)
    cache.set(key, result)
    return result
  }
}

const expensiveCompute = memoize((n) => {
  // pretend this is slow
  return n * n
})
expensiveCompute(4)  // computed
expensiveCompute(4)  // cached
```

### Classic var/closure gotcha

```javascript
// Bug: var is function-scoped, all callbacks close over the same `i`
for (var i = 0; i < 3; i++) {
  setTimeout(() => console.log(i), 0)
}
// Output: 3, 3, 3  (loop finishes before any callback runs)

// Fix 1: use let (block-scoped — each iteration gets its own binding)
for (let i = 0; i < 3; i++) {
  setTimeout(() => console.log(i), 0)
}
// Output: 0, 1, 2

// Fix 2: IIFE to capture value at each iteration (pre-ES6 style)
for (var i = 0; i < 3; i++) {
  (function(j) {
    setTimeout(() => console.log(j), 0)
  })(i)
}
```

### Memory leak via closure

```javascript
// Danger: the closure captures `largeData` even though only `id` is needed
function processRequest(request) {
  const largeData = request.body  // could be megabytes
  const id = request.id

  return function handler() {
    console.log('Processing:', id)
    // largeData is never used here but stays in memory as long as handler exists
  }
}

// Fix: only capture what you need
function processRequest(request) {
  const id = request.id  // extract the small value
  // largeData goes out of scope and can be GC'd

  return function handler() {
    console.log('Processing:', id)
  }
}
```

---

## Prototype Chain

Every object has an internal `[[Prototype]]` link. Property lookup walks the chain until it finds the property or reaches `null`.

```javascript
const animal = {
  breathe() { return `${this.name} breathes` },
}

const dog = Object.create(animal)  // dog's [[Prototype]] = animal
dog.name = 'Rex'
dog.bark = function() { return 'Woof!' }

dog.bark()     // 'Woof!' — own property, found immediately
dog.breathe()  // 'Rex breathes' — not on dog, found on animal via prototype chain
dog.toString() // found on Object.prototype, two hops up

Object.getPrototypeOf(dog) === animal   // true
Object.getPrototypeOf(animal) === Object.prototype  // true
Object.getPrototypeOf(Object.prototype) === null    // end of chain
```

### `__proto__` vs `prototype`

```javascript
// `prototype` — exists only on functions; it's what new instances inherit from
function Dog(name) { this.name = name }
Dog.prototype.bark = function() { return 'Woof!' }

const rex = new Dog('Rex')
// rex.__proto__ === Dog.prototype   (the instance's [[Prototype]])
// Dog.prototype !== Dog.__proto__   (Dog.__proto__ === Function.prototype)

// Rule: fn.prototype is the [[Prototype]] of instances created with new fn()
// __proto__ is the non-standard accessor for [[Prototype]] on any object
```

### Class syntax as sugar

`class` compiles to exactly the same prototype wiring — it's not a different object model.

```javascript
class Animal {
  constructor(name) { this.name = name }
  breathe() { return `${this.name} breathes` }  // goes on Animal.prototype
}

class Dog extends Animal {
  bark() { return 'Woof!' }  // goes on Dog.prototype
}

const rex = new Dog('Rex')
// Chain: rex → Dog.prototype → Animal.prototype → Object.prototype → null
rex.breathe()  // found on Animal.prototype

// class is syntactic sugar — typeof Animal === 'function'
Object.getPrototypeOf(Dog) === Animal  // true (static inheritance)
Object.getPrototypeOf(Dog.prototype) === Animal.prototype  // true (instance inheritance)
```

---

## Generators

Generators are functions that can pause and resume. They implement the iterator protocol.

```javascript
function* range(start, end, step = 1) {
  for (let i = start; i < end; i += step) {
    yield i  // pause here, return i as the yielded value
  }
}

const gen = range(0, 10, 2)
gen.next()  // { value: 0, done: false }
gen.next()  // { value: 2, done: false }
// ...
gen.next()  // { value: undefined, done: true }

// Generators are iterable — work with for...of, spread, destructuring
[...range(0, 5)]         // [0, 1, 2, 3, 4]
const [a, b] = range(10, 20)  // a=10, b=11

// Infinite sequence (lazy — only computes what you consume)
function* naturals() {
  let n = 0
  while (true) yield n++
}

function take(n, iter) {
  const result = []
  for (const val of iter) {
    result.push(val)
    if (result.length >= n) break
  }
  return result
}

take(5, naturals())  // [0, 1, 2, 3, 4]
```

### Two-way communication

```javascript
function* accumulator() {
  let total = 0
  while (true) {
    const input = yield total  // yield sends total out; receives next value via next(val)
    total += input
  }
}

const acc = accumulator()
acc.next()       // { value: 0, done: false } — start the generator
acc.next(10)     // { value: 10, done: false }
acc.next(5)      // { value: 15, done: false }
```

---

## Symbol and Well-Known Symbols

`Symbol` creates a unique, non-string key. Two symbols are never equal even if created with the same description.

```javascript
const id = Symbol('id')
const obj = { [id]: 42, name: 'Alice' }
obj[id]         // 42
obj['id']       // undefined — Symbol keys are not strings

// Symbols don't appear in for...in or Object.keys()
Object.keys(obj)         // ['name']
Object.getOwnPropertySymbols(obj)  // [Symbol(id)]
```

### Well-known symbols — customize built-in behavior

```javascript
class Range {
  constructor(start, end) {
    this.start = start
    this.end = end
  }

  // Make instances iterable with for...of
  [Symbol.iterator]() {
    let current = this.start
    const end = this.end
    return {
      next() {
        return current <= end
          ? { value: current++, done: false }
          : { value: undefined, done: true }
      }
    }
  }

  // Customize instanceof behavior
  static [Symbol.hasInstance](value) {
    return typeof value === 'object' && 'start' in value && 'end' in value
  }
}

[...new Range(1, 5)]  // [1, 2, 3, 4, 5]

// Other well-known symbols:
// Symbol.toPrimitive — control type coercion
// Symbol.toStringTag — customize Object.prototype.toString output
// Symbol.asyncIterator — enable for-await-of
```

---

## WeakMap and WeakRef

`WeakMap` holds keys weakly — the GC can collect the key object when nothing else references it, and the entry disappears. `WeakSet` is the same for set membership. Neither is enumerable.

```javascript
// Private data per instance without a closure (WeakMap pattern)
const _private = new WeakMap()

class Connection {
  constructor(url) {
    _private.set(this, { socket: null, url })
  }

  connect() {
    const state = _private.get(this)
    state.socket = openSocket(state.url)
  }
}
// When a Connection instance is GC'd, its entry in _private is automatically removed.

// DOM metadata without preventing GC
const elementMeta = new WeakMap()
function trackElement(el, data) {
  elementMeta.set(el, data)  // el can still be GC'd when removed from DOM
}
```

### WeakRef — hold a reference without preventing GC

```javascript
// Memory-sensitive cache: values can be reclaimed under memory pressure
class WeakCache {
  #cache = new Map()

  set(key, value) {
    this.#cache.set(key, new WeakRef(value))
  }

  get(key) {
    const ref = this.#cache.get(key)
    if (!ref) return undefined
    const value = ref.deref()
    if (value === undefined) {
      this.#cache.delete(key)  // clean up dead entry
      return undefined
    }
    return value
  }
}
```

> Interview note: Don't use WeakMap for most caching. Use a real LRU cache with Map. WeakMap is specifically for attaching metadata to objects whose lifetime you don't control (DOM nodes, third-party instances).

---

## Promise Internals and Combinators

### How `.then` chaining works

Each `.then` returns a **new** Promise. The new promise resolves with whatever the callback returns. If the callback returns a Promise, the chain waits for it.

```javascript
const p = Promise.resolve(1)
  .then(x => x + 1)           // returns 2
  .then(x => Promise.resolve(x * 10))  // returns a Promise; chain waits
  .then(x => { throw new Error('oops') })  // rejection propagates
  .catch(err => 'recovered')  // catches; returns 'recovered'
  .then(x => console.log(x)) // 'recovered'
```

### Combinators — decision table

| Method | Resolves when | Rejects when | Use case |
|---|---|---|---|
| `Promise.all` | All resolve | Any rejects | All required — fail fast |
| `Promise.allSettled` | All settle (either way) | Never | Parallel tasks, partial success ok |
| `Promise.race` | First to settle (resolve or reject) | First to settle if it rejects | Timeout pattern |
| `Promise.any` | First to resolve | All reject (AggregateError) | First success matters, others are fallbacks |

```javascript
// Timeout pattern with Promise.race
function withTimeout(promise, ms) {
  const timeout = new Promise((_, reject) =>
    setTimeout(() => reject(new Error(`Timed out after ${ms}ms`)), ms)
  )
  return Promise.race([promise, timeout])
}

// Fallback with Promise.any
const data = await Promise.any([
  fetchFromPrimary(),
  fetchFromReplica(),
  fetchFromCache(),
])
// Uses whichever responds first successfully

// Collect partial results with allSettled
const results = await Promise.allSettled([fetchA(), fetchB(), fetchC()])
const values = results
  .filter(r => r.status === 'fulfilled')
  .map(r => r.value)
const errors = results
  .filter(r => r.status === 'rejected')
  .map(r => r.reason)
```

---

## async/await Pitfalls

### Missing await — fire and forget accidentally

```javascript
// Bug: the async function is called but its result is not awaited
async function saveUser(user) {
  await db.insert(user)
  sendWelcomeEmail(user)  // async but no await — runs in parallel, errors are lost
}

// Fix: await if you need it to complete or handle its error
async function saveUser(user) {
  await db.insert(user)
  await sendWelcomeEmail(user)  // errors now propagate
}
```

### Sequential vs parallel

```javascript
// Sequential (slow): each awaits the previous one — total time = t1 + t2 + t3
async function sequential() {
  const user = await fetchUser(id)       // wait for this...
  const orders = await fetchOrders(id)   // then wait for this...
  const prefs = await fetchPrefs(id)     // then this
  return { user, orders, prefs }
}

// Parallel (fast): all three start simultaneously — total time = max(t1, t2, t3)
async function parallel() {
  const [user, orders, prefs] = await Promise.all([
    fetchUser(id),
    fetchOrders(id),
    fetchPrefs(id),
  ])
  return { user, orders, prefs }
}
// Use Promise.all when operations are independent. Sequential only when each depends on the previous.
```

### Swallowed errors

```javascript
// Bug: empty catch silently absorbs the error
async function loadData() {
  try {
    return await fetchData()
  } catch (err) {
    // nothing — caller gets undefined, no indication of failure
  }
}

// Fix: always either handle the error meaningfully or re-throw
async function loadData() {
  try {
    return await fetchData()
  } catch (err) {
    logger.error('loadData failed', { err })
    throw err  // propagate so the caller knows
  }
}

// Also: errors in Promise chains without .catch are unhandled rejections
fetchData()
  .then(process)
  // forgot .catch — Node.js will emit 'unhandledRejection'
```

### async in forEach — doesn't work as expected

```javascript
// Bug: forEach doesn't await — all callbacks fire, forEach returns immediately
const ids = [1, 2, 3]
ids.forEach(async (id) => {
  await db.delete(id)  // these run concurrently but forEach doesn't wait
})
// Code after this line runs before any deletion completes

// Fix: use Promise.all with map for parallel, or for...of for sequential
await Promise.all(ids.map(id => db.delete(id)))  // parallel
for (const id of ids) { await db.delete(id) }     // sequential
```

---

## Memory Leaks

### Event listeners not removed

```javascript
// Leak: every call adds another listener, none are removed
function setupSearch() {
  const input = document.getElementById('search')
  input.addEventListener('keyup', (e) => {
    search(e.target.value)
  })
}
// If setupSearch() is called multiple times (re-render), listeners accumulate

// Fix: store the reference and remove it, or use AbortController
function setupSearch() {
  const controller = new AbortController()
  const input = document.getElementById('search')
  input.addEventListener('keyup', (e) => search(e.target.value), {
    signal: controller.signal  // all listeners with this signal are removed at once
  })
  return () => controller.abort()  // call to clean up
}
```

### Uncleared timers and intervals

```javascript
// Leak: interval holds a reference to everything in its closure
class DataPoller {
  start() {
    this.intervalId = setInterval(() => this.fetchData(), 5000)
  }

  stop() {
    clearInterval(this.intervalId)  // must call this when done
  }
}
// If stop() is never called (component unmounted, object "discarded"),
// the interval keeps firing and the closure keeps the object alive.
```

### Closures capturing large objects

```javascript
// Leak: the event handler closes over the entire `app` object
function init(app) {
  document.addEventListener('click', () => {
    app.handleClick()  // app cannot be GC'd as long as the listener exists
  })
}

// Fix: extract only the method reference
function init(app) {
  const handleClick = app.handleClick.bind(app)
  document.addEventListener('click', handleClick)
  return () => document.removeEventListener('click', handleClick)
}
```

---

## `this` Binding Rules

Five rules, in order of precedence:

```javascript
// 1. new binding — this = the new object
function User(name) { this.name = name }
const alice = new User('Alice')  // this = alice

// 2. Explicit binding — call, apply, bind
function greet(greeting) { return `${greeting}, ${this.name}` }
greet.call({ name: 'Bob' }, 'Hi')    // 'Hi, Bob'
greet.apply({ name: 'Bob' }, ['Hi']) // same
const greetBob = greet.bind({ name: 'Bob' })
greetBob('Hi')                       // 'Hi, Bob'

// 3. Implicit binding — method call
const obj = { name: 'Carol', greet() { return this.name } }
obj.greet()  // 'Carol'

// 4. Default binding — standalone call
function whoAmI() { return this }
whoAmI()  // global object (or undefined in strict mode)

// 5. Arrow functions — lexical this, cannot be rebound
class Timer {
  constructor() { this.ticks = 0 }

  start() {
    // Arrow: `this` is the Timer instance (captured at definition time)
    setInterval(() => { this.ticks++ }, 1000)
    // Regular function would lose `this` here
  }
}

// Lost binding trap
const obj2 = { name: 'Dave', greet() { return this.name } }
const fn = obj2.greet    // detached — no longer a method call
fn()                     // undefined (or TypeError in strict mode)
const bound = obj2.greet.bind(obj2)
bound()                  // 'Dave'
```

---

## Temporal Dead Zone (TDZ) and Hoisting

```javascript
// var: hoisted and initialized to undefined
console.log(x)  // undefined (not a ReferenceError)
var x = 5

// let/const: hoisted but NOT initialized — accessing before declaration = TDZ error
console.log(y)  // ReferenceError: Cannot access 'y' before initialization
let y = 5

// Function declarations: fully hoisted (name AND body)
hello()  // 'Hello!' — works
function hello() { return 'Hello!' }

// Function expressions: only the variable is hoisted (as undefined)
world()  // TypeError: world is not a function
var world = function() { return 'World!' }
```

### TDZ in classes

```javascript
// Classes are also in TDZ before their declaration
const p = new Person()  // ReferenceError
class Person {}
```

---

## `==` Coercion Surprises

```javascript
// These all return true with ==
null == undefined   // true (only these two are == to each other, nothing else)
0 == ''             // true ('' converts to 0)
0 == '0'            // true
0 == false          // true (false converts to 0)
'' == false         // true
[] == false         // true ([] → '' → 0; false → 0)
[] == ![]           // true (bizarre)
null == 0           // false (null only == undefined)
NaN == NaN          // false (NaN is never equal to anything, including itself)

// Rule: always use ===
// Only acceptable use of ==: null check that also catches undefined
if (x == null) { ... }  // true for both null and undefined
```

### typeof quirks

```javascript
typeof null        // 'object' — historic bug, cannot be fixed
typeof undefined   // 'undefined'
typeof []          // 'object'
typeof {}          // 'object'
typeof function(){}  // 'function'
typeof NaN         // 'number'

// To check for null specifically:
value === null

// To check for array:
Array.isArray(value)
```

---

## Common Interview Questions

**Q: What is the output? (execution order)**
```javascript
async function main() {
  console.log('1')
  await Promise.resolve()
  console.log('2')
}
console.log('3')
main()
console.log('4')
// Output: 3, 1, 4, 2
// 3 runs sync. main() starts sync (logs 1), hits await, suspends.
// 4 runs sync. Microtask queue: logs 2.
```

**Q: Difference between `==` and `===`**
`===` never coerces — different types always return false. `==` applies an abstract equality algorithm with type coercion. Use `===` everywhere; the only justified `==` is `x == null` to catch both null and undefined.

**Q: What is `var` hoisting vs `let` TDZ?**
All three (`var`, `let`, `const`) are hoisted to the top of their scope. `var` is initialized to `undefined` immediately, so accessing it before its line returns `undefined`. `let` and `const` exist in a "temporal dead zone" from the top of the block until the declaration — accessing them there throws a `ReferenceError`.

**Q: How does `Promise.all` differ from `Promise.allSettled`?**
`Promise.all` short-circuits on the first rejection — one failure rejects the whole result. `Promise.allSettled` always waits for every promise and returns an array of `{ status, value/reason }` objects. Use `all` when all results are required and any failure means aborting. Use `allSettled` when you want partial success and need to handle each outcome individually.

**Q: What is a memory leak in JavaScript and name three causes?**
A memory leak is memory that is allocated but never released because the GC still sees a live reference. Common causes: (1) event listeners attached but never removed (DOM elements can't be GC'd), (2) closures that capture large objects that are no longer needed, (3) global variables accumulating data over time, (4) uncleaned timers/intervals whose callbacks reference objects.

**Q: Why do arrow functions not have their own `this`?**
Arrow functions capture `this` from the enclosing lexical scope at definition time. They do not get their own `this` binding regardless of how they are called — `call`/`apply`/`bind` have no effect on their `this`. This makes them suitable for callbacks inside methods (where you want `this` to refer to the class instance), but unsuitable as methods themselves or constructors.
