# Authentication Concepts

## 1. Authentication vs Authorization

These terms are frequently confused and used interchangeably, but they describe fundamentally different processes.

### Authentication (AuthN) — "Who are you?"

Authentication is the process of verifying identity. It answers: "Are you who you claim to be?"

- Username + password check
- Biometric verification (fingerprint, Face ID)
- Certificate validation (mTLS)
- One-time password (OTP/TOTP)

The output of authentication is an **identity**: "This is confirmed to be user alice@example.com."

### Authorization (AuthZ) — "What can you do?"

Authorization is the process of determining what an authenticated identity is allowed to do. It answers: "Do you have permission to perform this action on this resource?"

- Role-Based Access Control (RBAC): admins can delete users, regular users cannot
- Attribute-Based Access Control (ABAC): user can read documents they own
- Scope-based (OAuth2): a token with `read:email` scope cannot write data

The output of authorization is a **decision**: "Allow" or "Deny."

### Why the Distinction Matters

```
Authentication: Is this a valid JWT signed by our server for user_123?   → Yes
Authorization:  Can user_123 delete post_456 (owned by user_789)?        → No

Authentication failure → 401 Unauthorized  (confusingly named)
Authorization failure  → 403 Forbidden
```

```typescript
import { Request, Response, NextFunction } from 'express';

// Authentication middleware — verifies identity
export function authenticate(req: AuthRequest, res: Response, next: NextFunction): void {
  const token = extractBearerToken(req);
  if (!token) {
    res.status(401).json({ error: 'Authentication required' }); // 401
    return;
  }
  try {
    req.user = verifyToken(token);
    next();
  } catch {
    res.status(401).json({ error: 'Invalid or expired token' }); // 401
  }
}

// Authorization middleware — verifies permissions
export function authorize(permission: string) {
  return (req: AuthRequest, res: Response, next: NextFunction): void => {
    if (!req.user) {
      res.status(401).json({ error: 'Not authenticated' });
      return;
    }
    if (!req.user.permissions?.includes(permission)) {
      res.status(403).json({ error: 'Forbidden — insufficient permissions' }); // 403
      return;
    }
    next();
  };
}

// Usage
app.delete(
  '/api/users/:id',
  authenticate,               // Step 1: Who are you?
  authorize('users:delete'),  // Step 2: Can you do this?
  deleteUserHandler
);
```

---

## 2. Password Hashing

Passwords must never be stored in plaintext. If your database is breached, plaintext passwords expose users on every other site where they reuse the same password.

### Why MD5 and SHA1 are Wrong for Passwords

MD5 and SHA1 are general-purpose **cryptographic hash functions** designed to be fast. For password storage, fast is bad:

- A modern GPU can compute billions of MD5 hashes per second
- Pre-computed rainbow tables cover most common passwords for MD5/SHA1
- They lack a **salt** parameter built-in — developers often omit salting
- They have no **work factor** — you cannot make them slower as hardware improves

**Never use MD5, SHA1, SHA256, or any fast hash function directly for passwords.**

### bcrypt

bcrypt was designed specifically for password hashing. It incorporates:
- **Salting** — automatically generates a unique random salt per password; prevents rainbow table attacks
- **Work factor (cost)** — you control how slow the hash is; increase it as hardware improves
- **Slow by design** — thousands of iterations; brute-force is computationally expensive

```typescript
import bcrypt from 'bcrypt';

// Hashing a password
async function hashPassword(plaintext: string): Promise<string> {
  const saltRounds = 12; // Work factor: 2^12 iterations
  // At cost 12: ~250ms per hash on modern hardware — fast enough for UX,
  // slow enough to make brute-force impractical
  return bcrypt.hash(plaintext, saltRounds);
}

// Verifying a password
async function verifyPassword(plaintext: string, hash: string): Promise<boolean> {
  // bcrypt.compare is timing-safe — no timing oracle attacks
  return bcrypt.compare(plaintext, hash);
}

// Usage in registration
app.post('/auth/register', async (req, res) => {
  const { email, password } = req.body;

  if (password.length < 12) {
    return res.status(400).json({ error: 'Password must be at least 12 characters' });
  }

  const hash = await hashPassword(password);
  await db.users.create({ email, passwordHash: hash });
  res.status(201).json({ message: 'Account created' });
});

// Usage in login
app.post('/auth/login', async (req, res) => {
  const { email, password } = req.body;
  const user = await db.users.findByEmail(email);

  // Always run bcrypt.compare even if user not found to prevent timing attacks
  const hash = user?.passwordHash ?? '$2b$12$invalidhashpadding000000000000000000000000000000000000';
  const valid = await verifyPassword(password, hash);

  if (!user || !valid) {
    return res.status(401).json({ error: 'Invalid credentials' });
  }

  // Issue tokens...
});
```

**Choosing cost factor:**
- Cost 10: ~65ms — minimum acceptable; use for tests
- Cost 12: ~250ms — recommended for most production apps
- Cost 14: ~1000ms — for high-security apps where UX can tolerate it
- Benchmark on your production hardware and pick the highest cost that keeps login under 1 second

### argon2

argon2 won the 2015 Password Hashing Competition and is now the recommended choice for new systems. It is more resistant than bcrypt to GPU-based attacks because it is memory-hard.

- **argon2id** — the recommended variant; resists both side-channel and GPU attacks
- Parameters: time cost (iterations), memory cost (RAM), parallelism

```typescript
import argon2 from 'argon2';

async function hashPasswordArgon2(plaintext: string): Promise<string> {
  return argon2.hash(plaintext, {
    type: argon2.argon2id,
    memoryCost: 2 ** 16, // 64 MB — makes GPU attacks expensive
    timeCost: 3,          // 3 iterations
    parallelism: 1,
  });
}

async function verifyPasswordArgon2(plaintext: string, hash: string): Promise<boolean> {
  return argon2.verify(hash, plaintext);
}
```

**argon2 vs bcrypt:**
| | bcrypt | argon2id |
|--|--------|---------|
| Memory-hard | No | Yes |
| GPU resistance | Moderate | Strong |
| OWASP recommendation | Acceptable | Preferred |
| Node.js library maturity | Very mature | Mature |
| Hash output includes params | Yes | Yes |

**Choose argon2id for new projects. Use bcrypt if your environment does not support argon2 native bindings.**

---

## 3. OAuth2 Flows

OAuth2 is an authorization framework that allows a third-party application to obtain limited access to a service on behalf of a user, without the user sharing their password.

### Key Roles

- **Resource Owner** — the user
- **Client** — the application requesting access
- **Authorization Server** — issues access tokens (e.g., Google, GitHub, your own auth server)
- **Resource Server** — the API being accessed (e.g., Google Drive API)

### Authorization Code Flow

Used for server-side web apps where the client secret can be kept confidential.

```
User                  Client (server)           Authorization Server        Resource Server
 |                         |                            |                          |
 |-- clicks "Login" ------>|                            |                          |
 |                         |-- redirect to auth ------->|                          |
 |                         |   client_id, redirect_uri  |                          |
 |                         |   scope, state, response_type=code                    |
 |<-- browser redirected --|                            |                          |
 |                                                      |                          |
 |-- user authenticates ----------------------------- ->|                          |
 |-- user grants consent ----------------------------- >|                          |
 |                                                      |                          |
 |<-- redirect to redirect_uri ------------------------|                          |
 |   ?code=AUTH_CODE&state=ORIGINAL_STATE               |                          |
 |                         |<-- browser follows redirect|                          |
 |                         |                            |                          |
 |                         |-- POST /token ------------>|                          |
 |                         |   code, client_id          |                          |
 |                         |   client_secret            |                          |
 |                         |   redirect_uri             |                          |
 |                         |                            |                          |
 |                         |<-- access_token -----------|                          |
 |                         |   refresh_token            |                          |
 |                         |                            |                          |
 |                         |-- GET /userinfo (access_token) ---------------------->|
 |                         |<-- user data ----------------------------------------|
```

```typescript
import crypto from 'crypto';
import axios from 'axios';

const GITHUB_CLIENT_ID = process.env.GITHUB_CLIENT_ID!;
const GITHUB_CLIENT_SECRET = process.env.GITHUB_CLIENT_SECRET!;
const REDIRECT_URI = 'https://myapp.com/auth/callback';

// Step 1: Redirect user to authorization server
app.get('/auth/github', (req, res) => {
  // state prevents CSRF on the callback
  const state = crypto.randomBytes(16).toString('hex');
  req.session.oauthState = state;

  const params = new URLSearchParams({
    client_id: GITHUB_CLIENT_ID,
    redirect_uri: REDIRECT_URI,
    scope: 'user:email read:user',
    state,
    response_type: 'code',
  });

  res.redirect(`https://github.com/login/oauth/authorize?${params}`);
});

// Step 2: Handle the callback
app.get('/auth/callback', async (req, res) => {
  const { code, state } = req.query as { code: string; state: string };

  // Verify state to prevent CSRF
  if (state !== req.session.oauthState) {
    return res.status(400).json({ error: 'Invalid state parameter' });
  }
  delete req.session.oauthState;

  // Exchange authorization code for access token
  const tokenResponse = await axios.post(
    'https://github.com/login/oauth/access_token',
    {
      client_id: GITHUB_CLIENT_ID,
      client_secret: GITHUB_CLIENT_SECRET, // kept server-side, never exposed
      code,
      redirect_uri: REDIRECT_URI,
    },
    { headers: { Accept: 'application/json' } }
  );

  const accessToken = tokenResponse.data.access_token;

  // Fetch user data from the resource server
  const userResponse = await axios.get('https://api.github.com/user', {
    headers: { Authorization: `Bearer ${accessToken}` },
  });

  const githubUser = userResponse.data;
  // Create or update local user record, then create session
  const localUser = await upsertUser({ githubId: githubUser.id, email: githubUser.email });

  req.session.regenerate(() => {
    req.session.userId = localUser.id;
    res.redirect('/dashboard');
  });
});
```

### Authorization Code Flow with PKCE

See Section 4 for a complete explanation of PKCE.

### Client Credentials Flow

Used for **machine-to-machine (M2M)** communication — no user is involved. A service authenticates itself to another service.

```
Service A                   Authorization Server           Service B (Resource Server)
    |                              |                                  |
    |-- POST /token --------------->|                                  |
    |   grant_type=client_credentials                                  |
    |   client_id=svc_a                                                |
    |   client_secret=secret                                           |
    |   scope=service_b:read                                           |
    |                              |                                  |
    |<-- access_token -------------|                                  |
    |                              |                                  |
    |-- GET /api/data (access_token) --------------------------------->|
    |<-- 200 OK (data) ------------------------------------------------|
```

```typescript
interface ClientCredentialsConfig {
  tokenUrl: string;
  clientId: string;
  clientSecret: string;
  scope: string;
}

class ServiceAuthClient {
  private cachedToken: string | null = null;
  private tokenExpiry: number = 0;

  constructor(private config: ClientCredentialsConfig) {}

  async getAccessToken(): Promise<string> {
    // Return cached token if still valid (with 60s buffer)
    if (this.cachedToken && Date.now() < this.tokenExpiry - 60_000) {
      return this.cachedToken;
    }

    const response = await axios.post(
      this.config.tokenUrl,
      new URLSearchParams({
        grant_type: 'client_credentials',
        client_id: this.config.clientId,
        client_secret: this.config.clientSecret,
        scope: this.config.scope,
      }),
      { headers: { 'Content-Type': 'application/x-www-form-urlencoded' } }
    );

    this.cachedToken = response.data.access_token;
    this.tokenExpiry = Date.now() + response.data.expires_in * 1000;
    return this.cachedToken!;
  }

  async fetch(url: string): Promise<any> {
    const token = await this.getAccessToken();
    const response = await axios.get(url, {
      headers: { Authorization: `Bearer ${token}` },
    });
    return response.data;
  }
}

// Usage
const paymentService = new ServiceAuthClient({
  tokenUrl: 'https://auth.internal/token',
  clientId: 'order-service',
  clientSecret: process.env.ORDER_SERVICE_SECRET!,
  scope: 'payments:create payments:read',
});

const result = await paymentService.fetch('https://payments.internal/api/charge');
```

### Flow Comparison

| Flow | Who authenticates? | Client secret needed? | Use case |
|------|-------------------|----------------------|----------|
| Authorization Code | User (via browser) | Yes (server-side) | Web apps with server-side backend |
| Authorization Code + PKCE | User (via browser) | No | SPAs, mobile apps, native apps |
| Client Credentials | Service/application | Yes | M2M, backend services, cron jobs |
| Implicit (deprecated) | User | No | Do not use — replaced by PKCE |

---

## 4. PKCE (Proof Key for Code Exchange)

### The Problem PKCE Solves

The Authorization Code flow requires a `client_secret` to exchange the authorization code for a token. SPAs and mobile/native apps **cannot safely store a client secret** — the secret would be embedded in publicly downloadable JavaScript or a decompilable binary.

**Without PKCE on a public client:**
```
1. Attacker intercepts the authorization code (via redirect URI misconfiguration,
   referrer header, log file, or malicious app on mobile)
2. Attacker POST /token with the stolen code + no secret needed (public client)
3. Attacker receives access token
```

PKCE ensures that even if the authorization code is stolen, it is useless without the original code verifier that only the legitimate client knows.

### How PKCE Works (Step by Step)

```typescript
import crypto from 'crypto';

// Step 1: Client generates a cryptographically random code verifier (43-128 chars)
function generateCodeVerifier(): string {
  return crypto.randomBytes(32).toString('base64url'); // 43 chars of base64url
}

// Step 2: Client derives the code challenge from the verifier
// code_challenge = BASE64URL(SHA256(ASCII(code_verifier)))
function generateCodeChallenge(verifier: string): string {
  return crypto
    .createHash('sha256')
    .update(verifier)
    .digest('base64url');
}

// Step 3: Client initiates the auth flow with the code challenge (NOT the verifier)
function buildAuthorizationUrl(
  clientId: string,
  redirectUri: string,
  scope: string
): { url: string; codeVerifier: string; state: string } {
  const codeVerifier = generateCodeVerifier();
  const codeChallenge = generateCodeChallenge(codeVerifier);
  const state = crypto.randomBytes(16).toString('hex');

  const params = new URLSearchParams({
    response_type: 'code',
    client_id: clientId,
    redirect_uri: redirectUri,
    scope,
    state,
    code_challenge: codeChallenge,
    code_challenge_method: 'S256',  // SHA256
  });

  return {
    url: `https://auth.example.com/authorize?${params}`,
    codeVerifier, // Store this securely (sessionStorage, not localStorage)
    state,
  };
}

// Step 4: Authorization server stores the code_challenge alongside the auth code

// Step 5: Client receives the authorization code in the redirect callback

// Step 6: Client exchanges code for token — sends code_verifier (not challenge)
async function exchangeCodeForToken(
  code: string,
  codeVerifier: string,
  clientId: string,
  redirectUri: string
): Promise<{ accessToken: string; refreshToken?: string }> {
  const response = await fetch('https://auth.example.com/token', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: new URLSearchParams({
      grant_type: 'authorization_code',
      code,
      redirect_uri: redirectUri,
      client_id: clientId,
      code_verifier: codeVerifier, // The original verifier, not the challenge
      // No client_secret needed
    }),
  });

  const data = await response.json();
  return {
    accessToken: data.access_token,
    refreshToken: data.refresh_token,
  };
}

// Step 7: Authorization server verifies:
//   SHA256(received_code_verifier) === stored_code_challenge
//   If they match → issue tokens
//   If they don't → reject (stolen code is useless without the verifier)
```

### Why a Stolen Code Is Now Useless

```
Attacker intercepts authorization code → "abc123"

Attacker tries to exchange:
POST /token
  code=abc123
  client_id=my-spa

Authorization server:
  Looks up stored code_challenge for code "abc123": "xYz...hash"
  Attacker has no code_verifier → cannot produce a value that hashes to "xYz...hash"
  Request rejected.
```

### PKCE Summary

| | Without PKCE | With PKCE |
|--|-------------|-----------|
| Client secret | Required (impossible for public clients) | Not needed |
| Code interception | Full compromise | Useless — no verifier |
| Mobile / SPA safe | No | Yes |
| Additional round trips | No | No (same flow, extra parameters) |

---

## 5. Multi-Factor Authentication (MFA) and TOTP

### Why MFA?

Passwords alone are a single point of failure. Credential stuffing, phishing, and data breaches mean that stolen passwords are common. MFA requires a second proof of identity:

- **Something you know** — password
- **Something you have** — phone with authenticator app, hardware key (YubiKey)
- **Something you are** — biometrics (fingerprint, Face ID)

Even if an attacker has the password, they need the second factor.

### TOTP — Time-based One-Time Password (RFC 6238)

TOTP is how authenticator apps (Google Authenticator, Authy, 1Password) work.

**How it works:**

```
1. Server generates a secret key (e.g., 20 random bytes)
2. Server encodes the secret as Base32 and displays it as a QR code
3. User scans QR code with authenticator app — app stores the secret
4. To generate a code:
   - Take the current Unix time divided by 30 (30-second window)
   - Compute HMAC-SHA1(secret, time_step)
   - Truncate to a 6-digit number
5. Both server and app independently compute the same code because they share
   the same secret and use the same current time
6. Codes change every 30 seconds
```

**Why it's secure:**
- Codes expire every 30 seconds — replay attacks are useless after 30s
- The secret never leaves the server or authenticator app (no network transmission per login)
- Brute force of a 6-digit code has a 1/1,000,000 chance per 30s window

```typescript
import speakeasy from 'speakeasy';
import QRCode from 'qrcode';

// Step 1: Generate and store a secret when user enables MFA
async function setupMFA(userId: string, userEmail: string): Promise<{
  secret: string;
  qrCodeUrl: string;
  backupCodes: string[];
}> {
  const secret = speakeasy.generateSecret({
    name: `MyApp (${userEmail})`,
    length: 20,
  });

  // Generate QR code for the authenticator app
  // otpauth:// URI format is understood by all authenticator apps
  const qrCodeUrl = await QRCode.toDataURL(secret.otpauth_url!);

  // Generate one-time backup codes for account recovery
  const backupCodes = Array.from({ length: 8 }, () =>
    crypto.randomBytes(4).toString('hex').toUpperCase()
  );
  const hashedBackupCodes = await Promise.all(
    backupCodes.map(code => bcrypt.hash(code, 10))
  );

  // Store secret and hashed backup codes — not the backup codes in plaintext
  await db.users.update(userId, {
    totpSecret: secret.base32,              // Store the base32 secret
    totpEnabled: false,                     // Not enabled until verified
    backupCodes: hashedBackupCodes,
  });

  return {
    secret: secret.base32,
    qrCodeUrl,
    backupCodes, // Show to user once — they must save these
  };
}

// Step 2: Verify first-time setup (user confirms they can generate codes)
async function verifyMFASetup(userId: string, token: string): Promise<boolean> {
  const user = await db.users.findById(userId);
  if (!user.totpSecret) throw new Error('MFA not initialized');

  const valid = speakeasy.totp.verify({
    secret: user.totpSecret,
    encoding: 'base32',
    token,
    window: 1, // Allow 1 step before/after to account for clock drift
  });

  if (valid) {
    await db.users.update(userId, { totpEnabled: true });
  }

  return valid;
}

// Step 3: Verify TOTP during login
async function verifyTOTP(userId: string, token: string): Promise<boolean> {
  const user = await db.users.findById(userId);

  if (!user.totpEnabled || !user.totpSecret) {
    return false;
  }

  // Check TOTP code
  const totpValid = speakeasy.totp.verify({
    secret: user.totpSecret,
    encoding: 'base32',
    token,
    window: 1,
  });

  if (totpValid) return true;

  // Check backup codes (for account recovery)
  for (let i = 0; i < user.backupCodes.length; i++) {
    const codeMatch = await bcrypt.compare(token, user.backupCodes[i]);
    if (codeMatch) {
      // Consume the backup code — each code is single-use
      const remainingCodes = user.backupCodes.filter((_: any, idx: number) => idx !== i);
      await db.users.update(userId, { backupCodes: remainingCodes });
      return true;
    }
  }

  return false;
}

// Step 4: MFA-aware login flow
app.post('/auth/login', async (req, res) => {
  const { email, password, totpToken } = req.body;
  const user = await db.users.findByEmail(email);

  const passwordValid = user && await bcrypt.compare(password, user.passwordHash);
  if (!passwordValid) {
    return res.status(401).json({ error: 'Invalid credentials' });
  }

  if (user.totpEnabled) {
    if (!totpToken) {
      // Tell the client to prompt for TOTP code (second step)
      return res.status(200).json({
        requiresMfa: true,
        // Issue a short-lived, limited token — not a full access token
        // This prevents the first factor from being useful without the second
        mfaToken: signMfaToken(user.id),
      });
    }

    // Validate TOTP on second request
    const mfaValid = await verifyTOTP(user.id, totpToken);
    if (!mfaValid) {
      return res.status(401).json({ error: 'Invalid MFA code' });
    }
  }

  // Both factors verified — issue full tokens
  const accessToken = signAccessToken({ sub: user.id, email: user.email, role: user.role });
  const refreshToken = generateRefreshToken(user.id);

  res.cookie('refresh_token', refreshToken, {
    httpOnly: true, secure: true, sameSite: 'strict', path: '/auth',
    maxAge: 7 * 24 * 60 * 60 * 1000,
  });

  res.json({ accessToken });
});

// Helper: short-lived MFA challenge token
function signMfaToken(userId: string): string {
  return jwt.sign({ sub: userId, scope: 'mfa_challenge' }, JWT_SECRET, {
    expiresIn: '5m',
    algorithm: 'HS256',
  });
}
```

### How Authenticator Apps Work (Internals)

```
Shared secret (base32): JBSWY3DPEHPK3PXP

Time step = Math.floor(Date.now() / 1000 / 30)
         = Math.floor(1723728000 / 30)
         = 57457600

HMAC = HMAC-SHA1(base32decode(secret), toBytes(timeStep, 8))
     = 20-byte hash

// Dynamic truncation
offset = HMAC[19] & 0xf           // last byte, low nibble
code_int = ((HMAC[offset]   & 0x7f) << 24)
         | ((HMAC[offset+1] & 0xff) << 16)
         | ((HMAC[offset+2] & 0xff) << 8)
         | ((HMAC[offset+3] & 0xff))

TOTP = code_int % 1_000_000        // 6 digits, zero-padded
     = "482 901"
```

Both the server and the app perform this exact computation. They agree because they share the same secret and both know the current 30-second time step.

### MFA Security Notes

- **Clock drift** — allow `window: 1` (±30 seconds) to handle slight clock differences between devices
- **Backup codes** — always provide 8-10 one-time backup codes for account recovery; hash them like passwords
- **TOTP secret storage** — store encrypted at rest; it is as sensitive as a password
- **Rate limiting** — limit TOTP verification attempts (e.g., 5 attempts per 10 minutes) to prevent online brute force
- **Used code tracking** — optionally record used codes within the current window to prevent same-window replay
- **Recovery flow** — provide a secure account recovery path (support verification, backup email) so users who lose their authenticator can regain access
