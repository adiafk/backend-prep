# HTTP — Complete Developer Guide

HTTP (HyperText Transfer Protocol) is the foundation of data communication on the web. This guide covers everything from protocol versions to caching, with practical TypeScript/Node.js examples.

---

## Table of Contents

1. [HTTP/1.1 vs HTTP/2 vs HTTP/3](#1-http11-vs-http2-vs-http3)
2. [Request and Response Anatomy](#2-request-and-response-anatomy)
3. [Status Codes](#3-status-codes)
4. [HTTPS and TLS](#4-https-and-tls)
5. [CORS](#5-cors)
6. [DNS Resolution](#6-dns-resolution)
7. [Transport Layer — TCP vs UDP](#7-transport-layer--tcp-vs-udp)
8. [Keep-Alive and Connection Reuse](#8-keep-alive-and-connection-reuse)
9. [Compression](#9-compression)
10. [Caching Headers](#10-caching-headers)

---

## 1. HTTP/1.1 vs HTTP/2 vs HTTP/3

### HTTP/1.1 (1997)

HTTP/1.1 is a text-based protocol that introduced persistent connections (keep-alive) and chunked transfer encoding. Each request-response pair operates over TCP, and by default only one request can be in flight per connection at a time.

**Head-of-line blocking** is the core problem: if request A is slow, requests B and C behind it must wait on that same connection. Browsers work around this by opening 6–8 parallel TCP connections per origin, which wastes resources and causes TCP slow-start penalties.

Key characteristics:
- Text-based, human-readable
- Persistent connections with `Connection: keep-alive`
- No multiplexing — one outstanding request per connection
- Headers sent as plain text on every request (no compression)
- Browsers open multiple connections to work around blocking

**When it matters:** You are still likely to encounter HTTP/1.1 in legacy internal services, some proxy layers, and environments where HTTP/2 has not been deployed. Understanding it explains why bundling assets and reducing requests was critical pre-HTTP/2.

### HTTP/2 (2015)

HTTP/2 was designed to address HTTP/1.1's performance problems while remaining semantically identical — the same methods, status codes, and headers still mean the same thing.

**Key changes:**

- **Binary framing layer.** Instead of text, HTTP/2 breaks messages into binary frames. This is more efficient to parse and less error-prone.
- **Multiplexing.** Multiple requests and responses can be interleaved on a single TCP connection simultaneously. Request 1 doesn't have to wait for request 0 to finish.
- **Header compression (HPACK).** Headers are compressed using a shared dynamic table. Repeated headers (like `User-Agent` or `Cookie`) cost almost nothing after the first request.
- **Server push.** The server can preemptively send resources the client hasn't asked for yet (e.g., push `style.css` when the client requests `index.html`). In practice, server push has poor adoption and is largely unused.
- **Stream prioritization.** Clients can signal which responses are more important.

**Residual problem:** HTTP/2 still runs over TCP. If a single TCP packet is lost, TCP's congestion control pauses the entire connection — all streams on it stall. This is TCP-level head-of-line blocking, and HTTP/2 can't fix it.

**When it matters:** HTTP/2 is the default for most modern APIs and web servers today. You should ensure your production server (nginx, caddy, etc.) serves HTTP/2 for browser-facing traffic.

### HTTP/3 (2022)

HTTP/3 replaces TCP with **QUIC**, a protocol built on top of UDP. QUIC was developed by Google and standardized by the IETF.

**Key changes:**

- **QUIC instead of TCP.** QUIC implements its own reliable, ordered delivery per stream. A lost packet only stalls the stream it belongs to, not every stream on the connection. True end-to-end multiplexing without head-of-line blocking.
- **0-RTT and 1-RTT connection establishment.** TLS 1.3 is baked into QUIC. A new connection to a server you've talked to before can send data in the very first packet (0-RTT). TCP+TLS requires at minimum 2–3 round trips before data flows.
- **Connection migration.** QUIC uses connection IDs rather than IP:port tuples. When a mobile device switches from WiFi to LTE, the QUIC connection survives. TCP would break.
- **Built-in encryption.** All QUIC traffic is encrypted. There is no plain HTTP/3.

**When it matters:** HTTP/3 is most valuable for:
- Mobile users on unreliable networks (packet loss hurts TCP more than QUIC)
- High-latency connections where the reduced handshake time matters
- Real-time applications (gaming, video streaming, WebRTC-adjacent)

Most CDNs (Cloudflare, Fastly, AWS CloudFront) already serve HTTP/3. Node.js has experimental QUIC support. For most backend APIs today, HTTP/2 is the sweet spot.

### Comparison Table

| Feature | HTTP/1.1 | HTTP/2 | HTTP/3 |
|---|---|---|---|
| Transport | TCP | TCP | QUIC (UDP) |
| Format | Text | Binary | Binary |
| Multiplexing | No | Yes (per connection) | Yes (per stream) |
| Header compression | No | HPACK | QPACK |
| HOL blocking | TCP + app level | TCP level only | None |
| TLS required? | No | No (but browsers require it) | Yes (built-in) |
| 0-RTT reconnect | No | No | Yes |
| Connection migration | No | No | Yes |

---

## 2. Request and Response Anatomy

### HTTP Request Structure

```
METHOD /path?query=string HTTP/1.1
Host: api.example.com
Header-Name: Header-Value
Content-Type: application/json
Content-Length: 42

{"key": "value"}
```

A request has four parts:
1. **Request line** — method, path (with optional query string), HTTP version
2. **Headers** — key-value metadata, one per line
3. **Empty line** — blank line marking end of headers (CRLF)
4. **Body** — optional, only relevant for POST/PUT/PATCH

### HTTP Response Structure

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 56
Cache-Control: max-age=3600
ETag: "abc123"

{"id": 1, "name": "Alice", "email": "alice@example.com"}
```

A response has:
1. **Status line** — HTTP version, status code, reason phrase
2. **Response headers**
3. **Empty line**
4. **Body**

### HTTP Methods

| Method | Safe | Idempotent | Has Body | Use |
|---|---|---|---|---|
| GET | Yes | Yes | No | Retrieve a resource |
| POST | No | No | Yes | Create a resource or submit data |
| PUT | No | Yes | Yes | Replace a resource entirely |
| PATCH | No | No | Yes | Partial update |
| DELETE | No | Yes | No | Remove a resource |
| HEAD | Yes | Yes | No | Like GET but no response body — check headers |
| OPTIONS | Yes | Yes | No | List allowed methods, CORS preflight |
| TRACE | Yes | Yes | No | Diagnostic echo (rarely used, often disabled) |
| CONNECT | No | No | No | Establish a tunnel (used for HTTPS proxying) |

**Safe** means the request does not change server state. **Idempotent** means calling it multiple times has the same effect as calling it once.

A PUT to `/users/1` with the same body twice should produce the same result. A POST to `/orders` twice creates two orders.

### Important Request Headers

```
Host: api.example.com                          # Required in HTTP/1.1
Accept: application/json, text/html            # What formats the client accepts
Accept-Encoding: gzip, deflate, br             # Compression the client supports
Accept-Language: en-US,en;q=0.9               # Preferred languages
Authorization: Bearer eyJhbGciOiJIUzI1...      # Auth token
Content-Type: application/json                 # Format of the request body
Content-Length: 128                            # Byte length of body
Cookie: session=abc123; pref=dark              # Cookies
User-Agent: Mozilla/5.0 (...)                  # Client identifier
Referer: https://example.com/page             # Previous page URL
Origin: https://example.com                   # Origin for CORS
Cache-Control: no-cache                        # Caching directive
If-None-Match: "etag-value"                    # Conditional GET
If-Modified-Since: Wed, 21 Oct 2020 07:28:00   # Conditional GET
X-Request-ID: uuid-here                        # Tracing (custom header)
```

### Important Response Headers

```
Content-Type: application/json; charset=utf-8  # Format of response body
Content-Length: 256                            # Byte length of body
Content-Encoding: gzip                         # Applied compression
Cache-Control: max-age=3600, public            # Caching instructions
ETag: "33a64df551425fcc55e4d42a148795d9f25f89d" # Resource version fingerprint
Last-Modified: Wed, 21 Oct 2020 07:28:00 GMT  # When resource last changed
Location: /users/42                            # Redirect target or new resource URL
Set-Cookie: session=abc; HttpOnly; Secure      # Set a cookie
Access-Control-Allow-Origin: https://app.com  # CORS — allowed origin
Vary: Accept-Encoding                          # Which request headers affect response
X-RateLimit-Limit: 100                        # Rate limit cap
X-RateLimit-Remaining: 99                     # Remaining requests in window
Strict-Transport-Security: max-age=31536000   # HSTS — force HTTPS
X-Content-Type-Options: nosniff               # Security — no MIME sniffing
```

### Reading Request Headers in Node.js (TypeScript)

```typescript
import http from 'http';

const server = http.createServer((req, res) => {
  // All header names are lowercased in Node.js
  const contentType = req.headers['content-type'];
  const authorization = req.headers['authorization'];
  const userAgent = req.headers['user-agent'];

  // headers object is Record<string, string | string[]>
  // Some headers (like Set-Cookie) can appear multiple times
  const accept = req.headers['accept'];

  console.log({
    method: req.method,         // 'GET', 'POST', etc.
    url: req.url,               // '/path?query=value'
    httpVersion: req.httpVersion, // '1.1', '2.0'
    contentType,
    authorization,
  });

  res.writeHead(200, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({ ok: true }));
});
```

With Express:

```typescript
import express, { Request, Response } from 'express';

const app = express();
app.use(express.json());

app.get('/profile', (req: Request, res: Response) => {
  // Express lowercases all header names
  const token = req.headers['authorization']?.replace('Bearer ', '');
  const requestId = req.headers['x-request-id'] as string | undefined;

  // req.get() is a convenience wrapper for case-insensitive header access
  const contentType = req.get('Content-Type');

  if (!token) {
    return res.status(401).json({ error: 'Missing authorization header' });
  }

  res.status(200).json({ userId: 42, requestId });
});
```

---

## 3. Status Codes

Status codes are three-digit numbers. The first digit indicates the class of response.

### 1xx — Informational

These are provisional responses. The server is telling the client "I received your request, keep going."

| Code | Name | Meaning |
|---|---|---|
| 100 | Continue | Server received headers, client should send body. Used with `Expect: 100-continue`. |
| 101 | Switching Protocols | Server agrees to switch protocols (e.g., WebSocket upgrade). |
| 103 | Early Hints | Server sends headers early while it prepares the full response (for preloading resources). |

You rarely set these manually. They come from protocol negotiation.

### 2xx — Success

| Code | Name | When to use |
|---|---|---|
| 200 | OK | Generic success. GET, PUT, PATCH succeeded. |
| 201 | Created | POST created a new resource. Include `Location` header pointing to the new resource. |
| 202 | Accepted | Request accepted but processing is async (job queued). |
| 204 | No Content | Success but no body. Common for DELETE, or PUT when you don't return the updated resource. |
| 206 | Partial Content | Response to a range request (`Range` header). Used for video streaming and resumable downloads. |

### 3xx — Redirection

| Code | Name | When to use |
|---|---|---|
| 301 | Moved Permanently | Resource has a new permanent URL. Browsers cache this. Use for URL migrations. |
| 302 | Found | Temporary redirect. Browser does NOT cache. The method may change to GET on redirect. |
| 303 | See Other | After POST, redirect to a GET URL. The Post/Redirect/Get pattern. |
| 304 | Not Modified | Conditional GET — resource hasn't changed, use your cached version. No body. |
| 307 | Temporary Redirect | Temporary redirect that preserves the HTTP method (POST stays POST). |
| 308 | Permanent Redirect | Permanent redirect that preserves the HTTP method. |

**301 vs 302:** 301 is cached by browsers indefinitely. Never use it for anything you might want to change. 302 is safe for temporary redirects.

**307 vs 302:** 302 is technically supposed to preserve method, but old browsers changed POST to GET. 307 guarantees method preservation per spec.

### 4xx — Client Error

The client sent a bad request. The problem is on the caller's side.

| Code | Name | When to use |
|---|---|---|
| 400 | Bad Request | Malformed request, invalid JSON, missing required fields. |
| 401 | Unauthorized | Not authenticated. No valid credentials provided. Despite the name, it means "unauthenticated." |
| 403 | Forbidden | Authenticated but not authorized. You know who you are, but you can't do this. |
| 404 | Not Found | Resource doesn't exist. Also safe to return when you want to hide existence of a resource. |
| 405 | Method Not Allowed | The HTTP method is not supported for this endpoint. Return `Allow` header listing valid methods. |
| 409 | Conflict | State conflict — e.g., duplicate email on registration, optimistic lock failure. |
| 410 | Gone | Resource existed but was permanently deleted. Stronger signal than 404. |
| 422 | Unprocessable Entity | Request is well-formed but contains semantic errors (validation failures). Popular in REST APIs. |
| 429 | Too Many Requests | Rate limit exceeded. Include `Retry-After` header. |
| 431 | Request Header Fields Too Large | Headers exceeded server limits. |

### 5xx — Server Error

Something went wrong on the server side. The client did nothing wrong.

| Code | Name | When to use |
|---|---|---|
| 500 | Internal Server Error | Generic unexpected error. Use when you catch an unhandled exception. |
| 501 | Not Implemented | The method is recognized but not implemented. |
| 502 | Bad Gateway | The server (acting as a proxy/gateway) received an invalid response from an upstream. |
| 503 | Service Unavailable | Server is down or overloaded. Include `Retry-After` header. |
| 504 | Gateway Timeout | The upstream server didn't respond in time. |

### Returning Proper Status Codes in Express

```typescript
import express, { Request, Response, NextFunction } from 'express';

const app = express();
app.use(express.json());

interface User {
  id: number;
  email: string;
  name: string;
}

const users: Map<number, User> = new Map([
  [1, { id: 1, email: 'alice@example.com', name: 'Alice' }],
]);

// GET — return 200 or 404
app.get('/users/:id', (req: Request, res: Response) => {
  const id = parseInt(req.params.id, 10);

  if (isNaN(id)) {
    return res.status(400).json({
      error: 'Bad Request',
      message: 'id must be a number',
    });
  }

  const user = users.get(id);
  if (!user) {
    return res.status(404).json({
      error: 'Not Found',
      message: `User ${id} does not exist`,
    });
  }

  return res.status(200).json(user);
});

// POST — return 201 with Location header
app.post('/users', (req: Request, res: Response) => {
  const { email, name } = req.body;

  if (!email || !name) {
    return res.status(422).json({
      error: 'Unprocessable Entity',
      fields: {
        email: email ? undefined : 'required',
        name: name ? undefined : 'required',
      },
    });
  }

  // Check for duplicate email — 409 Conflict
  for (const user of users.values()) {
    if (user.email === email) {
      return res.status(409).json({
        error: 'Conflict',
        message: 'A user with this email already exists',
      });
    }
  }

  const newUser: User = { id: users.size + 1, email, name };
  users.set(newUser.id, newUser);

  return res
    .status(201)
    .header('Location', `/users/${newUser.id}`)
    .json(newUser);
});

// DELETE — return 204 (no body)
app.delete('/users/:id', (req: Request, res: Response) => {
  const id = parseInt(req.params.id, 10);

  if (!users.has(id)) {
    return res.status(404).json({ error: 'Not Found' });
  }

  users.delete(id);
  return res.status(204).send(); // No body for 204
});

// Global error handler — return 500
app.use((err: Error, req: Request, res: Response, next: NextFunction) => {
  console.error(err.stack);
  res.status(500).json({
    error: 'Internal Server Error',
    message: process.env.NODE_ENV === 'development' ? err.message : 'An unexpected error occurred',
  });
});
```

---

## 4. HTTPS and TLS

### What is TLS?

TLS (Transport Layer Security) is a cryptographic protocol that provides three guarantees:

1. **Confidentiality** — data is encrypted; a network observer cannot read it
2. **Integrity** — data cannot be altered in transit without detection
3. **Authentication** — the client can verify it's talking to the real server (not an impostor)

HTTPS is HTTP transported over a TLS connection. Port 443 is standard for HTTPS; port 80 is for plain HTTP.

### TLS Handshake (TLS 1.3)

TLS 1.3, standardized in 2018, reduced the handshake from 2 round trips (TLS 1.2) to 1. Here are the steps:

**Step 1 — ClientHello**
The client sends:
- TLS version (1.3)
- Supported cipher suites (e.g., `TLS_AES_256_GCM_SHA384`)
- A random nonce (`client_random`)
- Key share: a public key for the client's ephemeral Diffie-Hellman key pair
- SNI (Server Name Indication): the hostname the client wants to connect to (allows one IP to host multiple certificates)

**Step 2 — ServerHello**
The server responds in the same round trip with:
- Selected cipher suite
- A random nonce (`server_random`)
- Server's DH key share
- The server's certificate (containing its public key)
- A signature proving the server owns the certificate's private key
- `Finished` — encrypted with the derived key to verify both sides computed the same session key

Both sides can now derive the symmetric session key from the DH exchange. The client never sends the key over the wire — it's computed independently on both ends.

**Step 3 — Client Finished**
The client verifies:
- The certificate is signed by a trusted CA (Certificate Authority)
- The certificate's hostname matches the SNI
- The certificate hasn't expired
- The certificate hasn't been revoked (via OCSP or CRL)

Then sends its `Finished` message and the connection is established. Data flows encrypted.

**0-RTT resumption:** For returning clients, TLS 1.3 supports sending data immediately in the first message (0-RTT) using a pre-shared session ticket. This is fast but has replay attack risks — it should only be used for idempotent operations.

### Certificates

An X.509 certificate contains:
- Subject: the domain it's issued to (e.g., `api.example.com`)
- Issuer: the Certificate Authority that signed it
- Public key
- Validity period (not before / not after)
- Subject Alternative Names (SANs): additional domains this cert covers
- Signature by the issuer's private key

Certificate authorities (DigiCert, Let's Encrypt, etc.) form a chain of trust. Your server's certificate is signed by an intermediate CA, which is signed by a root CA. Browsers and operating systems ship with a trusted root store — a list of root CAs they trust.

**Let's Encrypt** provides free, auto-renewing DV (Domain Validation) certificates. For most APIs it's sufficient. EV (Extended Validation) certificates involve identity verification and show a green bar in older browsers — rarely worth the cost today.

### Why HTTPS Matters for APIs

Even if your API is "internal," HTTPS protects against:
- Passive surveillance on shared networks
- MITM (man-in-the-middle) attacks on corporate or cloud networks
- Token theft (JWTs, API keys in headers are plaintext over HTTP)
- DNS spoofing combined with HTTP

Set `Strict-Transport-Security` to prevent protocol downgrade:

```typescript
import express from 'express';

const app = express();

// Force HTTPS for 1 year, include subdomains, allow preloading
app.use((req, res, next) => {
  res.setHeader(
    'Strict-Transport-Security',
    'max-age=31536000; includeSubDomains; preload'
  );
  next();
});
```

---

## 5. CORS

### What is CORS?

CORS (Cross-Origin Resource Sharing) is a browser security mechanism that restricts how web pages can make requests to a different origin than the one that served the page.

**Origin** = scheme + hostname + port.

`https://app.example.com` and `https://api.example.com` are different origins. `https://api.example.com:443` and `https://api.example.com` are the same (443 is implied for HTTPS).

### Why Browsers Enforce CORS

Without CORS, any website could make JavaScript fetch requests to any other site using the visitor's browser — including their banking site — and read the responses. The attacker's page runs in the user's browser, so all cookies and credentials for that origin travel with the request automatically.

CORS is a browser-enforced policy. Servers cannot enforce it — they have no way to tell if a request came from a browser with JavaScript or from curl. The browser enforces the restriction by blocking the JavaScript code from reading the response.

Non-browser HTTP clients (curl, Postman, server-to-server calls) are never restricted by CORS.

### Simple vs Preflighted Requests

**Simple requests** don't trigger a preflight. They must meet all of:
- Method is GET, POST, or HEAD
- Headers are only: `Accept`, `Accept-Language`, `Content-Language`, `Content-Type`
- `Content-Type` is only: `application/x-www-form-urlencoded`, `multipart/form-data`, or `text/plain`

Any deviation — custom headers like `Authorization`, `Content-Type: application/json`, methods like PUT or DELETE — triggers a **preflight**.

### Preflight Request

Before sending the actual request, the browser sends an `OPTIONS` request:

```
OPTIONS /api/users HTTP/1.1
Host: api.example.com
Origin: https://app.example.com
Access-Control-Request-Method: POST
Access-Control-Request-Headers: Content-Type, Authorization
```

The server must respond with appropriate CORS headers:

```
HTTP/1.1 204 No Content
Access-Control-Allow-Origin: https://app.example.com
Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization
Access-Control-Max-Age: 86400
```

`Access-Control-Max-Age` caches the preflight result. Without it, the browser preflights every single request — expensive.

After the preflight succeeds, the browser sends the actual request, and the server response must again include `Access-Control-Allow-Origin`.

### CORS Response Headers

| Header | Purpose |
|---|---|
| `Access-Control-Allow-Origin` | Which origins are allowed. `*` allows all origins (but cannot be used with credentials). |
| `Access-Control-Allow-Methods` | Which HTTP methods are allowed for cross-origin requests. |
| `Access-Control-Allow-Headers` | Which request headers are allowed. |
| `Access-Control-Allow-Credentials` | Whether cookies/auth headers can be sent. Must be `true`, not `*`. |
| `Access-Control-Max-Age` | How long (seconds) to cache preflight results. |
| `Access-Control-Expose-Headers` | Which response headers are accessible to JavaScript (beyond the default safe list). |

### Implementing CORS in Express (TypeScript)

```typescript
import express, { Request, Response, NextFunction } from 'express';

const app = express();

const ALLOWED_ORIGINS = new Set([
  'https://app.example.com',
  'https://admin.example.com',
]);

// If you're in development, allow localhost
if (process.env.NODE_ENV === 'development') {
  ALLOWED_ORIGINS.add('http://localhost:3000');
  ALLOWED_ORIGINS.add('http://localhost:5173');
}

function corsMiddleware(req: Request, res: Response, next: NextFunction): void {
  const origin = req.headers['origin'];

  if (origin && ALLOWED_ORIGINS.has(origin)) {
    // Reflect the specific origin back (required when using credentials)
    res.setHeader('Access-Control-Allow-Origin', origin);
    res.setHeader('Vary', 'Origin'); // Tell caches that response varies by Origin
    res.setHeader('Access-Control-Allow-Credentials', 'true');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, PATCH, DELETE, OPTIONS');
    res.setHeader(
      'Access-Control-Allow-Headers',
      'Content-Type, Authorization, X-Request-ID'
    );
    res.setHeader('Access-Control-Max-Age', '86400'); // 24 hours
    // Expose custom response headers to client JS
    res.setHeader('Access-Control-Expose-Headers', 'X-RateLimit-Remaining, X-Request-ID');
  }

  // Respond to preflight immediately
  if (req.method === 'OPTIONS') {
    res.status(204).end();
    return;
  }

  next();
}

app.use(corsMiddleware);

// Alternatively, use the `cors` npm package:
// import cors from 'cors';
// app.use(cors({
//   origin: (origin, callback) => {
//     if (!origin || ALLOWED_ORIGINS.has(origin)) {
//       callback(null, true);
//     } else {
//       callback(new Error(`Origin ${origin} not allowed`));
//     }
//   },
//   credentials: true,
//   methods: ['GET', 'POST', 'PUT', 'PATCH', 'DELETE'],
//   allowedHeaders: ['Content-Type', 'Authorization'],
//   maxAge: 86400,
// }));
```

### Common CORS Mistakes

1. **Using `Access-Control-Allow-Origin: *` with credentials.** The browser rejects this. If you need cookies or `Authorization` headers, you must reflect the specific origin and set `Access-Control-Allow-Credentials: true`.

2. **Forgetting `Vary: Origin`.** Without this, a CDN might cache a response with `Access-Control-Allow-Origin: https://app.example.com` and serve it to `https://admin.example.com`, which then fails.

3. **Not handling OPTIONS preflight.** If your router doesn't have a route for `OPTIONS /api/resource`, the preflight returns 404 or 405, and the browser blocks the real request.

4. **Setting CORS headers only on successful responses.** Error responses (4xx, 5xx) also need CORS headers. If your error handler runs before the CORS middleware adds headers, the browser won't be able to read the error response body.

---

## 6. DNS Resolution

### What is DNS?

DNS (Domain Name System) translates human-readable hostnames like `api.example.com` into IP addresses like `203.0.113.42`. It's a distributed, hierarchical, cached database.

### DNS Record Types

| Record | Purpose | Example |
|---|---|---|
| **A** | Maps hostname to IPv4 address | `api.example.com. 300 IN A 203.0.113.42` |
| **AAAA** | Maps hostname to IPv6 address | `api.example.com. 300 IN AAAA 2001:db8::1` |
| **CNAME** | Alias from one name to another | `www.example.com. 3600 IN CNAME example.com.` |
| **MX** | Mail server for a domain | `example.com. 3600 IN MX 10 mail.example.com.` |
| **TXT** | Arbitrary text, used for SPF, DKIM, ownership verification | `example.com. 3600 IN TXT "v=spf1 include:..."` |
| **NS** | Nameservers for a domain | `example.com. 86400 IN NS ns1.example-dns.com.` |
| **SOA** | Start of Authority — administrative info about the zone | |
| **SRV** | Service location (hostname + port + priority) | `_http._tcp.example.com. SRV 10 5 80 web.example.com.` |
| **PTR** | Reverse DNS — IP to hostname | `42.113.0.203.in-addr.arpa. PTR api.example.com.` |

### A Records vs CNAME

An **A record** points directly to an IP address. You can point multiple A records at the same IP to distribute load (round-robin DNS), but there's no health checking — a dead server still gets traffic.

A **CNAME** is an alias. `www.example.com CNAME example.com` means "the address of `www.example.com` is whatever the address of `example.com` is." CNAMEs always point to another name, never an IP. This is useful for:
- CDNs (`assets.example.com CNAME d123.cloudfront.net`)
- Load balancers where the IP changes

**Restriction:** A CNAME cannot coexist with other records for the same name. You cannot have both a CNAME and a TXT record at `example.com` — this is why most DNS providers offer "ALIAS" or "ANAME" records, which behave like CNAME at the apex but are resolved server-side.

### TTL (Time To Live)

TTL is the number of seconds a DNS resolver should cache a record before re-querying. It's set by the zone owner.

```
api.example.com.   300   IN   A   203.0.113.42
                   ^^^
                   TTL = 300 seconds (5 minutes)
```

**Low TTL (60–300s):** Used when you need to change IPs quickly (failovers, deployments). Adds more DNS load and slightly higher latency per lookup.

**High TTL (3600–86400s):** Stable records you rarely change. Reduces DNS load, faster lookups from cache.

**Rule of thumb before migrations:** Lower TTL to 60s at least 24 hours before the migration, so old caches expire. After the migration, raise TTL back up.

### DNS Resolution Step-by-Step

Query: resolve `api.example.com` to an IP.

1. **Browser cache.** The browser checks its own in-memory DNS cache. If found and not expired, done.

2. **OS cache.** If not in browser cache, the OS's stub resolver checks `/etc/hosts` (for local overrides) then its own cache. If found and not expired, done.

3. **Recursive resolver.** The OS sends a UDP query to the configured recursive DNS resolver (typically your ISP's resolver, Google's `8.8.8.8`, or Cloudflare's `1.1.1.1`).

4. **Root nameserver.** If the recursive resolver doesn't have the answer cached, it queries one of the 13 root nameserver clusters (e.g., `a.root-servers.net`). The root server says: "I don't know `api.example.com`, but for `.com` domains, ask these TLD nameservers: `a.gtld-servers.net`."

5. **TLD nameserver.** The recursive resolver queries the `.com` TLD nameserver: "Where is `example.com`?" The TLD server returns the authoritative nameservers for `example.com` (e.g., `ns1.example-dns.com`).

6. **Authoritative nameserver.** The recursive resolver queries `ns1.example-dns.com`: "What is the A record for `api.example.com`?" The authoritative server returns the IP: `203.0.113.42`.

7. **Response cached and returned.** The recursive resolver caches the answer for the TTL duration and returns it to the OS. The OS returns it to the browser. The browser connects to `203.0.113.42`.

Total time for a cold lookup: 50–200ms. Cached lookups: <1ms.

---

## 7. Transport Layer — TCP vs UDP

### TCP (Transmission Control Protocol)

TCP provides a reliable, ordered, connection-oriented byte stream. It guarantees:

- **Delivery:** Lost packets are detected and retransmitted
- **Order:** Packets are reordered into the correct sequence
- **Integrity:** Checksums detect corruption
- **Flow control:** The receiver can slow down the sender if it's overwhelmed
- **Congestion control:** The sender backs off when the network is congested (slow start, AIMD)

**The Three-Way Handshake**

Before any data flows, TCP establishes a connection:

```
Client                          Server
  |                               |
  |--- SYN (seq=x) -------------->|   Client picks a random seq number,
  |                               |   sends SYN (synchronize) flag
  |                               |
  |<-- SYN-ACK (seq=y, ack=x+1) --|   Server picks its own seq number,
  |                               |   acknowledges client's SYN
  |                               |
  |--- ACK (ack=y+1) ------------>|   Client acknowledges server's SYN
  |                               |
  |   [connection established]    |
  |                               |
  |--- HTTP Request ------------->|
```

This takes 1 round-trip time (RTT) before the first byte of application data is sent. For HTTP/1.1 + TLS, you add another 1–2 RTTs for the TLS handshake.

**TCP Connection Teardown (Four-way handshake)**

```
Client             Server
  |--- FIN -------->|   Client done sending
  |<-- ACK ---------|
  |<-- FIN ---------|   Server done sending
  |--- ACK -------->|
```

**TCP Slow Start**

When a new TCP connection opens, the sender doesn't know how much bandwidth the network has. It starts with a small congestion window (typically 10 segments / ~14 KB) and doubles it every RTT until it reaches the slow start threshold or detects packet loss. This means new connections start slow and ramp up.

This is why HTTP/2 multiplexing over a single warmed-up connection can be faster than multiple HTTP/1.1 connections that each go through slow start.

### UDP (User Datagram Protocol)

UDP is connectionless. There's no handshake, no acknowledgment, no ordering, no retransmission. You send a packet and hope it arrives. It's much simpler:

- No connection setup latency
- No head-of-line blocking
- No flow or congestion control by the protocol (your app handles this if needed)
- Much lower overhead per packet

**When to use TCP:**
- HTTP/1.1 and HTTP/2 (always TCP)
- Databases
- Email
- File transfers
- Any situation where every byte must arrive correctly

**When to use UDP:**
- HTTP/3 (QUIC is built on UDP, but adds its own reliability per-stream)
- DNS queries (fast, tiny, retrying is cheap)
- Real-time audio/video (a late packet is useless — better to skip it than delay everything)
- Online gaming (position updates — old data is irrelevant)
- IoT sensors reporting metrics (occasional loss is acceptable)
- Multicast streaming

### UDP in Node.js

```typescript
import dgram from 'dgram';

// UDP Server
const server = dgram.createSocket('udp4');

server.on('message', (msg, rinfo) => {
  console.log(`Received "${msg}" from ${rinfo.address}:${rinfo.port}`);

  // Send response
  const response = Buffer.from(`echo: ${msg}`);
  server.send(response, rinfo.port, rinfo.address);
});

server.bind(41234, () => {
  console.log('UDP server listening on port 41234');
});

// UDP Client
const client = dgram.createSocket('udp4');
const message = Buffer.from('hello');

client.send(message, 41234, 'localhost', (err) => {
  if (err) {
    console.error('Send error:', err);
  }
  client.close();
});
```

---

## 8. Keep-Alive and Connection Reuse

### The Problem

Every new TCP connection incurs:
- 1 RTT for the TCP three-way handshake
- 1–2 RTTs for the TLS handshake
- TCP slow start ramp-up

If a browser loads a page with 50 resources and opened a new connection for each, it would spend more time on handshakes than on data transfer.

### HTTP Keep-Alive (Persistent Connections)

In HTTP/1.0, connections closed after each request/response. HTTP/1.1 made persistent connections the default.

With keep-alive, after the response is sent, the connection stays open. The next request reuses the existing TCP (and TLS) connection. The overhead of handshakes is paid only once.

```
Connection: keep-alive         # Request header asking to keep connection open
Keep-Alive: timeout=5, max=100 # Server response: keep open 5s, max 100 requests
Connection: close              # Either side can signal they want to close after this response
```

Node.js HTTP server defaults:

```typescript
import http from 'http';

const server = http.createServer((req, res) => {
  res.end('Hello');
});

server.keepAliveTimeout = 5000;  // 5 seconds (default: 5000ms in Node.js)
server.headersTimeout = 60000;   // 60 seconds max for headers to arrive

server.listen(3000);
```

### HTTP/2 and Multiplexing

HTTP/2 takes connection reuse further. Multiple requests and responses flow simultaneously over a single connection. There's no need for multiple parallel connections — one connection handles everything.

### Connection Pools in HTTP Clients

When your backend calls other services, use an HTTP agent with connection pooling:

```typescript
import http from 'http';
import https from 'https';

// Node.js http.globalAgent already pools connections,
// but you can tune it:
const agent = new https.Agent({
  keepAlive: true,
  maxSockets: 50,       // Max concurrent connections per host
  maxFreeSockets: 10,   // Max idle connections to keep
  timeout: 30000,       // Socket timeout in ms
});

// Pass it to fetch or http.request:
const res = await fetch('https://api.example.com/data', {
  // @ts-ignore — Node 18+ fetch supports agent
  agent,
});
```

With `node-fetch` or `axios`:

```typescript
import axios from 'axios';
import https from 'https';

const httpsAgent = new https.Agent({
  keepAlive: true,
  maxSockets: 100,
});

const client = axios.create({
  baseURL: 'https://api.example.com',
  httpsAgent,
  timeout: 10000,
});
```

**Why this matters:** Without connection pooling, every outbound HTTP request from your API creates a new TCP+TLS connection. Under load, this exhausts ephemeral ports and increases latency significantly.

---

## 9. Compression

### Why Compress?

A typical JSON API response is highly compressible. A 100 KB JSON response can compress to 10–15 KB — a 7–10x reduction. This matters for:

- Users on slow connections
- Mobile data costs
- Server bandwidth costs
- Perceived latency (smaller responses arrive faster)

### gzip

gzip is based on the DEFLATE algorithm (LZ77 + Huffman coding). It's been standard in HTTP since 1999. Nearly every client and server supports it.

- Compression ratio: moderate
- CPU cost: low
- Universally supported

### Brotli (br)

Brotli was developed by Google and standardized in 2016. It achieves better compression ratios than gzip, especially for text content, at comparable CPU cost.

- Compression ratio: 15–25% better than gzip on average
- CPU cost: slightly higher to compress (about the same to decompress)
- Supported by all modern browsers and most servers

Use brotli when the client supports it. Fall back to gzip if not.

### How Content Negotiation Works

The client signals what it supports in `Accept-Encoding`:

```
Accept-Encoding: gzip, deflate, br
```

The server picks the best option, compresses the response, and signals what it used:

```
Content-Encoding: br
```

The client decompresses transparently. If the server doesn't compress, it omits `Content-Encoding`.

### When NOT to Compress

- Already-compressed content: JPEG, PNG, MP4, ZIP, PDF. Compressing these wastes CPU and can make them slightly larger.
- Very small responses (<1 KB). Compression overhead isn't worth it.
- Real-time streaming where latency matters more than size.

### Compression in Express

```typescript
import express from 'express';
import compression from 'compression';
import shrinkRay from 'shrink-ray-current'; // Supports brotli

const app = express();

// Basic gzip compression (built into 'compression' package)
app.use(compression({
  level: 6,         // Compression level 1–9 (6 is default, good balance)
  threshold: 1024,  // Only compress responses > 1 KB
  filter: (req, res) => {
    // Don't compress responses with no-transform directive
    if (req.headers['x-no-compression']) {
      return false;
    }
    return compression.filter(req, res);
  },
}));

// For brotli support, use shrink-ray:
// app.use(shrinkRay());

app.get('/data', (req, res) => {
  // The compression middleware transparently compresses this
  res.json({ items: new Array(1000).fill({ id: 1, name: 'example' }) });
});
```

### Manual Compression with zlib

```typescript
import zlib from 'zlib';
import { promisify } from 'util';

const gzip = promisify(zlib.gzip);
const brotliCompress = promisify(zlib.brotliCompress);

async function compressResponse(data: string, acceptEncoding: string): Promise<{
  buffer: Buffer;
  encoding: string;
}> {
  if (acceptEncoding.includes('br')) {
    const buffer = await brotliCompress(data, {
      params: { [zlib.constants.BROTLI_PARAM_QUALITY]: 4 }, // Quality 1-11
    });
    return { buffer, encoding: 'br' };
  }

  if (acceptEncoding.includes('gzip')) {
    const buffer = await gzip(data, { level: 6 });
    return { buffer, encoding: 'gzip' };
  }

  return { buffer: Buffer.from(data), encoding: 'identity' };
}
```

---

## 10. Caching Headers

HTTP caching eliminates redundant work. When done correctly, clients and intermediaries (CDNs, proxies) can serve responses without contacting the origin server at all.

### Cache-Control

`Cache-Control` is the primary directive for controlling caching behavior. It can appear on both requests and responses.

**Response directives (what the server tells caches):**

```
Cache-Control: max-age=3600
```
Cache this response for 3600 seconds. After that, it's stale.

```
Cache-Control: public, max-age=86400
```
Any cache (browser, CDN, proxy) may cache this. `public` is the default for responses without credentials.

```
Cache-Control: private, max-age=300
```
Only the browser may cache this. CDNs and shared caches must not store it. Use for user-specific data.

```
Cache-Control: no-cache
```
Confusingly named. It does NOT mean "don't cache." It means "you may cache this, but you must revalidate with the origin before using the cached copy." The cache stores it, but always checks if it's still fresh before serving.

```
Cache-Control: no-store
```
This actually means "don't cache." Nothing about this response should be stored anywhere. Use for sensitive data (authentication responses, medical data).

```
Cache-Control: must-revalidate
```
Once stale, must revalidate with origin. Cannot serve a stale copy even if the origin is down.

```
Cache-Control: immutable
```
Tells the browser this URL's content will never change. Don't even bother revalidating. Useful for fingerprinted assets (`app.a1b2c3.js`).

```
Cache-Control: s-maxage=3600, max-age=0
```
`s-maxage` is like `max-age` but only for shared caches (CDNs). This says: CDN caches for 1 hour, browser doesn't cache.

### ETag

An ETag is a fingerprint of the response content. It's usually a hash of the response body.

```
ETag: "33a64df551425fcc55e4d42a148795d9f25f89d"
ETag: W/"weak-etag"   # W/ prefix means "weak" ETag — semantically equivalent but not byte-for-byte identical
```

When the browser has a cached response with an ETag, it can make a conditional request:

```
GET /api/users/1 HTTP/1.1
If-None-Match: "33a64df551425fcc55e4d42a148795d9f25f89d"
```

If the resource hasn't changed, the server returns `304 Not Modified` with no body — saving bandwidth. The browser uses its cached copy.

If the resource changed, the server returns `200 OK` with the new content and a new ETag.

### Last-Modified / If-Modified-Since

An older alternative to ETag using timestamps:

```
Last-Modified: Wed, 21 Oct 2020 07:28:00 GMT
```

Conditional request:

```
GET /api/users/1 HTTP/1.1
If-Modified-Since: Wed, 21 Oct 2020 07:28:00 GMT
```

ETags are preferred because timestamps have 1-second granularity and don't account for clock skew. ETags are more accurate.

### Implementing Caching Headers in Express

```typescript
import express, { Request, Response } from 'express';
import crypto from 'crypto';

const app = express();
app.use(express.json());

interface Product {
  id: number;
  name: string;
  price: number;
  updatedAt: Date;
}

const products: Product[] = [
  { id: 1, name: 'Widget', price: 9.99, updatedAt: new Date('2024-01-01') },
];

// Public API data — cache at CDN for 5 minutes, browser for 1 minute
app.get('/products', (req: Request, res: Response) => {
  const body = JSON.stringify(products);
  const etag = `"${crypto.createHash('sha1').update(body).digest('hex')}"`;

  // Check conditional request
  if (req.headers['if-none-match'] === etag) {
    return res.status(304).end();
  }

  res
    .setHeader('Cache-Control', 'public, max-age=60, s-maxage=300')
    .setHeader('ETag', etag)
    .setHeader('Vary', 'Accept-Encoding')
    .status(200)
    .json(products);
});

// User-specific data — only browser cache, short duration
app.get('/me/orders', (req: Request, res: Response) => {
  const orders = [{ id: 100, total: 29.99 }]; // Would normally come from DB
  const body = JSON.stringify(orders);
  const etag = `"${crypto.createHash('sha1').update(body).digest('hex')}"`;
  const lastModified = new Date('2024-03-15').toUTCString();

  if (req.headers['if-none-match'] === etag) {
    return res.status(304).end();
  }

  if (
    req.headers['if-modified-since'] &&
    new Date(req.headers['if-modified-since']) >= new Date('2024-03-15')
  ) {
    return res.status(304).end();
  }

  res
    .setHeader('Cache-Control', 'private, max-age=60, must-revalidate')
    .setHeader('ETag', etag)
    .setHeader('Last-Modified', lastModified)
    .status(200)
    .json(orders);
});

// Never cache — auth endpoint
app.post('/login', (req: Request, res: Response) => {
  res
    .setHeader('Cache-Control', 'no-store')
    .status(200)
    .json({ token: 'jwt-here' });
});

// Immutable fingerprinted assets (usually served by your static file server, not Express)
app.get('/static/:hash/:file', (req: Request, res: Response) => {
  res
    .setHeader('Cache-Control', 'public, max-age=31536000, immutable')
    .status(200)
    .sendFile(`/assets/${req.params.file}`);
});
```

### Cache-Control Decision Tree

```
Is this response user-specific (contains auth-dependent data)?
├── Yes → Cache-Control: private
│         Is it sensitive enough to not store at all?
│         ├── Yes → no-store (login responses, tokens)
│         └── No → private, max-age=N (N = how long the data stays valid)
│
└── No → Cache-Control: public
          How long is this valid?
          ├── Short-lived (frequently changing) → max-age=60 (or similar small number)
          ├── Medium → public, max-age=300, s-maxage=3600
          ├── Long-lived but might change → public, max-age=3600
          └── Never changes (fingerprinted) → public, max-age=31536000, immutable
```

---

## Quick Reference — Complete TypeScript Example

This shows headers, CORS, status codes, caching, and compression working together:

```typescript
import express, { Request, Response, NextFunction } from 'express';
import compression from 'compression';
import crypto from 'crypto';

const app = express();

// 1. Compression (must be early in the middleware chain)
app.use(compression({ threshold: 1024 }));

// 2. Parse JSON bodies
app.use(express.json());

// 3. CORS
const ALLOWED_ORIGINS = new Set(['https://app.example.com', 'http://localhost:3000']);

app.use((req: Request, res: Response, next: NextFunction) => {
  const origin = req.headers['origin'];

  if (origin && ALLOWED_ORIGINS.has(origin)) {
    res.setHeader('Access-Control-Allow-Origin', origin);
    res.setHeader('Vary', 'Origin');
    res.setHeader('Access-Control-Allow-Credentials', 'true');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');
    res.setHeader('Access-Control-Max-Age', '86400');
  }

  if (req.method === 'OPTIONS') {
    res.status(204).end();
    return;
  }

  next();
});

// 4. Security headers
app.use((req: Request, res: Response, next: NextFunction) => {
  res.setHeader('Strict-Transport-Security', 'max-age=31536000; includeSubDomains');
  res.setHeader('X-Content-Type-Options', 'nosniff');
  res.setHeader('X-Frame-Options', 'DENY');
  next();
});

// 5. Cacheable public endpoint with ETag
const data = { items: Array.from({ length: 100 }, (_, i) => ({ id: i, name: `Item ${i}` })) };
const dataJson = JSON.stringify(data);
const dataEtag = `"${crypto.createHash('sha1').update(dataJson).digest('hex')}"`;

app.get('/api/items', (req: Request, res: Response) => {
  if (req.headers['if-none-match'] === dataEtag) {
    return res.status(304).end();
  }

  res
    .setHeader('Cache-Control', 'public, max-age=60, s-maxage=300')
    .setHeader('ETag', dataEtag)
    .status(200)
    .json(data);
});

// 6. Non-cacheable, authenticated endpoint
app.get('/api/me', (req: Request, res: Response) => {
  const auth = req.headers['authorization'];

  if (!auth?.startsWith('Bearer ')) {
    return res.status(401).json({ error: 'Unauthorized', message: 'Bearer token required' });
  }

  // In reality: validate JWT here
  res
    .setHeader('Cache-Control', 'private, no-store')
    .status(200)
    .json({ id: 1, name: 'Alice' });
});

app.listen(3000, () => console.log('Server running on port 3000'));
```
