# 03 — Backend & Distributed Systems

## Monolith vs Microservices

A monolith keeps application components in one deployable unit. Microservices split capabilities into independently deployable services.

### Monolith advantages

- simpler development/deployment
- easier local debugging
- simpler transactions
- lower operational overhead

### Microservice advantages

- independent scaling
- independent deployments
- clearer ownership boundaries

### Microservice costs

- network failures
- distributed tracing
- service discovery
- data consistency
- operational complexity

The right interview answer is based on requirements, not ideology.

---

## Queues

A queue decouples producers from consumers and is useful when work can be processed asynchronously.

```text
API -> Queue -> Worker -> DB / External API
```

Know:

- acknowledgement
- visibility/lease timeout
- retries
- dead-letter queue
- concurrency
- ordering
- backpressure
- at-least-once delivery
- duplicate processing

---

## Bull / BullMQ

Redis-backed job queues are useful for long-running or retryable work such as emails, AI jobs, webhooks, notifications, and scheduled tasks.

Typical flow:

```text
Request
  |
Add job
  |
Redis queue
  |
Worker
  |
External API / Database
```

Important settings/concepts:

- worker concurrency
- delayed jobs
- retry/backoff
- failed jobs
- job deduplication
- graceful shutdown

### Resume connection

You built event-driven pipelines using Bull queues and Kafka/Pub-Sub with automatic retry and failure recovery. Be ready to explain why the queue exists, what happens when a worker crashes, and how duplicate jobs are handled.

---

## Kafka / Pub-Sub

Kafka is a durable distributed event-streaming platform. Pub/Sub describes a messaging pattern where publishers emit events and subscribers consume them.

Know:

- topic
- partition
- consumer group
- offset
- ordering within a partition
- retention
- replay
- consumer lag

### Queue vs Pub/Sub

Queue-style processing generally distributes work among consumers; Pub/Sub lets multiple subscribers receive the event.

---

## Event-driven architecture

Instead of synchronously calling every downstream component, a service can publish an event:

```text
User Created
   |
   +--> Email service
   +--> Analytics
   +--> Rewards
   +--> Notifications
```

Benefits:

- loose coupling
- asynchronous work
- independent consumers

Costs:

- eventual consistency
- ordering
- duplicates
- debugging complexity
- schema evolution

---

## Retry and Failure Recovery

Distributed systems fail partially. A dependency may be slow, unavailable, or return an ambiguous result.

Use:

- timeout
- bounded retries
- exponential backoff
- jitter
- idempotency
- circuit breakers where appropriate
- dead-letter queues
- durable failure state

### The ambiguous outcome problem

Suppose a payment provider processes the payment but your request times out. Retrying blindly can charge twice. The solution is an idempotency key and provider-side or application-side deduplication.

---

## Scaling

### Vertical scaling

Give one machine more CPU/RAM.

### Horizontal scaling

Add more application instances:

```text
             Load Balancer
            /     |      \
         API-1  API-2   API-3
```

Horizontal scaling requires shared state to live in systems accessible to all instances, such as Redis or a database.

---

## Distributed Systems Concepts

Know:

- partial failure
- timeouts
- consistency
- availability
- partition tolerance
- CAP theorem
- eventual consistency
- distributed locks
- leader election
- service discovery
- observability

### Interview question

**Q: Why can a system fail even when every server is healthy?**

**Answer:** Because the network itself can fail: packets can be delayed/dropped, connections can time out, DNS can fail, dependencies can become overloaded, or partitions can isolate healthy components.

---

## Webhook reliability

A reliable webhook consumer should:

1. authenticate/verify the event
2. persist or enqueue it
3. deduplicate using event ID/key
4. acknowledge quickly
5. process asynchronously
6. retry transient failures
7. retain failures for investigation

---

## Follow-up questions

- At-least-once vs exactly-once delivery?
- How do you preserve ordering?
- What if a consumer crashes after processing but before acknowledgement?
- How do you replay events?
- How do you handle poison messages?
- When would you choose Kafka over a Redis-backed queue?
- How do you scale workers without overwhelming a downstream API?
