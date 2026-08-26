# Database Indexing

Indexes are separate data structures that let the database find rows without scanning every row in a table. The tradeoff: faster reads, slower writes, more disk/memory. Every index must be updated on INSERT, UPDATE, and DELETE.

---

## B-Tree Index Internals

The default index type in PostgreSQL, MySQL, SQLite, and most relational databases.

### Structure

A B-tree (balanced tree) consists of:
- **Root node** — single entry point
- **Internal nodes** — keys + pointers to child nodes
- **Leaf nodes** — keys + pointers to heap (actual table rows), linked in sorted order

```
          [50]
         /    \
     [20,35]  [65,80]
    /  |   \   /  |  \
 [10] [25] [40][55][70][90]
  ↓    ↓    ↓   ↓   ↓   ↓
heap heap heap heap heap heap
```

Each node is a **page** (typically 8KB in PostgreSQL). A page holds many keys — the number of keys per page is the **fan-out factor** (often 100–400 for integer keys). High fan-out keeps the tree shallow: a billion-row table needs only 4–5 levels.

### O(log n) Lookups

To find a row by value:
1. Read root page (1 I/O)
2. Follow pointer to correct child (1 I/O per level)
3. Reach leaf node — get heap pointer (1 I/O)
4. Fetch heap page containing the actual row (1 I/O)

Total: O(log n) I/Os where n = number of rows. For 1 billion rows with fan-out 200: log₂₀₀(1,000,000,000) ≈ 4 levels.

### Why Range Scans Are Fast

Leaf nodes are **doubly linked** in sorted order. After finding the start of a range, the database walks leaf pages sequentially — no need to go back up the tree. Sequential page reads are fast because the OS can prefetch them.

```sql
-- B-tree walks to WHERE created_at = '2024-01-01', then scans right
SELECT * FROM orders
WHERE created_at BETWEEN '2024-01-01' AND '2024-01-31';
```

### Creating a B-Tree Index

```sql
-- Default (B-tree)
CREATE INDEX idx_orders_user_id ON orders (user_id);

-- Descending (useful when ORDER BY col DESC)
CREATE INDEX idx_orders_created ON orders (created_at DESC);

-- Never locks the table during build on large tables:
CREATE INDEX CONCURRENTLY idx_orders_status ON orders (status);
```

---

## Hash Indexes

Stores a hash of the indexed value. The hash maps directly to a bucket containing the heap pointer.

- **O(1) equality lookups** — faster than B-tree for pure equality
- **No range support** — hash is not ordered, so `> / < / BETWEEN` can't use it
- **Not replicated** pre-PostgreSQL 10 (now they are)
- PostgreSQL rarely chooses hash indexes over B-tree even for equality — B-tree is usually fast enough and more versatile

```sql
CREATE INDEX idx_users_email_hash ON users USING HASH (email);
-- Only useful for: WHERE email = $1
-- Useless for: WHERE email LIKE 'alice%'
```

### Heap-Only Tuples (HOT) Optimization

When you UPDATE a row and the changed column is not part of any index, PostgreSQL can write the new row version on the same heap page and link it from the old version — without updating any index. This is called a **Heap-Only Tuple** (HOT) update. It avoids the write amplification of updating every index on every update. HOT requires `fillfactor < 100` (default is 100; set to 70-90 to leave room on pages for HOT updates).

```sql
ALTER TABLE users SET (fillfactor = 80);
-- Now updates that don't touch indexed columns are HOT-eligible
```

---

## Composite (Multi-Column) Indexes

A composite index on `(a, b, c)` is sorted first by `a`, then by `b` within each `a`, then by `c` within each `(a, b)`.

### The Leading Column Rule

The index can satisfy queries that filter on a **prefix** of the column list:

```sql
CREATE INDEX idx_orders_user_status_date ON orders (user_id, status, created_at);
```

| Query Filter | Uses Index? |
|---|---|
| `WHERE user_id = $1` | Yes — leading column |
| `WHERE user_id = $1 AND status = $2` | Yes — (user_id, status) prefix |
| `WHERE user_id = $1 AND status = $2 AND created_at > $3` | Yes — full index |
| `WHERE status = $2` | No — not a prefix |
| `WHERE status = $2 AND created_at > $3` | No — not a prefix |
| `WHERE user_id = $1 AND created_at > $3` | Partial — user_id used, created_at skipped |

```sql
-- This query benefits from the composite index
SELECT * FROM orders
WHERE user_id = 42 AND status = 'pending'
ORDER BY created_at DESC;
-- PostgreSQL can use (user_id, status) from the index and created_at for ordering

-- This cannot use the index at all
SELECT * FROM orders WHERE status = 'pending';
-- Needs a separate index on (status) or (status, created_at)
```

### Column Order Strategy

Put the most selective (highest cardinality) column that appears in equality conditions first. Put range conditions last — they stop the index from being useful for subsequent columns.

```sql
-- Good: equality on user_id (high cardinality) first, range on date last
CREATE INDEX ON orders (user_id, status, created_at);

-- Bad: range on date prevents status from being used
CREATE INDEX ON orders (created_at, user_id, status);
```

---

## Covering Indexes (Index-Only Scans)

If all columns referenced in a query (SELECT list + WHERE + ORDER BY) are in the index, PostgreSQL can answer the query **without touching the heap at all**. This is an **index-only scan**.

```sql
CREATE INDEX idx_orders_covering ON orders (user_id, status, created_at, total);

-- Index-only scan: all columns (user_id, status, total) are in the index
SELECT user_id, status, total
FROM orders
WHERE user_id = 42 AND status = 'completed';
```

```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT user_id, status, total
FROM orders
WHERE user_id = 42 AND status = 'completed';

-- Index Only Scan using idx_orders_covering on orders
--   Index Cond: ((user_id = 42) AND (status = 'completed'))
--   Heap Fetches: 0   <-- no heap access
```

Covering indexes trade index size (and write overhead) for dramatically faster reads on hot query paths.

---

## Partial Indexes

An index with a `WHERE` clause that only includes rows matching the condition.

```sql
-- Only index orders that are pending — far smaller than indexing all orders
CREATE INDEX idx_pending_orders ON orders (created_at)
WHERE status = 'pending';

-- Queries on pending orders use this tiny, fast index
SELECT * FROM orders
WHERE status = 'pending' AND created_at < NOW() - INTERVAL '1 hour';

-- Other queries (completed orders) do not use this index
SELECT * FROM orders WHERE status = 'completed';
```

When to use partial indexes:
- A column has low cardinality overall but you only query one value frequently (`status = 'pending'` on an orders table where 99% are completed)
- You want to enforce uniqueness on a subset: `CREATE UNIQUE INDEX ON users(email) WHERE deleted_at IS NULL`
- To exclude NULLs: `CREATE INDEX ON events(user_id) WHERE user_id IS NOT NULL`

---

## Expression (Functional) Indexes

Index the result of an expression rather than a raw column value.

```sql
-- Without this, WHERE lower(email) = $1 forces a full scan
CREATE INDEX idx_users_email_lower ON users (lower(email));

-- Now this query uses the index
SELECT * FROM users WHERE lower(email) = lower('Alice@Example.com');

-- Index on extracted JSON field
CREATE INDEX idx_events_type ON events ((payload->>'event_type'));

-- Index on date part (useful for grouping by day)
CREATE INDEX idx_orders_day ON orders (DATE_TRUNC('day', created_at));
```

The expression in the index must exactly match the expression in the query (PostgreSQL is strict about this).

---

## Index Selectivity and Cardinality

**Cardinality** = number of distinct values in a column.

**Selectivity** = cardinality / total rows. High selectivity → index is useful. Low selectivity → not.

| Column | Distinct Values | Selectivity | Index Useful? |
|---|---|---|---|
| `user_id` on orders | millions | high | Yes |
| `status` ('pending','completed','cancelled') | 3 | very low | Usually no |
| `country` | ~200 | medium | Depends |
| `email` | = row count | 1.0 | Yes, excellent |

For low-selectivity columns, a sequential scan is often faster — the database would need to fetch too many heap pages anyway.

```sql
-- Check cardinality
SELECT COUNT(DISTINCT status) FROM orders;  -- returns 3 → low, avoid index
SELECT COUNT(DISTINCT user_id) FROM orders; -- returns millions → high, use index
```

---

## Why Too Many Indexes Hurts Writes

Every index is a separate data structure. On every `INSERT`, `UPDATE`, or `DELETE`:
1. The row change is written to the heap
2. **Every index on the table must be updated**
3. VACUUM must clean up dead index entries alongside dead heap rows

```
Table with 10 indexes:
INSERT 1 row → 11 writes (1 heap + 10 indexes)
UPDATE 1 row → up to 22 writes (old + new versions in each structure)
```

Practical rule: 5-8 indexes per table is common. Beyond that, benchmark write throughput. A heavily-written table (logs, events, metrics) may need zero indexes or only one.

---

## EXPLAIN ANALYZE Output

```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT u.id, COUNT(o.id) AS order_count
FROM users u
JOIN orders o ON o.user_id = u.id
WHERE o.status = 'completed'
  AND o.created_at > NOW() - INTERVAL '30 days'
GROUP BY u.id;
```

```
HashAggregate  (cost=1240.50..1340.50 rows=100 width=16)
               (actual time=45.2..46.1 rows=87 loops=1)
  ->  Hash Join  (cost=85.00..1200.00 rows=8000 width=8)
                 (actual time=2.1..40.3 rows=8432 loops=1)
        Hash Cond: (o.user_id = u.id)
        ->  Index Scan using idx_orders_status_date on orders
              (cost=0.43..900.00 rows=8000 width=8)
              (actual time=0.05..30.2 rows=8432 loops=1)
              Index Cond: ((status = 'completed')
                AND (created_at > (now() - '30 days'::interval)))
              Buffers: shared hit=210 read=45
        ->  Hash  (cost=60.00..60.00 rows=2000 width=8)
                  (actual time=1.8..1.8 rows=2000 loops=1)
              Buckets: 2048  Batches: 1  Memory Usage: 87kB
              Buffers: shared hit=60
Planning Time: 1.2 ms
Execution Time: 46.8 ms
```

Reading the output:

| Field | Meaning |
|---|---|
| `cost=X..Y` | Estimated startup cost .. total cost (in planner units) |
| `rows=N` | **Estimated** rows (from statistics) |
| `actual time=X..Y` | Actual milliseconds: first row .. all rows |
| `actual rows=N` | **Actual** rows returned |
| `loops=N` | Node executed N times (multiply actual time by loops) |
| `Buffers: hit=N` | Pages served from shared buffer cache (fast) |
| `Buffers: read=N` | Pages read from disk (slow — consider `pg_prewarm`) |

### Scan Types

**Sequential Scan** — reads every page in the table. Fast when returning > ~10% of rows or table is tiny. Bad on large tables for selective queries.

**Index Scan** — traverses B-tree, then fetches each heap page. Can cause random I/O if result set is large and scattered.

**Bitmap Index Scan** — builds a bitmap of matching heap pages from the index, then fetches heap pages in physical order. Chosen when Index Scan would cause too many random reads but Seq Scan would read too much. Handles OR conditions across multiple indexes.

```
Bitmap Heap Scan on orders
  Recheck Cond: (status = 'completed')
  ->  Bitmap Index Scan on idx_orders_status
        Index Cond: (status = 'completed')
```

**Index Only Scan** — index has all needed columns; heap is not accessed.

### When estimated rows ≠ actual rows

Large discrepancy means stale statistics. Run:
```sql
ANALYZE orders;
-- Or for the whole database:
ANALYZE;
```

Set `autovacuum_analyze_scale_factor = 0.01` on high-churn tables to trigger ANALYZE more aggressively.

---

## PostgreSQL-Specific Index Types

### BRIN (Block Range INdex)

Stores min/max values per range of pages. Tiny index (kilobytes vs megabytes for B-tree). Only useful for **naturally ordered** columns (monotonically increasing inserts: `created_at`, `id`, sensor readings).

```sql
-- Orders are inserted with increasing created_at — BRIN is perfect
CREATE INDEX idx_orders_brin ON orders USING BRIN (created_at);
-- Size: ~100KB vs ~500MB for B-tree on same column (10M rows)
```

Not useful for: randomly distributed values, columns updated in place.

### GIN (Generalized Inverted Index)

Best for: JSONB keys, full-text search, array containment. Stores a posting list per value — like a search engine's inverted index.

```sql
-- JSONB: enables @>, ?, ?|, ?& operators
CREATE INDEX idx_products_meta ON products USING GIN (metadata);
SELECT * FROM products WHERE metadata @> '{"brand": "Nike", "sport": "running"}';

-- Full-text search
CREATE INDEX idx_articles_fts ON articles
  USING GIN (to_tsvector('english', title || ' ' || body));
SELECT * FROM articles
WHERE to_tsvector('english', title || ' ' || body) @@ to_tsquery('english', 'postgres & index');

-- Array containment
CREATE INDEX idx_posts_tags ON posts USING GIN (tags);
SELECT * FROM posts WHERE tags @> ARRAY['nodejs', 'backend'];
```

### GiST (Generalized Search Tree)

Framework for custom index types. Used for: geometric data, range types, nearest-neighbor search.

```sql
-- Range overlap queries
CREATE INDEX idx_bookings_during ON bookings USING GIST (during);
SELECT * FROM bookings WHERE during && '[2024-01-15, 2024-01-20)'::tsrange;

-- PostGIS spatial queries
CREATE INDEX idx_locations_geom ON locations USING GIST (geom);
SELECT * FROM locations WHERE ST_DWithin(geom, ST_MakePoint(-0.1, 51.5), 1000);
```

### CREATE INDEX CONCURRENTLY

Building an index normally acquires a lock that blocks all writes. `CONCURRENTLY` builds in multiple passes without blocking — but takes 2-3x longer and cannot run inside a transaction block.

```sql
-- Safe on production — no write lock
CREATE INDEX CONCURRENTLY idx_orders_email ON orders (customer_email);

-- If it fails midway, leaves an invalid index — must drop it
SELECT indexname, pg_size_pretty(pg_relation_size(indexrelid))
FROM pg_stat_user_indexes
WHERE idx_scan = 0;  -- find unused / invalid indexes

DROP INDEX CONCURRENTLY idx_orders_email;  -- retry
```

### REINDEX After Heavy Deletions

Indexes accumulate bloat (empty pages) after many deletions. VACUUM removes dead heap rows but does not compact index pages below their high-water mark.

```sql
-- Rebuild a specific index (locks table briefly unless CONCURRENTLY)
REINDEX INDEX CONCURRENTLY idx_orders_user_id;

-- Rebuild all indexes on a table
REINDEX TABLE CONCURRENTLY orders;

-- Check index bloat
SELECT
  indexrelid::regclass AS index,
  pg_size_pretty(pg_relation_size(indexrelid)) AS size
FROM pg_stat_user_indexes
WHERE schemaname = 'public'
ORDER BY pg_relation_size(indexrelid) DESC;
```

---

## TypeScript: Analyzing Index Usage

```typescript
import { Pool } from 'pg'

const pool = new Pool({ connectionString: process.env.DATABASE_URL })

// Find indexes that are never used (candidates for removal)
async function findUnusedIndexes(): Promise<void> {
  const result = await pool.query(`
    SELECT
      schemaname,
      tablename,
      indexname,
      idx_scan AS scans,
      pg_size_pretty(pg_relation_size(indexrelid)) AS size
    FROM pg_stat_user_indexes
    WHERE idx_scan = 0
      AND indexname NOT LIKE 'pg_%'
    ORDER BY pg_relation_size(indexrelid) DESC
  `)
  console.table(result.rows)
}

// Find slow queries that might need an index
async function findSlowQueries(): Promise<void> {
  const result = await pool.query(`
    SELECT
      query,
      calls,
      mean_exec_time,
      total_exec_time,
      rows
    FROM pg_stat_statements
    WHERE mean_exec_time > 100  -- queries averaging > 100ms
    ORDER BY mean_exec_time DESC
    LIMIT 20
  `)
  console.table(result.rows)
}
```

---

## Interview Questions

**Q: What happens to a B-tree index during an UPDATE?**
An UPDATE in PostgreSQL is actually a DELETE + INSERT at the heap level (MVCC). The old row version is marked dead (xmax set), and a new version is written. Every index on the table must add a new entry pointing to the new heap location and eventually have the old entry removed by VACUUM. Unless HOT optimization applies (update on non-indexed column, same heap page), index entries multiply on every update.

**Q: Explain Bitmap Index Scan and when PostgreSQL chooses it over Index Scan.**
An Index Scan fetches heap pages one by one as it walks the index — fine for a small result set but causes random I/O for large ones. A Bitmap Index Scan first reads the entire index range to build an in-memory bitmap of which heap pages contain matching rows, then fetches those pages in physical order, which is sequential I/O. PostgreSQL chooses Bitmap when the query matches a significant fraction of rows (hundreds to thousands), or when combining two indexes with OR/AND (Bitmap AND/OR).

**Q: You have a query `WHERE status = 'active' AND created_at > '2024-01-01'` running slow. There's an index on (status) and an index on (created_at). What do you do?**
PostgreSQL might use both indexes via a Bitmap AND (intersecting the two bitmaps). But a composite index on `(status, created_at)` is almost always better — it avoids two index traversals and the bitmap merge. If 'active' is rare (high selectivity), put it first. If created_at is used more often alone, keep both the composite and the single-column index on created_at, and let the planner choose.

**Q: When would you NOT add an index?**
(1) The table is small (< a few thousand rows — seq scan is faster). (2) The column has very low cardinality (e.g., a boolean). (3) The query returns > 10-20% of rows. (4) The table is write-heavy (logs, events) and read latency is less critical than write throughput. (5) You already have a composite index that covers this column as its leading element.

**Q: What is the difference between a partial index and a composite index?**
A composite index includes multiple columns and is used for queries filtering on a prefix of those columns. A partial index includes all rows that satisfy a WHERE condition but only indexes a subset of rows. They solve different problems and can be combined: `CREATE INDEX ON orders (created_at) WHERE status = 'pending'` is a partial composite index.

---

## Related

- [PostgreSQL](../postgresql/README.md) — EXPLAIN ANALYZE, MVCC, connection pooling
- [SQL](../sql/README.md) — query patterns that index-tune
- [pgvector](../pgvector/README.md) — vector indexes (IVFFlat, HNSW)
- [Transactions](../transactions/README.md) — how locking interacts with index scans
- [Similarity Search](../../04-ai-ml/similarity-search/README.md) — approximate nearest neighbor
