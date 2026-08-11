# 09 — System Design

## Interview framework

Use this sequence:

### 1. Clarify requirements

Functional:

- What must the system do?

Non-functional:

- scale
- latency
- availability
- consistency
- security
- durability

### 2. Estimate scale

Estimate users, requests/sec, peak traffic, storage, bandwidth, and concurrent connections.

### 3. Define APIs

```text
POST /resource
GET  /resource/:id
```

### 4. Define data model

Choose SQL, MongoDB, Redis, object storage, vector storage, or graph storage based on access patterns.

### 5. Draw architecture

```text
Client
  |
Gateway / Load Balancer
  |
API servers
  |
  +---- Redis
  +---- Database
  +---- Queue
           |
         Workers
           |
     External services
```

### 6. Discuss scaling

- horizontal scaling
- load balancing
- caching
- queues
- indexes
- read replicas
- sharding
- autoscaling

### 7. Discuss failures

- timeouts
- retries/backoff
- idempotency
- circuit breakers
- DLQs
- degraded mode

### 8. Security

- authentication
- authorization
- secrets
- encryption
- validation
- rate limits
- audit logs

### 9. Observability

- logs
- metrics
- traces
- alerts
- request/correlation IDs

---

## Design: distributed rate limiter

Requirements:

- per-user/IP limits
- ~200 req/s target
- shared state across API instances
- burst handling

Architecture:

```text
Clients
   |
Load Balancer
   |
API instances
   |
Redis token bucket
```

Questions to answer:

- How do you make updates atomic?
- What happens if Redis is unavailable?
- What is burst capacity?
- How do you avoid synchronized retries?

---

## Design: durable job processing

```text
API
 |
Queue
 |
Workers
 |
Database / External API
```

Add:

- retries
- exponential backoff
- idempotency
- DLQ
- worker concurrency
- monitoring
- graceful shutdown

This maps directly to Bull/Kafka experience.

---

## Design: AI inference platform

Requirements might include:

- multiple providers
- streaming
- failover
- tool calling
- rate limits
- cost tracking
- reproducibility
- observability

Possible architecture:

```text
Client
  |
API Gateway
  |
Inference API
  |
Model Router
  |-------- OpenAI
  |-------- Fireworks
  |-------- Groq
  |-------- DeepSeek
  |
Tool Orchestrator
  |
Queue / Workers
  |
Persistence + Observability
```

Important issue: failover is safe for a pure inference call more often than for a tool call with side effects. A retry may create duplicate external actions unless the action is idempotent.

---

## Design: RAG service

```text
Documents
   |
Chunk + Embed
   |
PostgreSQL + pgvector
   |
Question
   |
Hybrid retrieval
   |
Rerank/filter
   |
Context
   |
LLM
```

Discuss:

- ingestion reliability
- chunking
- metadata
- vector/keyword retrieval
- recall
- latency
- cache
- evaluation
- access control

---

## Design: payment webhook system

```text
Stripe/Coinbase
       |
Webhook API
       |
Verify signature
       |
Dedup key
       |
Queue
       |
Worker
       |
Subscription DB
```

Requirements:

- duplicate-safe
- retryable
- durable
- auditable
- fast acknowledgement

This is a strong resume-driven system design topic.

---

## High-value system-design follow-ups

- What happens at 10x traffic?
- What if Redis goes down?
- What if the database becomes the bottleneck?
- How do you prevent duplicate work?
- How do you deploy without downtime?
- How do you detect a bad release?
- How do you rollback?
- How do you handle regional failure?
- What state must be shared?
- What can be eventually consistent?
