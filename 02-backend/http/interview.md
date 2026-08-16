# HTTP — Interview Questions and Answers

15 questions covering beginner, intermediate, and advanced HTTP topics commonly asked in backend engineering interviews.

---

## Beginner

---

### Q1. What is the difference between GET and POST?

**GET** is used to retrieve a resource. It is safe (no side effects) and idempotent (calling it multiple times has the same result). The request has no body — all parameters go in the URL (query string). GET responses are cacheable by default.

**POST** is used to submit data to the server, typically to create a resource. It is neither safe nor idempotent — sending the same POST twice can create two records. POST has a request body. POST responses are not cacheable by default.

In practice, people sometimes use GET for actions (bad) or POST for everything (also bad). REST conventions matter because they allow caches, proxies, and other tools to behave correctly.

Other key distinctions:
- GET requests are logged in browser history and server access logs, including query parameters. Sensitive data should never go in query strings.
- Browsers will not re-submit a POST automatically on page refresh (they prompt the user). This is why after form submissions you often redirect to a GET (Post/Redirect/Get pattern).
- `PUT` replaces a resource entirely. `PATCH` applies a partial update. `DELETE` removes a resource.

---

### Q2. What does a 401 status code mean, and how is it different from 403?

**401 Unauthorized** means the request lacks valid authentication credentials. Despite the name "unauthorized," it really means *unauthenticated*. The server doesn't know who you are. The response should include a `WWW-Authenticate` header describing how to authenticate (e.g., `WWW-Authenticate: Bearer realm="api"`). The right action is to log in and try again.

**403 Forbidden** means the server knows who you are (you are authenticated), but you are not allowed to perform this action. The right action is to stop trying — you won't get access by re-authenticating.

Example:
- Accessing `/admin` with no token → 401
- Accessing `/admin` with a valid token but you're not an admin → 403
- Accessing `/users/5` with a valid token but that user doesn't exist → 404 (or sometimes 403 if you want to hide the existence)

---

### Q3. What is HTTPS and why does it matter?

HTTPS is HTTP transported over a TLS (Transport Layer Security) connection. It runs on port 443 by default.

TLS provides three things:
1. **Confidentiality** — the data is encrypted. A network observer (ISP, someone on your WiFi, a man-in-the-middle) cannot read the request or response.
2. **Integrity** — the data cannot be tampered with in transit without detection. A MITM cannot modify your API request.
3. **Authentication** — the client verifies it's talking to the real server and not an impostor, via the server's certificate signed by a trusted Certificate Authority.

Without HTTPS:
- API keys, tokens, passwords in request headers are sent as plaintext
- A MITM can modify HTML/JavaScript to inject malicious code
- DNS spoofing + HTTP = easy phishing attacks

Modern browsers mark non-HTTPS sites as "Not Secure." Service workers, geolocation, camera/mic access, and many other APIs require HTTPS. Search engines penalize non-HTTPS sites.

---

### Q4. What is a status code, and what does each 2xx code mean?

A status code is a three-digit integer in every HTTP response that tells the client the outcome of its request. The first digit indicates the class.

The important 2xx codes (success):

- **200 OK** — the request succeeded. Default for successful GET, PUT, PATCH responses.
- **201 Created** — a new resource was created (typically a response to POST). Include a `Location` header pointing to the new resource URL.
- **202 Accepted** — the request was accepted but processing is asynchronous. The work is queued; the client should poll or wait for a webhook.
- **204 No Content** — success but there is no response body. Standard for DELETE and sometimes PUT when you don't return the updated resource. Do not include a body with 204.
- **206 Partial Content** — the server is returning only part of the resource, in response to a `Range` request header. Used for video streaming and resumable downloads.

---

### Q5. What are HTTP headers and what are they used for?

HTTP headers are key-value metadata attached to every request and response. They control behavior at many layers: content negotiation, caching, authentication, CORS, compression, cookies, security, and more.

Common request headers:
- `Host` — the domain of the server (required in HTTP/1.1)
- `Content-Type` — the format of the request body (`application/json`, `multipart/form-data`)
- `Authorization` — authentication credentials (`Bearer token`, `Basic base64`)
- `Accept` — formats the client can handle
- `Accept-Encoding` — compression algorithms the client supports
- `Cookie` — cookies attached to the request
- `Cache-Control: no-cache` — bypass cached responses

Common response headers:
- `Content-Type` — format of the response body
- `Cache-Control` — how long and by whom the response can be cached
- `Set-Cookie` — set a cookie in the browser
- `Location` — redirect target URL or newly created resource URL
- `ETag` — version fingerprint for conditional caching

Headers are case-insensitive per the HTTP spec. Node.js normalizes them to lowercase.

---

## Intermediate

---

### Q6. What is CORS and how does it work?

CORS (Cross-Origin Resource Sharing) is a browser security mechanism that controls which cross-origin JavaScript requests are allowed.

An **origin** is scheme + hostname + port. `https://app.example.com` and `https://api.example.com` are different origins.

**Why it exists:** Without CORS restrictions, JavaScript from any page could make requests to any other origin using the visitor's credentials (cookies, auth headers). Attacker.com could make an authenticated request to your-bank.com using your cookies and read the response.

CORS is enforced by the browser, not the server. The server just has to declare what it allows. Non-browser clients (curl, server-to-server) ignore CORS.

**How it works:**
1. The browser adds an `Origin` header to cross-origin requests.
2. If the request is "simple" (GET/POST/HEAD with standard headers and content types), the browser makes it directly and checks the response for `Access-Control-Allow-Origin`.
3. If the request is not simple (custom headers like `Authorization`, methods like PUT/DELETE, or `Content-Type: application/json`), the browser sends a **preflight** — an `OPTIONS` request first.
4. The server must respond to the preflight with appropriate `Access-Control-Allow-*` headers.
5. If the preflight passes, the browser sends the real request.

The server must include `Access-Control-Allow-Origin` on the actual response too, not just the preflight.

To allow credentials (cookies, Authorization headers), you cannot use `*` for `Allow-Origin`. You must reflect the specific origin and set `Access-Control-Allow-Credentials: true`.

---

### Q7. What is the difference between HTTP/1.1 and HTTP/2?

Both are semantically identical — same methods, headers, status codes, and URIs. The difference is in how they transport data.

**HTTP/1.1:**
- Text-based protocol
- One outstanding request per connection (head-of-line blocking at the application level)
- Browsers work around this by opening 6–8 parallel connections per origin
- No header compression — every request sends full headers
- Keep-alive allows connection reuse across sequential requests

**HTTP/2:**
- Binary framing layer — more efficient to parse
- **Multiplexing** — multiple requests and responses interleave on a single TCP connection simultaneously. No application-level head-of-line blocking.
- **HPACK header compression** — headers are compressed using a shared dictionary. Repeated headers (like `User-Agent`, `Cookie`) become very cheap after the first request.
- **Server push** — server can proactively send resources the client hasn't asked for yet (rarely used in practice)
- Stream prioritization

HTTP/2 still runs over TCP, so packet loss causes TCP-level head-of-line blocking across all streams. HTTP/3 solves this by using QUIC over UDP.

**Practical impact:** HTTP/2 reduces the need to bundle all JavaScript into one file, inline CSS, use CSS sprites, and other HTTP/1.1 performance tricks. With multiplexing, many small files often perform as well as one large bundle.

---

### Q8. Explain the HTTP caching model — Cache-Control, ETag, and 304 responses.

HTTP caching has two phases: deciding whether to cache, and validating whether a cached copy is still fresh.

**Cache-Control** controls whether and how long a response is cached:
- `max-age=N` — cache for N seconds
- `public` — any cache (CDN, proxy, browser) may store this
- `private` — only the browser may store this (not CDNs)
- `no-cache` — store it, but revalidate before every use
- `no-store` — don't cache at all
- `immutable` — content at this URL will never change (for fingerprinted assets)

**ETag** is a fingerprint (usually a hash) of the response content. The server includes it in the response:
```
ETag: "abc123hash"
```

When the cached response expires (or `no-cache` is set), the browser makes a **conditional request** instead of requesting the full resource:
```
If-None-Match: "abc123hash"
```

If the resource hasn't changed, the server responds with `304 Not Modified` and no body. The browser uses its cached copy. This saves bandwidth while still verifying freshness.

If the resource changed, the server responds `200 OK` with the new content and a new ETag.

`Last-Modified` / `If-Modified-Since` is the timestamp-based equivalent. ETags are preferred because timestamps have 1-second granularity.

**Correct caching by data type:**
- API responses with user data: `private, max-age=60`
- Public API data: `public, max-age=300, s-maxage=3600`
- Fingerprinted static assets: `public, max-age=31536000, immutable`
- Auth/sensitive endpoints: `no-store`

---

### Q9. What is the DNS resolution process?

When you navigate to `api.example.com`, the browser needs to find its IP address.

**Step-by-step:**
1. **Browser DNS cache** — check if we already resolved this recently and it hasn't expired. If yes, done.
2. **OS cache and /etc/hosts** — the OS resolver checks its cache and the local `/etc/hosts` file.
3. **Recursive resolver** — the OS sends a query to the configured DNS resolver (Google's 8.8.8.8, Cloudflare's 1.1.1.1, or your ISP's resolver). This resolver will do the recursive work on your behalf.
4. **Root nameservers** — if the recursive resolver doesn't have it cached, it queries a root nameserver. The root server doesn't know the IP but knows which servers handle `.com` TLD — it returns NS records for the `.com` TLD servers.
5. **TLD nameservers** — the recursive resolver queries the `.com` TLD server, which returns the authoritative nameservers for `example.com`.
6. **Authoritative nameserver** — the recursive resolver queries `example.com`'s nameserver, which returns the A record (IP address) for `api.example.com`.
7. **Cache and respond** — the recursive resolver caches the answer for the TTL duration and returns it to the client.

**TTL** (Time to Live) controls how long each record is cached. Short TTL means changes propagate faster but adds DNS lookup load. Long TTL means faster cached lookups but slower propagation.

Before migrating to a new IP, lower TTL to 60 seconds at least 24 hours in advance so caches expire quickly after the change.

---

### Q10. Explain the TCP three-way handshake and why it matters for HTTP performance.

TCP is connection-oriented. Before any data can flow, the client and server establish a connection:

1. **SYN** — client sends a SYN packet with a random sequence number (e.g., seq=1000). "I want to connect."
2. **SYN-ACK** — server responds with SYN-ACK, acknowledging the client's sequence number (ack=1001) and sending its own sequence number (seq=2000). "OK, I'm ready."
3. **ACK** — client acknowledges the server's sequence number (ack=2001). "Got it, connection established."

Only after this exchange can the first byte of application data flow. This takes 1 round-trip time (RTT).

**Why it matters for HTTP:**
- A request to `https://api.example.com` for a user in Australia (300ms RTT to a US server) costs: 1 RTT TCP handshake + 1 RTT TLS handshake + 1 RTT for the HTTP request = 900ms before you see any data.
- HTTP keep-alive amortizes the TCP handshake cost across many requests on the same connection.
- HTTP/2 multiplexes many requests over a single connection, so you pay the handshake cost once.
- HTTP/3 uses QUIC which combines the TCP-equivalent and TLS handshake, getting to data in 1 RTT (or 0-RTT for returning clients).

TCP also has **slow start**: new connections don't know the available bandwidth, so they start by sending a small amount of data and ramp up. Long-lived, reused connections have already ramped up and can immediately use full bandwidth.

---

## Advanced

---

### Q11. What is TCP head-of-line blocking and how does HTTP/3 solve it?

**Head-of-line blocking** occurs when one slow or lost message blocks all subsequent messages behind it.

HTTP/1.1 has application-level HOL blocking: within a single connection, you can only have one outstanding request. Request B can't be sent until request A gets a response. Browsers open multiple parallel connections to work around this, but each connection has TCP slow start overhead.

HTTP/2 solved application-level HOL blocking with multiplexing — many streams share one connection and don't wait for each other at the HTTP layer. However, they still share one TCP connection. TCP sees the connection as a single ordered byte stream. If one TCP packet is lost, TCP's retransmission logic holds all data until that packet is recovered — even data for completely unrelated HTTP/2 streams. This is **TCP-level HOL blocking**. Under high packet loss conditions (mobile networks), HTTP/2 can actually perform worse than HTTP/1.1 because all streams stall together.

HTTP/3 runs over **QUIC**, a transport protocol built on UDP. QUIC implements its own reliability mechanism, but independently per stream. A lost UDP packet only stalls the one stream it belongs to. Other streams continue unaffected. This eliminates HOL blocking at both the application and transport level.

Additional QUIC advantages:
- 0-RTT reconnection for known servers (TLS state from previous session)
- Connection migration (the connection survives IP changes — useful on mobile)
- Faster connection establishment (TLS is built into QUIC, not layered on top)

The trade-off: QUIC on UDP is more expensive to implement, and many corporate firewalls block non-TCP/80/443 traffic. QUIC typically falls back to HTTP/2 in such environments.

---

### Q12. How would you design a CORS policy for an API that serves both browser clients and mobile apps, and needs to support cookies?

Mobile apps and server-to-server clients are not subject to browser CORS restrictions. CORS is purely a browser mechanism. For those clients, CORS headers are irrelevant.

For browser clients requiring cookies (e.g., session cookies for an authenticated web app):

**Rules that must be followed:**
1. `Access-Control-Allow-Origin` must be the exact origin (not `*`). The browser refuses `*` when credentials are involved.
2. `Access-Control-Allow-Credentials: true` must be set.
3. The browser's fetch/XHR call must use `credentials: 'include'`.
4. The cookie must have `SameSite=None; Secure` to be sent cross-origin.

**Implementation strategy:**
```
Maintain an allowlist of trusted origins.
On each request:
  - Read the Origin header
  - If it's in the allowlist, reflect it back in Access-Control-Allow-Origin
  - Set Access-Control-Allow-Credentials: true
  - Set Vary: Origin (critical for CDN correctness)
  
Handle OPTIONS preflight:
  - Return 204 with Allow-Methods, Allow-Headers, Max-Age headers
  - Do not require authentication on OPTIONS requests
```

**Critical mistakes to avoid:**
- Returning `*` when `credentials: true` is needed — the browser will block the request
- Forgetting `Vary: Origin` — a CDN might cache a response for origin A and serve it to origin B, which then fails the CORS check
- Not adding CORS headers to error responses — a 4xx response without CORS headers means the browser blocks JS from reading the error details
- Setting CORS in a middleware that only runs for successful paths — errors bypass it

For a public API where browser clients don't need credentials, `Access-Control-Allow-Origin: *` is simpler and correct.

---

### Q13. Explain TLS 1.3 handshake in detail. What changed from TLS 1.2?

**TLS 1.2 handshake (2 RTTs):**
1. Client → ClientHello (cipher suites, random nonce)
2. Server → ServerHello + Certificate + ServerHelloDone
3. Client → ClientKeyExchange (sends encrypted pre-master secret using server's public key) + ChangeCipherSpec + Finished
4. Server → ChangeCipherSpec + Finished

Problems with TLS 1.2:
- 2 RTTs before data flows (on top of TCP's 1 RTT)
- RSA key exchange — if the server's private key is later compromised, all recorded past traffic can be decrypted (no forward secrecy by default, though ECDHE cipher suites added it)
- Many optional cipher suites, some weak (RC4, 3DES, MD5)

**TLS 1.3 handshake (1 RTT):**
1. **ClientHello** — the client sends: supported cipher suites, a random nonce, and its public key share for an ephemeral Diffie-Hellman key exchange. TLS 1.3 removed all non-forward-secret cipher suites, so DH is always used.

2. **ServerHello + Finished** — in the same network round trip, the server responds with: its DH key share, the selected cipher suite, its certificate, a signature proving it owns the certificate, and an encrypted `Finished` message using the derived session key.

Both sides can now independently compute the same session key from the DH exchange (the client's DH private key + server's DH public key = shared secret, and vice versa). The session key never travels on the wire.

3. **Client Finished** — client verifies the certificate, validates the server's signature, computes the session key, and sends its `Finished`. Application data can now flow.

**0-RTT:** For clients reconnecting to a server they've talked to before, TLS 1.3 supports pre-shared session tickets. The client can include application data in the very first packet (0-RTT). This is the fastest possible, but has a replay attack risk — that first message could be recorded and replayed. Only safe for idempotent requests (GET, but not POST).

**Key improvements over 1.2:**
- 1 RTT instead of 2 (0-RTT for returning clients)
- Forward secrecy always (ephemeral DH keys discarded after handshake — past traffic can't be decrypted even if private key is compromised)
- Fewer cipher suites (only 5 strong ones, all the weak ones removed)
- Encrypted certificate in handshake (privacy — observer can't see which certificate the server presented)

---

### Q14. You're seeing intermittent 502 and 504 errors in production. How do you debug them?

**502 Bad Gateway** means your load balancer or reverse proxy got an invalid or malformed response from the upstream backend. The upstream might have crashed, returned a partial response, or returned something the proxy didn't understand.

**504 Gateway Timeout** means the upstream didn't respond within the configured timeout.

**Debugging steps:**

1. **Determine the failure boundary.** Is it between client and nginx/ALB (network issue), or between nginx/ALB and your app server (backend issue)? Check access logs at each layer. Does the 502/504 appear in nginx logs with an upstream error? Or does nginx not even log the request?

2. **Check upstream server health.** Are app server processes running? CPU/memory exhausted? Out of file descriptors (`ulimit -n`)? Too many connections?

3. **Inspect timeout configuration.** 504s often mean a single slow endpoint or DB query that exceeds the proxy timeout. Check `proxy_read_timeout` (nginx), ALB idle timeout, or your app's own request timeout. Look for slow DB queries in slow query logs during the time window of errors.

4. **Check for connection pool exhaustion.** If your app connects to a database or another service, is the connection pool saturated? New requests queue up, proxy times out waiting. Check pool metrics.

5. **Look for deploys/restarts.** 502s often spike during deploys when old processes die before new ones are ready. Implement graceful shutdown: drain in-flight requests before stopping the process. Configure load balancer health checks with appropriate intervals so traffic shifts before the old process disappears.

6. **Memory and crash loops.** 502s can be caused by the upstream process crashing under load. Check application crash logs. OOM kills (`dmesg | grep -i kill`). Heap dumps.

7. **Correlation with traffic patterns.** Does it happen at peak traffic? Connection pool exhaustion. Random? Possible upstream instability or memory leak. At regular intervals? Cron job consuming resources or DB maintenance window.

8. **Timeouts to fix:** Increase `proxy_read_timeout` if legitimate requests take a long time, or reduce endpoint latency. Add response timeouts to your app so slow endpoints fail fast instead of holding connections until the proxy gives up.

---

### Q15. How do ETags work, and what are the pitfalls of generating them incorrectly in a distributed system?

**How ETags work:**
An ETag is a version identifier for a response. When a client has a cached response with an ETag, it sends `If-None-Match: "etag-value"` on the next request. If the server computes the same ETag for the current state of the resource, it returns `304 Not Modified` with no body (saving bandwidth). If the resource changed, it returns `200` with the new content and new ETag.

ETags can be **strong** (`"abc123"`) or **weak** (`W/"abc123"`). Strong ETags guarantee byte-for-byte identity. Weak ETags say the representations are semantically equivalent but may differ byte-for-byte (e.g., whitespace differences in JSON).

**Common pitfall — non-deterministic ETags:**
If you generate ETags by hashing the response body, make sure serialization is deterministic. In JavaScript, `JSON.stringify({b: 1, a: 2})` and `JSON.stringify({a: 2, b: 1})` may produce different strings even though the objects are identical. Object key order in JSON.stringify is insertion-order in V8 but not guaranteed across all environments or when data comes from database rows.

Fix: either sort keys before serialization, use the resource's `updatedAt` timestamp as the ETag (combined with the resource ID and version), or use the database row's modification counter.

**Distributed system pitfall — inconsistent ETags across instances:**
If you have 5 app servers and each hashes the response body independently, the ETag should be the same for the same data. But if:
- The serialization order differs (different DB query execution plans returning columns in different order)
- Server clocks are used as ETag components and drift between nodes
- The response includes machine-specific data (hostname, local timestamp)

...then the same resource returns different ETags from different servers. The client's cached ETag becomes invalid after a single server switch, defeating caching.

Fix: generate ETags from the resource's stable identifiers — its ID, version number, or `updatedAt` timestamp from the database, not from hashing the response body. These are consistent across all servers because they come from the same source of truth.

**Strong ETags and range requests:**
Strong ETags must be used for Range requests (partial content delivery for video streaming, resumable downloads). The client requests byte ranges and the server must guarantee the content at those byte positions is identical to what was there when the initial ETag was generated. Weak ETags are not valid for range requests.

**The Last-Modified alternative:**
`Last-Modified` timestamps have 1-second granularity. If a resource is modified twice in one second, the second modification is invisible to conditional caching. ETags handle this correctly because they're content-based, not time-based. In high-write systems, always prefer ETags over Last-Modified.
