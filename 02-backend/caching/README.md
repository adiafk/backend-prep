# Caching

## 1. Why Caching

Caching stores the result of an expensive operation so that subsequent requests for the same result can be served without repeating the work. Done correctly, it is one of the highest-leverage optimizations available.

### Latency Reduction

A database query might take 20-100ms. The same data served from an in-memory Redis cache takes <1ms. For a user-facing request that requires 10 database queries, caching can reduce response time by an order of magnitude. Network calls to third-party APIs (payment processors, weather services, mapping APIs) are even more costly — caching their responses eliminates round-trip latency entirely.

### Load Reduction

If 10,000 users request the homepage within a minute, and the homepage requires 5 database queries to render, that is 50,000 database queries per minute. If the homepage is cached for 60 seconds, that becomes 5 queries per minute — a 10,000x reduction. This allows a database that would otherwise be overwhelmed to serve its capacity for write-heavy or truly unique queries.

### Cost Control

Cloud database costs scale with IOPS, connection count, and CPU hours. Third-party APIs often charge per call. Caching reduces all of these. A well-cached system can serve 100x the traffic with the same infrastructure spend. For services paying per LLM inference token, caching responses to repeated prompts can dramatically reduce monthly API bills.

---

## 2. Cache Strategies

### Cache-Aside (Lazy Loading)

The application is responsible for managing the cache. On a read:

1. Check the cache for the requested data.
2. If found (cache hit): return the cached value.
3. If not found (cache miss): fetch from the source of truth (database), store the result in the cache, then return it.

On a write: update the source of truth. Either invalidate the cache entry (so the next read repopulates it) or update it directly.

```
Read:  app → cache (miss) → db → app → cache (write) → app → client
Read:  app → cache (hit) → app → client
Write: app → db → invalidate cache key
```

**Pros**:
- Simple to implement; the cache only holds data that is actually requested.
- Cache failures are non-fatal — the application falls back to the database transparently.
- Works with any caching layer without requiring changes to the database.

**Cons**:
- Cache misses are expensive (two round trips: cache miss + db fetch).
- Initial traffic after a cache cold start (or after invalidation) hits the database with full load.
- Data in the cache can become stale if the database is updated by a path that does not invalidate the cache.

**Best for**: General-purpose read-heavy workloads. The most common pattern.

---

### Write-Through

Every write goes to both the cache and the database simultaneously (or sequentially, with the cache updated first).

```
Write: app → cache (write) → db (write) → done
Read:  app → cache (always hits, if key exists) → client
```

**Pros**:
- Cache is always in sync with the database — reads are always served from cache after the first write.
- No stale data problem for keys that have been written at least once.

**Cons**:
- Every write pays the cost of two stores (cache + database). Higher write latency.
- Cache may fill with data that is rarely read (write-heavy workloads waste cache space).
- If the database write fails after the cache write succeeds, the cache contains data that was never persisted.

**Best for**: Read-heavy workloads where write latency is acceptable and stale reads are unacceptable. Session stores, user profile caches.

---

### Write-Behind (Write-Back)

The application writes to the cache only. The cache layer asynchronously flushes writes to the database in the background, typically in batches.

```
Write: app → cache (write, immediate return) → [async] cache → db
Read:  app → cache → client
```

**Pros**:
- Lowest write latency — the application does not wait for the database write.
- Database writes can be batched, reducing IOPS and improving throughput.

**Cons**:
- Data loss risk: if the cache crashes before flushing, writes are lost.
- Cache and database can be out of sync, complicating reads that bypass the cache.
- More complex to implement correctly (requires a durable queue or write-ahead log).

**Best for**: High write-throughput workloads where occasional data loss is acceptable (analytics counters, non-critical metrics). Rarely appropriate for financial or user data.

---

### Read-Through

The cache itself handles database lookups. The application always reads from the cache; on a miss, the cache automatically fetches from the database, stores the result, and returns it.

```
Read:  app → cache (miss → cache fetches from db) → app → client
Read:  app → cache (hit) → app → client
```

**Pros**:
- Application code is simple — it only ever talks to the cache.
- Cache miss logic is centralized in the cache layer, not duplicated across application code.

**Cons**:
- Requires a caching layer that supports this pattern natively (e.g., a caching proxy like Readthrough Redis libraries, or ORM-level caching).
- First-request latency is identical to cache-aside.

**Best for**: Applications using ORM-level caching or dedicated caching proxies.

---

### Strategy Comparison

| Strategy | Write Latency | Read Latency | Consistency | Complexity |
|---|---|---|---|---|
| Cache-aside | Low (db only) | High on miss | Eventual | Low |
| Write-through | High (cache + db) | Low | Strong | Low |
| Write-behind | Very low (cache only) | Low | Weak (async) | High |
| Read-through | Low (db only) | High on miss | Eventual | Low-Med |

---

## 3. Cache Invalidation

Phil Karlton famously said: "There are only two hard things in Computer Science: cache invalidation and naming things."

Cache invalidation is the process of removing or updating cached data when the underlying source of truth changes. Get it wrong and users see stale data. Get it too aggressive and the cache provides no benefit.

### TTL-Based Invalidation (Time-to-Live)

Every cached entry has an expiry. After the TTL, the entry is evicted and the next request fetches fresh data.

```
SET user:123:profile <data> EX 300   // expires in 5 minutes
```

**Pros**: Simple, requires no coordination between systems. Works without knowing when data changes.

**Cons**: Stale data window equals the TTL. Cannot guarantee freshness — data may be stale for up to TTL seconds after the source changes. Setting TTL too low eliminates the benefit; too high causes prolonged staleness.

**Choosing TTL values**:
- Highly volatile data (stock prices, real-time scores): very short TTL (5-30 seconds) or no caching.
- Moderately volatile data (user profiles, product details): medium TTL (1-15 minutes).
- Rarely changing data (configuration, lookup tables, country lists): long TTL (hours to days) with explicit invalidation on writes.

### Event-Based Invalidation

When the source of truth is updated, emit an event that explicitly deletes or updates the corresponding cache entry.

```
// On user profile update:
await db.update('users', userId, newData);
await redis.del(`user:${userId}:profile`);  // invalidate
```

**Pros**: Cache is invalidated immediately when data changes. No staleness window beyond propagation delay.

**Cons**:
- Every write path must remember to invalidate the cache. Easy to miss edge cases.
- In distributed systems, invalidation events may arrive out of order or be lost.
- If multiple services write to the same data, all must emit invalidation events.

### Versioned Keys (Cache Busting)

Embed a version number or content hash in the cache key. When data changes, increment the version — old keys are never explicitly deleted but naturally expire via TTL or LRU eviction.

```
// Key includes version or hash
const key = `user:${userId}:profile:v${userVersion}`;
```

**Pros**: Eliminates invalidation races — old and new versions coexist safely. Safe for CDNs where explicit purges are expensive.

**Cons**: Old keys accumulate until TTL or LRU evicts them. Requires tracking the current version somewhere (often in the database or a separate key).

### Two-Layer Invalidation

For maximum consistency: use short TTLs as a safety net while also performing explicit invalidation on writes. Even if an invalidation event is missed, the data will go stale within the TTL, limiting the damage.

---

## 4. Cache Stampede / Thundering Herd

### The Problem

A cache stampede (also called a thundering herd) occurs when a popular cache entry expires and many concurrent requests simultaneously find a cache miss. All of them query the database at the same time to repopulate the cache — potentially crashing the database with a sudden spike in load.

```
Cache entry for "homepage" expires at T
T+1ms: 500 concurrent requests arrive
All 500 miss the cache, all 500 query the database simultaneously
Database is overwhelmed
```

This is especially damaging because it happens at the worst possible time: when the system is under load.

### Solution 1: Mutex / Distributed Lock

When a cache miss occurs, acquire a distributed lock before querying the database. Other concurrent misses see the lock is held and wait (or return a stale value). Only one request queries the database and repopulates the cache.

```typescript
async function getWithLock(key: string): Promise<Data> {
  const cached = await redis.get(key);
  if (cached) return JSON.parse(cached);

  const lockKey = `lock:${key}`;
  const lockAcquired = await redis.set(lockKey, "1", {
    NX: true,   // Only set if not exists
    PX: 5000,   // Lock expires in 5 seconds to prevent deadlock
  });

  if (lockAcquired) {
    try {
      const data = await db.fetch(key);
      await redis.set(key, JSON.stringify(data), { EX: 300 });
      return data;
    } finally {
      await redis.del(lockKey);
    }
  } else {
    // Lock is held by another request. Wait briefly and retry.
    await sleep(50);
    return getWithLock(key);  // Retry — will likely hit the cache now
  }
}
```

### Solution 2: Probabilistic Early Expiry (XFetch)

Instead of waiting for expiry, each request computes a probability of recomputing the cache before it actually expires, weighted by how close it is to expiry. This spreads cache refresh across many requests rather than concentrating it at the moment of expiry.

```typescript
function shouldRecompute(ttlRemaining: number, beta: number, delta: number): boolean {
  // beta controls how aggressively to refresh early (default: 1)
  // delta is the time it took to compute the cached value
  return Math.random() < beta * delta * Math.log(Math.random() / -1) / ttlRemaining;
}
```

This algorithm (XFetch, from Vattani et al., 2015) is statistically optimal for single-node caches.

### Solution 3: Background Refresh

Track when cache entries were last refreshed. Before they expire, a background job proactively refreshes them. Requests always find a valid cache entry.

```typescript
// Store both the value and the last-refreshed timestamp
await redis.set(key, JSON.stringify({
  value: data,
  refreshedAt: Date.now(),
}), { EX: 600 });

// Background job: refresh entries that are > 80% through their TTL
```

**Tradeoff**: Increases background load. Keys must be tracked in a separate index for the background job to enumerate them.

### Solution 4: Stale-While-Revalidate

Return the stale cache entry immediately while asynchronously refreshing it in the background. The client gets fast response; the cache is updated for subsequent requests.

```typescript
const cached = await redis.get(key);
const ttl = await redis.ttl(key);

if (cached) {
  if (ttl < 60) {
    // About to expire — refresh in background, don't block
    refreshInBackground(key).catch(console.error);
  }
  return JSON.parse(cached);
}

// Full miss — must fetch synchronously
const data = await db.fetch(key);
await redis.set(key, JSON.stringify(data), { EX: 300 });
return data;
```

---

## 5. CDN Caching vs Application Caching vs Database Caching

### CDN Caching

A Content Delivery Network caches responses at edge nodes geographically distributed close to users.

**What it caches**: HTTP responses — HTML, JSON API responses, images, scripts, stylesheets, videos.

**When to use**: Public, non-personalized content. Static assets. API responses that are identical for all users (e.g., a public product catalog). Responses served to global audiences where geographic proximity matters.

**How it works**: The CDN honors HTTP caching headers (`Cache-Control`, `ETag`, `Last-Modified`). Responses with `Cache-Control: public, max-age=3600` are cached at the edge for 1 hour.

**Limitations**: Cannot cache private, user-specific content (without careful Vary header management). Cache invalidation ("purging") can be slow to propagate globally. Not suitable for highly dynamic content.

**Examples**: Cloudflare, AWS CloudFront, Fastly, Akamai.

---

### Application Caching

An in-process or out-of-process cache managed by the application layer.

**What it caches**: Database query results, computed values, aggregations, session data, third-party API responses.

**When to use**: User-specific data that cannot be CDN-cached. Expensive database joins or aggregations. Results of computationally expensive operations (rendering, ML inference). Data shared across multiple application instances.

**In-process cache** (e.g., Node.js `Map`, LRU cache library):
- Sub-microsecond access.
- Does not survive process restarts.
- Not shared across multiple instances — each instance has its own cache.
- Suitable for small, frequently accessed, non-critical data.

**Out-of-process cache** (Redis, Memcached):
- ~1ms access (network hop).
- Survives application restarts.
- Shared across all application instances.
- The standard for production distributed systems.

---

### Database Caching

Caching within the database layer itself.

**Query result cache** (e.g., MySQL Query Cache, now deprecated; PostgreSQL `pg_prewarm`):
- The database engine caches the result of frequently executed identical queries.
- Invalidated automatically when any underlying table changes.
- Limited control from the application layer.

**Buffer pool / page cache**:
- Every production database keeps frequently accessed data pages in memory (InnoDB buffer pool, PostgreSQL shared_buffers).
- Tuning buffer pool size is one of the most impactful database performance optimizations.
- This is why well-tuned databases with hot working sets in memory are much faster than cold databases.

**Read replicas**:
- Distribute read load across multiple database nodes.
- Not a cache per se, but achieves similar load distribution goals.

**When database caching helps**: When the working set (the data accessed frequently) fits in memory. When query patterns are repetitive.

---

### Choosing the Right Layer

| Scenario | Recommended Layer |
|---|---|
| Static assets (images, JS, CSS) | CDN |
| Public API responses (non-personalized) | CDN + Application cache |
| User-specific data | Application cache (Redis) |
| Database query results | Application cache (Redis) |
| Expensive computations | Application cache (Redis) |
| Frequently accessed DB rows | DB buffer pool (tune buffer size) |
| Global, low-latency reads | CDN edge cache |

---

## 6. HTTP Caching Headers

HTTP caching is built into the protocol. Browsers, proxies, and CDNs all honor these headers.

### Cache-Control

The primary directive for controlling caching behavior. Multiple directives can be combined.

**Response directives** (server → client/CDN):

`Cache-Control: public`
The response may be cached by any cache, including shared caches (CDNs, proxies). Use for non-personalized, non-sensitive content.

`Cache-Control: private`
The response may only be cached by the end client (browser). Shared caches (CDNs) must not cache it. Use for personalized or sensitive responses.

`Cache-Control: no-cache`
The response may be cached but must be revalidated with the origin server before each use. The cache sends a conditional request (with `ETag` or `Last-Modified`); the origin responds with `304 Not Modified` if unchanged (saving bandwidth but not round trips).

`Cache-Control: no-store`
The response must not be stored in any cache under any circumstances. Use for sensitive data (banking, authentication tokens).

`Cache-Control: max-age=3600`
The response is fresh for 3600 seconds (1 hour) from the time it was cached. After that, it is stale and must be revalidated or refetched.

`Cache-Control: s-maxage=86400`
Like `max-age`, but applies only to shared caches (CDNs/proxies). Overrides `max-age` for shared caches. Useful when you want CDNs to cache for longer than browsers.

`Cache-Control: stale-while-revalidate=60`
Allows serving a stale response for up to 60 seconds while revalidating asynchronously in the background. Eliminates revalidation latency from the user's perspective.

`Cache-Control: stale-if-error=600`
If the origin server returns an error (5xx), serve the stale cached response for up to 600 seconds. Improves resilience.

`Cache-Control: must-revalidate`
Once stale, the response must be revalidated before serving — do not serve stale-while-revalidate even if enabled. Use for data where serving stale is unacceptable.

**Common combinations**:

```
# Public, cacheable for 1 day by CDN, 1 hour by browser
Cache-Control: public, max-age=3600, s-maxage=86400

# Private user data, cacheable by browser for 5 minutes
Cache-Control: private, max-age=300

# Never cache (login pages, payment pages)
Cache-Control: no-store

# Fingerprinted static assets (hash in filename, cache forever)
Cache-Control: public, max-age=31536000, immutable
```

### ETag and Last-Modified (Conditional Requests)

These headers enable cache validation without re-downloading the full response body.

`ETag: "abc123"` — A unique identifier for a specific version of the resource (typically a hash of the content).

`Last-Modified: Fri, 15 Aug 2026 10:00:00 GMT` — When the resource was last changed.

On subsequent requests, the client sends:
```
If-None-Match: "abc123"
If-Modified-Since: Fri, 15 Aug 2026 10:00:00 GMT
```

The server responds with `304 Not Modified` (no body) if unchanged, saving bandwidth.

### Vary

Tells caches to store separate responses for different request header values.

```
Vary: Accept-Encoding        // Cache separate gzip/br versions
Vary: Accept-Language        // Cache separate language versions
Vary: Authorization          // Do NOT do this — caches per auth token, defeats caching
```

---

## 7. Redis as a Cache

Redis is not just a key-value store — its rich data structures make it a versatile caching tool for different access patterns.

### Strings (Most Common)

Store serialized JSON for simple key-value caching.

```
SET user:123:profile '{"name":"Alice","email":"alice@example.com"}' EX 300
GET user:123:profile
```

Use for: individual object caching, computed values, simple counters.

### Hashes

Store structured objects with field-level access, avoiding full deserialization to read one field.

```
HSET user:123 name "Alice" email "alice@example.com" plan "pro"
HGET user:123 plan          // Read one field without fetching entire object
HGETALL user:123            // Read all fields
HINCRBY user:123 loginCount 1  // Increment a field atomically
```

Use for: objects where individual fields are frequently read or updated independently.

### Sorted Sets

Store members with a numeric score, enabling range queries by score.

```
ZADD leaderboard 4523 "user:123"
ZADD leaderboard 6721 "user:456"
ZREVRANGE leaderboard 0 9 WITHSCORES  // Top 10 with scores
ZRANK leaderboard "user:123"           // Rank of a specific user
```

Use for: leaderboards, time-series data (use timestamp as score), rate limiting (sliding window log).

### Lists

Ordered list of strings, supporting push/pop from either end.

```
LPUSH recent:user:123 "item:789"
LTRIM recent:user:123 0 49    // Keep only the 50 most recent
LRANGE recent:user:123 0 -1   // Fetch all
```

Use for: recent activity feeds, queues, circular buffers.

### Sets

Unordered collection of unique strings, with efficient set operations.

```
SADD user:123:tags "premium" "verified"
SMEMBERS user:123:tags
SISMEMBER user:123:tags "premium"   // O(1) membership check
SINTER user:123:tags user:456:tags  // Common tags
```

Use for: tags, permissions, deduplication, social graph (followers/following).

### Bitmaps

Compact boolean arrays using string commands with bit offsets.

```
SETBIT user:123:logins:2026 227 1   // Day 227 of 2026 (Aug 15), user logged in
GETBIT user:123:logins:2026 227
BITCOUNT user:123:logins:2026       // Total login days this year
```

Use for: feature flags per user, daily activity tracking, bloom filter approximations.

### HyperLogLog

Probabilistic data structure for approximate distinct count, using fixed memory (~12KB) regardless of set size.

```
PFADD pageviews:homepage "user:123" "user:456" "user:789"
PFCOUNT pageviews:homepage   // Approximate unique visitors (±0.81% error)
```

Use for: unique visitor counts, cardinality estimation for analytics.

---

## 8. Complete TypeScript Cache-Aside Pattern with Redis

```typescript
import { createClient, RedisClientType } from "redis";

// ---------------------------------------------------------------------------
// Generic cache-aside implementation
// ---------------------------------------------------------------------------

export interface CacheOptions {
  ttlSeconds: number;
  keyPrefix?: string;
}

export class RedisCache {
  private redis: RedisClientType;
  private defaultTtl: number;
  private keyPrefix: string;

  constructor(
    redis: RedisClientType,
    options: { defaultTtlSeconds: number; keyPrefix?: string }
  ) {
    this.redis = redis;
    this.defaultTtl = options.defaultTtlSeconds;
    this.keyPrefix = options.keyPrefix ?? "cache";
  }

  private buildKey(key: string): string {
    return `${this.keyPrefix}:${key}`;
  }

  async get<T>(key: string): Promise<T | null> {
    const fullKey = this.buildKey(key);
    const value = await this.redis.get(fullKey);
    if (value === null) return null;
    try {
      return JSON.parse(value) as T;
    } catch {
      // Value is a plain string, not JSON
      return value as unknown as T;
    }
  }

  async set<T>(key: string, value: T, ttlSeconds?: number): Promise<void> {
    const fullKey = this.buildKey(key);
    const ttl = ttlSeconds ?? this.defaultTtl;
    await this.redis.set(fullKey, JSON.stringify(value), { EX: ttl });
  }

  async delete(key: string): Promise<void> {
    await this.redis.del(this.buildKey(key));
  }

  async deletePattern(pattern: string): Promise<number> {
    // SCAN is safe for production — does not block like KEYS
    const keys: string[] = [];
    const fullPattern = this.buildKey(pattern);
    for await (const key of this.redis.scanIterator({ MATCH: fullPattern, COUNT: 100 })) {
      keys.push(key);
    }
    if (keys.length === 0) return 0;
    return this.redis.del(keys);
  }

  /**
   * Cache-aside pattern: check cache first, fall back to loader function on miss.
   *
   * @param key - Cache key (without prefix)
   * @param loader - Async function to load data on cache miss
   * @param ttlSeconds - Optional TTL override
   */
  async getOrLoad<T>(
    key: string,
    loader: () => Promise<T>,
    ttlSeconds?: number
  ): Promise<T> {
    // Step 1: Check cache
    const cached = await this.get<T>(key);
    if (cached !== null) {
      return cached;
    }

    // Step 2: Cache miss — load from source of truth
    const value = await loader();

    // Step 3: Store in cache (do not await — non-blocking write)
    this.set(key, value, ttlSeconds).catch((err) => {
      console.error(`[Cache] Failed to write key ${key}:`, err);
    });

    return value;
  }

  /**
   * Write-through: update both cache and database atomically from caller's perspective.
   * The cache write happens synchronously; downstream write is caller's responsibility.
   */
  async invalidateOnWrite<T>(key: string, newValue?: T): Promise<void> {
    if (newValue !== undefined) {
      // Update the cache with the new value (write-through)
      await this.set(key, newValue);
    } else {
      // Invalidate — next read will repopulate from the database
      await this.delete(key);
    }
  }
}

// ---------------------------------------------------------------------------
// Domain-specific cache service example: UserCache
// ---------------------------------------------------------------------------

interface User {
  id: string;
  name: string;
  email: string;
  plan: "free" | "pro" | "enterprise";
  avatarUrl: string | null;
}

interface UserRepository {
  findById(id: string): Promise<User | null>;
  update(id: string, data: Partial<User>): Promise<User>;
}

export class UserCacheService {
  private cache: RedisCache;
  private userRepo: UserRepository;

  // TTL constants
  private static readonly USER_TTL = 300;          // 5 minutes
  private static readonly USER_LIST_TTL = 60;       // 1 minute (more volatile)

  constructor(redis: RedisClientType, userRepo: UserRepository) {
    this.cache = new RedisCache(redis, {
      defaultTtlSeconds: UserCacheService.USER_TTL,
      keyPrefix: "cache:user",
    });
    this.userRepo = userRepo;
  }

  private userKey(userId: string): string {
    return `${userId}:profile`;
  }

  /**
   * Get a user by ID using cache-aside pattern.
   * Returns null if the user does not exist in the database.
   *
   * Note: We need to distinguish between "cache miss" (not cached) and
   * "user does not exist" (null in database). We cache the null result
   * as a sentinel to avoid repeated DB lookups for non-existent users.
   */
  async getUser(userId: string): Promise<User | null> {
    const key = this.userKey(userId);

    return this.cache.getOrLoad(
      key,
      async () => {
        const user = await this.userRepo.findById(userId);

        // Cache null results as a sentinel string to prevent repeated DB hits
        // for non-existent users. The getOrLoad wrapper returns null from the
        // cache as a miss, so we need to handle this with a wrapper type.
        // This simplified example returns null directly; in production, use
        // a sentinel value like { __notFound: true }.
        return user;
      },
      UserCacheService.USER_TTL
    );
  }

  /**
   * Update a user and invalidate the cache entry.
   * Uses write-through: updates cache with new value immediately.
   */
  async updateUser(userId: string, data: Partial<User>): Promise<User> {
    // Write to source of truth first
    const updatedUser = await this.userRepo.update(userId, data);

    // Write-through: update cache with new value immediately
    await this.cache.invalidateOnWrite(this.userKey(userId), updatedUser);

    return updatedUser;
  }

  /**
   * Delete a user and remove their cache entry.
   */
  async deleteUser(userId: string): Promise<void> {
    // ... delete from database ...
    await this.cache.delete(this.userKey(userId));
  }

  /**
   * Preload a batch of users into cache.
   * Useful for warming the cache before expected high traffic,
   * or for populating a set of users retrieved in a single DB query.
   */
  async warmCache(users: User[]): Promise<void> {
    await Promise.all(
      users.map((user) =>
        this.cache.set(this.userKey(user.id), user, UserCacheService.USER_TTL)
      )
    );
  }
}

// ---------------------------------------------------------------------------
// Stale-while-revalidate wrapper
// ---------------------------------------------------------------------------

interface SWREntry<T> {
  value: T;
  cachedAt: number;     // Unix timestamp ms
  ttlMs: number;        // Total TTL in ms
  revalidatingAt?: number; // Set when a background refresh is in flight
}

/**
 * Stale-while-revalidate cache wrapper.
 * Returns stale data immediately while refreshing in the background.
 * Prevents cache stampedes by tracking in-flight revalidations.
 */
export class StaleWhileRevalidateCache<T> {
  private cache: RedisCache;
  private revalidating = new Set<string>();

  constructor(cache: RedisCache) {
    this.cache = cache;
  }

  async get(
    key: string,
    loader: () => Promise<T>,
    ttlMs: number,
    staleWindowMs: number
  ): Promise<T> {
    const entry = await this.cache.get<SWREntry<T>>(key);

    if (entry !== null) {
      const age = Date.now() - entry.cachedAt;
      const isStale = age > entry.ttlMs;
      const isWithinStaleWindow = age <= entry.ttlMs + staleWindowMs;

      if (!isStale) {
        // Fresh — return immediately
        return entry.value;
      }

      if (isStale && isWithinStaleWindow && !this.revalidating.has(key)) {
        // Stale but within grace window — return stale value, revalidate in background
        this.revalidating.add(key);
        this.revalidateInBackground(key, loader, ttlMs).finally(() => {
          this.revalidating.delete(key);
        });
        return entry.value;
      }

      if (isStale && isWithinStaleWindow && this.revalidating.has(key)) {
        // Revalidation in flight — return stale value while waiting
        return entry.value;
      }
    }

    // Full miss or expired beyond stale window — load synchronously
    const value = await loader();
    const newEntry: SWREntry<T> = {
      value,
      cachedAt: Date.now(),
      ttlMs,
    };
    // Store with extra time to cover the stale window
    await this.cache.set(key, newEntry, Math.ceil((ttlMs + staleWindowMs) / 1000));
    return value;
  }

  private async revalidateInBackground(
    key: string,
    loader: () => Promise<T>,
    ttlMs: number
  ): Promise<void> {
    try {
      const value = await loader();
      const newEntry: SWREntry<T> = {
        value,
        cachedAt: Date.now(),
        ttlMs,
      };
      await this.cache.set(key, newEntry, Math.ceil(ttlMs / 1000));
    } catch (error) {
      console.error(`[SWR] Background revalidation failed for ${key}:`, error);
      // Do not rethrow — the stale value continues to be served
    }
  }
}

// ---------------------------------------------------------------------------
// Usage example
// ---------------------------------------------------------------------------

/*
import { createClient } from "redis";
import { RedisCache, UserCacheService } from "./cache";

const redis = createClient({ url: process.env.REDIS_URL });
await redis.connect();

const cache = new RedisCache(redis, { defaultTtlSeconds: 300, keyPrefix: "myapp" });

// Simple cache-aside
const product = await cache.getOrLoad(
  `product:${productId}`,
  () => db.products.findById(productId),
  600
);

// Domain service
const userCache = new UserCacheService(redis, userRepository);
const user = await userCache.getUser("user-123");
const updated = await userCache.updateUser("user-123", { plan: "pro" });

// Invalidate all user cache entries (e.g., on schema migration)
await cache.deletePattern("*:profile");
*/
```

---

## Further Reading

- [Redis documentation: Data types](https://redis.io/docs/data-types/)
- [Cloudflare: Cache-Control directives explained](https://developers.cloudflare.com/cache/concepts/cache-control/)
- [MDN: HTTP caching](https://developer.mozilla.org/en-US/docs/Web/HTTP/Caching)
- [XFetch: Optimal Probabilistic Cache Stampede Prevention](https://cseweb.ucsd.edu/~avattani/papers/cache_stampede.pdf)
- [Redis Best Practices: Caching](https://redis.com/redis-best-practices/indexing-patterns/using-redis-as-a-cache/)
