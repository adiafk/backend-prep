# System Design: Rate Limiter

## Requirements

**Functional**:
- Limit number of requests per user/IP per time window
- Return HTTP 429 when limit exceeded
- Support per-endpoint limits (login: 5/min, API: 1000/min)
- Distributed (multiple API servers, same limits globally)

**Non-functional**:
- < 5ms overhead per request
- Highly available (rate limiter outage should not block legitimate traffic)
- Accurate within ~1% (some flexibility on exact counts is acceptable)

---

## Algorithms

### Fixed Window Counter
Divide time into fixed windows (e.g., 60s). Count requests per window.

```
Window 0-60s:    User A → 99 requests ✓
Window 0-60s:    User A → 100th request ✓
Window 0-60s:    User A → 101st request ✗ (rate limited)
Window 60-120s:  Counter resets to 0
```

**Problem**: burst at window boundary. A user can send 100 at second 59, then 100 more at second 61 — 200 in 2 seconds.

### Sliding Window Log
Store timestamps of each request in a sorted set. On each request, remove old entries, count remaining.

Accurate but memory-intensive (O(requests) per user).

### Sliding Window Counter (Hybrid) — Best Practical Choice
Approximate sliding window using two fixed-window counters.

```
current = count in current window × (elapsed fraction) + count in previous window × (remaining fraction)
```

```typescript
async function slidingWindowRateLimit(
  userId: string,
  limitPerMin: number
): Promise<{ allowed: boolean; remaining: number }> {
  const now = Date.now();
  const windowMs = 60_000;
  const currentWindow = Math.floor(now / windowMs);
  const prevWindow = currentWindow - 1;

  const [prevCount, currCount] = await redis.mget(
    `rl:${userId}:${prevWindow}`,
    `rl:${userId}:${currentWindow}`
  );

  const elapsedInCurrentWindow = (now % windowMs) / windowMs;
  const estimated =
    (Number(prevCount ?? 0) * (1 - elapsedInCurrentWindow)) +
    Number(currCount ?? 0);

  if (estimated >= limitPerMin) {
    return { allowed: false, remaining: 0 };
  }

  await redis
    .pipeline()
    .incr(`rl:${userId}:${currentWindow}`)
    .expire(`rl:${userId}:${currentWindow}`, 120)
    .exec();

  return { allowed: true, remaining: Math.floor(limitPerMin - estimated - 1) };
}
```

### Token Bucket
Bucket holds N tokens. Each request consumes 1. Tokens refill at rate R/sec. Allows short bursts up to bucket size.

**Best for**: APIs that want to allow bursting but with a sustained rate cap.

### Leaky Bucket
Requests enter a queue (the "bucket"). Processed at a fixed rate, excess dropped.

**Best for**: smoothing out bursty traffic (e.g., sending emails at steady rate).

---

## Architecture

### Centralized (Redis)

```
Client → API Server 1 ─┐
Client → API Server 2 ─┼─► Redis Rate Limiter ─► Accept / Reject
Client → API Server 3 ─┘
```

All servers check the same Redis. Accurate, adds ~1-2ms latency.

### Distributed / Local First

For very high QPS, each server maintains a local counter. Periodically sync to Redis. Allows up to N × servers × local_limit requests before sync, so slightly less accurate but much faster.

### At the Edge (Cloudflare / API Gateway)

Rate limiting before traffic hits your servers. Zero latency impact on your backend. Use for:
- DDoS protection
- Bot mitigation
- Broad IP-based limits

---

## Express Middleware Implementation

```typescript
import { createClient } from "redis";
import { Request, Response, NextFunction } from "express";

const redis = createClient({ url: process.env.REDIS_URL });

interface RateLimitOptions {
  windowSec: number;
  limit: number;
  keyFn?: (req: Request) => string;  // default: IP
}

function rateLimit(opts: RateLimitOptions) {
  return async (req: Request, res: Response, next: NextFunction) => {
    const key = opts.keyFn
      ? opts.keyFn(req)
      : `rl:ip:${req.ip}`;

    const now = Date.now();
    const windowMs = opts.windowSec * 1000;
    const windowStart = now - windowMs;

    // Sliding window log (timestamp sorted set)
    await redis
      .multi()
      .zRemRangeByScore(key, 0, windowStart)
      .zAdd(key, { score: now, value: `${now}-${Math.random()}` })
      .zCard(key)
      .expire(key, opts.windowSec * 2)
      .exec();

    const count = await redis.zCard(key);

    res.setHeader("X-RateLimit-Limit", opts.limit);
    res.setHeader("X-RateLimit-Remaining", Math.max(0, opts.limit - count));

    if (count > opts.limit) {
      return res.status(429).json({
        error: "Too many requests",
        retryAfter: opts.windowSec,
      });
    }

    next();
  };
}

// Usage
app.use("/api/login", rateLimit({ windowSec: 60, limit: 5, keyFn: (req) => `rl:login:${req.ip}` }));
app.use("/api/", rateLimit({ windowSec: 60, limit: 1000, keyFn: (req) => `rl:user:${req.user?.id ?? req.ip}` }));
```

---

## Headers to Return

```
HTTP/1.1 429 Too Many Requests
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 0
X-RateLimit-Reset: 1692144060   # Unix timestamp when window resets
Retry-After: 30                  # seconds to wait
```

---

## Scale and Availability

| Concern | Solution |
|---------|---------|
| Redis single point of failure | Redis Cluster or Redis Sentinel |
| Redis latency spike | Local cache + async sync to Redis |
| Rate limiter outage | Fail open (allow traffic) — losing rate limiting < losing all traffic |
| Multi-region | Each region has own Redis; limits are per-region (or use global Redis with higher latency) |
