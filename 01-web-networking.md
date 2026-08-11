# 01 — Web & Networking

## 1. HTTP / HTTPS

### Simple explanation

HTTP is the application-layer protocol used for request/response communication on the web. HTTPS is HTTP carried over TLS, providing confidentiality, integrity, and server authentication.

A typical request looks like:

```text
Client -> DNS -> TCP/TLS -> HTTP request -> Server
Server -> HTTP response -> Client
```

### Know these

- Methods: GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS
- Headers, body, query parameters, path parameters
- Content-Type and content negotiation
- Keep-alive
- HTTP/1.1, HTTP/2, HTTP/3
- TLS certificates and handshake
- TCP vs UDP
- QUIC

### Interview questions

**Q: What happens when you open an HTTPS URL?**

**Answer:** The client resolves DNS, establishes network connectivity, negotiates TLS, then sends an HTTP request. The server processes it and returns a response. Explain DNS caching, TCP/QUIC, TLS, routing, load balancing, application processing, and the response if asked to go deeper.

**Q: Why does HTTPS use asymmetric and symmetric cryptography?**

**Answer:** Asymmetric cryptography is useful for authenticating the server and securely establishing key material; symmetric encryption is then used for the actual data because it is much faster.

**Q: HTTP/1.1 vs HTTP/2?**

**Answer:** HTTP/2 introduces binary framing and multiplexing multiple streams over one connection, reducing head-of-line blocking at the HTTP layer and improving connection efficiency. HTTP/3 uses QUIC over UDP.

---

## 2. Status Codes

### 2xx

- 200 — success
- 201 — resource created
- 202 — accepted for asynchronous processing
- 204 — success with no response body

### 4xx

- 400 — malformed/invalid request
- 401 — missing/invalid authentication
- 403 — authenticated but not allowed
- 404 — resource not found
- 409 — state conflict
- 422 — semantically invalid input
- 429 — rate limit exceeded

### 5xx

- 500 — unexpected application failure
- 502 — gateway received an invalid upstream response
- 503 — service unavailable
- 504 — upstream timeout

**Interview trap:** 401 is not "permission denied"; 403 is normally the authorization failure.

---

## 3. REST API Design

Good REST design starts with resources and predictable semantics:

```text
GET    /users
GET    /users/:id
POST   /users
PATCH  /users/:id
DELETE /users/:id
```

Know:

- pagination
- filtering and sorting
- validation
- consistent error envelopes
- API versioning
- idempotency
- authentication/authorization
- rate limiting
- backward compatibility

### Idempotency

Repeating an idempotent operation should have the same intended effect. GET, PUT, and DELETE are generally idempotent; POST usually is not. For payments and webhooks, use an idempotency/deduplication key so retries cannot create duplicate effects.

---

## 4. RPC / gRPC

RPC lets a client call a remote service using a procedure-like contract. gRPC commonly uses Protocol Buffers and HTTP/2.

Know:

- service definitions
- generated stubs
- unary RPC
- client/server/bidirectional streaming
- deadlines and timeouts
- interceptors
- status codes
- strongly typed contracts

### REST vs gRPC

REST is often convenient for public/browser-facing APIs. gRPC is attractive for internal service-to-service communication where strong contracts, efficient serialization, and streaming matter.

---

## 5. Streaming

Compare:

- normal request/response
- chunked HTTP responses
- SSE
- WebSockets
- gRPC streaming
- LLM token streaming

Important concerns:

- buffering
- backpressure
- timeouts
- heartbeats
- reconnects
- partial failures
- connection limits

For an AI streaming endpoint, the server can emit partial model output instead of waiting for the complete generation.

---

## 6. CORS

CORS is a browser security mechanism built around the same-origin policy. The browser may send an OPTIONS preflight before a cross-origin request when the request is not considered simple.

Know:

- origin
- preflight
- OPTIONS
- `Access-Control-Allow-Origin`
- credentials
- cookies
- SameSite

CORS is primarily a browser enforcement mechanism; it is not a replacement for authentication or authorization.

---

## 7. DNS, IPs and Networking

Know:

- IPv4 / IPv6
- public/private IPs
- ports
- NAT
- subnets/CIDR
- firewalls
- DNS records: A, AAAA, CNAME, MX, TXT, NS
- TTL and caching
- TCP vs UDP
- reverse proxies

Basic DNS flow:

```text
Browser -> Resolver -> Root -> TLD -> Authoritative DNS -> IP
```

### Interview question

**Q: What is the difference between DNS and HTTP?**

**Answer:** DNS resolves a name to network addressing information; HTTP is the application protocol used after the client has enough network information to contact the server.

---

## 8. WebSockets

WebSockets provide persistent, bidirectional communication after an HTTP-based handshake.

Use them for things such as live dashboards, chat, presence, and interactive agent interfaces.

Scaling problem:

```text
Client A -> Instance 1
Client B -> Instance 2
```

If A needs to send a message to B, a shared messaging layer such as Redis Pub/Sub or Kafka can distribute events across instances.

Know:

- authentication
- connection lifecycle
- heartbeats
- reconnects
- backpressure
- horizontal scaling

---

## 9. Webhooks

A webhook is an HTTP callback generated by an external system when an event occurs.

Reliable webhook processing:

```text
Provider -> webhook endpoint -> verify -> dedupe -> queue -> worker -> DB
```

Never assume exactly-once delivery. Design for duplicates and retries.

Know:

- signature verification
- replay protection
- idempotency keys
- event IDs
- retries
- ordering
- dead-letter handling

---

## 10. Retries and Failure Handling

Use retries for transient failures, not permanent errors.

Good retry pattern:

```text
attempt
  -> exponential backoff
  -> jitter
  -> bounded attempts
  -> DLQ / failure state
```

Avoid retrying invalid input or authentication failures. Make retried operations idempotent.

### Follow-ups

- Why can retries make an outage worse?
- Why add jitter?
- What if the downstream operation succeeded but the response was lost?
- How would you prevent duplicate payments?
