# Scalability

## Vertical vs Horizontal Scaling

**Vertical (scale up)**: bigger machine — more CPU, RAM, faster SSD. Simple, no code changes. Hard limit: you can't add more CPUs infinitely. Single point of failure.

**Horizontal (scale out)**: more machines. Requires stateless servers (session stored externally), load balancer, distributed coordination. Scales to any size. Preferred for web tier.

---

## Stateless Architecture

Stateless servers can be replaced, duplicated, or restarted without losing data. The trick: move all state out of the server process.

```
User → Load Balancer → Server A (stateless)
                     → Server B (stateless)
                     → Server C (stateless)
                              ↓
                       Redis (sessions)
                       PostgreSQL (data)
                       S3 (files)
```

If Server A dies, the load balancer routes to B or C with no user impact. With sticky sessions, Server A's death loses session data.

---

## Database Scaling

### Read Replicas
Write to primary, read from replicas. 80-90% of database traffic is reads.

```
Writes → Primary DB
Reads  → Replica 1
         Replica 2
         Replica 3
```

Tradeoff: **replication lag** — replicas are slightly behind. Don't read your own writes from a replica immediately after writing.

### Sharding (Horizontal Partitioning)
Split data across multiple database servers by a shard key.

```
user_id % 3 = 0 → DB Shard A
user_id % 3 = 1 → DB Shard B
user_id % 3 = 2 → DB Shard C
```

Challenges:
- Cross-shard queries (JOIN across shards is expensive — run in application layer)
- Resharding when adding new shards
- Hot spots if shard key is skewed (e.g., celebrity users)

### Connection Pooling
Opening a new DB connection costs ~50-100ms. Pools reuse connections.

```typescript
import { Pool } from "pg";

const pool = new Pool({
  max: 20,       // max connections
  idleTimeoutMillis: 30_000,
  connectionTimeoutMillis: 2_000,
});
```

---

## Caching Layers

```
Client
  → CDN (static assets, public responses)
    → Load Balancer
      → App Server
        → Redis (hot data, sessions, computed results)
          → Database (source of truth)
```

Cache hit rates compound — a 99% CDN hit rate means the origin server handles 1% of traffic.

---

## CDN (Content Delivery Network)

Caches static assets and (with edge computing) dynamic responses close to users.

- **Push CDN**: you push content to CDN proactively (good for large files)
- **Pull CDN**: CDN fetches from origin on cache miss, caches for TTL (simpler, good for websites)

Cache-Control headers drive CDN behavior:
```
Cache-Control: public, max-age=31536000, immutable    # versioned static assets (1 year)
Cache-Control: public, max-age=300                    # API responses (5 min)
Cache-Control: no-store                               # never cache (user-specific data)
```

---

## Async Processing

Don't do slow work synchronously in the request path.

```
HTTP Request → API Server → Queue (BullMQ / SQS)
                         → Return 202 Accepted

                Queue Consumer → Process job (email, resize image, call 3rd party)
```

- Users get instant feedback
- Work is retried on failure
- Load is decoupled from spikes

---

## Rate Limiting

Protect services from abuse and resource exhaustion. Common place: API gateway or reverse proxy (nginx, Cloudflare).

```typescript
// Token bucket in Redis — allow N requests per window per user
async function checkRateLimit(userId: string, limit = 100, windowSec = 60) {
  const key = `rate:${userId}`;
  const now = Date.now();
  const windowMs = windowSec * 1000;

  const pipeline = redis.pipeline();
  pipeline.zremrangebyscore(key, 0, now - windowMs);
  pipeline.zadd(key, now, `${now}`);
  pipeline.zcard(key);
  pipeline.expire(key, windowSec);
  const results = await pipeline.exec();

  const count = results![2][1] as number;
  return count <= limit;
}
```

---

## Microservices vs Monolith

| | Monolith | Microservices |
|--|---------|--------------|
| Deployment | One unit | Independent |
| Scaling | Scale everything | Scale hotspots |
| Latency | In-process calls | Network calls |
| Failure isolation | One crash = all down | Partial failures |
| Developer velocity | Fast early | Complex later |

**Recommendation**: start with a modular monolith. Extract services only when a specific boundary needs independent scaling or deployment.

---

## Back-of-Envelope: When to Scale

Ask yourself:
1. What's my QPS? (1 req/s = 86,400 req/day)
2. What does each request cost? (CPU time, DB queries, memory)
3. Where's the bottleneck?

Typical single-server limits:
- Simple web app: ~5,000 req/s
- PostgreSQL (simple queries): ~10,000 queries/s
- Redis GET/SET: ~100,000 ops/s
- Network IO: ~1 Gbps ≈ 125 MB/s

If your numbers exceed these, you need horizontal scaling, caching, or both.

---

## The Rule of Three

Before adding complexity (cache, queue, microservice), check if the problem actually exists:
1. Is it slow right now, or are you speculating?
2. Have you measured the bottleneck?
3. Is the simpler approach actually at capacity?

Premature optimization is the most common scalability mistake.
