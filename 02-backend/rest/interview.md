# REST API Interview Questions & Answers

---

## Q1. What is REST and what are its core constraints?

REST (Representational State Transfer) is an architectural style for distributed hypermedia systems, described by Roy Fielding in his 2000 doctoral dissertation. It is not a protocol — it is a set of design constraints.

The six constraints are:

1. **Client-Server**: The client and server are separate, communicating over a uniform interface. They can evolve independently.
2. **Statelessness**: Each request must contain all information needed to process it. The server holds no session state between requests. This enables horizontal scaling.
3. **Cacheability**: Responses must declare whether they are cacheable, allowing clients and intermediaries to reuse responses.
4. **Uniform Interface**: The defining constraint. Simplifies and decouples the architecture. Requires resource-based URIs, manipulation through representations, self-descriptive messages, and HATEOAS.
5. **Layered System**: A client cannot tell whether it is talking to the origin server or an intermediary (load balancer, CDN).
6. **Code on Demand** (optional): Servers can send executable code to clients.

An API that satisfies all constraints (except the optional one) is described as "RESTful." In practice, most "REST APIs" are really HTTP APIs that follow some REST conventions but not all of them (particularly HATEOAS).

---

## Q2. What is the difference between PUT and PATCH?

Both modify an existing resource, but they have different semantics:

**PUT** replaces the entire resource with the representation sent in the request body. If a field is absent from the body, it is set to null or its default. PUT is idempotent.

```
PUT /users/42
{ "name": "Alice", "email": "alice@example.com", "role": "admin" }
# All three fields are required; omitting "role" would clear it
```

**PATCH** applies a partial update. Only the fields included in the request are modified; others remain unchanged. PATCH is not guaranteed to be idempotent (e.g., "increment counter by 1" is not idempotent).

```
PATCH /users/42
{ "name": "Alice Smith" }
# Only name changes; email and role are untouched
```

When to use which:
- Use PUT when the client is sending a complete replacement (e.g., uploading a file, saving a full form)
- Use PATCH for partial updates (e.g., updating a single profile field)

In practice, PATCH is more commonly used in modern APIs because it is more efficient and less error-prone.

---

## Q3. What is idempotency and why does it matter in REST APIs?

An operation is idempotent if executing it multiple times produces the same server state as executing it once.

Idempotency matters because networks are unreliable. Clients time out and retry requests without knowing if the original request succeeded. If retrying an idempotent operation is safe, you get fault tolerance for free.

Idempotency by HTTP method:
- **GET, HEAD, OPTIONS**: idempotent (read-only)
- **PUT**: idempotent (same full replacement produces same state)
- **DELETE**: idempotent (resource is absent whether you delete once or ten times)
- **POST**: not idempotent (may create duplicate resources)
- **PATCH**: usually not idempotent (depends on the patch semantics)

For non-idempotent operations that must be retry-safe (payment processing, email sending), use **idempotency keys**: the client sends a unique `Idempotency-Key` header, and the server stores the result and returns it on duplicate submissions instead of reprocessing.

---

## Q4. How would you design pagination for a REST API? What are the tradeoffs between offset and cursor pagination?

There are three main approaches:

**Offset/Limit**
```
GET /orders?offset=40&limit=20
```
Simple to implement with SQL `LIMIT`/`OFFSET`. Allows random access to any page. The major problems are: (1) instability — inserts or deletes between pages can cause records to be skipped or duplicated, and (2) performance — large offsets require scanning and discarding rows.

**Cursor-Based**
```
GET /orders?after=eyJpZCI6NDB9&limit=20
```
The cursor is an opaque token (usually base64-encoded last-item ID or composite sort key) returned with each response. Efficient — the query becomes `WHERE id > lastId`, which uses an index. Stable under concurrent writes. Cannot jump to an arbitrary page. Best for infinite scroll and real-time feeds.

**Keyset**
A transparent form of cursor pagination where the actual column value is passed:
```
GET /orders?after_id=40&limit=20
```
Same performance benefits as cursor pagination but couples the client to the data model.

For most production APIs, cursor pagination is preferred for large datasets. Offset pagination is acceptable for small datasets or admin UIs where users need to jump between pages.

Always fetch `limit + 1` records and use the extra record's existence to determine `hasNextPage` without an expensive `COUNT(*)` query.

---

## Q5. How do you handle API versioning? What are the tradeoffs?

Three main strategies:

**URL Path Versioning**: `/api/v1/users`
- Pros: immediately visible in URLs and logs, easy to route at the gateway, simple to test
- Cons: technically impure (the resource hasn't changed, only its representation), requires maintaining parallel URL trees

**Header Versioning**: `Accept: application/vnd.myapi.v2+json`
- Pros: stable URLs, aligns with REST principles (content negotiation)
- Cons: not discoverable, harder to test in a browser, complicates caching (must vary on the header)

**Query Parameter**: `/api/users?version=2`
- Rarely recommended; pollutes query strings and is semantically wrong

Most teams choose URL versioning for its simplicity and discoverability. The theoretical purity benefit of header versioning rarely justifies the operational complexity.

Best practices regardless of strategy:
- Only bump the major version for breaking changes (field removal, type changes, auth changes)
- Non-breaking additions (new optional fields) do not require a new version
- Signal upcoming deprecation with `Sunset` and `Deprecation` headers
- Give clients a defined migration window (typically 6–12 months)

---

## Q6. What HTTP status codes should a REST API use, and what is the difference between 401 and 403?

Core status codes every REST API should use correctly:

- `200 OK` — successful GET, PUT, PATCH
- `201 Created` — successful POST, include `Location` header
- `204 No Content` — successful DELETE or update with no body
- `400 Bad Request` — malformed syntax, invalid parameters
- `401 Unauthorized` — not authenticated (despite the name: "I don't know who you are")
- `403 Forbidden` — authenticated but not authorized ("I know who you are; you can't do this")
- `404 Not Found` — resource does not exist
- `409 Conflict` — duplicate key, optimistic lock failure, business rule conflict
- `422 Unprocessable Entity` — well-formed request but semantically invalid
- `429 Too Many Requests` — rate limit exceeded
- `500 Internal Server Error` — unexpected server failure

**401 vs 403**: The key distinction is authentication vs authorization.
- `401` means the request lacks valid authentication credentials. The server is saying: "Send credentials and try again." This triggers a login flow.
- `403` means credentials are valid but the authenticated user does not have permission. No amount of re-authentication will help.

Using them correctly matters for client behavior: a `401` should trigger token refresh or re-login; a `403` should show an "access denied" message.

---

## Q7. What should a consistent error response look like?

Errors should be as structured and predictable as success responses. Clients need to handle errors programmatically.

Minimum required fields:
```json
{
  "error": {
    "code": "VALIDATION_ERROR",
    "message": "The request body contains invalid fields.",
    "details": [
      { "field": "email", "message": "Must be a valid email address" },
      { "field": "age",   "message": "Must be a positive integer" }
    ]
  }
}
```

`code` is a machine-readable string constant (not just the HTTP status number). Clients can switch on it. `message` is human-readable. `details` provides field-level breakdowns for validation errors.

RFC 7807 (Problem Details for HTTP APIs) standardizes this with `application/problem+json`:
```json
{
  "type": "https://api.example.com/errors/validation-error",
  "title": "Validation Error",
  "status": 422,
  "detail": "Field 'email' is required.",
  "instance": "/users"
}
```

Critical rules:
- Always return `Content-Type: application/json` for errors
- Never expose stack traces, internal paths, or SQL queries in production errors
- Log the full error internally; return only a sanitized message to the client
- Include a `requestId` so clients can provide it when reporting issues

---

## Q8. What is HATEOAS and when is it worth implementing?

HATEOAS (Hypermedia as the Engine of Application State) is the REST constraint that responses include hypermedia links describing what actions the client can take next. The client navigates the API by following links, not by constructing URLs from external documentation.

Example:
```json
{
  "id": 99,
  "status": "pending",
  "_links": {
    "self":    { "href": "/orders/99" },
    "pay":     { "href": "/orders/99/payments", "method": "POST" },
    "cancel":  { "href": "/orders/99", "method": "DELETE" },
    "items":   { "href": "/orders/99/items" }
  }
}
```

The `pay` and `cancel` links only appear when those actions are valid given the current state. This pushes business logic to the server and makes the client dumber.

**Worth it when:**
- The API is public and consumed by many independent teams
- URL structure may change and you cannot coordinate client updates
- The resource has a complex state machine where available actions vary by state

**Usually not worth it when:**
- You own both the API and all clients
- The team lacks time to maintain hypermedia properly
- The API is simple CRUD

In practice, very few production APIs implement full HATEOAS. A practical middle ground is to include `_links` only for pagination (`next`, `prev`) and related resources, without full state-machine link sets.

---

## Q9. How would you design filtering and sorting for a collection endpoint?

**Filtering**: use query parameters named after the field:
```
GET /products?status=active&category=electronics&minPrice=50&maxPrice=500
```

For multiple values: either repeated parameters (`?status=active&status=pending`) or comma-separated (`?status=active,pending`). Pick one and document it.

**Sorting**: use a `sort` parameter with a `-` prefix for descending:
```
GET /products?sort=-price,name
# price DESC, then name ASC
```

Always whitelist allowed sort fields on the server. Never interpolate the sort field directly into SQL — use a mapping from allowed string names to column names.

```typescript
const SORT_FIELD_MAP: Record<string, string> = {
  price:     'products.price',
  name:      'products.name',
  createdAt: 'products.created_at',
};

const column = SORT_FIELD_MAP[requestedField];
if (!column) throw new Error('Invalid sort field');
```

**Full-text search**: use a dedicated `q` parameter:
```
GET /products?q=wireless+headphones
```

Keep filtering simple for public APIs. Structured filter syntax (`filter[status]=active`) is powerful but adds client complexity and is rarely needed outside developer-focused APIs.

---

## Q10. How do rate limiting headers work, and what happens when a client hits the rate limit?

Rate limiting headers communicate the current limit state with every response, so clients can proactively slow down before hitting the limit.

Standard headers:
```
X-RateLimit-Limit: 1000       # requests allowed per window
X-RateLimit-Remaining: 743    # requests left in the current window
X-RateLimit-Reset: 1705312800 # Unix timestamp when the window resets
```

When the limit is exceeded, the server returns `429 Too Many Requests` with a `Retry-After` header:
```
HTTP/1.1 429 Too Many Requests
Retry-After: 847
X-RateLimit-Limit: 1000
X-RateLimit-Remaining: 0
X-RateLimit-Reset: 1705312800
```

`Retry-After` is either a number of seconds or an HTTP date.

Well-behaved clients should:
1. Check `X-RateLimit-Remaining` before each request and slow down proactively
2. On receiving `429`, wait the `Retry-After` duration before retrying
3. Use exponential backoff with jitter if retrying without a `Retry-After` header

Rate limits are typically applied per API key, per user, or per IP. Most production APIs use the token bucket or sliding window algorithm to allow controlled bursting while protecting the backend.

---

## Q11. What is the difference between a "safe" and an "idempotent" method?

These are distinct properties defined in RFC 9110:

**Safe** means the method does not modify server state. The client can call a safe method without side effects. GET, HEAD, and OPTIONS are safe. A method being safe implies it is also idempotent.

**Idempotent** means calling the method multiple times produces the same server state as calling it once. The state after N calls is identical to the state after 1 call. GET, HEAD, OPTIONS, PUT, and DELETE are idempotent. POST is not.

Key distinction: safety is about whether state is modified at all. Idempotency is about whether multiple calls produce the same state as one call.

DELETE is idempotent but not safe — it modifies state (removes the resource), but calling it twice leaves the same state (resource is absent). The second call may return `404`, but the server state is identical.

PATCH is typically not idempotent because patches may contain relative operations ("append to list", "increment by 1") that are not safe to retry.

This distinction matters for HTTP infrastructure: caches, proxies, and retry logic treat safe and idempotent methods differently. Browsers automatically retry idempotent requests on network failure but not POST.

---

## Q12. How would you design a REST endpoint to handle a non-CRUD operation, like "publish an article" or "send an invoice"?

Non-CRUD actions are the most common source of poorly designed REST APIs. There are three main approaches:

**1. Model the action as a sub-resource (preferred)**

Think about what the action creates or modifies in the system, and model that as a resource:

```
# Publish an article → create a publication event
POST /articles/42/publications

# Send an invoice → create an invoice delivery
POST /invoices/99/deliveries

# Activate a user account → create an activation
POST /users/42/activations

# Cancel an order → transition its status
POST /orders/15/cancellations
```

This works well for state transitions. The sub-resource represents the event or state change.

**2. Use PATCH to change state directly**

```
PATCH /articles/42
{ "status": "published" }

PATCH /users/42
{ "status": "active" }
```

Clean and simple. Works when the state change has no additional data. Less explicit — the semantics of PATCH are general, not specific to the action.

**3. RPC-style endpoints as a last resort**

```
POST /articles/42/publish
POST /invoices/99/send
```

Breaks the uniform interface principle (verbs in URLs) but is simple and readable. Acceptable for internal APIs or when the above approaches feel forced.

**Guidance**: prefer option 1 for public APIs because it models history (you can retrieve past publications), is consistent with REST principles, and scales to complex workflows. Use option 2 for simple flag-flipping. Reserve option 3 for complex operations with no natural resource model.
