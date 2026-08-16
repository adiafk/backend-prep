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
STRING_AGG(col, ', ')  -- PostgreSQL: concatenate strings

-- Orders per user, only users with > 5 orders
SELECT user_id, COUNT(*) AS orders
FROM orders
GROUP BY user_id
HAVING COUNT(*) > 5
ORDER BY orders DESC;
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
  SELECT id, name, manager_id, 0 AS depth
  FROM employees WHERE manager_id IS NULL  -- root

  UNION ALL

  SELECT e.id, e.name, e.manager_id, o.depth + 1
  FROM employees e
  JOIN org o ON e.manager_id = o.id
)
SELECT * FROM org ORDER BY depth, name;
```

---

## Window Functions

```sql
-- ROW_NUMBER: unique rank per partition
SELECT
  name,
  department,
  salary,
  ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) AS dept_rank
FROM employees;

-- RANK vs DENSE_RANK: RANK skips numbers on ties, DENSE_RANK doesn't
-- e.g. two people tied at #1: RANK gives 1,1,3 — DENSE_RANK gives 1,1,2

-- Running total
SELECT
  created_at::date AS day,
  SUM(amount) AS daily_revenue,
  SUM(SUM(amount)) OVER (ORDER BY created_at::date) AS running_total
FROM orders
GROUP BY 1;

-- LAG/LEAD: compare to previous/next row
SELECT
  month,
  revenue,
  LAG(revenue) OVER (ORDER BY month) AS prev_month,
  revenue - LAG(revenue) OVER (ORDER BY month) AS delta
FROM monthly_revenue;
```

---

## Interview Questions

**Q: What is the difference between WHERE and HAVING?**
WHERE filters rows before grouping. HAVING filters groups after GROUP BY. You can't use aggregate functions in WHERE.

**Q: Why is SELECT * bad in production?**
(1) Fetches more data than needed — wastes bandwidth and memory. (2) Breaks if a column is added or reordered and the app assumes a specific column order. (3) Prevents index-only scans — the database may need to fetch the full row even if an index covers your query.

**Q: What is an index and when doesn't it help?**
An index is a separate data structure (usually B-tree) that PostgreSQL uses to find rows without scanning the whole table. It doesn't help when: the query returns a large fraction of the table (full scan is cheaper), the column has low cardinality (boolean, status with 2 values), you use a function on the column in WHERE (`WHERE LOWER(email) = ...` — use a functional index instead), or the table is tiny.
