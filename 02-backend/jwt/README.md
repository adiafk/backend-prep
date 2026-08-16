# JWT Authentication

## 1. JWT Anatomy

A JSON Web Token is three base64url-encoded segments separated by dots:

```
header.payload.signature
```

Example token:
```
eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJ1c2VyXzEyMyIsImVtYWlsIjoiYWxpY2VAZXhhbXBsZS5jb20iLCJyb2xlIjoiYWRtaW4iLCJpYXQiOjE3MjM3MjgwMDAsImV4cCI6MTcyMzczMTYwMH0.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c
```

### Header

```json
{
  "alg": "HS256",
  "typ": "JWT"
}
```

The header declares the signing algorithm and token type. It is base64url-encoded (not encrypted — anyone can decode it).

### Payload (Claims)

```json
{
  "sub": "user_123",
  "email": "alice@example.com",
  "role": "admin",
  "iat": 1723728000,
  "exp": 1723731600
}
```

**Registered claims** (standardized by RFC 7519):
| Claim | Meaning |
|-------|---------|
| `sub` | Subject — identifies the principal (user ID) |
| `iss` | Issuer — who created the token |
| `aud` | Audience — who the token is intended for |
| `exp` | Expiration time (Unix timestamp) |
| `iat` | Issued at (Unix timestamp) |
| `nbf` | Not before — token invalid before this time |
| `jti` | JWT ID — unique identifier, used for revocation |

**Private claims** are custom fields you add (`role`, `email`, `permissions`, etc.).

The payload is base64url-encoded, NOT encrypted. Do not put secrets, passwords, or sensitive PII in a JWT payload.

### Signature

```
HMACSHA256(
  base64url(header) + "." + base64url(payload),
  secret
)
```

The signature proves the token has not been tampered with. If you change one byte of the header or payload, the signature verification will fail. It does NOT hide the content.

### Base64url Encoding

Base64url is standard Base64 with two character substitutions (`+` becomes `-`, `/` becomes `_`) and padding (`=`) removed. This makes the token URL-safe and safe to use in HTTP headers.

```typescript
// Decoding manually (for illustration — never do this for verification)
const [header, payload, signature] = token.split('.');
const decodedPayload = JSON.parse(
  Buffer.from(payload, 'base64url').toString('utf8')
);
```

---

## 2. Signing Algorithms

### HS256 (HMAC-SHA256) — Symmetric

Both signing and verification use the **same secret key**.

```
signature = HMAC-SHA256(header + "." + payload, secret)
```

**When to use:**
- Single server or monolith where only one service signs and verifies tokens
- Internal service-to-service communication in a trusted network
- Simpler setup with no PKI overhead

**Risks:**
- Any service that can verify tokens can also forge them (same key)
- Secret must be shared securely to every service that verifies tokens
- If the secret leaks, all past and future tokens are compromised

### RS256 (RSA-SHA256) — Asymmetric

Signing uses a **private key**; verification uses the corresponding **public key**.

```
signature = RSA-SHA256(header + "." + payload, privateKey)
verified  = RSA-verify(signature, header + "." + payload, publicKey)
```

**When to use:**
- Microservices architecture — only the auth service holds the private key; all other services only need the public key
- Third-party or federated identity (OpenID Connect, social login)
- Public key can be distributed freely via JWKS endpoint (`/.well-known/jwks.json`)

**Other algorithms:**
- **ES256** (ECDSA with P-256) — asymmetric like RS256 but produces smaller signatures and is faster; preferred for new systems
- **PS256** (RSA-PSS) — probabilistic variant of RSA, more secure than RS256 for advanced threat models

### Decision Matrix

| Scenario | Recommendation |
|----------|---------------|
| Single service | HS256 |
| Multiple services | RS256 or ES256 |
| Public JWKS endpoint | RS256 or ES256 |
| Mobile / IoT (small token size matters) | ES256 |

---

## 3. Access Tokens vs Refresh Tokens

### Access Token

- **Purpose:** Prove identity on each API request
- **Lifetime:** Short — typically 15 minutes to 1 hour
- **Storage:** Memory (JS variable) or httpOnly cookie
- **Format:** JWT (self-contained, no DB lookup needed)
- **What it contains:** User ID, roles, permissions, expiry

### Refresh Token

- **Purpose:** Obtain a new access token without re-authenticating
- **Lifetime:** Long — days to weeks
- **Storage:** httpOnly cookie (strongly preferred) or secure server-side session
- **Format:** Opaque random string (stored in database) or JWT
- **What it contains:** Minimal — just a reference to the session

### Lifecycle

```
1. User logs in with credentials
2. Server issues: access_token (15min) + refresh_token (7 days)
3. Client uses access_token for API calls
4. access_token expires → client sends refresh_token to /auth/refresh
5. Server validates refresh_token against DB
6. Server issues new access_token (and optionally rotates refresh_token)
7. Client discards old tokens, stores new ones
8. User logs out → server invalidates refresh_token in DB
```

### Refresh Token Rotation

Each time a refresh token is used, issue a new one and invalidate the old one. Store a "family" ID — if a used (already-rotated) refresh token is presented, invalidate the entire family (detect token theft).

---

## 4. JWT Vulnerabilities

### alg:none Attack

Early JWT libraries accepted `"alg": "none"` in the header, meaning no signature was required. An attacker could craft an arbitrary payload and strip the signature entirely.

**Attack:**
```json
// Modified header
{ "alg": "none", "typ": "JWT" }
// Payload with elevated privileges
{ "sub": "attacker", "role": "admin" }
// Token: base64url(header).base64url(payload).  (empty signature)
```

**Mitigation:** Always explicitly specify which algorithms are allowed during verification. Never pass `algorithms: ['none']` or rely on the token's declared algorithm without an allowlist.

### Weak Secrets (HS256)

Attackers can offline-brute-force HS256 tokens if the secret is short or guessable. The JWT header and payload are public; they just need to find a secret that reproduces the signature.

**Mitigation:** Use a cryptographically random secret of at least 256 bits (32 bytes). Example: `openssl rand -hex 32`.

### Token Theft

If an access token is stored in localStorage, any XSS vulnerability in your app can steal it. The attacker can then make API calls as the victim until the token expires.

**Mitigation:**
- Store tokens in httpOnly cookies (inaccessible to JavaScript)
- Implement short access token lifetimes
- Use refresh token rotation so stolen refresh tokens are detected on reuse

### Replay Attacks

A stolen valid token can be replayed until it expires. JWTs are stateless — the server has no record of which tokens are "in use."

**Mitigations:**
- Short expiry times reduce the replay window
- Bind tokens to IP/user-agent (context binding) — though this breaks mobile users changing networks
- Use `jti` claim with a blocklist for high-value operations
- Refresh token rotation detects stolen refresh tokens

### JWT Header Injection (kid, jku, x5u)

Some JWT libraries support `kid` (key ID), `jku` (JWK Set URL), or `x5u` (X.509 URL) header parameters. A malicious token could point `jku` to an attacker-controlled URL to supply a forged key.

**Mitigation:** Pin allowed key sources in your verification code. Never fetch keys from URLs specified in the token itself.

---

## 5. Token Storage: localStorage vs httpOnly Cookies

### localStorage

```javascript
localStorage.setItem('access_token', token);
// Send on requests:
fetch('/api/data', { headers: { Authorization: `Bearer ${token}` } });
```

**Pros:**
- Easy to implement
- Works well for SPAs
- Not sent on cross-origin requests automatically

**Cons:**
- Accessible via JavaScript — any XSS attack steals the token
- Persists across browser restarts (can be a problem if you want session-scoped tokens)
- No built-in CSRF protection needed, but XSS is a bigger risk

### httpOnly Cookies

```
Set-Cookie: access_token=<jwt>; HttpOnly; Secure; SameSite=Strict; Path=/api
```

**Pros:**
- Inaccessible to JavaScript — XSS cannot steal the token
- Browser manages sending automatically
- `SameSite=Strict` provides CSRF protection

**Cons:**
- Requires CSRF protection if not using `SameSite=Strict` (for cross-origin APIs)
- More complex to implement in cross-domain architectures
- Cookies are sent automatically — you must handle CSRF explicitly with older `SameSite` settings

### Recommendation

| Use Case | Recommendation |
|----------|---------------|
| SPA with same-domain API | httpOnly cookie with `SameSite=Strict` |
| SPA with cross-domain API | httpOnly cookie + CSRF token, or careful localStorage with strict CSP |
| Native mobile app | Secure storage (Keychain / Keystore), Authorization header |
| Server-to-server | Environment variable or secrets manager, Authorization header |

**Do not store refresh tokens in localStorage.** Their long lifetime makes theft catastrophic.

---

## 6. Token Expiry and Refresh Flow

### Flow Diagram

```
Client                          Auth Server                    Resource Server
  |                                  |                                |
  |-- POST /auth/login ------------->|                                |
  |   { email, password }            |                                |
  |                                  |-- verify credentials           |
  |<-- 200 OK ----------------------|                                |
  |   access_token  (15min)         |                                |
  |   refresh_token (7days, cookie) |                                |
  |                                  |                                |
  |-- GET /api/data (access_token) --------------------------------->|
  |                                                                   |-- verify JWT
  |<-- 200 OK (data) ------------------------------------------------|
  |                                                                   |
  |   ... 15 minutes later ...                                        |
  |                                                                   |
  |-- GET /api/data (expired access_token) ------------------------->|
  |                                                                   |-- verify JWT
  |<-- 401 Unauthorized --------------------------------------------|
  |   { code: "TOKEN_EXPIRED" }      |                                |
  |                                  |                                |
  |-- POST /auth/refresh ----------->|                                |
  |   (refresh_token via cookie)     |                                |
  |                                  |-- lookup refresh_token in DB   |
  |                                  |-- validate not expired/revoked |
  |                                  |-- rotate: issue new refresh_token|
  |<-- 200 OK ----------------------|                                |
  |   new access_token              |                                |
  |   new refresh_token (rotated)   |                                |
  |                                  |                                |
  |-- GET /api/data (new access_token) ------------------------------>|
  |<-- 200 OK (data) ------------------------------------------------|
```

### Complete TypeScript Implementation

```typescript
import jwt from 'jsonwebtoken';
import crypto from 'crypto';
import { Request, Response, NextFunction } from 'express';

// ---------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------
const JWT_SECRET = process.env.JWT_SECRET!; // 32+ random bytes
const ACCESS_TOKEN_EXPIRY = '15m';
const REFRESH_TOKEN_EXPIRY = '7d';
const REFRESH_TOKEN_EXPIRY_MS = 7 * 24 * 60 * 60 * 1000;

// ---------------------------------------------------------------
// Types
// ---------------------------------------------------------------
interface TokenPayload {
  sub: string;
  email: string;
  role: string;
  iat?: number;
  exp?: number;
}

interface RefreshTokenRecord {
  token: string;
  userId: string;
  family: string;       // for rotation theft detection
  expiresAt: Date;
  revokedAt?: Date;
}

// In production use Redis or a database
const refreshTokenStore = new Map<string, RefreshTokenRecord>();

// ---------------------------------------------------------------
// Token creation
// ---------------------------------------------------------------
export function signAccessToken(payload: Omit<TokenPayload, 'iat' | 'exp'>): string {
  return jwt.sign(payload, JWT_SECRET, {
    expiresIn: ACCESS_TOKEN_EXPIRY,
    algorithm: 'HS256',
  });
}

export function generateRefreshToken(userId: string, family?: string): string {
  const token = crypto.randomBytes(40).toString('hex');
  const tokenFamily = family ?? crypto.randomBytes(16).toString('hex');

  refreshTokenStore.set(token, {
    token,
    userId,
    family: tokenFamily,
    expiresAt: new Date(Date.now() + REFRESH_TOKEN_EXPIRY_MS),
  });

  return token;
}

// ---------------------------------------------------------------
// Token verification
// ---------------------------------------------------------------
export function verifyAccessToken(token: string): TokenPayload {
  // Explicitly declare allowed algorithms — never trust the token's alg header
  return jwt.verify(token, JWT_SECRET, {
    algorithms: ['HS256'],
  }) as TokenPayload;
}

// ---------------------------------------------------------------
// Refresh flow with rotation
// ---------------------------------------------------------------
export async function rotateRefreshToken(
  incomingRefreshToken: string
): Promise<{ accessToken: string; refreshToken: string }> {
  const record = refreshTokenStore.get(incomingRefreshToken);

  if (!record) {
    throw new Error('INVALID_REFRESH_TOKEN');
  }

  if (record.revokedAt) {
    // Token reuse detected — this refresh token was already rotated.
    // Revoke the entire family to invalidate all sessions derived from it.
    revokeTokenFamily(record.family);
    throw new Error('REFRESH_TOKEN_REUSE_DETECTED');
  }

  if (record.expiresAt < new Date()) {
    refreshTokenStore.delete(incomingRefreshToken);
    throw new Error('REFRESH_TOKEN_EXPIRED');
  }

  // Invalidate the incoming token
  record.revokedAt = new Date();
  refreshTokenStore.set(incomingRefreshToken, record);

  // Issue new tokens
  const newRefreshToken = generateRefreshToken(record.userId, record.family);

  // Fetch fresh user data for the access token payload
  // In production: const user = await db.users.findById(record.userId);
  const accessToken = signAccessToken({
    sub: record.userId,
    email: 'user@example.com', // from DB in production
    role: 'user',
  });

  return { accessToken, refreshToken: newRefreshToken };
}

function revokeTokenFamily(family: string): void {
  for (const [token, record] of refreshTokenStore.entries()) {
    if (record.family === family && !record.revokedAt) {
      record.revokedAt = new Date();
      refreshTokenStore.set(token, record);
    }
  }
}

// ---------------------------------------------------------------
// Express authentication middleware
// ---------------------------------------------------------------
export interface AuthRequest extends Request {
  user?: TokenPayload;
}

export function requireAuth(
  req: AuthRequest,
  res: Response,
  next: NextFunction
): void {
  const authHeader = req.headers.authorization;

  if (!authHeader || !authHeader.startsWith('Bearer ')) {
    res.status(401).json({ error: 'Missing authorization header' });
    return;
  }

  const token = authHeader.slice(7); // Remove "Bearer "

  try {
    const payload = verifyAccessToken(token);
    req.user = payload;
    next();
  } catch (err) {
    if (err instanceof jwt.TokenExpiredError) {
      res.status(401).json({ error: 'TOKEN_EXPIRED' });
    } else if (err instanceof jwt.JsonWebTokenError) {
      res.status(401).json({ error: 'INVALID_TOKEN' });
    } else {
      res.status(500).json({ error: 'INTERNAL_ERROR' });
    }
  }
}

export function requireRole(role: string) {
  return (req: AuthRequest, res: Response, next: NextFunction): void => {
    if (!req.user) {
      res.status(401).json({ error: 'Unauthenticated' });
      return;
    }
    if (req.user.role !== role) {
      res.status(403).json({ error: 'Insufficient permissions' });
      return;
    }
    next();
  };
}

// ---------------------------------------------------------------
// Express routes
// ---------------------------------------------------------------
import express from 'express';
import cookieParser from 'cookie-parser';

const app = express();
app.use(express.json());
app.use(cookieParser());

// Login
app.post('/auth/login', async (req, res) => {
  const { email, password } = req.body;

  // In production: verify against DB with bcrypt
  if (email !== 'alice@example.com' || password !== 'correct-password') {
    return res.status(401).json({ error: 'Invalid credentials' });
  }

  const userId = 'user_123';
  const accessToken = signAccessToken({ sub: userId, email, role: 'user' });
  const refreshToken = generateRefreshToken(userId);

  // Refresh token in httpOnly cookie — inaccessible to JavaScript
  res.cookie('refresh_token', refreshToken, {
    httpOnly: true,
    secure: true,         // HTTPS only
    sameSite: 'strict',
    maxAge: REFRESH_TOKEN_EXPIRY_MS,
    path: '/auth',        // Only sent to /auth routes
  });

  res.json({ accessToken });
});

// Refresh
app.post('/auth/refresh', async (req, res) => {
  const refreshToken = req.cookies.refresh_token;

  if (!refreshToken) {
    return res.status(401).json({ error: 'No refresh token' });
  }

  try {
    const { accessToken, refreshToken: newRefreshToken } =
      await rotateRefreshToken(refreshToken);

    res.cookie('refresh_token', newRefreshToken, {
      httpOnly: true,
      secure: true,
      sameSite: 'strict',
      maxAge: REFRESH_TOKEN_EXPIRY_MS,
      path: '/auth',
    });

    res.json({ accessToken });
  } catch (err: any) {
    const statusMap: Record<string, number> = {
      INVALID_REFRESH_TOKEN: 401,
      REFRESH_TOKEN_EXPIRED: 401,
      REFRESH_TOKEN_REUSE_DETECTED: 401,
    };
    const status = statusMap[err.message] ?? 500;
    res.status(status).json({ error: err.message });
  }
});

// Logout
app.post('/auth/logout', (req, res) => {
  const refreshToken = req.cookies.refresh_token;
  if (refreshToken) {
    const record = refreshTokenStore.get(refreshToken);
    if (record) {
      record.revokedAt = new Date();
      refreshTokenStore.set(refreshToken, record);
    }
  }
  res.clearCookie('refresh_token', { path: '/auth' });
  res.json({ message: 'Logged out' });
});

// Protected route
app.get('/api/profile', requireAuth, (req: AuthRequest, res) => {
  res.json({ user: req.user });
});

// Admin-only route
app.delete('/api/users/:id', requireAuth, requireRole('admin'), (req: AuthRequest, res) => {
  res.json({ deleted: req.params.id });
});
```

---

## 7. Revoking JWTs

JWTs are stateless by design. Once issued, the server has no way to invalidate them before their `exp` claim — the server does not track which tokens are "active." This is the fundamental JWT revocation problem.

### The Problem

```
1. User logs in → receives JWT valid for 1 hour
2. User changes password (security incident)
3. Old JWT is still valid for up to 1 hour
4. Attacker with the stolen token can still make requests
```

### Solution 1: Short Expiry Times

The simplest mitigation. If tokens expire in 15 minutes, the maximum exploit window after revocation is 15 minutes.

**Tradeoff:** More frequent refresh requests; more load on the auth server.

### Solution 2: Token Blocklist (Denylist)

Store revoked `jti` (JWT ID) values in Redis or a database. On every request, check if the token's `jti` is in the blocklist.

```typescript
import { createClient } from 'redis';

const redis = createClient({ url: process.env.REDIS_URL });

// When signing tokens, always include jti
export function signAccessTokenWithJti(payload: Omit<TokenPayload, 'iat' | 'exp'>): string {
  const jti = crypto.randomUUID();
  return jwt.sign({ ...payload, jti }, JWT_SECRET, {
    expiresIn: ACCESS_TOKEN_EXPIRY,
    algorithm: 'HS256',
  });
}

// Revoke a specific token
export async function revokeToken(token: string): Promise<void> {
  const decoded = jwt.decode(token) as any;
  if (!decoded?.jti || !decoded?.exp) return;

  const ttl = decoded.exp - Math.floor(Date.now() / 1000);
  if (ttl > 0) {
    // Store with TTL matching the token's remaining lifetime
    await redis.setEx(`blocklist:${decoded.jti}`, ttl, '1');
  }
}

// Middleware with blocklist check
export async function requireAuthWithRevocation(
  req: AuthRequest,
  res: Response,
  next: NextFunction
): Promise<void> {
  const authHeader = req.headers.authorization;
  if (!authHeader?.startsWith('Bearer ')) {
    res.status(401).json({ error: 'Missing token' });
    return;
  }

  const token = authHeader.slice(7);
  try {
    const payload = jwt.verify(token, JWT_SECRET, {
      algorithms: ['HS256'],
    }) as TokenPayload & { jti?: string };

    if (payload.jti) {
      const blocked = await redis.get(`blocklist:${payload.jti}`);
      if (blocked) {
        res.status(401).json({ error: 'TOKEN_REVOKED' });
        return;
      }
    }

    req.user = payload;
    next();
  } catch (err) {
    if (err instanceof jwt.TokenExpiredError) {
      res.status(401).json({ error: 'TOKEN_EXPIRED' });
    } else {
      res.status(401).json({ error: 'INVALID_TOKEN' });
    }
  }
}
```

**Tradeoff:** Requires a fast external store (Redis) — adds latency to every request. Defeats some of the "stateless" benefits of JWTs.

### Solution 3: Refresh Token Rotation (Recommended)

Do not rely on revoking access tokens. Instead:
- Keep access tokens short-lived (15 minutes)
- Store refresh tokens in the database
- On revocation (logout, password change, suspicious activity), invalidate the refresh token in the DB
- The user will be forced to re-authenticate after at most 15 minutes

This is the most practical approach for most applications.

### Solution 4: Version Number in User Record

Store a `tokenVersion` integer on the user record. Include it in the JWT. On each request, verify the JWT's `tokenVersion` matches the DB. Increment the version to invalidate all existing tokens.

```typescript
// JWT payload includes version
{ sub: "user_123", tokenVersion: 5, role: "user" }

// Middleware verifies version
const user = await db.users.findById(payload.sub);
if (user.tokenVersion !== payload.tokenVersion) {
  return res.status(401).json({ error: 'TOKEN_INVALIDATED' });
}
```

**Tradeoff:** Requires one DB lookup per request — reduces statelessness but is simpler than a full blocklist.

### Comparison

| Strategy | Revocation Speed | DB Lookups | Complexity |
|----------|-----------------|------------|------------|
| Short expiry only | Up to expiry window | None | Low |
| Blocklist (Redis) | Immediate | 1 per request | Medium |
| Refresh token rotation | Up to access token TTL | On refresh only | Medium |
| Token version | Immediate | 1 per request | Low |
