# JavaScript

## Event Loop

JavaScript is single-threaded. The event loop is how it handles asynchronous operations.

```
Call Stack → Web APIs → Callback Queue → Microtask Queue
```

Order of execution per "tick":
1. Run all synchronous code in the call stack until empty
2. Drain the **microtask queue** completely (Promises, queueMicrotask)
3. Take ONE task from the **task queue** (setTimeout, setInterval, I/O)
4. Repeat

```javascript
console.log('1')

setTimeout(() => console.log('4'), 0)  // task queue

Promise.resolve()
  .then(() => console.log('2'))         // microtask
  .then(() => console.log('3'))         // microtask

console.log('5')

// Output: 1, 5, 2, 3, 4
// Sync runs first (1, 5), then microtasks (2, 3), then tasks (4)
```

---

## Closures

A closure is a function that retains access to its enclosing scope after the outer function returns.

```javascript
function makeCounter(initial = 0) {
  let count = initial  // captured by the returned function

  return {
    increment() { count++ },
    decrement() { count-- },
    value() { return count },
  }
}

const counter = makeCounter(10)
counter.increment()
counter.increment()
console.log(counter.value())  // 12
// `count` is inaccessible from outside — only the methods can access it
```

Practical uses: encapsulation, memoization, partial application, module pattern.

Classic closure gotcha with `var` in loops:
```javascript
// Wrong — all callbacks share the same `i`
for (var i = 0; i < 3; i++) {
  setTimeout(() => console.log(i), 0)  // prints 3, 3, 3
}

// Fixed with let (block-scoped) or IIFE
for (let i = 0; i < 3; i++) {
  setTimeout(() => console.log(i), 0)  // prints 0, 1, 2
}
```

---

## Prototypal Inheritance

```javascript
const animal = {
  breathe() { return `${this.name} breathes` }
}

const dog = Object.create(animal)
dog.name = 'Rex'
dog.bark = function() { return 'Woof!' }

dog.breathe()  // "Rex breathes" — found on prototype
dog.bark()     // "Woof!" — own property

Object.getPrototypeOf(dog) === animal  // true
```

Classes are syntactic sugar over prototypes:
```javascript
class Animal {
  constructor(name) { this.name = name }
  breathe() { return `${this.name} breathes` }
}

class Dog extends Animal {
  bark() { return 'Woof!' }
}
```

---

## `this` Binding

```javascript
const user = {
  name: 'Alice',
  greet() { console.log(`Hello, ${this.name}`) },
  greetArrow: () => { console.log(`Hello, ${this.name}`) },  // lexical this
}

user.greet()           // "Hello, Alice"  — method call, this = user
const fn = user.greet
fn()                   // "Hello, undefined" — lost binding (strict: TypeError)
fn.call({ name: 'Bob' })  // "Hello, Bob"
user.greetArrow()      // "Hello, undefined" — arrow uses outer (module) this
```

Rules (highest to lowest priority):
1. `new` call — `this` = newly created object
2. Explicit: `.call(ctx)`, `.apply(ctx)`, `.bind(ctx)`
3. Method call: `obj.method()` — `this` = `obj`
4. Default: standalone call — `this` = global (or `undefined` in strict mode)
5. Arrow functions: inherit `this` from enclosing lexical scope, can't be rebound

---

## Promises

```javascript
// Creating
const p = new Promise((resolve, reject) => {
  setTimeout(() => resolve('done'), 1000)
})

// Chaining — each .then returns a new Promise
fetch('/api/user')
  .then(res => res.json())
  .then(user => user.name)
  .catch(err => { console.error(err); return null })
  .finally(() => hideSpinner())

// Combinators
Promise.all([p1, p2, p3])          // all must resolve; rejects on first rejection
Promise.allSettled([p1, p2, p3])   // wait for all; never rejects
Promise.race([p1, p2, p3])         // resolves/rejects with the first to settle
Promise.any([p1, p2, p3])          // resolves with first to succeed; rejects if all fail
```

---

## Prototype Chain and `class`

```javascript
function Person(name) {
  this.name = name
}
Person.prototype.greet = function() { return `Hi, I'm ${this.name}` }

const alice = new Person('Alice')
alice.greet()  // found on Person.prototype, not on alice itself

// Property lookup chain:
// alice → Person.prototype → Object.prototype → null
```

---

## Common Interview Questions

**Q: What is the difference between `==` and `===`?**
`===` (strict equality): no type coercion. `1 === '1'` is false. Always use `===`. `==` (loose equality) applies type coercion: `1 == '1'` is true, `null == undefined` is true, `[] == false` is true. The coercion rules are counterintuitive — avoid `==` except for intentional null checks (`x == null` catches both null and undefined).

**Q: Explain the difference between `var`, `let`, and `const`.**
`var`: function-scoped, hoisted to the top of the function (declaration only, not initialization — accessing before initialization gives `undefined`). `let`: block-scoped, not accessible before declaration (temporal dead zone). `const`: block-scoped, must be initialized, cannot be reassigned (but the value can be mutated if it's an object/array). Use `const` by default, `let` when you need to reassign, never `var`.

**Q: What is event delegation?**
Instead of attaching event listeners to every child element, attach one listener to a parent and use `event.target` to determine which child was clicked. Efficient for dynamic lists (elements added after page load automatically work) and reduces memory usage (one listener vs hundreds).
```javascript
document.getElementById('list').addEventListener('click', (e) => {
  if (e.target.matches('li.item')) {
    handleItemClick(e.target.dataset.id)
  }
})
```
