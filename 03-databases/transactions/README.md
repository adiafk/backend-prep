# Transactions & Distributed Data

## ACID in Depth

### Atomicity
All operations in a transaction succeed together or fail together — no partial writes.

Implemented via **WAL undo log**: before modifying a page, the database writes what the page looked like before the change. If the transaction rolls back or the process crashes mid-transaction, the undo log is replayed to revert all changes.

```sql
BEGIN;
  UPDATE accounts SET balance = balance - 100 WHERE id = 1;
  UPDATE accounts SET balance = balance + 100 WHERE id = 2;
  -- If this crashes here, BOTH updates are rolled back on recovery
COMMIT;
```

### Consistency
A transaction brings the database from one valid state to another. The database enforces:
- **Column constraints**: NOT NULL, CHECK, data types
- **Referential integrity**: FOREIGN KEY constraints
- **Unique constraints**: no duplicate values
- **Application-level invariants**: your code must enforce these (e.g., balance cannot go negative)

```sql
-- Consistency enforced by constraint
ALTER TABLE accounts ADD CONSTRAINT positive_balance CHECK (balance >= 0);
-- Now: UPDATE accounts SET balance = -100 WHERE id = 1; fails at commit
```

### Isolation
Concurrent transactions do not see each other's partial state. The degree of isolation is configurable — stricter isolation is safer but has higher overhead (more lock contention or more transaction aborts).

### Durability
Once committed, data survives crashes. Implemented via **WAL redo log** (also called the WAL in PostgreSQL): before a commit is acknowledged to the client, the WAL record is written to durable storage (`fsync`). On recovery, PostgreSQL replays the WAL to reconstruct any changes that were committed but not yet written to the main data files.

---

## Read Phenomena with Concrete Examples

### Dirty Read
Reading data written by a transaction that has not yet committed. If that transaction rolls back, you've read data that never officially existed.

```sql
-- Transaction A (not yet committed)
BEGIN;
UPDATE accounts SET balance = 1000000 WHERE id = 1;
-- no COMMIT yet

-- Transaction B (with READ UNCOMMITTED — PostgreSQL doesn't actually support this)
SELECT balance FROM accounts WHERE id = 1;
-- Returns 1000000 — but A might roll back!
-- PostgreSQL prevents this by always providing at least READ COMMITTED behavior
```

### Non-Repeatable Read
Reading the same row twice within a transaction and getting different values because another transaction committed a change between the two reads.

```sql
-- Transaction A
BEGIN;
SELECT balance FROM accounts WHERE id = 1;  -- returns 100

-- Transaction B commits in between
UPDATE accounts SET balance = 200 WHERE id = 1;
COMMIT;

-- Transaction A reads again (READ COMMITTED level)
SELECT balance FROM accounts WHERE id = 1;  -- returns 200 now!
-- Same transaction, same query, different result
COMMIT;
```

### Phantom Read
A range query returns different rows on second execution because another transaction inserted or deleted rows matching the WHERE clause.

```sql
-- Transaction A
BEGIN;
SELECT COUNT(*) FROM orders WHERE status = 'pending';  -- returns 5

-- Transaction B inserts a new pending order and commits
INSERT INTO orders (status, total) VALUES ('pending', 99.99);
COMMIT;

-- Transaction A runs the same query again (REPEATABLE READ level)
SELECT COUNT(*) FROM orders WHERE status = 'pending';  -- returns 6!
-- A "phantom" row appeared
COMMIT;
```

### Lost Update
Two transactions read the same row, compute a new value, and write back. The second write overwrites the first — one update is silently lost.

```sql
-- Transaction A: read balance = 100, compute 100 - 30 = 70
SELECT balance FROM accounts WHERE id = 1;  -- 100

-- Transaction B: read balance = 100, compute 100 - 50 = 50, write 50
UPDATE accounts SET balance = 50 WHERE id = 1;
COMMIT;

-- Transaction A writes 70, overwriting B's committed write
UPDATE accounts SET balance = 70 WHERE id = 1;
COMMIT;
-- Net result: balance = 70. Transaction B's withdrawal of 50 was lost.
```

---

## Isolation Levels

| Isolation Level | Dirty Read | Non-Repeatable Read | Phantom Read | Lost Update |
|---|---|---|---|---|
| READ UNCOMMITTED | Possible | Possible | Possible | Possible |
| READ COMMITTED | Prevented | Possible | Possible | Possible |
| REPEATABLE READ | Prevented | Prevented | Possible* | Prevented |
| SERIALIZABLE | Prevented | Prevented | Prevented | Prevented |

*PostgreSQL REPEATABLE READ uses snapshot isolation and actually prevents phantom reads in most cases, despite the SQL standard not requiring it.

### PostgreSQL Actual Behavior

**READ COMMITTED** (default): each statement sees a fresh snapshot of committed data. A long transaction sees changes committed by other transactions between its statements.

**REPEATABLE READ**: the entire transaction sees a single snapshot taken at the start of the first statement. Other transactions' commits are invisible. Prevents non-repeatable reads and phantom reads. Implemented via snapshot isolation — no locking, but serialization failures (`ERROR 40001`) are possible and require retry.

**SERIALIZABLE**: adds conflict detection on top of REPEATABLE READ (Serializable Snapshot Isolation — SSI). Transactions run as if they executed serially. More aborts than REPEATABLE READ. Use for financial transfers or any "check then act" logic.

```sql
-- Set for the current transaction
BEGIN ISOLATION LEVEL REPEATABLE READ;
BEGIN ISOLATION LEVEL SERIALIZABLE;

-- Set for the session
SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL SERIALIZABLE;
```

---

## Optimistic vs Pessimistic Locking

### Pessimistic Locking
Lock the row when you read it. Other writers block until you commit or rollback.

```sql
BEGIN;
-- FOR UPDATE: exclusive lock. Other SELECT FOR UPDATE and UPDATE block.
SELECT * FROM accounts WHERE id = $1 FOR UPDATE;
-- FOR SHARE: shared lock. Other readers can read, but writers block.
SELECT * FROM accounts WHERE id = $1 FOR SHARE;

-- ... do work ...
UPDATE accounts SET balance = $2 WHERE id = $1;
COMMIT;
```

`FOR UPDATE SKIP LOCKED` — skip rows that are locked by another transaction. Useful for job queues: each worker picks up a different job.

```sql
-- Worker picks the next available job (skips jobs other workers have locked)
SELECT * FROM jobs
WHERE status = 'pending'
ORDER BY created_at
LIMIT 1
FOR UPDATE SKIP LOCKED;
```

Use pessimistic locking when: write conflicts are frequent, the work inside the transaction is fast, you can't tolerate retries.

### Optimistic Locking
Don't lock. Check that the row hasn't changed when writing. Retry if it has.

```sql
-- Read with version
SELECT id, balance, version FROM accounts WHERE id = $1;

-- Update only if version matches (retry if 0 rows affected)
UPDATE accounts
SET balance = $2, version = version + 1
WHERE id = $1 AND version = $3;
-- 0 rows updated = concurrent modification detected, retry
```

```typescript
async function debitAccount(accountId: number, amount: number): Promise<void> {
  for (let attempt = 0; attempt < 3; attempt++) {
    const { rows } = await pool.query(
      'SELECT balance, version FROM accounts WHERE id = $1',
      [accountId]
    )
    const { balance, version } = rows[0]
    if (balance < amount) throw new Error('Insufficient funds')

    const result = await pool.query(
      `UPDATE accounts
       SET balance = $1, version = version + 1
       WHERE id = $2 AND version = $3`,
      [balance - amount, accountId, version]
    )
    if (result.rowCount === 1) return  // success

    // Concurrent modification — retry after brief pause
    await new Promise(resolve => setTimeout(resolve, 10 * (attempt + 1)))
  }
  throw new Error('Failed after retries — too much contention')
}
```

Use optimistic locking when: write conflicts are rare, transactions span user think time (you can't hold a DB lock while the user fills a form), you want maximum concurrency.

---

## Deadlocks

A deadlock occurs when two transactions each hold a lock the other needs, and neither can proceed.

```sql
-- Transaction A
BEGIN;
UPDATE accounts SET balance = balance - 100 WHERE id = 1;  -- locks row 1
-- ... waiting for row 2 ...

-- Transaction B (concurrently)
BEGIN;
UPDATE accounts SET balance = balance - 50 WHERE id = 2;   -- locks row 2
UPDATE accounts SET balance = balance + 50 WHERE id = 1;   -- BLOCKS: row 1 locked by A

-- Back to A:
UPDATE accounts SET balance = balance + 100 WHERE id = 2;  -- BLOCKS: row 2 locked by B
-- DEADLOCK: A waits for B, B waits for A
```

PostgreSQL detects deadlocks within `deadlock_timeout` (default 1 second) and kills one transaction with error code `40P01`:
```
ERROR: deadlock detected
DETAIL: Process 12345 waits for ShareLock on transaction 67890
```

### Prevention

Always acquire locks in the same order across all transactions.

```typescript
async function transfer(fromId: number, toId: number, amount: number): Promise<void> {
  // Always lock lower ID first — prevents deadlock regardless of call order
  const [firstId, secondId] = fromId < toId ? [fromId, toId] : [toId, fromId]

  const client = await pool.connect()
  try {
    await client.query('BEGIN')
    await client.query('SELECT 1 FROM accounts WHERE id = $1 FOR UPDATE', [firstId])
    await client.query('SELECT 1 FROM accounts WHERE id = $1 FOR UPDATE', [secondId])
    await client.query('UPDATE accounts SET balance = balance - $1 WHERE id = $2', [amount, fromId])
    await client.query('UPDATE accounts SET balance = balance + $1 WHERE id = $2', [amount, toId])
    await client.query('COMMIT')
  } catch (err) {
    await client.query('ROLLBACK')
    throw err
  } finally {
    client.release()
  }
}
```

---

## Distributed Transactions

When data spans multiple databases or services, ACID guarantees break down. Each database provides local ACID but there is no global coordinator enforcing cross-service atomicity.

### Two-Phase Commit (2PC)

Coordinator asks all participants to "prepare" (vote yes/no), then commits if all voted yes.

**Phase 1 (Prepare)**: coordinator sends PREPARE to all participants. Each participant writes to its local WAL and votes YES (can commit) or NO (must abort).

**Phase 2 (Commit/Abort)**: if all voted YES, coordinator sends COMMIT to all. If any voted NO, sends ABORT.

**Problems**: blocking protocol — if the coordinator crashes after Phase 1, participants hold locks indefinitely waiting for Phase 2. Rarely worth it in microservices. 2PC is used in distributed SQL databases (Spanner, CockroachDB) where it's hidden and well-optimized.

### Saga Pattern

Break a distributed transaction into a sequence of **local transactions**. Each step publishes an event or message. If a step fails, previously completed steps are reversed via **compensating transactions**.

```
Order Service:    1. CREATE order (status=pending)       → emit OrderCreated
Payment Service:  2. CHARGE card                         → emit PaymentCompleted
                     on fail → emit PaymentFailed
Inventory:        3. RESERVE stock                       → emit StockReserved
                     on fail → emit ReservationFailed
Order Service:    4. UPDATE order (status=confirmed)

Compensations:
PaymentFailed     → Order Service: CANCEL order (status=cancelled)
ReservationFailed → Payment Service: REFUND charge
                  → Order Service: CANCEL order
```

**Choreography**: services react to events directly (no central coordinator). Each service subscribes to events it cares about and knows what compensating action to take. Loose coupling, but hard to trace the overall flow.

**Orchestration**: a central saga orchestrator tells each service what to do and handles failures. Easier to reason about, visualize, and debug. Use orchestration unless you have a specific reason not to.

```typescript
// Orchestration example: order saga orchestrator
class OrderSaga {
  async execute(orderId: string, userId: string, amount: number): Promise<void> {
    try {
      await this.orderService.createOrder(orderId, userId)
      await this.paymentService.charge(userId, amount, orderId)
      await this.inventoryService.reserve(orderId)
      await this.orderService.confirm(orderId)
    } catch (err) {
      await this.compensate(orderId, userId, amount, err)
      throw err
    }
  }

  private async compensate(
    orderId: string,
    userId: string,
    amount: number,
    originalError: unknown
  ): Promise<void> {
    // Run compensations in reverse order, swallow errors (best-effort)
    await this.inventoryService.release(orderId).catch(() => {})
    await this.paymentService.refund(userId, amount, orderId).catch(() => {})
    await this.orderService.cancel(orderId).catch(() => {})
  }
}
```

Sagas do NOT provide isolation — another process can read an order in `pending` state while the saga is in progress. For true isolation, use pessimistic locking at each step or accept eventual consistency.

---

## Outbox Pattern

Eliminates the **dual-write problem**: writing to a database AND publishing to a message broker in the same operation. If the DB write succeeds but the broker publish fails (or vice versa), data is inconsistent.

### The Problem

```typescript
// WRONG: not atomic — what if the publish fails after the DB write?
await db.query('INSERT INTO orders ...')   // success
await messageBroker.publish('order.created', event)  // fails! Event lost.
```

### The Solution

Write the event to an `outbox` table in the **same database transaction** as the business data. A separate poller reads unprocessed outbox rows and publishes them.

```sql
CREATE TABLE outbox (
  id          BIGSERIAL PRIMARY KEY,
  event_type  TEXT NOT NULL,
  payload     JSONB NOT NULL,
  published   BOOLEAN DEFAULT FALSE,
  created_at  TIMESTAMPTZ DEFAULT NOW()
);
```

```typescript
// Write business data + outbox entry in one transaction
async function createOrder(order: Order): Promise<void> {
  const client = await pool.connect()
  try {
    await client.query('BEGIN')
    const { rows } = await client.query(
      'INSERT INTO orders (user_id, total) VALUES ($1, $2) RETURNING id',
      [order.userId, order.total]
    )
    const orderId = rows[0].id

    // Outbox entry: same transaction as the order insert
    await client.query(
      'INSERT INTO outbox (event_type, payload) VALUES ($1, $2)',
      ['order.created', JSON.stringify({ orderId, userId: order.userId, total: order.total })]
    )

    await client.query('COMMIT')
  } catch (err) {
    await client.query('ROLLBACK')
    throw err
  } finally {
    client.release()
  }
}

// Separate poller: reads outbox and publishes (at-least-once delivery)
async function processOutbox(): Promise<void> {
  const { rows } = await pool.query(
    'SELECT * FROM outbox WHERE published = FALSE ORDER BY id LIMIT 100'
  )

  for (const row of rows) {
    await messageBroker.publish(row.event_type, row.payload)
    await pool.query('UPDATE outbox SET published = TRUE WHERE id = $1', [row.id])
  }
}
```

The poller provides **at-least-once delivery** — if it crashes after publishing but before updating `published = TRUE`, it will republish on next run. Make your consumers idempotent (ignore duplicate event IDs).

For high throughput, use **Debezium** or **logical replication** to capture outbox table changes via CDC instead of polling.

---

## Interview Questions

**Q: If two transactions both read a value then update it, what problem can occur?**
Lost update — both read `balance=100`, both compute `100-50=50`, both write 50. Net result: 50 instead of 0. Fix with `SELECT FOR UPDATE` (pessimistic) or `WHERE balance = $original_value AND version = $original_version` (optimistic locking with retry).

**Q: Why don't microservices use 2PC?**
2PC requires all participants to be available and hold locks until the protocol completes. In a microservices system, any service can be temporarily unavailable. The blocking nature of 2PC means a coordinator crash can lock your entire system indefinitely. Sagas are preferred because each step commits independently and failures are handled through compensation.

**Q: What is "phantom read" and when does it matter?**
Phantom read: a transaction runs the same SELECT twice and gets different rows because another transaction inserted rows matching the WHERE clause. Matters for integrity checks like "is a username taken?" — you check, another transaction inserts the same username, you both think you're first. SERIALIZABLE isolation prevents this.

**Q: What is the outbox pattern and why does it exist?**
The outbox pattern solves the dual-write problem: you can't atomically write to a database AND publish to a message broker. The solution is to write a record to an `outbox` table in the same transaction as your business data, then have a separate process poll the outbox and publish events. This guarantees that an event is published if and only if the corresponding database change committed.

**Q: What is the difference between saga choreography and orchestration?**
Choreography: each service listens for events and reacts by publishing its own events. No central coordinator — services are loosely coupled but the overall flow is implicit and hard to observe. Orchestration: a single saga orchestrator calls each service in sequence and handles failures explicitly. Easier to trace, debug, and change the flow. Orchestration is generally preferred for complex, multi-step sagas.

**Q: What happens to ACID properties in a microservices architecture?**
Atomicity breaks: there's no global transaction spanning multiple services. Consistency becomes eventual: each service is consistent internally, but cross-service consistency is achieved over time through compensating transactions and retries. Isolation is weakened: intermediate states are visible to other services (a saga step may be partially complete). Durability remains per-service: each service's local database is durable. The trade-off is accepted for the scalability and fault-isolation benefits of microservices.

---

## Related

- [PostgreSQL](../postgresql/README.md) — MVCC internals, isolation levels, EXPLAIN ANALYZE
- [SQL](../sql/README.md) — SELECT FOR UPDATE, upserts, CTEs
- [Redis](../redis/README.md) — MULTI/EXEC, distributed locks, Lua scripting
- [Queues](../../02-backend/queues/README.md) — outbox pattern with message brokers
- [Authentication](../../02-backend/authentication/README.md) — session transactions
- [System Design Fundamentals](../../11-system-design/fundamentals/README.md) — CAP theorem, consistency models
