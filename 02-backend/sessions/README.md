# Session-Based Authentication

## 1. Session-Based Auth vs Token-Based Auth

### How Session-Based Auth Works

```
1. User submits credentials
2. Server verifies credentials
3. Server creates a session record in a store (memory/Redis/DB)
   Session: { id: "abc123", userId: "user_456", createdAt: ..., data: {...} }
4. Server sends a session ID to the client in a cookie
   Set-Cookie: sessionId=abc123; HttpOnly; Secure
5. On each request, client sends the cookie automatically
6. Server looks up "abc123" in the session store to authenticate the request
7. On logout, server deletes the session record
```

### How Token-Based Auth (JWT) Works

```
1. User submits credentials
2. Server verifies credentials
3. Server signs a JWT containing user data
4. Server sends JWT to client (cookie or response body)
5. Client sends JWT in Authorization header or cookie on each request
6. Server verifies JWT signature locally — NO store lookup needed
7. On logout, client discards the token (server cannot force revocation)
```

### Tradeoff Comparison

| Aspect | Session-Based | Token-Based (JWT) |
|--------|--------------|-------------------|
| **State** | Stateful — server stores session data | Stateless — all data in the token |
| **Revocation** | Instant — delete session from store | Difficult — must wait for expiry or use blocklist |
| **Scalability** | Requires shared store (Redis) across instances | Easy horizontal scaling — no shared state |
| **Storage size** | Tiny cookie (just an ID) | Larger token, grows with claims |
| **DB lookups** | Every request | Only on token refresh (unless using version strategy) |
| **Cross-domain** | Harder — cookies have same-origin restrictions | Easy — Authorization header works anywhere |
| **Mobile / native apps** | Awkward — cookies are browser-native | Natural — store token in secure storage |
| **Microservices** | Session store becomes a bottleneck | Each service verifies token independently |

### When to Use Each

**Use sessions when:**
- Building a traditional server-rendered web app (Express + templates, Rails, Django)
- Instant revocation is a hard requirement (banking, healthcare)
- Your app is a monolith on a single server or behind a single load balancer with sticky sessions
- You need to store significant server-side state per user

**Use JWTs when:**
- Building a SPA or mobile app consuming a REST/GraphQL API
- Operating a microservices architecture
- Supporting cross-domain or cross-origin clients
- Scaling horizontally without a shared session store

---

## 2. Session Stores

### In-Memory (default in most frameworks)

```typescript
import session from 'express-session';

app.use(session({
  secret: process.env.SESSION_SECRET!,
  resave: false,
  saveUninitialized: false,
  // Default MemoryStore — DO NOT use in production
}));
```

**Characteristics:**
- Zero configuration, works out of the box
- Sessions are lost on every server restart
- Does not scale — sessions are local to one process
- Will eventually leak memory (sessions accumulate)

**Use only for:** local development and quick prototypes.

### Redis Store

```typescript
import session from 'express-session';
import RedisStore from 'connect-redis';
import { createClient } from 'redis';

const redisClient = createClient({ url: process.env.REDIS_URL });
await redisClient.connect();

app.use(session({
  store: new RedisStore({ client: redisClient }),
  secret: process.env.SESSION_SECRET!,
  resave: false,
  saveUninitialized: false,
  cookie: {
    httpOnly: true,
    secure: true,
    sameSite: 'strict',
    maxAge: 24 * 60 * 60 * 1000, // 24 hours
  },
}));
```

**Characteristics:**
- Persistent across server restarts
- Shared across all server instances — horizontal scaling works
- Sub-millisecond lookup latency
- Built-in TTL support — expired sessions automatically purged
- Can survive Redis restarts if persistence is configured (AOF/RDB)

**Use for:** production web apps, any multi-instance deployment.

### Database Store (PostgreSQL / MySQL)

```typescript
import session from 'express-session';
import pgSession from 'connect-pg-simple';

const PgSession = pgSession(session);

app.use(session({
  store: new PgSession({
    conString: process.env.DATABASE_URL,
    tableName: 'user_sessions',
    pruneSessionInterval: 60 * 15, // prune expired sessions every 15 min
  }),
  secret: process.env.SESSION_SECRET!,
  resave: false,
  saveUninitialized: false,
}));
```

SQL table required:
```sql
CREATE TABLE user_sessions (
  sid    VARCHAR      NOT NULL COLLATE "default",
  sess   JSON         NOT NULL,
  expire TIMESTAMPTZ  NOT NULL,
  CONSTRAINT "session_pkey" PRIMARY KEY (sid) NOT DEFERRABLE INITIALLY IMMEDIATE
);

CREATE INDEX ON user_sessions (expire);
```

**Characteristics:**
- Sessions stored alongside your application data
- Can JOIN session data with user records
- Slower than Redis (disk I/O, query overhead)
- Good for apps that already have a database and low-to-moderate traffic
- Transactional — session updates are ACID-compliant

**Use for:** smaller applications where adding Redis is overhead, or when you need to query sessions relationally.

### Store Comparison

| Store | Speed | Persistence | Horizontal Scaling | Complexity |
|-------|-------|-------------|-------------------|------------|
| In-Memory | Fastest | No | No | None |
| Redis | Very fast | Yes (configurable) | Yes | Low |
| Database | Moderate | Yes | Yes | Low |

---

## 3. Session Fixation and Session Hijacking

### Session Fixation

**What it is:** An attacker supplies or forces a known session ID on the victim before authentication. After the victim authenticates, the attacker uses that known session ID to access the authenticated session.

**Attack flow:**
```
1. Attacker visits the site and receives a valid (unauthenticated) session ID
2. Attacker tricks victim into using that same session ID
   (e.g., via a crafted link: https://bank.com/login?PHPSESSID=attacker_known_id)
3. Victim logs in — server authenticates the session but does NOT regenerate the ID
4. Attacker now has an authenticated session with the known ID
```

**Mitigation — Regenerate session ID on login:**
```typescript
app.post('/auth/login', async (req, res) => {
  const { email, password } = req.body;
  const user = await verifyCredentials(email, password);

  if (!user) {
    return res.status(401).json({ error: 'Invalid credentials' });
  }

  // CRITICAL: Regenerate session ID after authentication
  // This prevents session fixation — the old ID is invalidated
  req.session.regenerate((err) => {
    if (err) return res.status(500).json({ error: 'Session error' });

    req.session.userId = user.id;
    req.session.email = user.email;
    req.session.role = user.role;
    req.session.loginAt = new Date().toISOString();

    res.json({ message: 'Logged in' });
  });
});
```

Also regenerate the session ID on privilege escalation (e.g., when a user elevates to admin mode).

### Session Hijacking

**What it is:** An attacker steals a valid session ID and uses it to impersonate the victim.

**Methods:**
- **Network sniffing** — intercepting cookies over HTTP (not HTTPS)
- **XSS** — stealing cookies via injected JavaScript if `HttpOnly` is not set
- **Cross-site request forgery** — tricking the browser into sending requests with the victim's cookie
- **Predictable session IDs** — guessing or brute-forcing weak session IDs
- **Browser history / logs** — if session ID appears in URLs

**Mitigations:**

```typescript
// 1. Use cryptographically random, high-entropy session IDs
//    express-session generates these by default using uid-safe

// 2. Set HttpOnly to block JavaScript access
// 3. Set Secure to require HTTPS
// 4. Set SameSite to prevent CSRF
app.use(session({
  secret: process.env.SESSION_SECRET!,
  resave: false,
  saveUninitialized: false,
  name: '__Host-sid', // __Host- prefix enforces Secure + no Domain + Path=/
  cookie: {
    httpOnly: true,    // No JavaScript access
    secure: true,      // HTTPS only
    sameSite: 'strict',
    maxAge: 30 * 60 * 1000, // 30 minute idle timeout
  },
}));

// 5. Bind session to IP and User-Agent (optional, breaks mobile users)
app.use((req, res, next) => {
  if (req.session.userId) {
    const currentFingerprint = `${req.ip}:${req.headers['user-agent']}`;
    if (req.session.fingerprint && req.session.fingerprint !== currentFingerprint) {
      req.session.destroy(() => {});
      return res.status(401).json({ error: 'Session invalidated' });
    }
    req.session.fingerprint = currentFingerprint;
  }
  next();
});

// 6. Implement idle timeout — destroy sessions inactive for > N minutes
app.use((req, res, next) => {
  if (req.session.userId) {
    const now = Date.now();
    const lastActivity = req.session.lastActivity ?? now;
    const IDLE_TIMEOUT = 30 * 60 * 1000; // 30 minutes

    if (now - lastActivity > IDLE_TIMEOUT) {
      req.session.destroy(() => {});
      return res.status(401).json({ error: 'Session timed out' });
    }
    req.session.lastActivity = now;
  }
  next();
});

// 7. Logout — destroy session server-side
app.post('/auth/logout', (req, res) => {
  req.session.destroy((err) => {
    if (err) return res.status(500).json({ error: 'Logout failed' });
    res.clearCookie('__Host-sid');
    res.json({ message: 'Logged out' });
  });
});
```

---

## 4. Cookie Attributes

Understanding cookie attributes is essential for securing session cookies and any cookies that carry sensitive tokens.

### HttpOnly

```
Set-Cookie: sessionId=abc123; HttpOnly
```

**What it does:** Prevents JavaScript from accessing the cookie via `document.cookie`. The cookie is still sent in HTTP requests — it is only hidden from scripts.

**Why it matters:** Mitigates XSS-based session theft. Even if an attacker injects malicious JavaScript, they cannot read the cookie value.

**When to use:** Always on session cookies and token cookies. There is almost never a reason to omit this on authentication cookies.

### Secure

```
Set-Cookie: sessionId=abc123; Secure
```

**What it does:** Instructs the browser to only send the cookie over HTTPS connections. Never sent over plain HTTP.

**Why it matters:** Prevents network sniffing attacks (man-in-the-middle on HTTP). Without `Secure`, the session ID can be stolen in transit.

**When to use:** Always in production. In local development over HTTP, you may need to omit it, but never in a deployed environment.

### SameSite

```
Set-Cookie: sessionId=abc123; SameSite=Strict
Set-Cookie: sessionId=abc123; SameSite=Lax
Set-Cookie: sessionId=abc123; SameSite=None; Secure
```

**What it does:** Controls when the browser sends the cookie on cross-site requests.

| Value | Cookie sent on... | CSRF protection |
|-------|-------------------|-----------------|
| `Strict` | Same-site requests only | Strong — not sent on any cross-site navigation |
| `Lax` | Same-site + top-level GET navigations (links, redirects) | Moderate — blocks POST-based CSRF |
| `None` | All requests (cross-site included) | None — requires `Secure` |

**Strict** — Best security. The user will appear logged out when arriving from an external link (e.g., from an email link). The cookie is sent only when the request originates from the same site.

**Lax** — Good default for most apps. Users can arrive from external links and still be logged in. Protects against cross-site POST attacks (the most common CSRF vector).

**None** — Used when the cookie must be sent cross-origin (e.g., an embedded widget, a third-party API). Must always be paired with `Secure`.

**Why it matters:** `SameSite=Strict` or `Lax` is the primary modern defense against CSRF attacks, often replacing or supplementing CSRF tokens.

### Domain

```
Set-Cookie: sessionId=abc123; Domain=example.com
```

**What it does:** Specifies which domains the browser should send the cookie to. If set to `example.com`, the cookie is sent to `example.com` and all subdomains (`app.example.com`, `api.example.com`).

**Why it matters:** Without `Domain`, the cookie is only sent to the exact origin that set it (more restrictive). Setting `Domain` broadens the scope.

**When to use:** Only set `Domain` when you intentionally need the cookie shared across subdomains. For maximum restriction, omit the `Domain` attribute entirely — the cookie will only be sent to the exact domain.

**Warning:** Setting `Domain` to a public suffix (e.g., `.co.uk`) is blocked by browsers. Setting it to a parent domain you do not control is a security risk.

### Path

```
Set-Cookie: sessionId=abc123; Path=/
Set-Cookie: refresh_token=xyz; Path=/auth
```

**What it does:** Limits the cookie to a specific URL path prefix. The cookie is only sent when the request URL starts with the specified path.

**Why it matters:** Reduces the exposure of sensitive cookies. A refresh token with `Path=/auth` is only sent to `/auth/*` endpoints — not to every API route.

**When to use:**
- Set `Path=/` for general session cookies that need to work across your entire app
- Set `Path=/auth` for refresh tokens — they are only needed at the token refresh endpoint
- Set a specific path for cookies used by admin panels (`Path=/admin`)

### The __Host- and __Secure- Cookie Prefixes

Cookie prefixes enforce specific security attributes. Browsers reject cookies with these prefixes if the attributes are missing.

```
Set-Cookie: __Host-sessionId=abc123; Secure; HttpOnly; Path=/
```

**`__Host-` prefix requirements:**
- Must have `Secure`
- Must not have `Domain` (most restrictive — tied to exact host)
- Must have `Path=/`

**`__Secure-` prefix requirements:**
- Must have `Secure`

Using `__Host-` is the most secure option for session cookies as it prevents subdomain attacks (a compromised subdomain cannot set or override the session cookie for the main domain).

### Complete Secure Cookie Example

```typescript
import session from 'express-session';

app.use(session({
  name: '__Host-sid',         // Enforce Secure + no Domain + Path=/
  secret: process.env.SESSION_SECRET!,
  resave: false,
  saveUninitialized: false,
  store: redisStore,
  cookie: {
    httpOnly: true,           // No JS access — mitigates XSS token theft
    secure: true,             // HTTPS only — prevents network sniffing
    sameSite: 'strict',       // No cross-site sending — prevents CSRF
    maxAge: 24 * 60 * 60 * 1000,  // 24 hours absolute expiry
    // Domain: intentionally omitted — restricts to exact host
    // Path: forced to '/' by __Host- prefix
  },
}));
```

### Cookie Security Checklist

| Attribute | Purpose | Required for auth cookies? |
|-----------|---------|---------------------------|
| `HttpOnly` | Blocks JS access | Yes |
| `Secure` | HTTPS only | Yes (production) |
| `SameSite=Strict/Lax` | CSRF protection | Yes |
| `Domain` | Cross-subdomain scope | Only if needed |
| `Path` | Limit URL scope | Recommended |
| `__Host-` prefix | Strictest host binding | Strongly recommended |
| `maxAge` / `expires` | Session lifetime | Yes — avoid session cookies for sensitive apps |
