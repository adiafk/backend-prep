# 07 — TypeScript & Node.js

## TypeScript

Know:

- interfaces vs type aliases
- unions/intersections
- generics
- narrowing
- type guards
- utility types
- `unknown` vs `any`
- `never`
- readonly
- literal types
- function types
- modules
- ESM/CommonJS
- tsconfig
- runtime validation

Important:

> TypeScript types are removed at runtime.

Therefore untrusted input still needs runtime validation.

### Example

```ts
function isString(value: unknown): value is string {
  return typeof value === 'string';
}
```

---

## Node.js event loop

Node.js uses an event-driven architecture and non-blocking I/O so one process can handle many concurrent I/O operations.

Conceptually:

```text
JavaScript execution
       |
   async I/O
       |
   event loop
       |
callbacks/promises
```

Know:

- call stack
- event loop
- microtasks
- timers
- I/O callbacks
- Promises
- async/await
- streams
- buffers
- worker threads
- process lifecycle
- memory/GC

### Interview question

**Why can Node.js handle many concurrent requests despite JavaScript execution being single-threaded?**

Because I/O is delegated to the runtime/OS and callbacks/promises are resumed when operations complete. CPU-heavy synchronous work still blocks the event loop.

---

## NestJS / Express

Know the common backend layers:

```text
Controller / Route
       |
Service / Business Logic
       |
Repository / Data Access
       |
Database / External Service
```

Also know middleware, guards, interceptors, validation, dependency injection, exception handling, and request lifecycle at a practical level.

### Architecture interview question

**Where should business logic live?**

Avoid putting significant domain behavior directly into controllers. Controllers should coordinate transport concerns; services/domain components should contain business rules; data-access components should isolate persistence concerns.

---

## Async reliability

Know how to handle:

- timeouts
- rejected promises
- cancellation/abort signals
- concurrency limits
- retries
- partial failures
- graceful shutdown

For external APIs, always consider a timeout rather than allowing an unlimited pending request.

---

## Backend performance

Common bottlenecks:

- database queries
- missing/wrong indexes
- network latency
- external APIs
- serialization
- CPU-heavy code
- event-loop blocking
- cache misses
- excessive logging

Measure before optimizing.

### Resume connection

Your stack is strongly Node.js/TypeScript/NestJS-oriented, so be prepared to explain production API architecture, async execution, validation, error handling, database access, caching, queues, and observability.
