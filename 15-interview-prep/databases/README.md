# Database Interview Questions

## PostgreSQL

**Q: What is the difference between an index scan and a sequential scan?**
Sequential scan: reads every row in the table from disk. O(n). For small tables or queries returning > ~5-10% of rows, this is often faster than an index scan (less random I/O). Index scan: uses the B-tree to find row locations, then fetches those specific rows. Efficient for high-selectivity queries (few rows match). Index-only scan: returns data directly from the index without fetching heap rows — only possible if all required columns are in the index.

**Q: Why does adding an index sometimes slow down queries?**
Write operations (INSERT, UPDATE, DELETE) must maintain every index on the table — more indexes = more overhead per write. On write-heavy tables with many indexes, the index maintenance can be slower than the read benefit. Also: indexes consume storage and RAM (buffer cache). If a table has 15 indexes, a single row update touches 15 index structures.

**Q: What is the difference between a transaction and a savepoint?**
A transaction is an all-or-nothing unit: either everything commits or everything rolls back. A savepoint is a named point within a transaction. You can roll back to a savepoint without rolling back the entire transaction. Useful for nested operations where you want to attempt something, roll it back on failure, and continue.
```sql
BEGIN;
INSERT INTO audit_log ...;  -- always keep this
SAVEPOINT before_risky;
UPDATE ... ;                -- attempt this
-- if something goes wrong:
ROLLBACK TO before_risky;  -- undo the update, keep the audit log
COMMIT;
```

**Q: What is vacuum in PostgreSQL and why is it needed?**
PostgreSQL's MVCC creates new row versions on every UPDATE and marks old versions as dead. VACUUM reclaims storage occupied by dead row versions and updates visibility maps and statistics. Without vacuum, tables grow without bound (table bloat) and queries slow down. Autovacuum runs automatically; you rarely need to run it manually. VACUUM FULL rewrites the table to reclaim space — requires exclusive lock, use during maintenance windows only.

---

## MongoDB

**Q: How do you model a one-to-many relationship in MongoDB?**
Two approaches:
1. Embed: store the "many" as an array inside the parent document. Best when: the "many" are always accessed with the parent, the array is bounded in size (< a few hundred items). Example: user with an embedded array of addresses.
2. Reference: store parent's `_id` in the child document. Best when: the "many" is unbounded (blog posts have unlimited comments), the child is queried independently, the child is referenced by multiple parents.

**Q: Why does `findOne()` with no index on a 10M document collection take 3 seconds?**
Without an index on the query field, MongoDB performs a collection scan — reading every document from disk until it finds a match. 10M documents × ~300 bytes average = ~3GB. A collection scan on 3GB of data → 3 seconds. Creating an index on the query field reduces this to a sub-millisecond B-tree lookup.

---

## Redis

**Q: How would you implement a leaderboard with Redis?**
Use a sorted set. Member = user ID, score = points. Score is a float, so it handles any numeric value.
```
ZADD leaderboard 1500 "user:42"
ZADD leaderboard 2300 "user:99"
ZINCRBY leaderboard 100 "user:42"          # add 100 points
ZREVRANGE leaderboard 0 9 WITHSCORES       # top 10
ZREVRANK leaderboard "user:42"             # rank of a specific user
```
Sorted sets maintain O(log n) insert and O(log n) rank queries — perfect for leaderboards.

**Q: How would you use Redis for distributed session storage?**
Store sessions as hashes keyed by session ID. The session ID is stored in an httpOnly cookie on the client.
```
HSET session:abc123 userId user:42 role admin
EXPIRE session:abc123 86400   # 24-hour TTL
```
On each request: read session ID from cookie → HGETALL session:abc123 → attach to request object. On logout: DEL session:abc123. This works across multiple API server instances because all instances share the same Redis.

---

## Distributed Databases

**Q: Explain the difference between vertical and horizontal scaling for databases.**
Vertical scaling (scale up): add more CPU/RAM/SSD to a single machine. Simple, no application changes. Limited by the largest available machine. Cost-per-performance worsens as you scale up. Horizontal scaling (scale out): add more machines. For databases: read replicas (reads scale, writes don't), sharding (partition data across nodes, writes and reads scale). Horizontal scaling is complex: joins across shards are expensive, transactions across shards require coordination.

**Q: What is a read replica and what are its limitations?**
A read replica is a continuously synchronized copy of the primary database, used to serve read queries. Limitations: replication lag — the replica may be milliseconds to seconds behind the primary. This means reads may return stale data. You cannot read your own writes reliably unless you route writes and immediate follow-up reads to the primary. You also cannot write to a replica.

**Q: What is database sharding?**
Sharding horizontally partitions data across multiple database instances. Each shard holds a subset of the data. Common strategies: range sharding (user IDs 1-1M on shard 1, 1M-2M on shard 2), hash sharding (hash(user_id) % num_shards → uniform distribution, no hot spots), directory sharding (lookup table maps keys to shards). Challenges: cross-shard queries require scatter-gather (query all shards, merge results), cross-shard transactions require 2PC or sagas, resharding is operationally painful.
