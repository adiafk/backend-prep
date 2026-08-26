# TypeScript

Related: [JavaScript](../javascript/README.md) · [TypeScript/Node.js deep-dive](../../07-typescript-nodejs.md)

---

## Type Inference and When It Fails

TypeScript infers types from context — most of the time you don't need annotations.

```typescript
// Inferred correctly
const count = 0                    // number
const name = 'Alice'               // string
const nums = [1, 2, 3]            // number[]
const user = { id: 1, name: 'A' } // { id: number, name: string }

// Inference widens literals to primitives
const x = 'hello'         // type: string (let — can be reassigned)
const y = 'hello' as const  // type: 'hello' (literal)

// Inference fails — you get `any` without realizing it
const parsed = JSON.parse(input)  // any — JSON.parse always returns any
async function load() {
  const res = await fetch('/api')
  return res.json()  // any — must annotate the return type explicitly
}

// Fix: annotate where inference can't help
async function load(): Promise<User[]> {
  const res = await fetch('/api/users')
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return res.json()
}
```

### Contextual typing

```typescript
// TypeScript infers parameter types from context
const nums = [1, 2, 3]
nums.map(n => n.toFixed(2))  // n is inferred as number from the array type

// Without context, parameters default to any (error with noImplicitAny)
const fn = (x) => x.toUpperCase()  // error: x implicitly has type 'any'
```

---

## Structural Typing (Duck Typing)

TypeScript's type system is structural, not nominal. A type is compatible if it has the required shape — the name doesn't matter.

```typescript
interface Point {
  x: number
  y: number
}

function printPoint(p: Point) {
  console.log(`(${p.x}, ${p.y})`)
}

// This object satisfies Point structurally — TS accepts it
const coord = { x: 3, y: 4, label: 'origin' }
printPoint(coord)  // OK — extra properties are allowed when passed via variable

// But object literals trigger excess property checking (deliberate strictness)
printPoint({ x: 3, y: 4, label: 'origin' })  // Error: 'label' is not in Point

// Why: passing an object literal is almost always a mistake if you include extras.
// Assigning to a variable first and then passing is an explicit "I know what I'm doing."
```

### Structural compatibility in practice

```typescript
class Dog {
  name: string
  constructor(name: string) { this.name = name }
  bark() { return 'Woof' }
}

class Cat {
  name: string
  constructor(name: string) { this.name = name }
  bark() { return 'Meow' }  // same shape as Dog.bark
}

function makeNoise(animal: Dog) {
  return animal.bark()
}

makeNoise(new Cat('Whiskers'))  // OK — Cat is structurally compatible with Dog
// This surprises people coming from Java/C# where this would be a compile error.
```

---

## interface vs type alias

Both can describe object shapes. The differences matter in specific cases.

```typescript
// interface: supports declaration merging
interface Window {
  title: string
}
interface Window {
  scrollY: number  // merged — both declarations combine
}
// Useful for module augmentation; dangerous when unintentional.

// type: no merging — second declaration of the same name is an error
type Config = { host: string }
type Config = { port: number }  // Error: Duplicate identifier 'Config'

// type: can represent anything — unions, intersections, primitives, tuples
type ID = string | number
type Nullable<T> = T | null
type Pair = [string, number]
type EventName = `on${string}`  // template literal type

// interface: cannot express unions at the top level
interface ID = string | number  // syntax error

// Mapped types: only work with type (not as interface syntax)
type Optional<T> = { [K in keyof T]?: T[K] }

// Declaration merging is only for interface
// Use interface for: public API shapes, objects that consumers might extend
// Use type for: unions, utility types, anything that isn't a plain object shape
```

---

## Generics with Constraints

```typescript
// Unconstrained — T can be anything
function identity<T>(value: T): T {
  return value
}

// Constrained with extends — T must have these properties
function getProperty<T, K extends keyof T>(obj: T, key: K): T[K] {
  return obj[key]
}
getProperty({ name: 'Alice', age: 30 }, 'name')  // 'Alice', type: string
getProperty({ name: 'Alice', age: 30 }, 'phone') // Error: not a key

// Multiple constraints
function merge<T extends object, U extends object>(a: T, b: U): T & U {
  return { ...a, ...b }
}

// Default type parameters
interface Response<T = unknown> {
  data: T
  status: number
  message: string
}
const r: Response = { data: 'anything', status: 200, message: 'OK' }
const r2: Response<User> = { data: { id: '1', name: 'Alice' }, status: 200, message: 'OK' }

// Constrained to have a specific method
function first<T extends { length: number }>(collection: T): number {
  return collection.length  // safe because we know it has .length
}
first('hello')   // 5
first([1, 2, 3]) // 3
```

---

## Conditional Types

`T extends U ? X : Y` — evaluated at the type level like a ternary.

```typescript
// Basic: extract non-nullable
type NonNullable<T> = T extends null | undefined ? never : T
type A = NonNullable<string | null>  // string

// Distributive conditional types — when T is a union, it distributes
type ToString<T> = T extends number ? string : T
type B = ToString<number | boolean>  // string | boolean

// `infer` — extract a type from inside another type
type ReturnType<T extends (...args: any) => any> =
  T extends (...args: any) => infer R ? R : never

type Unwrap<T> = T extends Promise<infer U> ? U : T
type C = Unwrap<Promise<string>>  // string
type D = Unwrap<number>           // number

// Real example: flatten one level of arrays
type Flatten<T> = T extends Array<infer Item> ? Item : T
type E = Flatten<string[]>     // string
type F = Flatten<number[][]>   // number[]  (only one level)

// Conditional types in generic functions
function process<T extends string | number>(
  value: T
): T extends string ? string[] : number {
  if (typeof value === 'string') {
    return value.split(',') as any
  }
  return (value * 2) as any
}
```

---

## Mapped Types

Transform every property in a type programmatically.

```typescript
// The four standard modifiers: ?, readonly, -?, -readonly
type Partial<T>  = { [K in keyof T]?: T[K] }        // all optional
type Required<T> = { [K in keyof T]-?: T[K] }        // remove optional
type Readonly<T> = { readonly [K in keyof T]: T[K] } // all readonly
type Mutable<T>  = { -readonly [K in keyof T]: T[K] } // remove readonly

// Record: keys from a union, values of type V
type Record<K extends keyof any, V> = { [P in K]: V }
type StatusCount = Record<'active' | 'inactive' | 'pending', number>
// { active: number; inactive: number; pending: number }

// Pick and Omit: filter keys
type Pick<T, K extends keyof T> = { [P in K]: T[P] }
type Omit<T, K extends keyof any> = Pick<T, Exclude<keyof T, K>>

// Remap keys with `as`
type Getters<T> = {
  [K in keyof T as `get${Capitalize<string & K>}`]: () => T[K]
}

interface User { name: string; age: number }
type UserGetters = Getters<User>
// { getName: () => string; getAge: () => number }
```

---

## Template Literal Types

Build string types programmatically.

```typescript
type EventName = 'click' | 'focus' | 'blur'
type Handler = `on${Capitalize<EventName>}`
// 'onClick' | 'onFocus' | 'onBlur'

// CSS property values
type CSSUnit = 'px' | 'em' | 'rem' | '%'
type CSSValue = `${number}${CSSUnit}`
// Not quite — TypeScript can't represent "any number" here, but you can use specific values:
type SpacingScale = 0 | 4 | 8 | 16 | 32
type SpacingValue = `${SpacingScale}px`  // '0px' | '4px' | '8px' | '16px' | '32px'

// HTTP method + path pairs for type-safe routing
type HTTPMethod = 'GET' | 'POST' | 'PUT' | 'DELETE'
type Route = `${HTTPMethod} /api/${string}`

// Extract parts of a string type
type ExtractVerb<T extends string> =
  T extends `${infer Verb} ${string}` ? Verb : never
type V = ExtractVerb<'GET /api/users'>  // 'GET'

// Useful for: event names, CSS-in-JS, typed route definitions, property accessor paths
type DeepKeyOf<T, Prefix extends string = ''> =
  T extends object
    ? { [K in keyof T & string]:
        | `${Prefix}${K}`
        | DeepKeyOf<T[K], `${Prefix}${K}.`>
      }[keyof T & string]
    : never

type Keys = DeepKeyOf<{ a: { b: { c: string } } }>
// 'a' | 'a.b' | 'a.b.c'
```

---

## Discriminated Unions with Exhaustiveness Checking

The most important TypeScript pattern for modeling state.

```typescript
type Result<T> =
  | { status: 'ok';      value: T }
  | { status: 'error';   error: Error }
  | { status: 'loading' }

// The discriminant ('status') tells TypeScript which branch you're in
function render<T>(result: Result<T>): string {
  switch (result.status) {
    case 'ok':      return String(result.value)  // result.value is T here
    case 'error':   return result.error.message  // result.error is Error here
    case 'loading': return 'Loading...'
    default:
      // If you add a new variant and forget to handle it, TypeScript errors here
      const _exhaustive: never = result
      throw new Error(`Unhandled status: ${JSON.stringify(_exhaustive)}`)
  }
}

// Why `never` works for exhaustiveness:
// After all cases are handled, result has type `never` (empty type — no values possible).
// If you add `| { status: 'cancelled' }` and forget a case, result in `default`
// is `{ status: 'cancelled' }`, which is not assignable to `never` — compile error.
```

### Real-world API response modeling

```typescript
type ApiResponse<T> =
  | { kind: 'success'; data: T; statusCode: 200 | 201 }
  | { kind: 'clientError'; message: string; statusCode: 400 | 401 | 403 | 404 }
  | { kind: 'serverError'; message: string; statusCode: 500 | 502 | 503 }

function handleResponse<T>(res: ApiResponse<T>) {
  switch (res.kind) {
    case 'success':
      processData(res.data)   // res.data: T — fully typed
      break
    case 'clientError':
      showUserError(res.message)
      break
    case 'serverError':
      reportToSentry(res.message, res.statusCode)
      break
  }
}
```

---

## `unknown` vs `any` vs `never`

```typescript
// any: opt out of type checking entirely — dangerous
let x: any = 'hello'
x.toUpperCase()   // OK
x.doesNotExist()  // also OK — no error, but will crash at runtime
x = 42
x.toFixed()       // OK

// unknown: must narrow before use — type-safe any
let y: unknown = 'hello'
y.toUpperCase()         // Error: Object is of type 'unknown'
if (typeof y === 'string') {
  y.toUpperCase()       // OK — narrowed to string
}

// never: the empty type — no value can have this type
// Used for: impossible branches, exhaustiveness checks, infinite loops
function fail(message: string): never {
  throw new Error(message)  // never returns
}

function infiniteLoop(): never {
  while (true) {}  // never returns
}

// never propagates in unions: string | never = string
// never absorbs in intersections: string & never = never

// Mental model:
// any     = "I don't care, trust me"
// unknown = "I don't know yet, I'll check before using"
// never   = "This cannot happen"
```

---

## Type Guards

```typescript
// typeof — works for primitives
function format(value: string | number): string {
  if (typeof value === 'string') return value.trim()
  return value.toFixed(2)
}

// instanceof — works for class instances
function handleError(err: unknown): string {
  if (err instanceof Error) return err.message
  return String(err)
}

// in operator — works for checking property existence
type Circle = { kind: 'circle'; radius: number }
type Rect = { kind: 'rect'; width: number; height: number }
type Shape = Circle | Rect

function area(s: Shape): number {
  if ('radius' in s) return Math.PI * s.radius ** 2  // s: Circle
  return s.width * s.height                           // s: Rect
}

// Custom type predicate — the most powerful form
function isUser(value: unknown): value is User {
  return (
    typeof value === 'object' &&
    value !== null &&
    typeof (value as any).id === 'string' &&
    typeof (value as any).name === 'string'
  )
}

// Assertion function — throws instead of returning false
function assertIsUser(value: unknown): asserts value is User {
  if (!isUser(value)) throw new TypeError('Expected User')
}

// After calling assertIsUser, TypeScript knows value is User
assertIsUser(payload)
console.log(payload.name)  // no error — narrowed to User
```

---

## Declaration Merging and Module Augmentation

```typescript
// Declaration merging: multiple interface declarations for the same name combine
interface Request {
  user?: User
}

// In Express.js projects, augment the existing Request type:
declare global {
  namespace Express {
    interface Request {
      user?: User
      requestId: string
    }
  }
}
// Now req.user and req.requestId are typed everywhere without casting

// Augmenting a third-party module
declare module 'express-serve-static-core' {
  interface Request {
    user?: User
  }
}

// Merging a namespace and a function (common in libraries)
function validator(schema: Schema): Middleware { ... }
namespace validator {
  export function isEmail(value: string): boolean { ... }
  export function isUUID(value: string): boolean { ... }
}
// validator() and validator.isEmail() both work
```

---

## Strict Mode Flags

```json
// tsconfig.json — strict: true enables all of these
{
  "compilerOptions": {
    "strict": true,
    // Equivalent to all of:
    "strictNullChecks": true,       // null/undefined not assignable to other types
    "noImplicitAny": true,          // error on implicit any
    "strictFunctionTypes": true,    // stricter function parameter compatibility
    "strictBindCallApply": true,    // bind/call/apply check argument types
    "strictPropertyInitialization": true,  // class properties must be initialized
    "noImplicitThis": true,         // error when `this` is any
    "alwaysStrict": true            // emit 'use strict'
  }
}
```

```typescript
// Without strictNullChecks: null and undefined are assignable to everything
let name: string = null  // allowed without strictNullChecks, error with it

// With strictNullChecks: must handle null explicitly
function getUser(id: string): User | null { ... }
const user = getUser('1')
console.log(user.name)  // Error: user is possibly null
if (user) console.log(user.name)  // OK

// Non-null assertion (!) — use sparingly
console.log(user!.name)  // tell TS "I know this isn't null"
// If user is actually null at runtime, this crashes
```

---

## Common Gotchas

### enum pitfalls

```typescript
// Numeric enums have reverse mappings — can cause surprises
enum Direction { Up = 0, Down = 1, Left = 2, Right = 3 }
Direction[0]   // 'Up' — reverse mapping exists on numeric enums
Direction['Up'] // 0

// Numeric enums are not type-safe — any number is assignable
function move(dir: Direction) { ... }
move(42)  // no error even though 42 is not a Direction

// String enums: no reverse mapping, fully type-safe
enum Status { Active = 'ACTIVE', Inactive = 'INACTIVE' }
move(Status.Active)  // OK; move('ACTIVE') is also OK if using string enum

// Prefer union types over enums for most cases
type Direction = 'up' | 'down' | 'left' | 'right'
// No runtime overhead, simpler, works well with JSON
```

### readonly arrays

```typescript
const arr: readonly number[] = [1, 2, 3]
arr.push(4)    // Error: Property 'push' does not exist on type 'readonly number[]'
arr[0] = 99   // Error

// ReadonlyArray<T> and readonly T[] are equivalent
// Note: readonly is shallow — elements of an object array are still mutable
const users: readonly User[] = [{ id: '1', name: 'Alice' }]
users[0].name = 'Bob'  // No error — shallow readonly
users.push(newUser)    // Error — array itself is readonly
```

### `as` vs `satisfies`

```typescript
// as: type assertion — "I know better than you, trust me"
// Suppresses errors but lies to the type system
const user = {} as User  // no error even though required fields are missing
user.name  // no error — but user.name is undefined at runtime

// satisfies: validate that a value matches a type without widening it
// Keeps the inferred type while checking against the constraint
const config = {
  port: 3000,
  host: 'localhost',
} satisfies { port: number; host: string }
config.port   // type: number ✓
config.host   // type: string ✓

// More useful example: satisfies preserves literal types
const palette = {
  red: [255, 0, 0],
  green: '#00ff00',
} satisfies Record<string, string | number[]>
// palette.red   — type: number[] (not string | number[])
// palette.green — type: string  (not string | number[])
// `as` would lose this precision; `satisfies` preserves it
```

### Function overloads

```typescript
// Overloads: multiple signatures, one implementation
function createElement(tag: 'a'): HTMLAnchorElement
function createElement(tag: 'div'): HTMLDivElement
function createElement(tag: string): HTMLElement
function createElement(tag: string): HTMLElement {
  return document.createElement(tag)
}

const anchor = createElement('a')  // type: HTMLAnchorElement
const div = createElement('div')   // type: HTMLDivElement
```

---

## Common Interview Questions

**Q: What is the difference between `interface` and `type`?**
Both define object shapes. `interface` supports declaration merging (two `interface Foo` declarations combine) and is preferred for public API shapes. `type` is more powerful — it can represent unions, intersections, mapped types, conditional types, and primitives. Use `interface` for objects that might be extended or merged; use `type` for everything else.

**Q: What does `unknown` solve that `any` doesn't?**
`any` disables type checking — you can call any method, access any property, assign anywhere, without error. `unknown` requires you to narrow the type before using it. It's the right type for: external API responses (`res.json()`), `catch` block error values, user input. Use `any` only as an explicit escape hatch, never as a default.

**Q: What is a discriminated union and how does TypeScript provide exhaustiveness checking?**
A discriminated union is a union where each member has a shared literal property (the discriminant). TypeScript narrows the type in each branch of a switch/if. Exhaustiveness checking works by assigning the value in the `default` branch to `never` — if any case is unhandled, the type there is not `never`, and you get a compile error. This catches missing cases when you add new union members.

**Q: What is the difference between `as` and `satisfies`?**
`as` is a type assertion that overrides TypeScript's judgment — it suppresses errors but can be wrong at runtime. `satisfies` validates that a value matches a type while preserving the inferred literal type. Use `satisfies` to check a value against a constraint without losing precision; use `as` only when you're certain TS is wrong and you can't fix the type properly.

**Q: How do conditional types distribute over unions?**
When you write `T extends U ? X : Y` and T is a union, TypeScript distributes the conditional over each member: `(A | B) extends U ? X : Y` becomes `(A extends U ? X : Y) | (B extends U ? X : Y)`. This is why `NonNullable<string | null>` = `string` — `string extends null | undefined ? never : string` gives `string`, and `null extends null | undefined ? never : null` gives `never`, and `string | never` = `string`.
