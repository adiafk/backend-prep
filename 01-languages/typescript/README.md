# TypeScript

## Type System Fundamentals

```typescript
// Primitives
let name: string = "Alice"
let age: number = 30
let active: boolean = true
let nothing: null = null
let missing: undefined = undefined

// Union
type Status = 'pending' | 'active' | 'closed'
type ID = string | number

// Intersection
type Admin = User & { permissions: string[] }

// Literal types
type Direction = 'north' | 'south' | 'east' | 'west'
type Port = 80 | 443 | 8080

// Tuple
type Pair = [string, number]  // fixed length, typed positions
```

---

## Generics

```typescript
// Generic function
function first<T>(arr: T[]): T | undefined {
  return arr[0]
}

// Generic with constraint
function getProperty<T, K extends keyof T>(obj: T, key: K): T[K] {
  return obj[key]
}

// Generic interface
interface Repository<T> {
  findById(id: string): Promise<T | null>
  save(entity: T): Promise<T>
  delete(id: string): Promise<void>
}

// Generic class
class Stack<T> {
  private items: T[] = []
  push(item: T): void { this.items.push(item) }
  pop(): T | undefined { return this.items.pop() }
  peek(): T | undefined { return this.items[this.items.length - 1] }
}

// Conditional types
type NonNullable<T> = T extends null | undefined ? never : T
type ReturnType<T extends (...args: any) => any> = T extends (...args: any) => infer R ? R : never
```

---

## Utility Types

```typescript
interface User {
  id: string
  name: string
  email: string
  password: string
  role: 'admin' | 'user'
}

// Partial — all properties optional
type UserUpdate = Partial<User>

// Required — all properties required
type RequiredUser = Required<User>

// Pick — subset of properties
type UserPublic = Pick<User, 'id' | 'name' | 'role'>

// Omit — exclude properties
type UserWithoutPassword = Omit<User, 'password'>

// Readonly — immutable
type ImmutableUser = Readonly<User>

// Record — key-value map
type UserMap = Record<string, User>
type StatusMap = Record<Status, number>

// ReturnType — extract return type of a function
function getUser() { return { id: '1', name: 'Alice' } }
type UserReturn = ReturnType<typeof getUser>  // { id: string, name: string }

// Parameters — extract parameter tuple
function createUser(name: string, email: string): User { ... }
type CreateUserParams = Parameters<typeof createUser>  // [string, string]
```

---

## Type Narrowing

```typescript
// typeof
function process(input: string | number) {
  if (typeof input === 'string') {
    return input.toUpperCase()  // TypeScript knows it's string here
  }
  return input.toFixed(2)       // TypeScript knows it's number here
}

// instanceof
function formatError(err: unknown) {
  if (err instanceof Error) {
    return err.message  // TypeScript knows it's Error
  }
  return String(err)
}

// Discriminated unions — the most important pattern
type Shape =
  | { kind: 'circle';    radius: number }
  | { kind: 'rectangle'; width: number; height: number }

function area(shape: Shape): number {
  switch (shape.kind) {
    case 'circle':    return Math.PI * shape.radius ** 2
    case 'rectangle': return shape.width * shape.height
    // TypeScript errors if you forget a case — exhaustive check
  }
}

// Type predicate
function isUser(value: unknown): value is User {
  return typeof value === 'object' && value !== null && 'id' in value
}
```

---

## never and unknown

```typescript
// unknown: type-safe alternative to any
// Must narrow before using
function parseJSON(input: unknown) {
  if (typeof input === 'string') {
    return JSON.parse(input)  // now safe to use
  }
  throw new TypeError('Expected string')
}

// never: a value that can never exist
// Used for exhaustive checks:
function assertNever(value: never): never {
  throw new Error(`Unhandled case: ${JSON.stringify(value)}`)
}

function handleShape(shape: Shape): number {
  switch (shape.kind) {
    case 'circle': return Math.PI * shape.radius ** 2
    case 'rectangle': return shape.width * shape.height
    default: return assertNever(shape)  // TypeScript errors if a case is missing
  }
}
```

---

## Async Patterns

```typescript
// Basic async/await
async function fetchUser(id: string): Promise<User> {
  const res = await fetch(`/api/users/${id}`)
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return res.json() as Promise<User>
}

// Error handling
async function safeGetUser(id: string): Promise<User | null> {
  try {
    return await fetchUser(id)
  } catch (err) {
    console.error('Failed to fetch user:', err)
    return null
  }
}

// Parallel execution
const [user, orders] = await Promise.all([
  fetchUser(userId),
  fetchOrders(userId),
])

// Promise.allSettled — don't fail if one rejects
const results = await Promise.allSettled([fetchA(), fetchB(), fetchC()])
const successes = results
  .filter((r): r is PromiseFulfilledResult<Data> => r.status === 'fulfilled')
  .map(r => r.value)
```

---

## Interview Questions

**Q: What is the difference between `interface` and `type`?**
Both can describe object shapes. Key differences: `interface` can be extended with `extends` and merged (declaration merging — two `interface User` declarations merge). `type` can represent unions, intersections, tuples, primitives, and mapped types — things `interface` cannot. Prefer `interface` for objects that may be extended; `type` for unions, utility types, and complex compositions.

**Q: What does `unknown` solve that `any` doesn't?**
`any` disables type checking entirely — you can do anything with it without error. `unknown` is the type-safe alternative: you must narrow it to a specific type before using it. Use `unknown` when you don't know the type at compile time (user input, API responses, error objects in catch blocks). Use `any` only as a last resort escape hatch.

**Q: What is a discriminated union and why is it useful?**
A discriminated union is a union type where each member has a shared literal property (`kind`, `type`, `tag`) that uniquely identifies it. TypeScript narrows the type automatically in switch statements, providing exhaustiveness checking — if you add a new variant to the union without handling it, TypeScript errors at compile time.
