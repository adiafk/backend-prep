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

## Redis Streams

Streams are a persistent, append-only log. Unlike pub/sub, messages survive disconnections and can be replayed. Consumer groups allow parallel processing with acknowledgement.

```bash
# Producer: append message to stream
XADD orders * order_id "ord_123" user_id "42" total "99.99"
# Returns a message ID like "1735689600000-0"

# Consumer: read new messages
XREAD COUNT 10 STREAMS orders 0  # read from beginning
XREAD COUNT 10 BLOCK 0 STREAMS orders $  # block until new message

# Consumer groups: multiple consumers share the load
XGROUP CREATE orders order-processors $ MKSTREAM
XREADGROUP GROUP order-processors worker-1 COUNT 5 STREAMS orders >
# ">" means: give me messages not yet delivered to any consumer in the group

# Acknowledge processing (remove from PEL - pending entries list)
XACK orders order-processors 1735689600000-0
```

```typescript
// TypeScript: consume from a stream with a consumer group
async function processOrders(): Promise<void> {
  const GROUP = 'order-processors'
  const CONSUMER = `worker-${process.pid}`

  while (true) {
    const messages = await redis.xReadGroup(
      GROUP,
      CONSUMER,
      [{ key: 'orders', id: '>' }],  // ">" = undelivered messages
      { COUNT: 10, BLOCK: 5000 }     // wait up to 5s for new messages
    )

    if (!messages) continue

    for (const { name, messages: msgs } of messages) {
      for (const { id, message } of msgs) {
        try {
          await processOrder(message)
          await redis.xAck(name, GROUP, id)  // ACK after successful processing
        } catch (err) {
          // Don't ACK — message stays in PEL for retry
          console.error(`Failed to process ${id}:`, err)
        }
      }
    }
  }
}
```

**Streams vs Pub/Sub**: pub/sub = fire-and-forget, no persistence. Streams = persistent log with consumer groups, ACK, replay. Use Streams when delivery guarantees matter. Use BullMQ (built on Streams) for job queue features: priorities, delays, retries, backoff.

---

## Keyspace Notifications

Subscribe to server-side events (key expired, key deleted, key set). Requires `notify-keyspace-events` config to be set.

```bash
# Enable in redis.conf or via command
CONFIG SET notify-keyspace-events "KEA"
# K = keyspace events, E = keyevent events, A = all commands
# Ex = expiry events only
```

```typescript
// Subscribe to expiry events (e.g. to clean up associated data when a session expires)
const sub = createClient()
await sub.pSubscribe('__keyevent@0__:expired', (key) => {
  if (key.startsWith('session:')) {
    const sessionId = key.replace('session:', '')
    cleanupSession(sessionId)
  }
})
```

---

## MULTI/EXEC Transactions

Groups commands into an atomic block. All succeed or all fail. Not a true rollback — if a command fails at runtime, others still execute.

```typescript
// MULTI/EXEC: commands queue up and execute atomically
const pipeline = redis.multi()
pipeline.set('key1', 'value1')
pipeline.incr('counter')
pipeline.expire('key1', 3600)
const results = await pipeline.exec()
// [null, 1, 1] — null = OK for SET, 1 from INCR, 1 from EXPIRE

// WATCH for optimistic locking (CAS - Compare and Swap)
// WATCH marks a key. If it changes before EXEC, the transaction aborts.
async function transferCredits(fromId: string, toId: string, amount: number): Promise<void> {
  let retries = 3
  while (retries-- > 0) {
    await redis.watch(`credits:${fromId}`)
    const balance = Number(await redis.get(`credits:${fromId}`))
    if (balance < amount) throw new Error('Insufficient credits')

    const result = await redis
      .multi()
      .decrBy(`credits:${fromId}`, amount)
      .incrBy(`credits:${toId}`, amount)
      .exec()

    if (result !== null) return  // exec succeeded
    // result === null means WATCH detected a change — retry
  }
  throw new Error('Transaction aborted after retries')
}
```

**MULTI/EXEC limitation**: if two transactions WATCH the same key, one will succeed and one will abort (must retry). For high-contention scenarios, use Lua scripts instead — they run atomically without WATCH.

---

## Lua Scripting

Lua scripts execute atomically inside Redis — no other commands run between script steps. This is more powerful than MULTI/EXEC for complex atomic operations.

```typescript
// Rate limiting with atomic Lua (compare-and-increment in one shot)
const rateLimitScript = `
  local key = KEYS[1]
  local limit = tonumber(ARGV[1])
  local window = tonumber(ARGV[2])

  local current = tonumber(redis.call('GET', key) or '0')
  if current >= limit then
    return 0  -- rate limited
  end

  redis.call('INCR', key)
  if current == 0 then
    redis.call('EXPIRE', key, window)  -- set expiry only on first request
  end
  return 1  -- allowed
`

async function checkRateLimit(userId: string): Promise<boolean> {
  const allowed = await redis.eval(rateLimitScript, {
    keys: [`ratelimit:${userId}`],
    arguments: ['100', '60']  // 100 req/60 sec
  })
  return allowed === 1
}
```

Use Lua when: you need multiple reads + conditional writes atomically, MULTI/EXEC with WATCH would cause too many retries, or the logic is too complex for a pipeline.

---

## Persistence

Redis offers two persistence mechanisms, often used together.

### RDB (Redis Database — Snapshots)

Periodically writes a point-in-time snapshot of the entire dataset to disk.

```
save 900 1      # save after 900 seconds if at least 1 key changed
save 300 10     # save after 300 seconds if at least 10 keys changed
save 60 10000   # save after 60 seconds if at least 10000 keys changed
```

- **Fast restart** — loading RDB is much faster than replaying AOF
- **Data loss risk** — if Redis crashes, you lose all writes since the last snapshot
- **Low disk I/O** — only one file written periodically

### AOF (Append-Only File)

Logs every write command. On restart, Redis replays the log.

```
appendonly yes
appendfsync everysec   # fsync every second (default)
# appendfsync always   # fsync on every write (safe but slow)
# appendfsync no       # let OS decide (fastest, least durable)
```

| fsync setting | Data loss risk | Performance |
|---|---|---|
| `always` | None (up to last command) | Slow |
| `everysec` | Up to 1 second of writes | Good |
| `no` | OS-dependent (usually seconds) | Fast |

AOF files grow large over time. Redis compacts them via **AOF rewrite** (replays current state, not full history).

### Hybrid Mode (Recommended)

```
aof-use-rdb-preamble yes
```

AOF file starts with an RDB snapshot, followed by incremental AOF records. Fast restart (RDB) + minimal data loss (AOF).

**For caches**: persistence is optional — you can reconstruct from the source database.
**For queues/sessions**: use AOF with `everysec` at minimum. Losing a job queue to a crash is a bad incident.

---

## Redis Sentinel (High Availability)

Sentinel monitors Redis instances and performs automatic failover.

```
sentinel monitor mymaster 127.0.0.1 6379 2
# "2" = quorum: minimum votes needed to declare master as down
```

Sentinel provides:
- **Monitoring** — heartbeats to detect if primary is down
- **Notification** — alerts on state changes
- **Automatic failover** — promotes a replica to primary
- **Configuration provider** — clients ask Sentinel for the current primary address

**Quorum prevents split-brain**: if the network partitions, the majority side's Sentinels (≥ quorum) declare the primary down and elect a new one. The minority side does not elect a new primary.

```typescript
import { createClient } from 'redis'

const client = createClient({
  sentinels: [
    { host: 'sentinel-1', port: 26379 },
    { host: 'sentinel-2', port: 26379 },
    { host: 'sentinel-3', port: 26379 },
  ],
  name: 'mymaster',  // master set name from sentinel config
})
```

---

## Redis Cluster (Horizontal Scaling)

Redis Cluster shards data across multiple primary nodes. Each primary can have replicas for HA.

### Hash Slots

The keyspace is divided into **16,384 slots** (0–16383). Each key maps to a slot via `CRC16(key) mod 16384`. Each shard owns a range of slots.

```
Shard 1: slots 0–5460    (keys A–G roughly)
Shard 2: slots 5461–10922
Shard 3: slots 10923–16383
```

When a client sends a command to the wrong shard, the shard replies with a `MOVED` redirect:
```
MOVED 3999 127.0.0.1:6381
# key hashes to slot 3999, which is on 127.0.0.1:6381
```

Smart clients (like `ioredis`) cache the slot map and route commands to the correct shard directly.

### Hash Tags

Force related keys to the same slot (needed for multi-key commands and Lua scripts):
```
# These all hash the {} contents only, so they land on the same slot
{user:42}:session
{user:42}:cart
{user:42}:preferences
```

```typescript
import { createCluster } from 'redis'

const cluster = createCluster({
  rootNodes: [
    { host: 'redis-node-1', port: 6379 },
    { host: 'redis-node-2', port: 6379 },
    { host: 'redis-node-3', port: 6379 },
  ],
})

await cluster.connect()
await cluster.set('{user:42}:session', 'data', { EX: 3600 })
```

### Adding Nodes Requires Resharding

Adding a new shard requires redistributing hash slots. Redis Cluster handles this online (no downtime) but it's operationally significant — plan for it with `redis-cli --cluster reshard`.

---

## Redlock (Distributed Lock Across Nodes)

Redlock acquires locks on N/2+1 Redis nodes. If the majority succeeds, the lock is held. Designed for distributed environments where a single Redis instance is a single point of failure.

```typescript
import Redlock from 'redlock'

const redlock = new Redlock([redis1, redis2, redis3])

const lock = await redlock.acquire(['lock:resource:42'], 5000)  // 5s TTL
try {
  await doWork()
} finally {
  await lock.release()
}
```

**Controversy**: Martin Kleppmann argued Redlock is unsafe for correctness-critical use cases — a slow process can hold an expired lock while another process acquires the same lock after TTL expiry, creating a window where both think they hold it. For true distributed mutual exclusion with correctness guarantees, use a system with linearizability (etcd, ZooKeeper). For "best-effort" mutual exclusion (reducing duplicate work, not preventing it entirely), Redlock is practical.

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
Redis is I/O-bound, not CPU-bound. It uses an event loop (like Node.js) to multiplex thousands of concurrent connections on a single thread. There's no lock contention, no context switching overhead. Memory operations are nanoseconds; network I/O is the bottleneck. (Redis 6+ added threaded I/O for reading/writing network data, but command processing remains single-threaded.)

**Q: What happens when Redis runs out of memory?**
Depends on `maxmemory-policy`: `noeviction` (reject writes), `allkeys-lru` (evict least recently used), `volatile-lru` (LRU among keys with TTL), `allkeys-lfu`, `volatile-ttl`. For a cache, use `allkeys-lru`. For a queue, use `noeviction` (you can't lose jobs).

**Q: Redis pub/sub vs Redis Streams vs BullMQ — when each?**
Pub/sub: fire-and-forget broadcast, no persistence, subscriber must be online. Streams: persistent ordered log, consumer groups, replay from any point, acknowledgement. BullMQ: job queue with priorities, delayed jobs, repeatable jobs, UI dashboard — built on Streams. Use Streams or BullMQ when delivery guarantees matter.

**Q: What is the difference between RDB and AOF persistence?**
RDB saves periodic snapshots — fast to load, but you lose data since the last snapshot (up to minutes). AOF logs every write command — you lose at most 1 second of data with `everysec`, but loading a large AOF on restart is slow. Hybrid mode (default in modern Redis) combines them: AOF file starts with an RDB snapshot, then appends incremental changes. Use hybrid for the best of both.

**Q: How does Redis Cluster route commands to the correct shard?**
Each of the 16,384 hash slots is assigned to a shard. When a client issues a command, it computes `CRC16(key) mod 16384` to determine the slot, then routes to the shard that owns that slot. If a command lands on the wrong shard, the shard returns a `MOVED` redirect with the correct address. Smart clients cache the slot→shard mapping to avoid redirects on the hot path.

**Q: What is MULTI/EXEC and how does WATCH enable optimistic locking?**
`MULTI` begins a transaction; commands after it are queued (not executed). `EXEC` runs all queued commands atomically. `WATCH key` marks a key for monitoring — if the key changes between WATCH and EXEC, EXEC returns nil (aborted) and you must retry. This is optimistic locking: you don't lock anything, you just detect conflict and retry. For atomic reads + conditional writes without retries, use a Lua script instead.

---

## Related

- [Caching](../../02-backend/caching/README.md) — cache patterns, TTL strategy, stampede prevention
- [Rate Limiting](../../02-backend/rate-limiting/README.md) — sliding window, token bucket in Redis
- [Queues](../../02-backend/queues/README.md) — BullMQ, job processing patterns
- [Sessions](../../02-backend/sessions/README.md) — session storage and cookie security
- [Transactions](../transactions/README.md) — ACID and distributed transactions
