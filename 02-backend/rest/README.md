# REST API Design

## 1. REST Principles

REST (Representational State Transfer) is an architectural style defined by Roy Fielding in 2000. It is not a protocol or standard but a set of constraints that, when followed, produce scalable, maintainable web services.

### The Six Constraints

**1. Client-Server Separation**
The client and server are independent. The server exposes resources; the client consumes them. Neither knows about the other's internal implementation. This separation allows them to evolve independently.

**2. Statelessness**
Each request from the client must contain all the information needed to understand and process it. The server stores no session state between requests. Authentication tokens, pagination cursors, and filters must be sent on every request.

Why it matters:
- Any server instance can handle any request (horizontal scaling is trivial)
- No sticky sessions required
- Requests are easier to reason about in isolation
- Debugging is simpler — the request itself tells the full story

What statelessness does NOT mean: the server has no persistent state. Databases, caches, and queues hold persistent state. Statelessness refers to *session* state — per-client conversational context held in server memory.

**3. Cacheability**
Responses must declare whether they can be cached. GET responses that are cacheable reduce server load and improve latency. Use `Cache-Control`, `ETag`, and `Last-Modified` headers correctly.

**4. Uniform Interface**
This is the central constraint that defines REST. It has four sub-constraints:
- **Resource identification in requests**: URIs identify resources (`/users/42`)
- **Resource manipulation through representations**: clients manipulate resources by sending representations (JSON, XML)
- **Self-descriptive messages**: each message includes enough information to describe how to process it (Content-Type header, status codes)
- **HATEOAS**: responses include links to related actions (covered in section 9)

**5. Layered System**
A client cannot tell whether it is talking directly to the origin server or an intermediary (load balancer, CDN, API gateway). This enables transparent infrastructure changes.

**6. Code on Demand (optional)**
Servers can send executable code to clients (e.g., JavaScript). Rarely used in REST APIs.

### Resource-Based URLs

Resources are nouns, not verbs. A resource represents a thing — not an action.

```
# Resources are nouns
/users
/orders
/products
/users/42/orders

# Not actions
/getUser         # wrong
/createOrder     # wrong
/deleteProduct   # wrong
```

Resources exist in two forms:
- **Collection**: `/users` — represents the set of all users
- **Instance**: `/users/42` — represents a specific user

---

## 2. URL Design Best Practices

### Good vs. Bad URL Examples

| Bad | Good | Why |
|-----|------|-----|
| `/getUsers` | `/users` | Verbs belong in HTTP methods, not URLs |
| `/user/42` | `/users/42` | Collections use plural nouns |
| `/Users/42` | `/users/42` | URLs are lowercase |
| `/users/42/getOrders` | `/users/42/orders` | Sub-resources, not actions |
| `/users?userId=42` | `/users/42` | ID belongs in the path, not query string |
| `/api/v1/delete-user/42` | `DELETE /api/v1/users/42` | Deletion is an HTTP method |
| `/api/v1/users/42/activate` | `POST /api/v1/users/42/activations` | Model state transitions as sub-resources |
| `/search-users` | `/users?q=john` | Search is a filter on the collection |

### Hierarchy Rules

Use path nesting only when the child resource has no meaning without the parent:

```
# Good nesting — an order line item only exists within an order
GET /orders/99/items/3

# Questionable nesting — users can be queried independently
GET /companies/5/users/42
# Prefer:
GET /users/42?companyId=5
# Or both (aliased)
GET /users/42
GET /companies/5/users/42
```

Avoid nesting more than two levels deep. `/a/:id/b/:id/c/:id` becomes hard to read and maintain.

### Naming Conventions

- Lowercase letters and hyphens for multi-word resources: `/order-items`, not `/orderItems` or `/order_items`
- No trailing slash: `/users` not `/users/`
- No file extensions: `/users` not `/users.json`
- Consistent plural nouns throughout

### Query Strings vs. Path Segments

| Use path segment for | Use query string for |
|---------------------|---------------------|
| Resource identity (`/users/42`) | Filtering (`?status=active`) |
| Required hierarchy (`/orders/5/items`) | Sorting (`?sort=created_at:desc`) |
| | Pagination (`?page=2&limit=20`) |
| | Searching (`?q=john`) |
| | Sparse fieldsets (`?fields=id,name`) |

---

## 3. HTTP Method Semantics

### GET — Retrieve

- **Safe**: does not modify server state
- **Idempotent**: multiple identical calls have the same effect
- No request body (technically allowed, but avoid it — many proxies strip it)

```
GET /users           # list all users
GET /users/42        # get user 42
GET /users/42/orders # get orders for user 42
```

### POST — Create

- **Not safe**: modifies server state
- **Not idempotent**: calling twice may create two resources
- Request body contains the representation of the resource to create
- Response: `201 Created` with `Location` header pointing to the new resource

```
POST /users
Content-Type: application/json

{ "name": "Alice", "email": "alice@example.com" }

HTTP/1.1 201 Created
Location: /users/43
```

POST is also used for actions that do not map cleanly to CRUD:
```
POST /payments/99/refund
POST /users/42/password-reset
POST /emails/send
```

### PUT — Replace

- **Not safe**: modifies server state
- **Idempotent**: calling PUT with the same body multiple times produces the same result
- Replaces the **entire** resource. Fields not included are set to null/default.
- Use when the client controls the resource ID (e.g., upload a file at a known key)

```
PUT /users/42
Content-Type: application/json

{ "name": "Alice Smith", "email": "alice@example.com", "role": "admin" }
```

If you omit `role`, it gets overwritten to null. This is the key difference from PATCH.

### PATCH — Partial Update

- **Not safe**: modifies server state
- **May or may not be idempotent** depending on the patch semantics
- Only sends the fields that change. Other fields are unaffected.

```
PATCH /users/42
Content-Type: application/json

{ "name": "Alice Smith" }
```

Structured patch formats (JSON Patch, JSON Merge Patch):
```
# JSON Merge Patch (RFC 7396) — simpler, most common
PATCH /users/42
Content-Type: application/merge-patch+json

{ "name": "Alice Smith" }

# JSON Patch (RFC 6902) — more expressive
PATCH /users/42
Content-Type: application/json-patch+json

[
  { "op": "replace", "path": "/name", "value": "Alice Smith" },
  { "op": "add", "path": "/tags/0", "value": "admin" }
]
```

### DELETE — Remove

- **Not safe**: modifies server state
- **Idempotent**: deleting the same resource twice returns `404` the second time, but the system state is the same (resource is absent)
- No request body
- Response: `204 No Content` (no body) or `200 OK` with a confirmation body

```
DELETE /users/42

HTTP/1.1 204 No Content
```

### HEAD and OPTIONS

- `HEAD`: same as GET but returns headers only, no body. Used to check resource existence or get metadata.
- `OPTIONS`: returns allowed methods for a resource. Used by CORS preflight requests.

### Method Summary Table

| Method | Safe | Idempotent | Body | Success Code |
|--------|------|------------|------|--------------|
| GET | Yes | Yes | No | 200 |
| POST | No | No | Yes | 201 |
| PUT | No | Yes | Yes | 200/204 |
| PATCH | No | Usually No | Yes | 200/204 |
| DELETE | No | Yes | No | 204 |
| HEAD | Yes | Yes | No | 200 |
| OPTIONS | Yes | Yes | No | 200/204 |

---

## 4. API Versioning

Versioning lets you evolve your API without breaking existing clients.

### URL Path Versioning

```
GET /api/v1/users
GET /api/v2/users
```

**Pros:**
- Immediately visible in URLs, logs, and browser history
- Easy to route at the load balancer or API gateway level
- Simple to test with curl or a browser
- No special client configuration needed

**Cons:**
- Violates the "URI identifies a resource" principle — the resource hasn't changed, only its representation
- URL proliferation (`/v1/users`, `/v2/users`, `/v3/users`)
- Old versions require maintenance indefinitely or client breakage

### Header Versioning

```
GET /api/users
Accept: application/vnd.myapi.v2+json
```

Or with a custom header:
```
GET /api/users
API-Version: 2
```

**Pros:**
- Cleaner URLs — the resource URI stays stable
- More aligned with REST's uniform interface principle
- Easier to deprecate gracefully

**Cons:**
- Less discoverable — you can't bookmark or share a versioned URL
- Harder to test in a browser
- Caching becomes more complex (must vary on the version header)

### Query Parameter Versioning

```
GET /api/users?version=2
```

Rarely recommended. Query parameters are for filtering, not API versioning. It pollutes every request and is easy to forget.

### Tradeoffs Summary

| Strategy | Discoverability | REST Purity | Cache-Friendly | Operational Ease |
|----------|----------------|-------------|----------------|-----------------|
| URL path | High | Low | High | High |
| Header | Low | High | Medium | Medium |
| Query param | Medium | Low | Low | Low |

**Recommendation for most teams**: URL versioning. The pragmatic benefits (visibility, routing, simplicity) outweigh the theoretical purity concerns. Use header versioning only if you have strict REST compliance requirements or a sophisticated client ecosystem.

### Versioning Strategy

- Start at `v1` — never `v0`
- Only bump the major version on breaking changes:
  - Removing a field
  - Changing a field's type
  - Changing authentication requirements
  - Changing URL structure
- Non-breaking additions (new optional fields, new endpoints) do not require a version bump
- Maintain old versions for a defined deprecation period (commonly 6-12 months)
- Use `Sunset` and `Deprecation` headers to signal impending removal:

```
HTTP/1.1 200 OK
Deprecation: true
Sunset: Sat, 01 Jan 2027 00:00:00 GMT
Link: <https://api.example.com/v2/users>; rel="successor-version"
```

---

## 5. Pagination

Pagination is required for any collection endpoint that can return unbounded results. Never return all records by default.

### Offset/Limit Pagination

The client specifies how many records to skip and how many to return.

```
GET /users?offset=20&limit=10
# or equivalently
GET /users?page=3&limit=10   # page 3, 10 per page
```

Response:
```json
{
  "data": [...],
  "pagination": {
    "total": 143,
    "offset": 20,
    "limit": 10,
    "hasMore": true
  }
}
```

**Pros:**
- Simple to implement with SQL `LIMIT`/`OFFSET`
- Allows random access to any page
- Users can jump to page N

**Cons:**
- Unstable under inserts/deletes: if a record is added before your offset, you may see duplicates or skip records
- Performance degrades on large offsets: `OFFSET 100000` requires the DB to scan and discard 100,000 rows
- Requires a `COUNT(*)` query for `total`, which is expensive on large tables

### Cursor-Based Pagination

The server returns an opaque cursor pointing to the last item returned. The client sends that cursor on the next request.

```
GET /users?limit=10
# Response includes cursor

GET /users?after=eyJpZCI6MjB9&limit=10
```

Response:
```json
{
  "data": [...],
  "pagination": {
    "nextCursor": "eyJpZCI6MzB9",
    "hasNextPage": true
  }
}
```

The cursor is typically a base64-encoded representation of the last item's sort key:
```typescript
const cursor = Buffer.from(JSON.stringify({ id: lastItem.id })).toString('base64');
const decoded = JSON.parse(Buffer.from(cursor, 'base64').toString('utf-8'));
```

**Pros:**
- Stable under concurrent inserts/deletes
- Efficient — no offset scan, just `WHERE id > lastId`
- Works well for infinite scroll / "load more" UIs

**Cons:**
- No random access — can't jump to page 7
- Cursors may expire if the dataset changes dramatically
- More complex to implement

### Keyset Pagination

A specific form of cursor pagination where the "cursor" is the actual column value(s) rather than an opaque token. More transparent but couples the client to the data structure.

```
GET /users?after_id=20&limit=10
```

SQL equivalent:
```sql
SELECT * FROM users WHERE id > 20 ORDER BY id ASC LIMIT 10;
```

For compound sort keys (e.g., sort by `created_at` then `id` for tie-breaking):
```sql
SELECT * FROM users
WHERE (created_at, id) > ('2024-01-15 10:00:00', 42)
ORDER BY created_at ASC, id ASC
LIMIT 10;
```

### TypeScript Implementation Examples

```typescript
// types.ts
interface PaginationParams {
  limit?: number;
  offset?: number;
  cursor?: string;
}

interface PaginatedResponse<T> {
  data: T[];
  pagination: OffsetPagination | CursorPagination;
}

interface OffsetPagination {
  type: 'offset';
  total: number;
  offset: number;
  limit: number;
  hasMore: boolean;
}

interface CursorPagination {
  type: 'cursor';
  nextCursor: string | null;
  hasNextPage: boolean;
}

// Offset pagination helper
function buildOffsetPagination(
  total: number,
  offset: number,
  limit: number
): OffsetPagination {
  return {
    type: 'offset',
    total,
    offset,
    limit,
    hasMore: offset + limit < total,
  };
}

// Cursor encoding/decoding
function encodeCursor(data: Record<string, unknown>): string {
  return Buffer.from(JSON.stringify(data)).toString('base64url');
}

function decodeCursor(cursor: string): Record<string, unknown> {
  try {
    return JSON.parse(Buffer.from(cursor, 'base64url').toString('utf-8'));
  } catch {
    throw new Error('Invalid pagination cursor');
  }
}

// Cursor pagination helper
function buildCursorPagination<T extends { id: number }>(
  items: T[],
  limit: number
): { data: T[]; pagination: CursorPagination } {
  const hasNextPage = items.length > limit;
  const data = hasNextPage ? items.slice(0, limit) : items;
  const lastItem = data[data.length - 1];

  return {
    data,
    pagination: {
      type: 'cursor',
      nextCursor: hasNextPage && lastItem ? encodeCursor({ id: lastItem.id }) : null,
      hasNextPage,
    },
  };
}
```

---

## 6. Filtering, Sorting, and Searching

### Filtering

Use query parameters with clear, consistent names:

```
GET /users?status=active
GET /users?status=active&role=admin
GET /users?createdAfter=2024-01-01&createdBefore=2024-12-31
GET /products?minPrice=10&maxPrice=100&category=electronics
```

For complex filtering, some APIs use bracket notation or a filter object:
```
GET /users?filter[status]=active&filter[role]=admin
GET /users?filter={"status":"active","role":"admin"}  # URL-encoded
```

The simple `?key=value` form is preferred for public APIs. Reserve structured filter syntax for internal or developer-facing APIs where clients are sophisticated.

**Multiple values for the same field:**
```
GET /users?status=active&status=pending   # repeated param = OR
GET /users?status=active,pending           # comma-separated
```

Pick one and document it. Repeated parameters are more HTTP-native; comma-separated is more readable.

### Sorting

```
GET /users?sort=name              # ascending by name
GET /users?sort=-name             # descending (minus prefix)
GET /users?sort=name,-createdAt   # name ASC, createdAt DESC

# Alternative colon syntax
GET /users?sort=name:asc,createdAt:desc
```

TypeScript sort parsing:
```typescript
type SortDirection = 'asc' | 'desc';

interface SortField {
  field: string;
  direction: SortDirection;
}

function parseSortParam(sort: string, allowedFields: string[]): SortField[] {
  return sort.split(',').map((part) => {
    const desc = part.startsWith('-');
    const field = desc ? part.slice(1) : part;

    if (!allowedFields.includes(field)) {
      throw new Error(`Invalid sort field: ${field}`);
    }

    return { field, direction: desc ? 'desc' : 'asc' };
  });
}
```

Always whitelist allowed sort fields to prevent SQL injection through column names.

### Searching

Full-text search uses a `q` or `search` parameter:
```
GET /users?q=alice
GET /products?q=wireless+headphones
```

Field-specific search:
```
GET /users?name=alice&email=alice@
```

For advanced search (used by GitHub, Elasticsearch APIs):
```
GET /search/users?q=language:typescript+repos:>10
```

### Sparse Fieldsets

Allow clients to request only the fields they need, reducing payload size:
```
GET /users?fields=id,name,email
GET /users/42?fields=id,name,orders
```

---

## 7. Error Response Format

Consistent error responses are as important as consistent success responses. Clients must be able to programmatically handle errors.

### Minimum Error Object

```json
{
  "error": {
    "code": "VALIDATION_ERROR",
    "message": "Request validation failed",
    "details": [
      {
        "field": "email",
        "message": "Must be a valid email address"
      },
      {
        "field": "age",
        "message": "Must be a positive integer"
      }
    ]
  }
}
```

Rules:
- Always return `application/json` for errors, even if the request was not JSON
- Use a machine-readable `code` string (not just the HTTP status code)
- Use a human-readable `message`
- Include field-level details for validation errors
- Never expose stack traces or internal system details in production

### HTTP Status Codes

**2xx — Success**
- `200 OK` — successful GET, PUT, PATCH
- `201 Created` — successful POST that created a resource
- `204 No Content` — successful DELETE or PUT/PATCH with no response body
- `206 Partial Content` — partial response (used with range requests)

**3xx — Redirection**
- `301 Moved Permanently` — resource has a new permanent URL
- `304 Not Modified` — cached response is still valid (used with ETags)

**4xx — Client Errors**
- `400 Bad Request` — malformed request, invalid parameters
- `401 Unauthorized` — authentication required or failed (despite the name)
- `403 Forbidden` — authenticated but not authorized
- `404 Not Found` — resource does not exist
- `405 Method Not Allowed` — method not supported for this endpoint
- `409 Conflict` — conflict with current state (duplicate key, optimistic lock failure)
- `410 Gone` — resource existed but has been permanently deleted
- `422 Unprocessable Entity` — request is well-formed but semantically invalid
- `429 Too Many Requests` — rate limit exceeded

**5xx — Server Errors**
- `500 Internal Server Error` — unexpected server failure
- `502 Bad Gateway` — upstream service failure
- `503 Service Unavailable` — server temporarily unavailable (overloaded or in maintenance)
- `504 Gateway Timeout` — upstream service timed out

**401 vs 403:**
- `401`: "I don't know who you are. Please authenticate."
- `403`: "I know who you are. You don't have permission to do this."

### Problem Details (RFC 7807 / problem+json)

A standardized error format for HTTP APIs:

```
HTTP/1.1 422 Unprocessable Entity
Content-Type: application/problem+json

{
  "type": "https://api.example.com/errors/validation-error",
  "title": "Validation Error",
  "status": 422,
  "detail": "The request body contains invalid fields.",
  "instance": "/users/42",
  "errors": [
    {
      "pointer": "/email",
      "detail": "Must be a valid email address"
    }
  ]
}
```

Fields:
- `type`: a URI that uniquely identifies the error type (links to documentation)
- `title`: human-readable summary of the error type (stable across instances)
- `status`: the HTTP status code
- `detail`: human-readable explanation of this specific occurrence
- `instance`: a URI reference to the specific request that caused the error

Problem+json is worth adopting for public APIs because it is a recognized standard clients can build tooling around.

---

## 8. Idempotency

### Definition

An operation is idempotent if applying it multiple times produces the same result as applying it once. The state of the system after N calls is identical to after 1 call.

Crucially, idempotency is about **server state**, not about the response. A DELETE call that returns `404` on subsequent calls is still idempotent because the server state (resource is absent) is the same as after the first successful call.

### Which Methods Are Idempotent?

| Method | Idempotent | Why |
|--------|------------|-----|
| GET | Yes | Read-only, no state change |
| HEAD | Yes | Read-only |
| OPTIONS | Yes | Read-only |
| PUT | Yes | Replaces resource with same representation each time |
| DELETE | Yes | Resource is absent regardless of how many times called |
| POST | No | Each call may create a new resource |
| PATCH | Usually No | Relative operations like "increment by 1" are not idempotent |

### Why It Matters

Networks are unreliable. Clients retry requests when they do not receive a response (due to timeouts, network drops). If an operation is idempotent, safe to retry without fear of duplicate side effects.

If an operation is not idempotent (POST), retrying can cause duplicate orders, duplicate payments, or duplicate records.

### Idempotency Keys

For non-idempotent operations that must be safe to retry (e.g., payment processing), use an idempotency key:

```
POST /payments
Idempotency-Key: a8098c1a-f86e-11da-bd1a-00112444be1e
Content-Type: application/json

{ "amount": 5000, "currency": "USD", "customerId": "cus_123" }
```

The server stores the key and the result. If the same key is submitted again, it returns the cached result instead of processing again. Stripe, Braintree, and most payment APIs implement this pattern.

Implementation:
1. Client generates a unique key (UUID) per operation attempt
2. Server checks if key exists in a key-value store (Redis)
3. If key exists, return the stored response
4. If key does not exist, process the request and store the result with the key
5. Key expires after a reasonable window (24 hours to 7 days)

---

## 9. HATEOAS

### What It Is

HATEOAS (Hypermedia as the Engine of Application State) is a REST constraint where responses include links describing what the client can do next. The client navigates the API by following links, not by constructing URLs from documentation.

Example response with HATEOAS:
```json
{
  "id": 42,
  "name": "Alice",
  "status": "active",
  "_links": {
    "self": { "href": "/users/42" },
    "orders": { "href": "/users/42/orders" },
    "deactivate": {
      "href": "/users/42/activations/42",
      "method": "DELETE"
    },
    "update": {
      "href": "/users/42",
      "method": "PUT"
    }
  }
}
```

The `_links` property tells the client exactly what operations are available and where. The "deactivate" link only appears if the user can actually be deactivated — if the user is already inactive, that link is absent.

This means:
- Clients do not need to know URL patterns
- Clients do not need to hard-code business logic ("can I deactivate this user?")
- The server controls the flow; clients just follow links

### When It Is Worth It

HATEOAS adds significant complexity. It is worth implementing when:

- You are building a public API where clients come from many teams or third parties who cannot easily coordinate on URL changes
- You want to evolve URL structure without breaking clients
- You are building a long-lived API with complex state machines (e.g., order workflows)
- Clients are generic/dumb consumers that should not encode business logic

It is probably not worth it when:

- You control both the API and all clients (internal frontend + backend)
- Your API is simple CRUD with no complex state transitions
- Your team does not have the bandwidth to maintain hypermedia properly
- Your clients are sophisticated and already know the API structure

In practice, very few production REST APIs implement full HATEOAS. Partial hypermedia — including `_links` for pagination cursors and related resources — is a reasonable middle ground.

---

## 10. Rate Limiting

### Purpose

Rate limiting protects your API from:
- Abuse and denial-of-service attacks
- Runaway client bugs that flood the API
- Overloading downstream services
- Ensuring fair usage across all clients

### Rate Limiting Headers

The industry-standard headers for communicating rate limit status:

```
X-RateLimit-Limit: 1000        # total requests allowed in the window
X-RateLimit-Remaining: 743     # requests remaining in the current window
X-RateLimit-Reset: 1705312800  # Unix timestamp when the window resets
X-RateLimit-Window: 3600       # window duration in seconds (optional)
```

When the rate limit is exceeded:

```
HTTP/1.1 429 Too Many Requests
X-RateLimit-Limit: 1000
X-RateLimit-Remaining: 0
X-RateLimit-Reset: 1705312800
Retry-After: 847               # seconds until the client may retry

{
  "error": {
    "code": "RATE_LIMIT_EXCEEDED",
    "message": "Too many requests. Please retry after 847 seconds.",
    "retryAfter": 847
  }
}
```

`Retry-After` can be either a number of seconds or an HTTP date:
```
Retry-After: 120
Retry-After: Fri, 15 Jan 2025 12:00:00 GMT
```

### Rate Limiting Algorithms

**Fixed Window**: count requests in a fixed time bucket (e.g., 1000 req/hour). Simple but allows burst at window boundaries.

**Sliding Window**: count requests in the past N seconds from now. Smoother, eliminates boundary burst, slightly more complex.

**Token Bucket**: clients have a bucket of tokens that refills at a fixed rate. Each request consumes a token. Allows controlled bursting. Used by most cloud providers.

**Leaky Bucket**: requests are processed at a fixed rate; excess requests are queued or dropped. Produces very smooth output rates.

### Multiple Rate Limit Tiers

Real APIs often have multiple limits simultaneously:
```
X-RateLimit-Limit-Second: 50      # per-second burst limit
X-RateLimit-Limit-Hour: 5000      # per-hour sustained limit
X-RateLimit-Limit-Day: 50000      # per-day limit
```

---

## Full Example: TypeScript Express Endpoint

A production-quality `/users` endpoint demonstrating pagination, filtering, sorting, and error handling.

```typescript
import express, { Request, Response, NextFunction } from 'express';
import { z } from 'zod';

const app = express();
app.use(express.json());

// ─── Types ────────────────────────────────────────────────────────────────────

interface User {
  id: number;
  name: string;
  email: string;
  role: 'admin' | 'user' | 'moderator';
  status: 'active' | 'inactive';
  createdAt: string;
}

interface ApiError {
  code: string;
  message: string;
  details?: Array<{ field?: string; message: string }>;
}

interface PaginatedResponse<T> {
  data: T[];
  pagination: {
    total?: number;
    limit: number;
    nextCursor: string | null;
    hasNextPage: boolean;
  };
  meta: {
    requestId: string;
    timestamp: string;
  };
}

// ─── Validation Schema ────────────────────────────────────────────────────────

const ALLOWED_SORT_FIELDS = ['name', 'email', 'createdAt', 'id'] as const;
const ALLOWED_ROLES = ['admin', 'user', 'moderator'] as const;
const ALLOWED_STATUSES = ['active', 'inactive'] as const;

const getUsersSchema = z.object({
  // Pagination
  limit: z
    .string()
    .optional()
    .transform((v) => (v ? parseInt(v, 10) : 20))
    .pipe(z.number().int().min(1).max(100)),
  cursor: z.string().optional(),

  // Filtering
  status: z.enum(ALLOWED_STATUSES).optional(),
  role: z.enum(ALLOWED_ROLES).optional(),
  q: z.string().max(100).optional(),
  createdAfter: z.string().datetime().optional(),
  createdBefore: z.string().datetime().optional(),

  // Sorting
  sort: z.string().optional(),

  // Sparse fieldsets
  fields: z.string().optional(),
});

type GetUsersQuery = z.infer<typeof getUsersSchema>;

// ─── Cursor Utilities ─────────────────────────────────────────────────────────

function encodeCursor(data: Record<string, unknown>): string {
  return Buffer.from(JSON.stringify(data)).toString('base64url');
}

function decodeCursor(cursor: string): Record<string, unknown> {
  try {
    const decoded = Buffer.from(cursor, 'base64url').toString('utf-8');
    return JSON.parse(decoded);
  } catch {
    throw createApiError('INVALID_CURSOR', 'Pagination cursor is invalid or expired', 400);
  }
}

// ─── Sort Parsing ─────────────────────────────────────────────────────────────

interface SortField {
  field: string;
  direction: 'asc' | 'desc';
}

function parseSortParam(sort: string): SortField[] {
  return sort.split(',').map((part) => {
    const desc = part.startsWith('-');
    const field = desc ? part.slice(1) : part;

    if (!(ALLOWED_SORT_FIELDS as readonly string[]).includes(field)) {
      throw createApiError(
        'INVALID_SORT_FIELD',
        `Sort field "${field}" is not allowed. Allowed: ${ALLOWED_SORT_FIELDS.join(', ')}`,
        400
      );
    }

    return { field, direction: desc ? 'desc' : 'asc' };
  });
}

// ─── Error Utilities ──────────────────────────────────────────────────────────

class ApiException extends Error {
  constructor(
    public statusCode: number,
    public code: string,
    message: string,
    public details?: ApiError['details']
  ) {
    super(message);
    this.name = 'ApiException';
  }
}

function createApiError(code: string, message: string, status: number): ApiException {
  return new ApiException(status, code, message);
}

// ─── Mock Database Layer ──────────────────────────────────────────────────────

// Simulates a database query with filtering, sorting, cursor pagination
async function queryUsers(params: {
  limit: number;
  cursorId?: number;
  status?: string;
  role?: string;
  q?: string;
  createdAfter?: string;
  createdBefore?: string;
  sort: SortField[];
}): Promise<{ items: User[]; total: number }> {
  // In a real implementation, this would be a Prisma/TypeORM/Knex query:
  //
  // const query = db
  //   .selectFrom('users')
  //   .selectAll()
  //   .where('id', '>', params.cursorId ?? 0)
  //   .if(Boolean(params.status), (qb) => qb.where('status', '=', params.status!))
  //   .if(Boolean(params.role), (qb) => qb.where('role', '=', params.role!))
  //   .if(Boolean(params.q), (qb) =>
  //     qb.where((eb) =>
  //       eb.or([
  //         eb('name', 'ilike', `%${params.q}%`),
  //         eb('email', 'ilike', `%${params.q}%`),
  //       ])
  //     )
  //   )
  //   .orderBy(params.sort[0]?.field ?? 'id', params.sort[0]?.direction ?? 'asc')
  //   .limit(params.limit + 1); // fetch one extra to determine hasNextPage
  //
  // const [items, [{ count }]] = await Promise.all([query.execute(), countQuery.execute()]);
  // return { items, total: Number(count) };

  // Stub for this example:
  return {
    items: [],
    total: 0,
  };
}

// ─── Route Handler ────────────────────────────────────────────────────────────

app.get('/api/v1/users', async (req: Request, res: Response, next: NextFunction) => {
  const requestId = crypto.randomUUID();

  // 1. Validate query parameters
  const parseResult = getUsersSchema.safeParse(req.query);
  if (!parseResult.success) {
    const details = parseResult.error.errors.map((e) => ({
      field: e.path.join('.'),
      message: e.message,
    }));
    return next(new ApiException(400, 'VALIDATION_ERROR', 'Invalid query parameters', details));
  }

  const query: GetUsersQuery = parseResult.data;

  // 2. Parse cursor
  let cursorId: number | undefined;
  if (query.cursor) {
    const decoded = decodeCursor(query.cursor);
    if (typeof decoded.id !== 'number') {
      return next(createApiError('INVALID_CURSOR', 'Cursor does not contain a valid ID', 400));
    }
    cursorId = decoded.id;
  }

  // 3. Parse sort
  const sort: SortField[] = query.sort
    ? parseSortParam(query.sort)
    : [{ field: 'id', direction: 'asc' }];

  // 4. Fetch data (fetch limit+1 to detect next page)
  const { items, total } = await queryUsers({
    limit: query.limit + 1,
    cursorId,
    status: query.status,
    role: query.role,
    q: query.q,
    createdAfter: query.createdAfter,
    createdBefore: query.createdBefore,
    sort,
  });

  // 5. Determine pagination metadata
  const hasNextPage = items.length > query.limit;
  const pageItems = hasNextPage ? items.slice(0, query.limit) : items;
  const lastItem = pageItems[pageItems.length - 1];
  const nextCursor = hasNextPage && lastItem ? encodeCursor({ id: lastItem.id }) : null;

  // 6. Apply sparse fieldsets
  let responseData: Partial<User>[] = pageItems;
  if (query.fields) {
    const allowedFields = new Set<keyof User>(['id', 'name', 'email', 'role', 'status', 'createdAt']);
    const requestedFields = query.fields.split(',').filter((f): f is keyof User =>
      allowedFields.has(f as keyof User)
    );

    if (requestedFields.length > 0) {
      responseData = pageItems.map((item) => {
        const partial: Partial<User> = {};
        for (const field of requestedFields) {
          (partial as Record<string, unknown>)[field] = item[field];
        }
        return partial;
      });
    }
  }

  // 7. Build and send response
  const response: PaginatedResponse<Partial<User>> = {
    data: responseData,
    pagination: {
      total,
      limit: query.limit,
      nextCursor,
      hasNextPage,
    },
    meta: {
      requestId,
      timestamp: new Date().toISOString(),
    },
  };

  res.status(200).json(response);
});

// ─── Global Error Handler ─────────────────────────────────────────────────────

app.use((err: unknown, req: Request, res: Response, _next: NextFunction) => {
  // Log full error internally (never expose to client)
  console.error('[ERROR]', err);

  if (err instanceof ApiException) {
    const body: { error: ApiError } = {
      error: {
        code: err.code,
        message: err.message,
        ...(err.details && { details: err.details }),
      },
    };
    return res.status(err.statusCode).json(body);
  }

  // Unexpected error — do not leak details
  res.status(500).json({
    error: {
      code: 'INTERNAL_SERVER_ERROR',
      message: 'An unexpected error occurred. Please try again later.',
    },
  });
});

// ─── Example Request/Response ─────────────────────────────────────────────────

/*
GET /api/v1/users?status=active&role=admin&sort=-createdAt&limit=25&fields=id,name,email

HTTP/1.1 200 OK
Content-Type: application/json
X-RateLimit-Limit: 1000
X-RateLimit-Remaining: 847
X-RateLimit-Reset: 1705312800

{
  "data": [
    { "id": 99, "name": "Alice", "email": "alice@example.com" },
    { "id": 87, "name": "Bob",   "email": "bob@example.com" }
  ],
  "pagination": {
    "total": 143,
    "limit": 25,
    "nextCursor": "eyJpZCI6ODd9",
    "hasNextPage": true
  },
  "meta": {
    "requestId": "a8098c1a-f86e-11da-bd1a-00112444be1e",
    "timestamp": "2025-01-15T10:30:00.000Z"
  }
}
*/

export default app;
```

### Rate Limiting Middleware Example

```typescript
import rateLimit from 'express-rate-limit';
import RedisStore from 'rate-limit-redis';
import { createClient } from 'redis';

const redisClient = createClient({ url: process.env.REDIS_URL });
await redisClient.connect();

const apiLimiter = rateLimit({
  windowMs: 60 * 60 * 1000, // 1 hour
  max: 1000,
  standardHeaders: true,    // Send X-RateLimit-* headers
  legacyHeaders: false,     // Disable X-RateLimit-* legacy headers
  store: new RedisStore({
    sendCommand: (...args: string[]) => redisClient.sendCommand(args),
  }),
  keyGenerator: (req) => {
    // Rate limit by API key, falling back to IP
    return (req.headers['x-api-key'] as string) ?? req.ip ?? 'anonymous';
  },
  handler: (req, res) => {
    res.status(429).json({
      error: {
        code: 'RATE_LIMIT_EXCEEDED',
        message: 'Too many requests. Check the Retry-After header.',
      },
    });
  },
});

app.use('/api/', apiLimiter);
```

---

## Quick Reference

### Response Envelope Pattern

```json
{
  "data": { ... },
  "pagination": { ... },
  "meta": {
    "requestId": "uuid",
    "timestamp": "ISO8601"
  }
}
```

For errors:
```json
{
  "error": {
    "code": "MACHINE_READABLE_CODE",
    "message": "Human readable message",
    "details": [...]
  }
}
```

### Checklist for a New REST Endpoint

- [ ] URL uses plural noun, no verbs
- [ ] Correct HTTP method for the operation
- [ ] Returns appropriate status code
- [ ] Validates all input (path params, query params, body)
- [ ] Returns consistent error format with machine-readable code
- [ ] Collection endpoint supports pagination
- [ ] No unbounded queries (always has a max limit)
- [ ] Sort fields are whitelisted (prevent injection)
- [ ] Sensitive data is not logged or exposed in errors
- [ ] Rate limit headers are present on all responses
