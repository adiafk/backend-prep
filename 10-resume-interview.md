# 10 — Resume-Based Interview Preparation

This section turns the supplied resume into interview stories and deep technical follow-ups.

## Resume summary

Backend engineer with ~2 years of experience focused on AI model evaluation infrastructure and distributed systems. Core strengths include Node.js/NestJS, queues, durable storage, IAM/access control, observability, reliability, C++, TypeScript, JavaScript, and SQL.

---

# 1. Deterministic AI Evaluation Harness

### Resume claim

Built deterministic eval/test harnesses that grade AI model outputs against fixed check suites and produce reproducible pass/fail scores.

### Simple explanation

An evaluation harness is an automated test system for model behavior. Instead of manually judging every response, it runs known test cases through a model and applies consistent evaluation rules.

### Architecture

```text
Test Suite
   |
Test Runner
   |
Model Adapter
   |
LLM / Agent
   |
Output Normalization
   |
Evaluator
   |
Score + Regression Report
```

### Interview questions

**Q: Why is determinism difficult with LLMs?**

Because model generation can vary due to sampling and provider/model behavior. Reproducibility requires controlling test inputs, evaluation logic, configuration, and as many model/provider variables as possible.

**Q: How do you test a nondeterministic model?**

Separate deterministic infrastructure from probabilistic behavior. Use fixed datasets/check suites, stable evaluation rules, constrained generation where available, statistical thresholds where exact matching is inappropriate, and track model/provider versions.

**Q: What would make an eval flaky?**

Changing prompts, model versions, provider behavior, external data, tool outputs, random sampling, unstable test fixtures, or an evaluator that is itself nondeterministic.

### Follow-ups

- How do you compare model versions?
- How do you evaluate tool calls?
- Exact match vs semantic evaluation?
- How would you prevent regression in production?
- How do you store evaluation results?

---

# 2. Tool-call Orchestration / Agent Workflows

### Resume claim

Built APIs driving agent workflows and tool-call orchestration.

### Core design

```text
User Request
    |
Agent / Planner
    |
Tool selection
    |
Validate arguments
    |
Execute tool
    |
Validate result
    |
Continue / respond
```

### What can go wrong?

- malformed arguments
- unavailable tools
- timeout
- unauthorized action
- malicious tool output
- duplicate side effects
- infinite loops
- provider failure

### Strong answer

Treat tools as privileged APIs, not arbitrary functions. Define strict schemas, authorize every action, validate inputs/outputs, apply timeouts and retries, trace calls, and make side-effecting actions idempotent where possible.

### Follow-ups

- How do you prevent prompt injection?
- How do you limit tool permissions?
- What if a tool call succeeds but the agent times out?
- How do you handle parallel tools?
- How do you store agent state?

---

# 3. Multi-provider LLM Routing and Failover

### Resume claim

Implemented routing across OpenAI, Fireworks, Groq, and DeepSeek with automatic provider failover.

### Architecture

```text
Request
  |
Model Router
  |
Provider Adapter
  |
Provider A
  |
 failure/timeout
  v
Provider B
```

### What should the router consider?

- model capability
- latency
- price
- rate limit
- availability
- context length
- streaming support
- structured-output support

### Critical interview question

**When is automatic failover unsafe?**

For pure inference, retrying another provider may be safe if the request is idempotent. For a tool call that creates an order, sends an email, charges a card, or mutates external state, failover can duplicate the side effect unless the operation has an idempotency mechanism.

---

# 4. Hybrid RAG with pgvector

### Resume claim

Implemented hybrid retrieval using PostgreSQL/pgvector, combining vector and keyword search.

### Pipeline

```text
Documents
 -> clean
 -> chunk
 -> embed
 -> pgvector

Query
 -> vector search
 -> keyword search
 -> combine/rank
 -> context
 -> LLM
```

### Questions

- Why not vector search alone?
- What is recall?
- How do embeddings work?
- Explain cosine similarity.
- How do you choose chunk size?
- What happens if the correct chunk is not retrieved?
- How do you evaluate retrieval quality?

### Strong answer

Hybrid retrieval handles two different signals: semantic similarity and lexical exactness. Vector search is good for conceptual matches, while keyword search is often stronger for exact names, identifiers, and rare terms.

---

# 5. MongoDB + Redis Optimization

### Resume claim

Reduced average API response time from ~700ms to ~150ms using composite MongoDB indexes and Redis caching.

### Interview story structure

```text
Problem
 -> measure
 -> identify hot path
 -> inspect query
 -> add correct composite index
 -> cache hot reads
 -> benchmark
 -> verify production impact
```

### Questions

**Q: Why did the index help?**

A suitable index can let MongoDB locate matching records without scanning large portions of the collection.

**Q: Why a composite index?**

Because the query pattern filtered/sorted by multiple fields. The index must reflect the access pattern and field ordering matters.

**Q: What is the cost of indexing?**

Extra storage and write/update overhead. Too many indexes can also increase memory pressure.

**Q: Why Redis?**

To avoid repeated expensive database reads for hot data where some staleness was acceptable.

### Follow-ups

- Cache invalidation?
- Cache stampede?
- Redis outage?
- How did you benchmark?
- p95/p99 latency?
- Did the optimization improve throughput as well?

---

# 6. Bull Queues + Kafka / Pub-Sub

### Resume claim

Built event-driven distributed pipelines for batched distribution with automatic retry and failure recovery.

### Architecture

```text
Producer
   |
Queue / Kafka
   |
Consumer workers
   |
External service / DB
```

### Why asynchronous processing?

It prevents slow downstream work from blocking the request path, provides buffering during traffic spikes, and allows controlled worker concurrency/retries.

### Follow-ups

- At-least-once vs exactly-once?
- How do you handle duplicates?
- What if a worker crashes?
- How do you monitor consumer lag?
- What is a DLQ?
- How do you preserve ordering?
- How do you prevent downstream overload?

---

# 7. Token-bucket Rate Limiting

### Resume claim

Implemented token-bucket rate limiting stable around ~200 req/s.

### Model

```text
Bucket capacity = burst limit
Refill rate = allowed sustained rate
Each request consumes tokens
```

If no token is available, reject or delay the request according to policy.

### Questions

- Why token bucket?
- What is burst capacity?
- How do multiple API instances share state?
- Why Redis?
- How do you make updates atomic?
- What happens if Redis is unavailable?

---

# 8. JWT + Refresh Rotation + Sessions

### Resume claim

Implemented JWT access tokens with Redis-backed refresh-token rotation, Google OAuth 2.0, and concurrent-session handling.

### Interview flow

```text
Login
 -> access token
 -> refresh token

Refresh
 -> validate
 -> rotate
 -> issue new pair
 -> invalidate previous
```

### New-device login

If the product requires one active session/device policy, the backend can identify the previous session and invalidate stale session/token state in Redis/database.

### Follow-ups

- Why short-lived access tokens?
- How do you revoke tokens?
- What if a refresh token is stolen?
- How does PKCE work?
- How do you handle multiple devices?

---

# 9. Payment Webhooks

### Resume claim

Built idempotent Stripe/Coinbase Commerce webhooks using deduplication keys to prevent double charges on retried events.

### Correct architecture

```text
Payment provider
      |
Webhook endpoint
      |
Verify signature
      |
Check event/dedup key
      |
Persist/enqueue
      |
Worker
      |
Update subscription/payment state
```

### Core insight

Webhook delivery is generally at-least-once from the application's perspective. The same event may arrive more than once. Therefore processing must be idempotent.

### Follow-ups

- What if the webhook endpoint times out after processing?
- What if two duplicate events arrive simultaneously?
- How do you atomically create the dedup record?
- How do you retry failed processing?
- How do you audit payment state?

---

# 10. AWS / Linode / PM2 / CI-CD

### Resume claim

Ran deployment and infrastructure automation using GitHub Actions and PM2 on AWS EC2/S3 and Linode.

### Deployment pipeline

```text
Git push
 -> CI
 -> tests
 -> build
 -> deploy
 -> health check
 -> rolling replacement
 -> verify
```

### Questions

- How do you deploy without downtime?
- How do you rollback?
- Where are secrets stored?
- What if a health check fails?
- How do you know a release is bad?
- PM2 vs Kubernetes?
- EC2 vs EKS?

---

# 11. Observability / Incident Response

### Resume claim

Used Sentry and Grafana for monitoring, error tracking, and incident response.

Know the three pillars:

- logs
- metrics
- traces

Useful production metrics:

- request rate
- error rate
- p50/p95/p99 latency
- CPU/memory
- DB latency
- cache hit rate
- queue depth
- consumer lag
- external provider error rate

### Incident answer template

```text
Detect
 -> quantify impact
 -> identify scope
 -> mitigate
 -> find root cause
 -> fix
 -> verify
 -> prevent recurrence
```

---

# 12. Resume Cross-Questioning

Interviewers may connect multiple bullets together.

### Example

**"You improved latency with Redis. How would that change if traffic grew 10x?"**

Expected areas:

- horizontal API scaling
- shared Redis
- cache capacity
- database connection pools
- DB read replicas
- load balancing
- rate limiting
- queueing
- observability

### Example

**"Your LLM provider fails over. How do you know the second provider is equivalent?"**

Discuss capability metadata, model behavior differences, structured output compatibility, prompt differences, cost/latency, and evaluation/regression testing.

### Example

**"Your webhook is idempotent. Is the whole payment flow exactly once?"**

No. The application should provide idempotent effects and durable state transitions, but distributed systems rarely provide magical exactly-once end-to-end execution. Explain the boundaries and guarantees.

---

# 13. Behavioral + Technical Stories

Prepare 5 stories:

1. Hardest technical problem
2. Biggest performance improvement
3. Production incident/failure
4. Architecture/design decision
5. Feature where you disagreed or changed the approach

Use:

```text
Situation
Task
Action
Result
What I learned
```

Keep the technical detail ready for follow-ups.

---

# 14. Final Resume Checklist

You should be able to explain each of these without notes:

- deterministic eval harness
- reproducibility
- model evaluation
- agent workflow
- tool calling
- multi-provider routing
- provider failover
- hybrid RAG
- pgvector
- embeddings
- retrieval recall
- MongoDB indexing
- Redis caching
- latency measurement
- queues
- Kafka/Pub-Sub
- retries
- failure recovery
- token bucket
- JWT
- refresh rotation
- OAuth 2.0
- session invalidation
- payment webhooks
- idempotency
- EC2
- S3
- Linode
- PM2
- GitHub Actions
- rolling deployments
- Sentry
- Grafana
- disaster recovery
- C++ fundamentals
- TypeScript
- SQL
