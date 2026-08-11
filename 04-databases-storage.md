# 04 — Databases & Storage

## MongoDB

MongoDB stores BSON documents in collections.

Know:

- document design
- embedding vs referencing
- indexes
- compound indexes
- aggregation
- transactions
- replication
- sharding
- read/write concerns
- Mongoose schemas/models

### Indexes

Indexes speed up reads by avoiding unnecessary scans, but they consume memory/storage and add write/update cost because the index must be maintained.

For a compound index, field order matters. Design it from actual query patterns and filtering/sorting requirements.

### Resume connection

You reduced average API latency from ~700ms to ~150ms using composite MongoDB/Mongoose indexes and Redis caching on hot read paths.

Be ready for:

- How did you identify the slow query?
- Why a composite index?
- What fields were in it?
- How did you verify index usage?
- What is the trade-off of adding indexes?
- What did Redis cache?
- How did you handle stale data?
- What happens if Redis fails?

---

## PostgreSQL / SQL

Know:

- primary/foreign keys
- joins
- indexes
- transactions
- ACID
- isolation levels
- constraints
- normalization
- query plans
- CTEs
- aggregations
- window functions

### Transaction

```text
BEGIN
  -> operation 1
  -> operation 2
  -> COMMIT
```

If a transaction cannot complete safely, rollback should leave the database in a valid state.

Know isolation concepts:

- dirty reads
- non-repeatable reads
- phantom reads
- serializability

---

## Redis

Redis is an in-memory data store with structures such as:

- strings
- hashes
- lists
- sets
- sorted sets
- streams

Use cases:

- caching
- rate limiting
- sessions
- token state
- Pub/Sub
- queues
- distributed coordination

### Cache-aside

```text
Read
 |
Redis hit -> return
 |
miss
 |
DB -> Redis -> return
```

Know cache invalidation, TTL, cache stampede, stale data, and what happens if Redis becomes unavailable.

---

## GraphQL

GraphQL provides a typed schema through which clients request the fields they need.

Know:

- schema
- query
- mutation
- subscription
- resolver
- variables
- fragments
- introspection
- authorization
- N+1 problem

### REST vs GraphQL

REST exposes resource endpoints. GraphQL gives clients more control over the requested shape but introduces schema/resolver complexity and can make query cost harder to control.

---

## pgvector

pgvector adds vector storage and similarity search to PostgreSQL.

This is valuable when application data and semantic retrieval need to live in the same durable database ecosystem.

Know:

- vector columns
- embeddings
- cosine similarity / inner product / distance
- approximate nearest-neighbor indexes at a high level
- metadata filtering
- hybrid search

### Resume connection

You implemented hybrid RAG retrieval over PostgreSQL/pgvector, combining vector search with keyword search to ground model responses in durable, queryable context.

---

## Vector databases

Understand the distinction between:

- relational database with vector extension
- dedicated vector database
- document database with vector capability

Choose based on:

- existing data model
- filtering needs
- scale
- operational simplicity
- query latency
- retrieval features

---

## Neo4j / Graph databases

Graph databases model relationships directly:

```text
User --OWNS--> Agent --USES--> Tool
```

Useful when relationship traversal is central to the workload.

---

## ChromaDB / SurrealDB

Know what problem each system is designed to solve and be able to compare it against PostgreSQL/pgvector, MongoDB, and graph databases rather than memorizing product names.

---

## Interview questions

- When would you choose MongoDB over PostgreSQL?
- Why can an index slow writes?
- How do composite indexes work?
- What causes a slow query?
- What is a transaction?
- Explain ACID.
- What would you cache in Redis?
- How do you prevent cache stampede?
- Redis vs Kafka?
- Why use pgvector instead of a separate vector database?
- How does hybrid search improve retrieval?
