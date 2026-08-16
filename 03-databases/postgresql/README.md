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

Things to look for:
- **Seq Scan** on large tables — usually needs an index
- **Nested Loop** on large result sets — may need a Hash Join
- **Buffers: hit** vs **read** — high `read` means cache miss
- **actual rows** vs **rows** — large difference = stale statistics (run `ANALYZE`)

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

Use `SERIALIZABLE` for financial transfers. Use `REPEATABLE READ` when you need consistent snapshots within a transaction.

```sql
BEGIN ISOLATION LEVEL REPEATABLE READ;
```

---

## Connection Pooling

PostgreSQL forks a process per connection — expensive. At 200+ concurrent connections, use PgBouncer or a driver-level pool.

```typescript
// pg with pool (node-postgres)
import { Pool } from 'pg'

const pool = new Pool({
  connectionString: process.env.DATABASE_URL,
  max: 20,           // max connections in pool
  idleTimeoutMillis: 30000,
  connectionTimeoutMillis: 2000,
})

// Always release connections back to pool
const client = await pool.connect()
try {
  const result = await client.query('SELECT * FROM users WHERE id = $1', [userId])
  return result.rows[0]
} finally {
  client.release()
}
```

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
Multi-Version Concurrency Control: every row update creates a new row version (with xmin/xmax transaction IDs). Readers see a consistent snapshot of the data without blocking writers; writers don't block readers. Dead row versions are cleaned up by VACUUM.
