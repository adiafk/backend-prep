# Redis

Redis is an in-memory data structure store. Single-threaded, sub-millisecond latency. Used for caching, sessions, rate limiting, pub/sub, queues, and distributed locks.

---

## Data Structures

### Strings
Simplest type. Counters, simple cache values, flags.

```bash
SET user:1:name "Alice"
GET user:1:name

# Set with TTL (expires in 3600 seconds)
SET session:abc123 "user:1" EX 3600

# Atomic increment
INCR page:views:home
INCRBY page:views:home 5
```

```typescript
import { createClient } from 'redis'
const redis = createClient({ url: process.env.REDIS_URL })
await redis.set('key', 'value', { EX: 3600 })
const val = await redis.get('key')
```

### Hashes
Map of field:value pairs. Good for user sessions and object caching (avoid serializing/deserializing the whole object when you need one field).

```bash
HSET user:1 name "Alice" email "alice@example.com" role "admin"
HGET user:1 name
HGETALL user:1
HINCRBY user:1 loginCount 1
```

### Sets
Unordered collection of unique strings. Tags, memberships, unique visitors.

```bash
SADD post:42:tags "nodejs" "backend" "api"
SMEMBERS post:42:tags
SISMEMBER post:42:tags "nodejs"  # returns 1 or 0

# Set operations
SUNION post:42:tags post:99:tags  # union
SINTER user:1:friends user:2:friends  # intersection (mutual friends)
```

### Sorted Sets
Like sets but each member has a score. Leaderboards, rate limiting, time-sorted data.

```bash
ZADD leaderboard 1500 "alice" 2300 "bob" 900 "carol"
ZRANK leaderboard "alice"          # rank (0-indexed, ascending)
ZREVRANK leaderboard "alice"       # rank (descending — higher score = lower rank number)
ZRANGE leaderboard 0 9 WITHSCORES  # top 10
ZINCRBY leaderboard 100 "alice"    # increment score
```

### Lists
Ordered sequence. FIFO queue, activity feed, recent items.

```bash
LPUSH queue:emails "job1" "job2"  # push to left (head)
RPOP queue:emails                  # pop from right (tail) — FIFO
LRANGE feed:user:1 0 19           # get first 20 items
LLEN queue:emails
```

---

## TTL (Time To Live)

```bash
EXPIRE key 300         # set TTL on existing key (seconds)
EXPIREAT key 1735689600  # expire at Unix timestamp
TTL key                # check remaining TTL (-1 = no expiry, -2 = key doesn't exist)
PERSIST key            # remove TTL (make permanent)
```

---

## Caching Patterns

### Cache-aside (most common)

```typescript
async function getUser(id: string) {
  const cacheKey = `user:${id}`

  // 1. Check cache
  const cached = await redis.get(cacheKey)
  if (cached) return JSON.parse(cached)

  // 2. Miss — fetch from DB
  const user = await db.query('SELECT * FROM users WHERE id = $1', [id])
  if (!user) return null

  // 3. Store in cache with TTL
  await redis.set(cacheKey, JSON.stringify(user), { EX: 300 })
  return user
}

// On update: invalidate cache
async function updateUser(id: string, data: Partial<User>) {
  await db.query('UPDATE users SET ...', [...])
  await redis.del(`user:${id}`)  // force next read to re-fetch
}
```

---

## Distributed Lock

Prevents two processes from doing the same work simultaneously (e.g. processing the same job).

```typescript
const lockKey = `lock:job:${jobId}`
const lockToken = crypto.randomUUID()  // unique per lock attempt

// Acquire: SET if Not eXists with EXpiry
const acquired = await redis.set(lockKey, lockToken, {
  NX: true,  // only set if key doesn't exist
  EX: 30,    // auto-release after 30s (prevents deadlock if process crashes)
})

if (!acquired) {
  throw new Error('Could not acquire lock — another process is running')
}

try {
  await doWork()
} finally {
  // Release only if we hold the lock (compare token before deleting)
  const script = `
    if redis.call("get", KEYS[1]) == ARGV[1] then
      return redis.call("del", KEYS[1])
    else
      return 0
    end
  `
  await redis.eval(script, { keys: [lockKey], arguments: [lockToken] })
}
```

---

## Rate Limiting with Sliding Window

```typescript
async function isRateLimited(userId: string, limit = 100, windowSecs = 60): Promise<boolean> {
  const key = `ratelimit:${userId}`
  const now = Date.now()
  const windowStart = now - windowSecs * 1000

  const pipeline = redis.multi()
  // Remove entries outside the window
  pipeline.zRemRangeByScore(key, '-inf', windowStart)
  // Count remaining entries in window
  pipeline.zCard(key)
  // Add current request
  pipeline.zAdd(key, { score: now, value: `${now}` })
  // Set expiry to clean up idle keys
  pipeline.expire(key, windowSecs)

  const results = await pipeline.exec()
  const count = results[1] as number
  return count >= limit
}
```

---

## Pub/Sub

```typescript
// Publisher
const publisher = createClient()
await publisher.publish('notifications:user:42', JSON.stringify({
  type: 'order_shipped',
  orderId: 'order_123'
}))

// Subscriber (separate connection — a subscribed client can't do other commands)
const subscriber = createClient()
await subscriber.subscribe('notifications:user:42', (message) => {
  const event = JSON.parse(message)
  console.log('Received:', event)
})
```

Use pub/sub for: real-time notifications, broadcasting events between server instances. For reliable delivery (guaranteed processing), use a queue (BullMQ/Redis Streams) instead — pub/sub drops messages if no subscriber is listening.

---

## Session Storage

```typescript
// express-session with Redis store
import session from 'express-session'
import { RedisStore } from 'connect-redis'

app.use(session({
  store: new RedisStore({ client: redis }),
  secret: process.env.SESSION_SECRET!,
  resave: false,
  saveUninitialized: false,
  cookie: {
    secure: process.env.NODE_ENV === 'production',
    httpOnly: true,
    maxAge: 1000 * 60 * 60 * 24  // 24 hours
  }
}))
```

---

## Interview Questions

**Q: Why is Redis single-threaded but still fast?**
Redis is I/O-bound, not CPU-bound. It uses an event loop (like Node.js) to multiplex thousands of concurrent connections on a single thread. There's no lock contention, no context switching overhead. Memory operations are nanoseconds; network I/O is the bottleneck.

**Q: What happens when Redis runs out of memory?**
Depends on `maxmemory-policy`: `noeviction` (reject writes), `allkeys-lru` (evict least recently used), `volatile-lru` (LRU among keys with TTL), `allkeys-lfu`, `volatile-ttl`. For a cache, use `allkeys-lru`. For a queue, use `noeviction` (you can't lose jobs).

**Q: Redis pub/sub vs Redis Streams vs BullMQ — when each?**
Pub/sub: fire-and-forget broadcast, no persistence, subscriber must be online. Streams: persistent ordered log, consumer groups, replay from any point, acknowledgement. BullMQ: job queue with priorities, delayed jobs, repeatable jobs, UI dashboard. Use Streams or BullMQ when delivery guarantees matter.
