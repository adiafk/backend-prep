# Rate Limiting

## 1. Why Rate Limiting Exists

Rate limiting controls the number of requests a client can make to a service within a given time window. It is not optional infrastructure — it is a fundamental safeguard for any public-facing API.

### DDoS Protection

A Distributed Denial of Service attack floods a service with more traffic than it can handle, rendering it unavailable. Rate limiting is the first line of defense at the application layer. Even when network-layer DDoS mitigations (e.g., Cloudflare, AWS Shield) absorb volumetric attacks, application-layer rate limiting handles slower, more targeted attacks that bypass those defenses — for example, an attacker making thousands of legitimate-looking login attempts per second.

### Fair Use

Without rate limiting, a single misbehaving or aggressive client can consume a disproportionate share of compute resources, degrading service quality for everyone else. Rate limiting enforces fairness: each client gets a defined quota, and no one client can crowd out others. This is especially important in multi-tenant SaaS systems where one tenant's traffic spike should not affect another tenant's latency.

### Cost Control

Many backend services incur per-request costs: database queries, third-party API calls, LLM inference, object storage reads. A client sending 10,000 requests per minute where 100 is reasonable can trigger unexpected billing spikes. Rate limiting creates a hard ceiling on per-client cost exposure. It also prevents accidental runaway loops in client code from becoming expensive incidents.

### Additional Motivations

- **Credential stuffing / brute force prevention**: Limits login attempts, preventing automated password guessing.
- **Web scraping deterrence**: Raises the cost for bots harvesting your data.
- **SLA enforcement**: Allows tiered products where free-tier clients get lower limits than paid clients.

---

## 2. Rate Limiting Algorithms

### Fixed Window

**How it works**: Divide time into fixed intervals (e.g., each minute from :00 to :59). Count requests within the current window. If the count exceeds the limit, reject until the window resets.

```
Window 1: 12:00:00 - 12:00:59  [count: 0 → 100] limit = 100
Window 2: 12:01:00 - 12:01:59  [count: 0 → 100] limit = 100
```

**Pros**:
- Simple to implement (one counter per window).
- Predictable reset times clients can rely on.
- Very low memory footprint.

**Cons**:
- Susceptible to the "boundary burst" problem. A client can send 100 requests at 12:00:59 and another 100 at 12:01:00 — 200 requests in 2 seconds while technically never exceeding the per-minute limit.
- Not suitable for smooth traffic shaping.

---

### Sliding Window (Log-based)

**How it works**: Maintain a timestamped log of every request. When a new request arrives, remove entries older than the window duration, then count remaining entries. If count < limit, allow and record the timestamp; otherwise reject.

**Pros**:
- Eliminates the boundary burst problem entirely.
- Accurate — the limit applies to any rolling time window, not just clock-aligned windows.

**Cons**:
- High memory usage for high-traffic clients (one log entry per request).
- More complex to implement correctly in a distributed system.

---

### Sliding Window (Counter-based approximation)

**How it works**: Keep counters for the current and previous fixed windows. Estimate the count using a weighted average:

```
estimated_count = previous_window_count * (1 - elapsed_fraction) + current_window_count
```

Where `elapsed_fraction` is how far into the current window you are (0.0 to 1.0).

**Pros**:
- Low memory (two counters per client, not a full log).
- Much more accurate than pure fixed window.
- This is the approach used by Cloudflare and many production systems.

**Cons**:
- Still an approximation; edge cases near window boundaries can allow slightly more than the limit.

---

### Token Bucket

**How it works**: Each client has a "bucket" with a maximum capacity of N tokens. Tokens are added at a constant rate (e.g., 10 tokens/second). Each request consumes one token. If the bucket is empty, the request is rejected (or queued). If the bucket is full, additional tokens are discarded.

```
capacity = 100 tokens
refill_rate = 10 tokens/second
current = 80 tokens

request arrives → consume 1 token → current = 79
10 seconds idle → refill 100 tokens → current = 100 (capped)
burst of 100 requests → consume 100 tokens → current = 0 → next request rejected
```

**Pros**:
- Allows controlled bursting up to bucket capacity.
- Smooth average rate enforcement.
- Works well for APIs where occasional short bursts are acceptable.

**Cons**:
- Two parameters to tune (capacity and refill rate), which can be non-intuitive.
- State management across distributed nodes requires synchronization.

---

### Leaky Bucket

**How it works**: Requests enter a queue (the "bucket"). Requests are processed (leak) at a fixed constant rate. If the queue is full, incoming requests are dropped.

```
leak_rate = 10 requests/second
queue_capacity = 50

burst of 200 → 50 queued, 150 dropped
queue drains at 10/second regardless of incoming rate
```

**Pros**:
- Enforces a perfectly smooth output rate regardless of input burst patterns.
- Good for protecting downstream systems that cannot handle any spikes.

**Cons**:
- Does not allow any bursting — all traffic is smoothed to the constant rate.
- High-capacity queues can introduce latency for legitimate requests during bursts.
- Usually implemented with a queue, adding complexity.

---

### Algorithm Comparison Summary

| Algorithm | Burst Allowed | Memory Usage | Accuracy | Complexity |
|---|---|---|---|---|
| Fixed Window | Yes (boundary) | Very low | Low | Very simple |
| Sliding Window (log) | No | High | Exact | Moderate |
| Sliding Window (counter) | No (approx.) | Very low | High | Moderate |
| Token Bucket | Yes (controlled) | Low | High | Moderate |
| Leaky Bucket | No | Moderate | High | Higher |

---

## 3. Where to Implement Rate Limiting

### Nginx (Network/Reverse Proxy Layer)

Nginx has built-in rate limiting via `ngx_http_limit_req_module`.

```nginx
# Define a shared memory zone keyed by client IP
limit_req_zone $binary_remote_addr zone=api_limit:10m rate=100r/m;

server {
    location /api/ {
        limit_req zone=api_limit burst=20 nodelay;
        limit_req_status 429;
        proxy_pass http://backend;
    }
}
```

**When to use**: Simple IP-based rate limiting before requests reach application code. Very low latency overhead. Does not require application changes.

**Tradeoffs**:
- Limited logic — cannot rate limit by user ID, API key, or request body content.
- No easy way to share state across multiple Nginx instances without external coordination.
- Configuration changes require Nginx reload.

---

### API Gateway (AWS API Gateway, Kong, Apigee, etc.)

API gateways sit in front of your services and can apply rate limiting as a cross-cutting concern across all services in your organization.

**When to use**: Microservice architectures where many services need rate limiting with a consistent policy. Managed solutions (e.g., AWS API Gateway) handle distributed state automatically.

**Tradeoffs**:
- Adds a network hop and potential single point of failure.
- Managed gateways can be expensive at high throughput.
- Less flexibility than application-layer code for complex business logic (e.g., "premium users get 10x the limit").
- Vendor lock-in with managed solutions.

---

### Application Layer

Rate limiting logic embedded in the service itself (middleware, interceptors, etc.).

**When to use**: When rate limiting logic depends on application context — user tier, endpoint cost, business rules. Full control over limit values, error responses, and bypass logic.

**Tradeoffs**:
- Every service must implement its own rate limiting, or you need a shared library.
- Requires an external store (Redis) to share state across service instances.
- Adds application complexity and another dependency to manage.

---

### Layered Approach (Recommended)

Production systems typically combine layers:

1. **CDN/WAF** (Cloudflare, AWS WAF): Absorb large-scale volumetric attacks before they reach your infrastructure.
2. **Nginx/Load Balancer**: Coarse IP-based limits to protect origin servers.
3. **API Gateway**: Per-service limits, authentication-aware limits.
4. **Application layer**: Fine-grained per-user, per-endpoint, per-tier limits.

---

## 4. Rate Limit Headers

When a rate limiter rejects or allows a request, it should communicate quota state to the client via standard headers. This allows clients to implement backoff logic, display quota information, and avoid retrying prematurely.

### Standard Headers

**`X-RateLimit-Limit`**

The maximum number of requests allowed in the current window.

```
X-RateLimit-Limit: 100
```

**`X-RateLimit-Remaining`**

The number of requests remaining in the current window.

```
X-RateLimit-Remaining: 43
```

**`X-RateLimit-Reset`**

The Unix timestamp (seconds) when the current window resets and the counter returns to the full limit.

```
X-RateLimit-Reset: 1723756800
```

Some implementations use a relative seconds-until-reset value instead. The Unix epoch form is more interoperable.

**`Retry-After`**

Sent on `429 Too Many Requests` responses. Tells the client how many seconds to wait before retrying.

```
HTTP/1.1 429 Too Many Requests
Retry-After: 30
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 0
X-RateLimit-Reset: 1723756800
```

`Retry-After` can also be an HTTP date:
```
Retry-After: Fri, 15 Aug 2026 10:00:00 GMT
```

### Emerging Standard: IETF RateLimit Header Fields

The IETF draft (draft-ietf-httpapi-ratelimit-headers) standardizes these headers. Some frameworks are adopting the `RateLimit-Limit`, `RateLimit-Remaining`, and `RateLimit-Reset` naming (without the `X-` prefix). GitHub, Stripe, and other major APIs use the `X-RateLimit-*` convention today.

### Best Practices

- Always include rate limit headers on **every** response, not just on rejection. This lets clients proactively throttle themselves.
- On `429` responses, always include `Retry-After`.
- Use UTC Unix timestamps (not relative seconds) for `Reset` to avoid clock skew issues on the client.
- Document your rate limit tiers in your API reference so clients can plan accordingly.

---

## 5. Distributed Rate Limiting with Redis

In a horizontally scaled system, rate limit state must be shared across all instances. Redis is the standard solution due to its atomic operations and sub-millisecond latency.

### Basic Fixed Window with INCR + EXPIRE

```
INCR user:{userId}:window:{windowKey}
EXPIRE user:{userId}:window:{windowKey} 60
```

The `INCR` command atomically increments a counter, creating it if it does not exist. `EXPIRE` sets a TTL so the key auto-deletes when the window closes.

**Race condition**: If two requests arrive simultaneously on fresh keys, both may see count=1 after INCR but before EXPIRE is called by either. The key may never get an expiry. Fix with a Lua script to make INCR+EXPIRE atomic, or use `SET key 1 EX 60 NX` for initialization.

### Sliding Window with Sorted Sets

Redis sorted sets (ZSET) store members with a numeric score. Using the request timestamp as both the member and score allows efficient range queries:

```
ZADD user:{userId}:requests {timestamp} {requestId}
ZREMRANGEBYSCORE user:{userId}:requests 0 {windowStart}
ZCARD user:{userId}:requests
EXPIRE user:{userId}:requests {windowSeconds}
```

Wrap in a Lua script for atomicity (see full implementation below).

### Redis Lua Scripts for Atomicity

Redis executes Lua scripts atomically — no other command runs on the Redis server between script lines. This is the correct way to implement multi-step rate limiting logic without race conditions.

---

## 6. Per-User vs Per-IP vs Per-Endpoint Rate Limits

Different identifiers serve different purposes and should often be combined.

### Per-IP Rate Limiting

**Use case**: Unauthenticated endpoints (login, registration, public APIs). The only identifier available before a user is authenticated.

**Limitations**:
- NAT and corporate proxies can place thousands of legitimate users behind one IP. Aggressive IP limits may block entire offices.
- IPv6 allows address rotation, making IP-based limits easier to circumvent.
- Does not help with authenticated abuse (a single user using multiple IPs).

**Implementation key**: `ratelimit:ip:{clientIp}:window:{windowKey}`

### Per-User Rate Limiting

**Use case**: Authenticated APIs. Enforces fair use per account regardless of how many IPs the client uses.

**Advantages**:
- Accurate attribution — one user cannot consume another's quota.
- Supports tiered limits (free vs paid plans).

**Implementation key**: `ratelimit:user:{userId}:window:{windowKey}`

### Per-Endpoint Rate Limiting

**Use case**: Some endpoints are significantly more expensive than others. A search endpoint hitting Elasticsearch is orders of magnitude more expensive than a profile read from cache.

**Pattern**: Assign cost weights per endpoint. A search might cost 10 tokens from the bucket; a profile read costs 1.

**Implementation key**: `ratelimit:user:{userId}:endpoint:{endpointKey}:window:{windowKey}`

### Combining Strategies

A robust system layers multiple strategies:

| Layer | Identifier | Limit | Purpose |
|---|---|---|---|
| Global | IP | 1000 req/min | DDoS protection |
| Auth | IP | 10 req/min | Brute force protection |
| API | User ID | 500 req/min | Fair use |
| Expensive endpoint | User ID + endpoint | 20 req/min | Cost control |

---

## 7. Complete TypeScript Redis Sliding Window Rate Limiter

This implementation uses Redis sorted sets and Lua scripting for an exact, atomic sliding window rate limiter.

```typescript
import { createClient, RedisClientType } from "redis";

export interface RateLimitConfig {
  windowMs: number;      // Window size in milliseconds
  maxRequests: number;   // Maximum requests per window
  keyPrefix?: string;    // Optional key namespace
}

export interface RateLimitResult {
  allowed: boolean;
  limit: number;
  remaining: number;
  resetAt: number;       // Unix timestamp (seconds) when the window resets
  retryAfter?: number;   // Seconds to wait before retrying (only set when rejected)
}

// Lua script executed atomically on Redis.
// Arguments:
//   KEYS[1] = the rate limit key (sorted set)
//   ARGV[1] = current timestamp in milliseconds
//   ARGV[2] = window start timestamp (current - windowMs)
//   ARGV[3] = max allowed requests
//   ARGV[4] = window size in milliseconds (used to set TTL)
//   ARGV[5] = unique request ID (to avoid collisions in the sorted set)
const SLIDING_WINDOW_SCRIPT = `
  local key = KEYS[1]
  local now = tonumber(ARGV[1])
  local window_start = tonumber(ARGV[2])
  local max_requests = tonumber(ARGV[3])
  local window_ms = tonumber(ARGV[4])
  local request_id = ARGV[5]

  -- Remove entries outside the current window
  redis.call('ZREMRANGEBYSCORE', key, '-inf', window_start)

  -- Count requests in the current window
  local count = redis.call('ZCARD', key)

  if count < max_requests then
    -- Allow: add this request to the sorted set
    redis.call('ZADD', key, now, request_id)
    -- Refresh TTL to window size (clean up eventually even if client goes idle)
    redis.call('PEXPIRE', key, window_ms)
    return {1, count + 1, max_requests - count - 1}
  else
    -- Reject: do not add to the set
    -- Find the oldest entry to calculate reset time
    local oldest = redis.call('ZRANGE', key, 0, 0, 'WITHSCORES')
    local reset_at = 0
    if #oldest > 0 then
      reset_at = tonumber(oldest[2]) + window_ms
    end
    return {0, count, 0, reset_at}
  end
`;

export class SlidingWindowRateLimiter {
  private redis: RedisClientType;
  private config: Required<RateLimitConfig>;
  private scriptSha: string | null = null;

  constructor(redis: RedisClientType, config: RateLimitConfig) {
    this.redis = redis;
    this.config = {
      keyPrefix: "ratelimit",
      ...config,
    };
  }

  /**
   * Load the Lua script into Redis and cache its SHA for EVALSHA calls.
   * EVALSHA is more efficient than EVAL — Redis only compiles the script once.
   */
  private async ensureScriptLoaded(): Promise<string> {
    if (this.scriptSha) {
      return this.scriptSha;
    }
    this.scriptSha = await this.redis.scriptLoad(SLIDING_WINDOW_SCRIPT);
    return this.scriptSha;
  }

  /**
   * Check and apply rate limiting for a given identifier.
   *
   * @param identifier - Unique client identifier (user ID, IP address, etc.)
   * @param endpointKey - Optional endpoint identifier for per-endpoint limits
   * @returns RateLimitResult with allow/deny decision and quota headers
   */
  async check(
    identifier: string,
    endpointKey?: string
  ): Promise<RateLimitResult> {
    const now = Date.now();
    const windowStart = now - this.config.windowMs;
    const requestId = `${now}-${Math.random().toString(36).slice(2)}`;

    const keyParts = [this.config.keyPrefix, identifier];
    if (endpointKey) {
      keyParts.push(endpointKey);
    }
    const key = keyParts.join(":");

    try {
      const sha = await this.ensureScriptLoaded();

      const result = (await this.redis.evalSha(sha, {
        keys: [key],
        arguments: [
          now.toString(),
          windowStart.toString(),
          this.config.maxRequests.toString(),
          this.config.windowMs.toString(),
          requestId,
        ],
      })) as [number, number, number, number?];

      const [allowed, currentCount, remaining, resetAtMs] = result;
      const windowResetAt = resetAtMs
        ? Math.ceil(resetAtMs / 1000)
        : Math.ceil((now + this.config.windowMs) / 1000);

      if (allowed === 1) {
        return {
          allowed: true,
          limit: this.config.maxRequests,
          remaining,
          resetAt: windowResetAt,
        };
      } else {
        const retryAfterMs = resetAtMs ? resetAtMs - now : this.config.windowMs;
        return {
          allowed: false,
          limit: this.config.maxRequests,
          remaining: 0,
          resetAt: windowResetAt,
          retryAfter: Math.ceil(Math.max(retryAfterMs, 0) / 1000),
        };
      }
    } catch (error) {
      // If EVALSHA fails because the script was evicted (NOSCRIPT error),
      // reload it and retry once.
      if (error instanceof Error && error.message.includes("NOSCRIPT")) {
        this.scriptSha = null;
        return this.check(identifier, endpointKey);
      }
      throw error;
    }
  }

  /**
   * Build HTTP rate limit headers from a RateLimitResult.
   * Attach these headers to every API response.
   */
  static toHeaders(result: RateLimitResult): Record<string, string> {
    const headers: Record<string, string> = {
      "X-RateLimit-Limit": result.limit.toString(),
      "X-RateLimit-Remaining": result.remaining.toString(),
      "X-RateLimit-Reset": result.resetAt.toString(),
    };
    if (result.retryAfter !== undefined) {
      headers["Retry-After"] = result.retryAfter.toString();
    }
    return headers;
  }
}

// ---------------------------------------------------------------------------
// Express middleware factory
// ---------------------------------------------------------------------------

import type { Request, Response, NextFunction } from "express";

export interface RateLimitMiddlewareOptions extends RateLimitConfig {
  /**
   * Function to extract the rate limit identifier from the request.
   * Defaults to authenticated user ID if available, falling back to IP.
   */
  identifierFn?: (req: Request) => string;

  /**
   * Function to extract an endpoint key for per-endpoint limits.
   * Return undefined to apply a single global limit.
   */
  endpointKeyFn?: (req: Request) => string | undefined;

  /**
   * Custom handler when the rate limit is exceeded.
   * Defaults to 429 with JSON error body.
   */
  onLimitExceeded?: (
    req: Request,
    res: Response,
    result: RateLimitResult
  ) => void;
}

export function createRateLimitMiddleware(
  redis: RedisClientType,
  options: RateLimitMiddlewareOptions
) {
  const limiter = new SlidingWindowRateLimiter(redis, options);

  const defaultIdentifier = (req: Request): string => {
    // Prefer authenticated user ID for post-auth routes.
    // (req as any).user is a common Express pattern for auth middleware results.
    const userId = (req as any).user?.id;
    if (userId) return `user:${userId}`;

    // Fall back to IP address.
    const forwarded = req.headers["x-forwarded-for"];
    const ip = Array.isArray(forwarded)
      ? forwarded[0]
      : forwarded?.split(",")[0]?.trim() ?? req.socket.remoteAddress ?? "unknown";
    return `ip:${ip}`;
  };

  const defaultOnLimitExceeded = (
    _req: Request,
    res: Response,
    result: RateLimitResult
  ) => {
    res.status(429).json({
      error: "Too Many Requests",
      message: `Rate limit exceeded. Retry after ${result.retryAfter} seconds.`,
      retryAfter: result.retryAfter,
      resetAt: result.resetAt,
    });
  };

  return async (req: Request, res: Response, next: NextFunction) => {
    const identifier = options.identifierFn
      ? options.identifierFn(req)
      : defaultIdentifier(req);

    const endpointKey = options.endpointKeyFn
      ? options.endpointKeyFn(req)
      : undefined;

    try {
      const result = await limiter.check(identifier, endpointKey);
      const headers = SlidingWindowRateLimiter.toHeaders(result);

      // Always set rate limit headers, even on successful requests.
      Object.entries(headers).forEach(([name, value]) => {
        res.setHeader(name, value);
      });

      if (!result.allowed) {
        const handler = options.onLimitExceeded ?? defaultOnLimitExceeded;
        return handler(req, res, result);
      }

      return next();
    } catch (error) {
      // If rate limiting infrastructure is down, fail open (allow the request)
      // rather than failing closed (denying all traffic). Log the error.
      console.error("[RateLimiter] Redis error — failing open:", error);
      return next();
    }
  };
}

// ---------------------------------------------------------------------------
// Usage example
// ---------------------------------------------------------------------------

/*
import express from "express";
import { createClient } from "redis";
import { createRateLimitMiddleware } from "./rate-limiter";

const app = express();
const redis = createClient({ url: process.env.REDIS_URL });
await redis.connect();

// Global API limit: 100 requests per minute per user/IP
const globalLimiter = createRateLimitMiddleware(redis, {
  windowMs: 60_000,
  maxRequests: 100,
  keyPrefix: "ratelimit:global",
});

// Expensive endpoint limit: 10 requests per minute per user
const searchLimiter = createRateLimitMiddleware(redis, {
  windowMs: 60_000,
  maxRequests: 10,
  keyPrefix: "ratelimit:search",
  identifierFn: (req) => `user:${(req as any).user?.id ?? "anon"}`,
});

app.use("/api", globalLimiter);
app.get("/api/search", searchLimiter, (req, res) => {
  res.json({ results: [] });
});
*/
```

---

## Further Reading

- [IETF RateLimit Header Fields for HTTP](https://datatracker.ietf.org/doc/draft-ietf-httpapi-ratelimit-headers/)
- [Cloudflare: How we built rate limiting](https://blog.cloudflare.com/counting-things-a-lot-of-different-things/)
- [Redis documentation: EVALSHA](https://redis.io/docs/manual/programmability/eval-intro/)
- [Stripe Rate Limiting](https://stripe.com/blog/rate-limiters)
