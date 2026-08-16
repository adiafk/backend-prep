# Backend Engineering Interview Questions

## HTTP & Networking

**Q: What happens when you type a URL into a browser and press Enter?**
1. Browser checks local cache for DNS record. If miss, queries OS resolver → ISP DNS → authoritative DNS.
2. DNS returns the IP address.
3. Browser opens a TCP connection (three-way handshake: SYN → SYN-ACK → ACK).
4. If HTTPS, TLS handshake: exchange certificates, negotiate cipher, establish session keys.
5. Browser sends HTTP GET request.
6. Server processes request, sends HTTP response with status code and body.
7. Browser parses HTML, discovers sub-resources (CSS, JS, images), fetches them in parallel.
8. JavaScript executes, DOM is built, page renders.

**Q: What is the difference between HTTP/1.1, HTTP/2, and HTTP/3?**
HTTP/1.1: one request per TCP connection (with keep-alive, connection reuse but still sequential). Head-of-line blocking.
HTTP/2: multiplexing — multiple requests/responses over a single TCP connection simultaneously. Binary framing, header compression (HPACK), server push. Head-of-line blocking at TCP level.
HTTP/3: built on QUIC (UDP-based). Eliminates TCP head-of-line blocking. Faster handshake (0-RTT resumption). Better on unreliable networks (mobile).

**Q: What is CORS and why does it exist?**
Cross-Origin Resource Sharing is a browser security mechanism enforcing the same-origin policy. A browser script from `app.com` cannot read responses from `api.com` unless `api.com` explicitly allows it via `Access-Control-Allow-Origin` headers. CORS only applies to browsers — curl and server-to-server calls are unaffected. For mutation requests (POST/PUT/DELETE), browsers send a preflight OPTIONS request first to check if the server permits the actual request.

---

## Authentication & Security

**Q: JWT vs Sessions — when would you choose each?**
Sessions: store state server-side, send opaque ID to client. Easy to invalidate (delete the session). Requires shared session store (Redis) for multi-server setups. Better for: monoliths, when you need instant logout.
JWT: stateless, state encoded in the token. No DB lookup needed to verify (just signature check). Hard to invalidate before expiry without a blocklist. Better for: microservices where multiple services need to verify auth without calling a central auth service, mobile apps.

**Q: Explain the OAuth2 authorization code flow with PKCE.**
PKCE (Proof Key for Code Exchange) prevents authorization code interception attacks.
1. App generates `code_verifier` (random string) and `code_challenge = SHA256(code_verifier)`.
2. App redirects user to auth server with `code_challenge` in the URL.
3. User authenticates, auth server redirects back with `authorization_code`.
4. App exchanges code for tokens by sending `code_verifier` — auth server hashes it and compares to stored `code_challenge`.
An attacker who intercepts the authorization code can't exchange it without the original `code_verifier`.

**Q: How does bcrypt work and why is it used for passwords?**
bcrypt is a slow hashing algorithm designed for passwords. It incorporates a salt (random data) to prevent rainbow table attacks, and a cost factor that controls how slow it is. As hardware gets faster, you increase the cost factor — the hash remains hard to crack. Never use MD5, SHA-1, or even SHA-256 directly for passwords — they're fast by design, which makes brute-force easy.

---

## Databases & Caching

**Q: When would you use a cache and what are the risks?**
Use a cache when: data is read far more often than written, data is expensive to compute or fetch (DB query, external API), some staleness is acceptable. Risks: cache invalidation — stale data served after an update. Cache stampede — cache expires, many requests hit the DB simultaneously before anyone re-caches. Solutions: probabilistic early expiration, locking, background refresh.

**Q: What is the N+1 query problem?**
Fetching a list of N objects and then making one DB query per object to fetch a related entity = 1 + N queries. Example: fetch 100 orders, then fetch the user for each order = 101 queries. Fix: use a JOIN or an ORM's eager loading (`include`/`populate`). N+1 is often the single biggest cause of slow APIs.

**Q: Explain database connection pooling.**
Opening a new DB connection takes 10-50ms and consumes server resources. A connection pool maintains a set of reusable connections. The app checks out a connection from the pool, uses it, returns it. Pool size: typically 10-20 connections per app instance. Too few: requests queue waiting for a connection. Too many: DB runs out of connection slots (PostgreSQL default max_connections=100). Rule: `pool_size × instances ≤ max_connections - 10 (for migrations, psql, etc.)`.

---

## Distributed Systems & Queues

**Q: What does "idempotent" mean and why does it matter?**
An operation is idempotent if applying it multiple times produces the same result as applying it once. GET is idempotent; POST is not by default. Idempotency matters in distributed systems because: network failures may mean a request succeeds but the client doesn't receive the response, causing a retry. If the operation isn't idempotent, the retry causes a duplicate (double charge, double send). Solution: idempotency keys — client sends a unique key; server deduplicates by checking if the key was already processed.

**Q: What is backpressure in a queue system?**
Backpressure is a mechanism for a slow consumer to signal to a fast producer to slow down. Without backpressure, a fast producer overwhelms a slow consumer, filling queues unboundedly until memory exhausts. In BullMQ: limit worker concurrency (`{ concurrency: 5 }`). In Node.js streams: `readable.pipe(writable)` handles backpressure automatically — when the writable buffer is full, the readable pauses.

**Q: Explain the circuit breaker pattern.**
A circuit breaker prevents an application from repeatedly calling a service that is failing. States: Closed (normal), Open (fast-fail all calls), Half-Open (allow a probe call to check recovery). When error rate exceeds threshold, breaker opens — callers get an immediate error instead of waiting for timeouts. After a cooldown, it half-opens to test recovery. Prevents cascading failures where a slow dependency causes all your threads to block waiting.

---

## Rate Limiting

**Q: What are the tradeoffs between fixed window and sliding window rate limiting?**
Fixed window: reset counter at fixed intervals (e.g. every minute). Simple, but allows burst: a client can make 100 requests at 11:59 and 100 more at 12:00 — 200 requests in 2 seconds. Sliding window: track every request timestamp, count requests in the last N seconds. No burst at window boundaries, but more memory per user (O(requests) vs O(1)). Token bucket: smooth out bursts while allowing short-term bursts up to bucket size — best for per-user rate limiting.
