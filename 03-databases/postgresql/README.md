# PostgreSQL

## Why PostgreSQL

PostgreSQL is the production-grade relational database for most backend systems. Key strengths:
- Full ACID compliance with multiple isolation levels
- JSONB — queryable, indexable JSON stored as binary
- Extensibility — pgvector, PostGIS, full-text search built in
- Window functions, CTEs, lateral joins — SQL done right
- Logical replication, streaming replication, partitioning

---

## Data Types Worth Knowing

| Type | Use When |
|------|----------|
| `text` | Strings without length limit |
| `varchar(n)` | When you need a hard limit |
| `uuid` | Primary keys (gen_random_uuid()) |
| `timestamptz` | Always store with timezone |
| `jsonb` | Semi-structured data, indexable |
| `integer` / `bigint` | Counts, IDs |
| `numeric(p,s)` | Money — never use float for money |
| `boolean` | Flags |
| `text[]` | Array of strings |

---

## MVCC Internals

PostgreSQL uses **Multi-Version Concurrency Control** (MVCC) to allow readers and writers to proceed without blocking each other.

### How It Works

Every row in a heap has two hidden system columns:
- `xmin` — transaction ID that inserted this row version
- `xmax` — transaction ID that deleted (or updated) this row version (0 = still live)

When you UPDATE a row, PostgreSQL does NOT modify the old row. It:
1. Sets `xmax` on the old version (marks it dead)
2. Inserts a new row version with `xmin = current_txid`

```sql
-- See the hidden columns
SELECT xmin, xmax, id, name FROM users WHERE id = 1;
--  xmin  | xmax | id | name
-- -------+------+----+------
--  10042 |    0 |  1 | Alice   <- live row, xmax=0 means not yet deleted
```

### Snapshot Isolation

When a transaction starts, PostgreSQL captures a **snapshot**: the set of transaction IDs that are currently in-progress. A row is visible to this transaction if:
- `xmin` is committed AND `xmin < snapshot start` (or `xmin` is the current transaction)
- `xmax` is 0, OR `xmax` is in-progress (not yet committed), OR `xmax > snapshot start`

This means readers see a consistent point-in-time view of the database without acquiring any locks. Writers create new versions without disturbing readers.

### What VACUUM Does

Dead row versions (old `xmax`-marked versions) accumulate on heap pages. `VACUUM` reclaims them:
- Scans heap pages for dead tuples (row versions where `xmax` is a committed transaction)
- Marks those pages as free space (does not shrink the file — use `VACUUM FULL` for that)
- Removes dead index entries pointing to dead tuples
- Updates the **visibility map** (pages where all rows are visible to all transactions — enables index-only scans)
- Advances `pg_database.datfrozenxid` to prevent transaction ID wraparound

```sql
-- Manual vacuum on a specific table
VACUUM (VERBOSE, ANALYZE) orders;

-- Check autovacuum settings per table
SELECT schemaname, tablename, n_dead_tup, n_live_tup, last_autovacuum
FROM pg_stat_user_tables
WHERE n_dead_tup > 10000
ORDER BY n_dead_tup DESC;
```

**Transaction ID Wraparound**: PostgreSQL uses 32-bit transaction IDs. After ~2 billion transactions, IDs wrap around. VACUUM prevents this by freezing old row versions (replacing xmin with a special frozen marker). If wraparound approaches, PostgreSQL will refuse writes. Monitor `pg_database.datfrozenxid` in production.

---

## Indexes

### B-tree (default)
Best for: equality, range queries, ORDER BY.
```sql
CREATE INDEX idx_users_email ON users (email);
CREATE INDEX idx_orders_created ON orders (created_at DESC);
```

### GIN (Generalized Inverted Index)
Best for: JSONB keys, full-text search, array containment.
```sql
CREATE INDEX idx_meta ON products USING GIN (metadata);
-- enables: WHERE metadata @> '{"category": "electronics"}'
CREATE INDEX idx_fts ON articles USING GIN (to_tsvector('english', body));
```

### Partial index
Index only rows matching a condition — smaller, faster.
```sql
CREATE INDEX idx_active_users ON users (email) WHERE deleted_at IS NULL;
```

### Composite index
Column order matters — leftmost prefix rule.
```sql
CREATE INDEX idx_user_status ON orders (user_id, status);
-- works for: WHERE user_id = $1
-- works for: WHERE user_id = $1 AND status = $2
-- does NOT help: WHERE status = $2 alone
```

See [Indexing](../indexing/README.md) for B-tree internals, BRIN, GiST, and covering indexes.

---

## EXPLAIN ANALYZE

Always run `EXPLAIN (ANALYZE, BUFFERS)` on slow queries.

```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT u.name, COUNT(o.id)
FROM users u
JOIN orders o ON o.user_id = u.id
WHERE o.created_at > NOW() - INTERVAL '30 days'
GROUP BY u.id;
```

### Reading the Output

```
HashAggregate  (cost=8400.00..8500.00 rows=5000 width=40)
               (actual time=120.3..122.1 rows=4871 loops=1)
  ->  Hash Join  (cost=1500.00..7200.00 rows=240000 width=16)
                 (actual time=8.4..95.2 rows=237440 loops=1)
        Hash Cond: (o.user_id = u.id)
        ->  Index Scan using idx_orders_created on orders
              (cost=0.43..5200.00 rows=240000 width=8)
              (actual time=0.06..60.1 rows=237440 loops=1)
              Index Cond: (created_at > (now() - '30 days'::interval))
              Buffers: shared hit=1840 read=320
        ->  Hash  (cost=900.00..900.00 rows=48000 width=16)
                  (actual time=7.9..7.9 rows=48000 loops=1)
              Buckets: 65536  Batches: 1  Memory Usage: 2880kB
              Buffers: shared hit=648
Planning Time: 1.8 ms
Execution Time: 122.9 ms
```

| Field | What It Means |
|---|---|
| `cost=X..Y` | Planner estimate: startup cost .. total cost (arbitrary units) |
| `rows=N` (no "actual") | **Estimated** rows — from table statistics |
| `actual rows=N` | **Real** rows returned during execution |
| `loops=N` | Node ran N times; multiply `actual time` by `loops` to get total |
| `Buffers: hit=N` | Pages found in shared buffer cache — fast |
| `Buffers: read=N` | Pages fetched from disk — slow; high values indicate cache pressure |

**Large gap between `rows` and `actual rows`** → stale statistics. Run `ANALYZE table_name`.

**High `Buffers: read`** → data not in cache. Consider `pg_prewarm` or increasing `shared_buffers`.

**Nested Loop on large tables** → often needs a Hash Join. Nested Loop is O(n×m); Hash Join is O(n+m).

### Scan Types

- **Seq Scan** — reads every heap page. Fast for small tables or when returning > 10% of rows.
- **Index Scan** — traverses B-tree, then fetches heap rows. Random I/O. Best for small, selective result sets.
- **Bitmap Index Scan** — builds a bitmap of matching heap pages first, then reads them in physical order (sequential). Chosen when result set is medium-sized or when combining two indexes with BitmapAnd/BitmapOr.
- **Index Only Scan** — all needed columns in the index; heap not accessed. Requires the visibility map to be up to date (VACUUM populates it).

---

## Essential Query Patterns

### CTE (Common Table Expression)
```sql
WITH monthly_revenue AS (
  SELECT
    DATE_TRUNC('month', created_at) AS month,
    SUM(amount) AS revenue
  FROM orders
  WHERE status = 'completed'
  GROUP BY 1
)
SELECT month, revenue,
       revenue - LAG(revenue) OVER (ORDER BY month) AS delta
FROM monthly_revenue
ORDER BY month;
```

### Window Functions
```sql
-- Rank users by spend within each country
SELECT
  name,
  country,
  total_spend,
  RANK() OVER (PARTITION BY country ORDER BY total_spend DESC) AS country_rank,
  ROW_NUMBER() OVER (ORDER BY total_spend DESC) AS global_rank,
  LAG(total_spend) OVER (ORDER BY total_spend DESC) AS prev_spend
FROM users;
```

### UPSERT
```sql
INSERT INTO user_settings (user_id, key, value)
VALUES ($1, $2, $3)
ON CONFLICT (user_id, key)
DO UPDATE SET value = EXCLUDED.value, updated_at = NOW();
```

### JSONB Queries
```sql
-- Exact match on nested key
SELECT * FROM products WHERE metadata @> '{"brand": "Nike"}';

-- Key exists
SELECT * FROM events WHERE payload ? 'error_code';

-- Extract value
SELECT metadata->>'color' AS color FROM products;
SELECT metadata->'dimensions'->>'width' AS width FROM products;

-- Update nested key without overwriting whole object
UPDATE products
SET metadata = jsonb_set(metadata, '{price}', '29.99')
WHERE id = $1;
```

### Pagination (cursor-based — prefer over OFFSET)
```sql
-- OFFSET pagination degrades at high pages (scans all prior rows)
-- Cursor pagination stays O(log n)
SELECT id, created_at, title
FROM posts
WHERE created_at < $1  -- $1 is the cursor from the last item
ORDER BY created_at DESC
LIMIT 20;
```

---

## Transactions and Isolation Levels

```sql
BEGIN;
  UPDATE accounts SET balance = balance - 100 WHERE id = $1;
  UPDATE accounts SET balance = balance + 100 WHERE id = $2;
COMMIT;
-- On error: ROLLBACK;
```

| Level | Dirty Read | Non-Repeatable Read | Phantom Read |
|-------|-----------|--------------------|-----------:|
| READ COMMITTED (default) | ✗ | ✓ | ✓ |
| REPEATABLE READ | ✗ | ✗ | ✓ (mostly) |
| SERIALIZABLE | ✗ | ✗ | ✗ |

PostgreSQL's REPEATABLE READ actually implements **snapshot isolation** — it prevents phantom reads in practice even though the SQL standard only requires it to prevent non-repeatable reads. SERIALIZABLE adds conflict detection across transactions (SSI — Serializable Snapshot Isolation).

```sql
BEGIN ISOLATION LEVEL REPEATABLE READ;
BEGIN ISOLATION LEVEL SERIALIZABLE;
```

Use SERIALIZABLE for financial transfers. Use REPEATABLE READ when you need consistent snapshots within a transaction (reporting, multi-step reads).

---

## Connection Pooling with PgBouncer

PostgreSQL forks an OS process per connection (~5–10MB RAM each). At 200+ concurrent connections, the overhead becomes significant. PgBouncer is a lightweight proxy that maintains a smaller pool of actual Postgres connections and multiplexes application connections onto them.

### PgBouncer Modes

| Mode | Description | Prepared Statements | Use Case |
|---|---|---|---|
| **Session** | App connection maps 1:1 to Postgres connection for the duration of the session | Supported | Applications needing prepared statements |
| **Transaction** | Postgres connection released back to pool after each transaction | Not supported | Most web apps — highest efficiency |
| **Statement** | Postgres connection released after each statement | Not supported | Legacy; avoid |

Transaction mode is the most efficient (single connection serves many app connections) and is the default choice for most web backends. The catch: prepared statements don't work across transactions in this mode — use `$1` placeholders with `query()` rather than `prepare()`.

```typescript
// With PgBouncer in transaction mode, use a pool through the bouncer endpoint
import { Pool } from 'pg'

const pool = new Pool({
  connectionString: process.env.DATABASE_URL,  // points to PgBouncer, not Postgres directly
  max: 20,           // connections to PgBouncer (PgBouncer holds fewer to Postgres)
  idleTimeoutMillis: 30000,
  connectionTimeoutMillis: 2000,
})

const client = await pool.connect()
try {
  const result = await client.query('SELECT * FROM users WHERE id = $1', [userId])
  return result.rows[0]
} finally {
  client.release()  // returns connection to pool, not to Postgres
}
```

PgBouncer config (`pgbouncer.ini`):
```ini
[databases]
mydb = host=postgres port=5432 dbname=mydb

[pgbouncer]
pool_mode = transaction
max_client_conn = 1000   ; connections from applications
default_pool_size = 25   ; actual Postgres connections per database
```

---

## Partitioning

Partitioning splits a large table into smaller physical tables (partitions) while maintaining a single logical table interface. The query planner can skip irrelevant partitions entirely (**partition pruning**).

### Range Partitioning

```sql
-- Partition orders by month
CREATE TABLE orders (
  id         BIGSERIAL,
  created_at TIMESTAMPTZ NOT NULL,
  user_id    BIGINT NOT NULL,
  total      NUMERIC(10,2)
) PARTITION BY RANGE (created_at);

CREATE TABLE orders_2024_01 PARTITION OF orders
  FOR VALUES FROM ('2024-01-01') TO ('2024-02-01');

CREATE TABLE orders_2024_02 PARTITION OF orders
  FOR VALUES FROM ('2024-02-01') TO ('2024-03-01');

-- Automate with pg_partman extension in production
```

### List Partitioning

```sql
-- Partition users by account status
CREATE TABLE users (
  id     BIGSERIAL,
  status TEXT NOT NULL,
  email  TEXT NOT NULL
) PARTITION BY LIST (status);

CREATE TABLE users_active   PARTITION OF users FOR VALUES IN ('active');
CREATE TABLE users_inactive PARTITION OF users FOR VALUES IN ('inactive', 'deleted');
```

### Hash Partitioning

```sql
-- Even distribution across 4 shards by user_id
CREATE TABLE events (
  id      BIGSERIAL,
  user_id BIGINT NOT NULL,
  payload JSONB
) PARTITION BY HASH (user_id);

CREATE TABLE events_0 PARTITION OF events FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE events_1 PARTITION OF events FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE events_2 PARTITION OF events FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE events_3 PARTITION OF events FOR VALUES WITH (MODULUS 4, REMAINDER 3);
```

### Partition Pruning

```sql
EXPLAIN SELECT * FROM orders WHERE created_at >= '2024-01-01' AND created_at < '2024-02-01';
-- Planner only scans orders_2024_01, skips all other partitions
```

Each partition has its own indexes, VACUUM runs, and statistics. Indexes must be created on each partition or defined on the parent (PostgreSQL propagates them automatically when creating partitions after the index exists on parent).

---

## Migrations

Never alter production tables manually. Use versioned migrations (node-pg-migrate, Flyway, Liquibase, or Prisma Migrate).

Safe migration checklist for a column add:
1. Add column as nullable (no table lock needed)
2. Backfill in batches (don't update all rows in one transaction)
3. Add NOT NULL constraint only after backfill
4. Add index `CONCURRENTLY` — does not lock the table

```sql
-- Safe index creation on large table
CREATE INDEX CONCURRENTLY idx_users_phone ON users (phone);
```

---

## Common Anti-Patterns

**`SELECT *` prevents index-only scans.** If your index covers the columns you need, `SELECT *` forces a heap fetch anyway. Always name your columns.

**Missing index on foreign key column.** PostgreSQL does not auto-index FK columns. A JOIN on an un-indexed FK does a sequential scan on the child table.
```sql
-- After: ALTER TABLE orders ADD CONSTRAINT fk_user FOREIGN KEY (user_id) REFERENCES users(id);
CREATE INDEX idx_orders_user_id ON orders (user_id);  -- required manually
```

**N+1 queries.** Fetching a list of 100 users then querying orders for each user = 101 queries. Use a JOIN or a `WHERE user_id = ANY($1)` with the IDs collected in one trip.
```typescript
// N+1 (bad)
const users = await db.query('SELECT * FROM users LIMIT 100')
for (const user of users.rows) {
  user.orders = await db.query('SELECT * FROM orders WHERE user_id = $1', [user.id])
}

// Single query (good)
const result = await db.query(`
  SELECT u.*, json_agg(o.*) AS orders
  FROM users u
  LEFT JOIN orders o ON o.user_id = u.id
  GROUP BY u.id
  LIMIT 100
`)
```

**Using `VARCHAR(n)` without a real constraint reason.** `VARCHAR(255)` from MySQL habits. In PostgreSQL, `TEXT` and `VARCHAR` have identical performance. Only use `VARCHAR(n)` if the limit is a genuine business rule.

**`LIKE '%foo%'` leading wildcard.** Leading wildcard prevents B-tree index use. Use full-text search (`tsvector/tsquery`) or `pg_trgm` for substring search.
```sql
-- Won't use index
WHERE name LIKE '%alice%'

-- pg_trgm can index this
CREATE INDEX ON users USING GIN (name gin_trgm_ops);
WHERE name ILIKE '%alice%'  -- now uses the GIN index
```

---

## Replication

**Streaming replication** (physical): standby continuously applies WAL from primary. For read scaling and HA failover.

**Logical replication**: subscribe to changes on specific tables. Used for CDC (change data capture), analytics pipelines.

Direct reads to replica:
```
primary  ← writes
replica1 ← reads (reporting queries, analytics)
replica2 ← standby (failover)
```

---

## Interview Questions

**Q: What's the difference between a clustered and non-clustered index in PostgreSQL?**
PostgreSQL doesn't have a true clustered index concept by default — all indexes are non-clustered (heap-based). You can use `CLUSTER table USING index` to physically reorder rows by an index once, but it doesn't stay clustered on future writes.

**Q: Why is OFFSET-based pagination slow at high page numbers?**
`OFFSET 10000 LIMIT 20` forces the database to scan and discard 10,000 rows before returning 20. Cursor-based pagination uses an indexed WHERE clause so it's always O(log n) regardless of page number.

**Q: When would you use JSONB vs a separate table?**
JSONB for: truly variable schema, sparse attributes, rapid schema evolution, data you need to query occasionally. Separate table for: data with known structure, data you JOIN frequently, data with referential integrity requirements. Never put JSONB where you know the schema upfront.

**Q: Explain MVCC in PostgreSQL.**
Multi-Version Concurrency Control: every row update creates a new row version with `xmin`/`xmax` transaction IDs. Readers see a consistent snapshot of the data without blocking writers; writers don't block readers. Readers never acquire locks. Dead row versions accumulate until VACUUM reclaims them. This is why heavy UPDATE workloads need aggressive autovacuum settings — dead rows bloat the table and eventually require a full table scan even for indexed queries.

**Q: Why does PostgreSQL need VACUUM and how does it relate to MVCC?**
MVCC creates dead row versions on every UPDATE and DELETE. Without cleanup, the table grows indefinitely and indexes accumulate dead entries. VACUUM scans heap pages, reclaims dead tuples, removes dead index entries, and updates the visibility map (which enables index-only scans). It also advances the frozen XID to prevent transaction ID wraparound — a 32-bit counter that would corrupt data if it wrapped around to zero.

**Q: What does PgBouncer transaction mode gain you and what does it cost?**
Transaction mode allows hundreds of application connections to multiplex onto a small pool of real Postgres connections, releasing the connection to the pool after each transaction. This dramatically reduces Postgres process count and memory usage. The cost: prepared statements are session-scoped and don't survive being re-assigned to a different underlying connection, so named prepared statements (`PREPARE foo AS ...`) don't work. Use parameterized queries with `$1` placeholders instead.

---

## Related

- [Indexing](../indexing/README.md) — B-tree internals, EXPLAIN ANALYZE deep dive, index types
- [SQL](../sql/README.md) — window functions, CTEs, JOINs
- [Transactions](../transactions/README.md) — ACID, isolation levels, locking
- [pgvector](../pgvector/README.md) — vector search extension
- [Caching](../../02-backend/caching/README.md) — Redis + Postgres cache-aside
- [Rate Limiting](../../02-backend/rate-limiting/README.md) — rate limiting with Postgres
