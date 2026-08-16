# System Design Interview Prep

## The Framework

Every system design interview follows the same structure. Stick to this and you'll never run out of things to say.

1. **Clarify requirements** (5 min) — functional, non-functional, constraints
2. **Estimate scale** (5 min) — QPS, storage, bandwidth
3. **High-level design** (10 min) — major components, data flow
4. **Deep dive** (15 min) — the parts the interviewer cares about
5. **Trade-offs** (5 min) — what you'd change at 10× scale, what you deprioritized

---

## Requirements Clarification Checklist

Always ask these before drawing anything:

- Who are the users? How many DAU/MAU?
- What are the core features? (top 3)
- What's the read/write ratio?
- Is consistency or availability more important?
- What's the latency requirement?
- Any geo-distribution requirements?
- Mobile/web/both?

---

## Estimation Framework

```
QPS = DAU × (actions per user per day) / 86400

Storage = QPS × avg object size × seconds in period
```

Common reference points:
- 1M DAU × 10 actions = 116 QPS (low traffic)
- 100M DAU × 10 actions = 11,600 QPS (medium)
- 1B DAU × 40 actions = 462,000 QPS (WhatsApp scale)

Memory:
- 1M users × 1 KB each = 1 GB
- Redis can hold ~10GB in a single instance comfortably

---

## Component Toolbox

Know when to reach for each:

| Need | Use |
|------|-----|
| Scale read traffic | Read replicas, CDN, caching |
| Scale write traffic | Sharding, write-ahead log, CQRS |
| Async processing | Message queue (SQS, Kafka, BullMQ) |
| Real-time push | WebSockets, SSE |
| Full-text search | Elasticsearch |
| Session storage | Redis |
| File storage | S3 / object storage |
| Time-series data | Cassandra, InfluxDB |
| Inter-service communication | gRPC (sync), Kafka (async) |

---

## Common Design Questions + Key Insights

### URL Shortener
- Core: base62 encode an auto-incremented ID → 7 chars = 62^7 = 3.5T URLs
- Read: heavy (redirects). Cache the mapping in Redis (TTL = forever for popular URLs)
- Analytics: async — push click event to Kafka, aggregate in background

### News Feed / Timeline
- Fan-out on write (push model): write to all followers' feeds on post. Fast reads, slow writes. Good for < 1000 followers.
- Fan-out on read (pull model): merge followed users' posts at read time. Slow reads, fast writes. Good for celebrities.
- Hybrid: push for regular users, pull for celebrities.

### Search Autocomplete
- Trie in memory for prefix lookups. Top-N results at each node.
- At scale: pre-compute top suggestions per prefix, store in Redis, update periodically via batch job.

### Video Streaming (YouTube)
- Upload → transcode to multiple bitrates (async, multiple workers) → store on CDN
- Adaptive bitrate streaming (HLS/DASH): client picks resolution based on network speed
- Metadata in PostgreSQL, video bytes in object storage + CDN

### Distributed Cache
- Consistent hashing: hash the key, assign to nearest node on the ring. Adding/removing nodes only remaps ~1/N of keys.
- Eviction: LRU for most caches. LFU if access patterns are highly skewed.
- Cache aside vs write-through vs write-back: cache aside is safest (explicit control), write-through keeps cache warm, write-back risks data loss.

---

## 10 Most Common Questions

**Q: How do you handle database scaling?**
Start with read replicas for read-heavy workloads. Add connection pooling. For write scaling: sharding (pick a good shard key — avoid hot spots). For global: consider multi-region active-active with eventual consistency.

**Q: SQL vs NoSQL — when do you choose each?**
SQL when you need ACID transactions, complex joins, and your schema is stable. NoSQL when you need horizontal scaling, flexible schema, or your access patterns map well to documents/key-value/wide-column. Don't choose NoSQL to "scale" — PostgreSQL handles millions of QPS with proper indexing and replication.

**Q: How do you handle cache invalidation?**
TTL for data that can tolerate staleness. Event-driven invalidation (write to DB, publish event, subscriber deletes cache key) for consistency. Write-through (update DB + cache atomically) for strong consistency. Cache stampede: use mutex (SETNX) or probabilistic early expiration.

**Q: How do you prevent hot spots in a distributed system?**
Choose shard keys that distribute evenly (hash-based, not range-based on skewed values). Add a random suffix to hot keys (write to key:1, key:2, ... key:N, read and aggregate). For queues, partition by user and assign partitions across workers.

**Q: How do you design for high availability?**
N+1 redundancy everywhere. Health checks + automatic failover. Circuit breaker pattern for downstream services. Graceful degradation (serve stale data from cache if DB is down). Chaos engineering to find single points of failure.

**Q: What is eventual consistency and when is it acceptable?**
In a distributed system, nodes may temporarily disagree on state. Eventually (after replication lag), they converge. Acceptable for: social feeds, likes/views counts, analytics. Not acceptable for: financial transactions, inventory (can't oversell), authentication.

**Q: How would you implement a distributed lock?**
Redis `SET key value NX EX ttl` — atomic set-if-not-exists with expiry. Redlock algorithm for multi-node Redis. Always include: auto-expiry (client crash safety), unique value (only owner can release), compare-and-delete (check value before deleting).

**Q: How do you handle thundering herd on cache miss?**
All requests miss simultaneously and flood the DB. Fix: single-flight (first request fetches, others wait), or probabilistic early expiration (refresh before TTL expires), or background refresh (serve stale while refreshing).

**Q: Describe CAP theorem and give a real example.**
CAP: in a network partition, choose consistency (C) or availability (A). Consistency: all nodes return the same value (ZooKeeper, HBase). Availability: all nodes respond (Cassandra, DynamoDB). Modern nuance: PACELC — partition tolerance is mandatory; tradeoff is E(lsewhere) latency vs consistency.

**Q: How do you design the data model for a social graph?**
Graph DB (Neo4j) for complex traversal queries (friends-of-friends). At scale (Facebook): in-memory graph (TAO) for low-latency lookups, Cassandra for relationship storage. Bidirectional edges: store both directions explicitly for O(1) lookup. "Soft" vs "hard" edges for different relationship types.

---

## Scoring Rubric (What Interviewers Look For)

| Area | Strong signal | Weak signal |
|------|-------------|------------|
| Problem clarity | Asks right questions, identifies constraints | Dives in without clarifying |
| Scale thinking | Back-of-envelope, identifies bottlenecks | Vague or ignores scale |
| Component choice | Explains trade-offs | Names tools without justification |
| Depth | Can go deep on one area when pushed | Surface-level on everything |
| Trade-offs | Acknowledges downsides of their design | Claims their design has no weaknesses |
| Communication | Thinks out loud, iterates on feedback | Silent, defensive |
