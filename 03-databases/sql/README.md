# SQL Fundamentals

## Core SELECT Anatomy

```sql
SELECT   columns        -- 6. what to return
FROM     table          -- 1. where to get data
JOIN     other ON cond  -- 2. combine tables
WHERE    condition      -- 3. filter rows
GROUP BY columns        -- 4. aggregate groups
HAVING   condition      -- 5. filter groups (after GROUP BY)
ORDER BY columns        -- 7. sort results
LIMIT    n OFFSET m     -- 8. paginate
```

---

## JOINs

```sql
-- INNER JOIN: only rows with matches in both tables
SELECT u.name, o.total
FROM users u
INNER JOIN orders o ON o.user_id = u.id;

-- LEFT JOIN: all users, even those with no orders
SELECT u.name, COUNT(o.id) AS order_count
FROM users u
LEFT JOIN orders o ON o.user_id = u.id
GROUP BY u.id, u.name;

-- Find users with NO orders
SELECT u.name
FROM users u
LEFT JOIN orders o ON o.user_id = u.id
WHERE o.id IS NULL;

-- Self-join: employees and their managers
SELECT e.name AS employee, m.name AS manager
FROM employees e
LEFT JOIN employees m ON e.manager_id = m.id;
```

---

## Aggregates

```sql
COUNT(*), COUNT(col)  -- COUNT(col) ignores NULLs
SUM(col)
AVG(col)
MIN(col), MAX(col)
STRING_AGG(col, ', ')  -- PostgreSQL: concatenate strings with separator

-- Orders per user, only users with > 5 orders
SELECT user_id, COUNT(*) AS orders
FROM orders
GROUP BY user_id
HAVING COUNT(*) > 5
ORDER BY orders DESC;

-- Collect all tags per post into an array
SELECT post_id, ARRAY_AGG(tag) AS tags
FROM post_tags
GROUP BY post_id;
```

---

## Subqueries

```sql
-- Scalar subquery (returns one value)
SELECT name,
  (SELECT COUNT(*) FROM orders o WHERE o.user_id = u.id) AS order_count
FROM users u;

-- IN with subquery
SELECT name FROM users
WHERE id IN (SELECT DISTINCT user_id FROM orders WHERE total > 1000);

-- EXISTS (faster than IN for large sets — stops at first match)
SELECT name FROM users u
WHERE EXISTS (
  SELECT 1 FROM orders o
  WHERE o.user_id = u.id AND o.total > 1000
);
```

### Correlated Subqueries vs JOINs

A **correlated subquery** references the outer query and re-executes for each outer row — O(n²) in the worst case. A JOIN computes the result once and is usually more efficient.

```sql
-- Correlated subquery: runs once per user row
SELECT u.name,
  (SELECT MAX(o.total) FROM orders o WHERE o.user_id = u.id) AS largest_order
FROM users u;

-- Equivalent JOIN: computes MAX once per user_id
SELECT u.name, sub.largest_order
FROM users u
LEFT JOIN (
  SELECT user_id, MAX(total) AS largest_order
  FROM orders
  GROUP BY user_id
) sub ON sub.user_id = u.id;
```

Use correlated subqueries when: the logic doesn't translate cleanly to a JOIN, or the subquery returns a scalar that participates in an expression. Use JOINs when you're joining sets — the planner can optimize them far better.

---

## NULLs

NULL is not a value — it's the absence of a value. Use `IS NULL` / `IS NOT NULL`, not `= NULL`.

```sql
SELECT * FROM users WHERE phone IS NULL;
SELECT * FROM users WHERE phone IS NOT NULL;

-- COALESCE: return first non-NULL value
SELECT COALESCE(phone, email, 'no contact') AS contact FROM users;

-- NULLIF: return NULL if two values are equal (division by zero protection)
SELECT total / NULLIF(quantity, 0) AS unit_price FROM orders;
```

---

## Window Functions

Window functions compute values across a set of rows related to the current row — without collapsing rows like GROUP BY does.

```sql
-- ROW_NUMBER: unique rank per partition (no ties — always unique)
SELECT
  name,
  department,
  salary,
  ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) AS dept_rank
FROM employees;
-- dept_rank resets to 1 for each department

-- RANK vs DENSE_RANK:
-- Two employees tied at #1 salary:
-- RANK gives:        1, 1, 3  (skips 2)
-- DENSE_RANK gives:  1, 1, 2  (no gaps)
SELECT
  name,
  salary,
  RANK()       OVER (ORDER BY salary DESC) AS rank_with_gaps,
  DENSE_RANK() OVER (ORDER BY salary DESC) AS rank_no_gaps
FROM employees;

-- Running total with SUM OVER
SELECT
  created_at::date AS day,
  SUM(amount) AS daily_revenue,
  SUM(SUM(amount)) OVER (ORDER BY created_at::date) AS running_total
FROM orders
GROUP BY 1
ORDER BY 1;
-- Note: SUM(SUM(amount)) — inner SUM is from GROUP BY, outer SUM accumulates across rows

-- LAG/LEAD: compare current row to previous/next row
SELECT
  month,
  revenue,
  LAG(revenue, 1, 0) OVER (ORDER BY month) AS prev_month,  -- default 0 if no prior row
  revenue - LAG(revenue) OVER (ORDER BY month) AS delta,
  LEAD(revenue) OVER (ORDER BY month) AS next_month
FROM monthly_revenue
ORDER BY month;

-- Moving average (7-day)
SELECT
  day,
  revenue,
  AVG(revenue) OVER (
    ORDER BY day
    ROWS BETWEEN 6 PRECEDING AND CURRENT ROW  -- current + 6 prior rows
  ) AS moving_avg_7d
FROM daily_revenue;

-- NTILE: bucket rows into N groups (percentiles)
SELECT
  name,
  salary,
  NTILE(4) OVER (ORDER BY salary) AS quartile  -- 1=bottom 25%, 4=top 25%
FROM employees;
```

---

## CTEs and Recursive Queries

```sql
-- Simple CTE for readability
WITH active_users AS (
  SELECT id, name FROM users WHERE deleted_at IS NULL
),
recent_orders AS (
  SELECT user_id, SUM(total) AS revenue
  FROM orders WHERE created_at > NOW() - INTERVAL '30 days'
  GROUP BY user_id
)
SELECT u.name, COALESCE(r.revenue, 0) AS revenue
FROM active_users u
LEFT JOIN recent_orders r ON r.user_id = u.id;

-- Recursive CTE: walk an org hierarchy
WITH RECURSIVE org AS (
  -- Anchor: start at the root (employees with no manager)
  SELECT id, name, manager_id, 0 AS depth, ARRAY[id] AS path
  FROM employees WHERE manager_id IS NULL

  UNION ALL

  -- Recursive: add each employee whose manager is already in the result
  SELECT e.id, e.name, e.manager_id, o.depth + 1, o.path || e.id
  FROM employees e
  JOIN org o ON e.manager_id = o.id
  WHERE NOT e.id = ANY(o.path)  -- cycle detection
)
SELECT * FROM org ORDER BY depth, name;
```

Recursive CTEs work by:
1. Executing the anchor query (no recursion)
2. Repeatedly executing the recursive part, joining with the previous iteration's results
3. Stopping when no new rows are added

Use cases: org charts, filesystem trees, bill-of-materials explosion, graph traversal (BFS).

---

## LATERAL Joins

`LATERAL` allows the right side of a join to reference columns from the left side — like a correlated subquery in the FROM clause, but it can return a set of rows (not just a scalar).

```sql
-- Get the 3 most recent orders for each user
SELECT u.id, u.name, o.id AS order_id, o.total, o.created_at
FROM users u
CROSS JOIN LATERAL (
  SELECT id, total, created_at
  FROM orders
  WHERE user_id = u.id      -- references u.id from outer query
  ORDER BY created_at DESC
  LIMIT 3
) o;

-- Unnesting a function result with LATERAL
SELECT u.id, tag
FROM users u,
     LATERAL UNNEST(u.tags) AS tag   -- implicit LATERAL for set-returning functions
WHERE tag LIKE 'premium%';
```

LATERAL is essential when a set-returning function or subquery needs per-row context from the outer query.

---

## UPSERT (ON CONFLICT)

```sql
-- Insert or update on unique constraint violation
INSERT INTO user_settings (user_id, key, value)
VALUES ($1, $2, $3)
ON CONFLICT (user_id, key)
DO UPDATE SET
  value = EXCLUDED.value,       -- EXCLUDED = the row that was rejected
  updated_at = NOW();

-- Insert or ignore (do nothing on conflict)
INSERT INTO events (id, type, payload)
VALUES ($1, $2, $3)
ON CONFLICT (id) DO NOTHING;

-- Upsert with conditional update (only update if new value is more recent)
INSERT INTO user_presence (user_id, last_seen)
VALUES ($1, NOW())
ON CONFLICT (user_id)
DO UPDATE SET last_seen = EXCLUDED.last_seen
WHERE EXCLUDED.last_seen > user_presence.last_seen;
```

---

## JSONB Operations

```sql
-- Arrow operators
metadata->'key'         -- returns JSONB (preserves type)
metadata->>'key'        -- returns TEXT (for top-level key)
metadata#>'{a,b}'       -- returns JSONB at path a.b
metadata#>>'{a,b}'      -- returns TEXT at path a.b

-- Containment
metadata @> '{"brand": "Nike"}'  -- does metadata contain this?
'{"a":1,"b":2}' <@ metadata      -- is left contained in right?

-- Key existence
metadata ? 'brand'               -- key exists?
metadata ?| ARRAY['brand','sku'] -- any key exists?
metadata ?& ARRAY['brand','sku'] -- all keys exist?

-- Modify
jsonb_set(metadata, '{price}', '29.99')        -- set nested key
metadata || '{"color": "red"}'::jsonb           -- merge objects
metadata - 'unwanted_key'                       -- remove key
jsonb_set(metadata, '{nested,key}', '"value"') -- set nested

-- Aggregate into JSON
SELECT
  user_id,
  json_agg(json_build_object('id', id, 'total', total)) AS orders
FROM orders
GROUP BY user_id;
```

---

## Full-Text Search

```sql
-- tsvector: preprocessed representation of text (stemmed, stopwords removed)
-- tsquery: search expression

-- Basic FTS
SELECT title
FROM articles
WHERE to_tsvector('english', title || ' ' || body) @@ to_tsquery('english', 'postgres & index');

-- GIN index for FTS (required for performance on large tables)
CREATE INDEX idx_articles_fts ON articles
  USING GIN (to_tsvector('english', title || ' ' || body));

-- Generated stored column (auto-updates, indexes efficiently)
ALTER TABLE articles ADD COLUMN search_vector tsvector
  GENERATED ALWAYS AS (
    setweight(to_tsvector('english', coalesce(title, '')), 'A') ||
    setweight(to_tsvector('english', coalesce(body, '')), 'B')
  ) STORED;

CREATE INDEX ON articles USING GIN (search_vector);

-- Search with ranking
SELECT title, ts_rank(search_vector, query) AS rank
FROM articles, to_tsquery('english', 'database & performance') query
WHERE search_vector @@ query
ORDER BY rank DESC
LIMIT 20;

-- Phrase search and wildcards
to_tsquery('english', 'postgres <-> performance')  -- adjacent words
to_tsquery('english', 'post:*')                    -- prefix match
websearch_to_tsquery('english', 'postgres performance -mysql')  -- Google-style
```

---

## COPY for Bulk Imports

`COPY` is an order of magnitude faster than `INSERT` for bulk loads — it bypasses trigger infrastructure, skips per-row parsing overhead, and uses optimized binary protocol.

```sql
-- Import from CSV file (server-side)
COPY orders (id, user_id, total, created_at)
FROM '/var/data/orders.csv'
WITH (FORMAT CSV, HEADER true, NULL '');

-- Export to CSV
COPY (SELECT * FROM orders WHERE created_at > '2024-01-01')
TO '/var/data/orders_export.csv'
WITH (FORMAT CSV, HEADER true);
```

```typescript
// Client-side COPY via pg COPY stream
import { from as copyFrom } from 'pg-copy-streams'
import { createReadStream } from 'fs'

const client = await pool.connect()
try {
  const stream = client.query(
    copyFrom(`COPY orders (id, user_id, total) FROM STDIN WITH (FORMAT CSV, HEADER)`)
  )
  const fileStream = createReadStream('orders.csv')
  await pipeline(fileStream, stream)  // pipes CSV into Postgres
} finally {
  client.release()
}
```

For large imports, disable indexes and constraints before COPY, then rebuild them after — much faster than maintaining them row by row.

---

## Interview Questions

**Q: What is the difference between WHERE and HAVING?**
WHERE filters rows before grouping. HAVING filters groups after GROUP BY. You can't use aggregate functions in WHERE.

**Q: Why is SELECT * bad in production?**
(1) Fetches more data than needed — wastes bandwidth and memory. (2) Breaks if a column is added or reordered and the app assumes a specific column order. (3) Prevents index-only scans — the database must fetch the full heap row even if an index covers the query.

**Q: What is an index and when doesn't it help?**
An index is a separate data structure (usually B-tree) that PostgreSQL uses to find rows without scanning the whole table. It doesn't help when: the query returns a large fraction of the table (full scan is cheaper), the column has low cardinality (boolean, status with 2 values), you use a function on the column in WHERE (`WHERE LOWER(email) = ...` — use a functional index instead), or the table is tiny.

**Q: What is the difference between RANK and DENSE_RANK?**
Both assign ranks based on ordering within a partition. When rows tie (same ORDER BY value), RANK assigns the same rank to all tied rows and then skips numbers — two rows at rank 1, next rank is 3. DENSE_RANK also ties, but does not skip — next rank is 2. Use DENSE_RANK when you want no gaps in the ranking sequence.

**Q: How do window functions differ from GROUP BY aggregates?**
GROUP BY collapses rows — you get one output row per group. Window functions compute aggregates across related rows but keep the original rows intact. You can see both the row's own values and aggregate values (like running totals or ranks) in the same result set. Window functions are evaluated after WHERE, GROUP BY, and HAVING — you can use them alongside GROUP BY.

**Q: What is a LATERAL join and when do you need it?**
LATERAL allows a subquery in the FROM clause to reference columns from earlier in the FROM clause — like a correlated subquery but returning multiple rows. You need it when you want to apply a set-returning function or a TOP-N subquery to each row of an outer table — for example, getting the 3 most recent orders per user, or unnesting a per-user array.

---

## Related

- [PostgreSQL](../postgresql/README.md) — MVCC, EXPLAIN ANALYZE, partitioning, PgBouncer
- [Indexing](../indexing/README.md) — B-tree, GIN, covering indexes, partial indexes
- [Transactions](../transactions/README.md) — ACID, isolation levels, locking
- [MongoDB](../mongodb/README.md) — document model, when to choose NoSQL
