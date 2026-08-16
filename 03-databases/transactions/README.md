# Transactions & Distributed Data

## ACID

**Atomicity** — All operations in a transaction succeed or all fail. No partial writes.

**Consistency** — A transaction brings the database from one valid state to another. Constraints, triggers, and cascades are enforced.

**Isolation** — Concurrent transactions do not interfere with each other. What level of interference is allowed depends on the isolation level.

**Durability** — Once committed, the data survives crashes. Achieved via WAL (Write-Ahead Log).

---

## Isolation Levels and What They Prevent

| Isolation Level | Dirty Read | Non-Repeatable Read | Phantom Read |
|---|---|---|---|
| READ UNCOMMITTED | ✓ possible | ✓ possible | ✓ possible |
| READ COMMITTED | ✗ prevented | ✓ possible | ✓ possible |
| REPEATABLE READ | ✗ | ✗ | ✓ possible |
| SERIALIZABLE | ✗ | ✗ | ✗ |

**Dirty read**: reading data written by an uncommitted transaction (that may roll back).

**Non-repeatable read**: reading the same row twice within a transaction and getting different values because another transaction updated it in between.

**Phantom read**: a query returning different rows on second execution because another transaction inserted or deleted rows matching the WHERE clause.

PostgreSQL default is **READ COMMITTED**. For financial operations, use **SERIALIZABLE** or handle conflicts explicitly with optimistic locking.

---

## Optimistic vs Pessimistic Locking

### Pessimistic Locking
Lock the row when you read it. Other writers block.

```sql
BEGIN;
SELECT * FROM accounts WHERE id = $1 FOR UPDATE;  -- acquires row lock
-- ... do work ...
UPDATE accounts SET balance = ... WHERE id = $1;
COMMIT;
```

Use when: write conflicts are frequent, work inside the transaction is fast.

### Optimistic Locking
Don't lock. Check that the row hasn't changed when writing. If it has, retry.

```sql
-- Read with version
SELECT id, balance, version FROM accounts WHERE id = $1;

-- Update only if version matches (retry if 0 rows affected)
UPDATE accounts
SET balance = $2, version = version + 1
WHERE id = $1 AND version = $3;
```

Use when: write conflicts are rare, transactions span user think time (you can't hold a DB lock while the user fills a form).

---

## Deadlocks

A deadlock occurs when two transactions each hold a lock the other needs.

```
Transaction A: locks row 1, waits for row 2
Transaction B: locks row 2, waits for row 1
→ Neither can proceed
```

PostgreSQL detects deadlocks and rolls back one transaction with error code `40P01`.

**Prevention**: always acquire locks in the same order across transactions.

```typescript
// Consistent order: always lock lower ID first
const [fromId, toId] = amount > 0
  ? [Math.min(a, b), Math.max(a, b)]
  : [Math.max(a, b), Math.min(a, b)]
```

---

## Distributed Transactions

When data spans multiple databases or services, ACID guarantees break down.

### Two-Phase Commit (2PC)
Coordinator asks all participants to "prepare" (vote yes/no), then commits if all voted yes.

**Problems**: blocking protocol (coordinator failure leaves participants blocked), slow, rarely worth it in microservices.

### Saga Pattern
Break a distributed transaction into a sequence of local transactions. Each step publishes an event. If a step fails, compensating transactions undo prior steps.

```
Order Service:    CREATE order (status=pending)
Payment Service:  CHARGE card  → on fail → emit PaymentFailed
Inventory:        RESERVE stock → on fail → emit ReservationFailed
Order Service:    UPDATE order (status=confirmed)

Compensations:
PaymentFailed     → ORDER Service: CANCEL order
ReservationFailed → PAYMENT Service: REFUND charge
```

**Choreography**: services react to events directly (no central coordinator).
**Orchestration**: a central saga orchestrator tells each service what to do.

Orchestration is easier to reason about and debug. Use choreography when loose coupling is more important than observability.

---

## Interview Questions

**Q: If two transactions both read a value then update it, what problem can occur?**
Lost update — both read `balance=100`, both compute `100-50=50`, both write 50. Net result: 50 instead of 0. Fix with `SELECT FOR UPDATE` (pessimistic) or `WHERE balance = $original_value` (optimistic locking with retry).

**Q: Why don't microservices use 2PC?**
2PC requires all participants to be available and to hold locks until the protocol completes. In a microservices system, any service can be temporarily unavailable. The blocking nature of 2PC means a coordinator failure can lock your entire system. Sagas are preferred because each step commits independently and failures are handled through compensation.

**Q: What is "phantom read" and when does it matter?**
Phantom read: a transaction runs the same SELECT twice and gets different rows because another transaction inserted rows that match the WHERE clause. Matters for integrity checks like "is a username taken?" — you check, another transaction inserts the same username, you both think you're first. Serializable isolation prevents this.
