# Developer Engineering Preparation Roadmap

A structured 12-week program to build deep, interview-ready engineering knowledge across backend systems, AI/ML, and full-stack development.

---

## Table of Contents

1. [Overview](#overview)
2. [12-Week Learning Plan](#12-week-learning-plan)
   - [Weeks 1-2: Foundations + HTTP](#weeks-1-2-foundations--http)
   - [Weeks 3-4: Databases](#weeks-3-4-databases)
   - [Weeks 5-6: Auth + Security](#weeks-5-6-auth--security)
   - [Weeks 7-8: AI/ML/RAG](#weeks-7-8-aimlrag)
   - [Weeks 9-10: System Design](#weeks-9-10-system-design)
   - [Weeks 11-12: DSA Intensive](#weeks-11-12-dsa-intensive)
3. [Specialized Tracks](#specialized-tracks)
   - [Backend Engineer Track](#backend-engineer-track)
   - [AI/ML Engineer Track](#aiml-engineer-track)
   - [Full-Stack Track](#full-stack-track)
4. [Resources](#resources)
5. [Milestone Checklist](#milestone-checklist)

---

## Overview

This roadmap is designed for engineers preparing for technical interviews and senior-level roles. Each two-week phase is self-contained with clear deliverables, projects, and review checkpoints. The program assumes 2-3 hours of focused daily study.

**Prerequisites:** Comfort with at least one programming language (Python, JavaScript, Go, or Java), basic command-line usage, and version control with Git.

---

## 12-Week Learning Plan

### Weeks 1-2: Foundations + HTTP

**Goal:** Build a reliable mental model of how the web works at the protocol level, and sharpen your programming fundamentals.

#### Week 1 — Programming Foundations

- Review Python or JavaScript runtimes: the event loop, call stack, heap memory
- Understand concurrency primitives: threads, processes, async/await, callbacks
- Practice writing clean, idiomatic code: type annotations, error handling, logging
- Study the OSI model layers 1-7 with focus on layers 3 (IP), 4 (TCP/UDP), 7 (HTTP)
- Implement a simple TCP socket server from scratch

#### Week 2 — HTTP Deep Dive

- HTTP/1.1 vs HTTP/2 vs HTTP/3: multiplexing, header compression, QUIC
- Request/response lifecycle: DNS resolution, TCP handshake, TLS handshake, request parsing
- HTTP methods, status codes, idempotency, and safe methods
- Headers in depth: Content-Type, Accept, Authorization, CORS preflight, Cache-Control
- RESTful API design principles: resource naming, versioning, pagination, HATEOAS
- Build a REST API server (Express.js or FastAPI) with proper error handling and middleware

**Milestone: Phase 1 Complete when you can:**
- [ ] Explain what happens between typing a URL and seeing a page, step by step
- [ ] Describe the difference between HTTP/2 multiplexing and HTTP/1.1 pipelining
- [ ] Design a clean REST API for a given domain (e.g., a blog or e-commerce system)
- [ ] Implement request validation, rate limiting, and structured logging middleware
- [ ] Pass a mock interview question on API design without referring to notes

---

### Weeks 3-4: Databases

**Goal:** Understand relational and non-relational databases deeply enough to make sound architectural choices and write efficient queries.

#### Week 3 — Relational Databases

- ACID properties: atomicity, consistency, isolation, durability — what each guarantees
- Isolation levels: read uncommitted, read committed, repeatable read, serializable
- Indexing: B-tree vs hash indexes, composite indexes, covering indexes, index selectivity
- Query planning: EXPLAIN/EXPLAIN ANALYZE, sequential scan vs index scan, join strategies
- Transactions and locking: optimistic vs pessimistic concurrency, deadlocks, MVCC
- Schema design: normalization (1NF through 3NF), denormalization trade-offs, foreign keys
- Practice: write 20+ SQL queries covering JOINs, window functions, CTEs, subqueries

#### Week 4 — Non-Relational Databases + Caching

- Document stores (MongoDB): data modeling, aggregation pipeline, sharding, replica sets
- Key-value stores (Redis): data structures (strings, lists, sets, sorted sets, hashes), pub/sub, TTL
- Column-family stores (Cassandra): partition keys, clustering keys, eventual consistency, CAP theorem
- Time-series databases: use cases, retention policies, downsampling
- Caching strategies: cache-aside, read-through, write-through, write-behind, cache eviction (LRU, LFU)
- Database replication: primary-replica, multi-primary, synchronous vs asynchronous replication
- Build a project: an API backed by PostgreSQL with Redis caching, connection pooling via PgBouncer

**Milestone: Phase 2 Complete when you can:**
- [ ] Write complex SQL queries using window functions and CTEs without looking up syntax
- [ ] Explain what MVCC is and how PostgreSQL implements it
- [ ] Choose between relational, document, and key-value storage given a real use case
- [ ] Describe how a Redis sorted set can power a leaderboard system
- [ ] Demonstrate a query optimization (show EXPLAIN output before and after adding an index)

---

### Weeks 5-6: Auth + Security

**Goal:** Implement authentication and authorization correctly, and understand common attack vectors well enough to defend against them in code reviews and system design discussions.

#### Week 5 — Authentication

- Password security: bcrypt/argon2 hashing, salting, PBKDF2, why MD5/SHA1 are insufficient
- Session-based auth: server-side sessions, session fixation, session hijacking, secure/httpOnly cookies
- JWT in depth: header/payload/signature structure, HS256 vs RS256 vs ES256, expiry, refresh tokens
- OAuth 2.0 flows: authorization code, client credentials, device flow, implicit (deprecated)
- OpenID Connect: ID tokens, userinfo endpoint, PKCE extension
- Implement a full auth system: registration, login, JWT issuance, token refresh, logout with token revocation

#### Week 6 — Security

- OWASP Top 10: SQL injection, XSS, CSRF, SSRF, insecure deserialization, broken access control
- Input validation and sanitization: parameterized queries, allowlist vs denylist
- Transport security: TLS 1.2 vs 1.3, certificate pinning, HSTS, mixed content
- Secrets management: environment variables, vault systems (HashiCorp Vault), key rotation
- Rate limiting and brute-force protection: token bucket, leaky bucket, sliding window algorithms
- Security headers: CSP, X-Frame-Options, Referrer-Policy, Permissions-Policy
- Conduct a security audit of your Week 2 REST API and fix all identified vulnerabilities

**Milestone: Phase 3 Complete when you can:**
- [ ] Implement OAuth 2.0 authorization code flow with PKCE from scratch
- [ ] Explain why JWTs cannot be invalidated server-side without additional infrastructure
- [ ] Find and fix an SQL injection vulnerability in a code sample
- [ ] Describe how CSRF attacks work and list three mitigations
- [ ] Configure a Content Security Policy header that blocks common XSS vectors

---

### Weeks 7-8: AI/ML/RAG

**Goal:** Understand the practical engineering behind modern AI systems — how to integrate LLMs into applications, build retrieval-augmented generation pipelines, and evaluate AI-powered features.

#### Week 7 — LLMs and AI Integration

- Transformer architecture fundamentals: attention mechanism, positional encoding, tokenization
- LLM APIs: prompt engineering, system prompts, few-shot prompting, temperature and top-p sampling
- Context windows, token limits, and cost optimization strategies
- Streaming responses: server-sent events, chunked transfer, client-side rendering of streamed output
- Function/tool calling: schema definition, handling tool results, multi-step agentic loops
- Structured output: JSON mode, schema enforcement, output parsing
- Build a project: a CLI assistant that uses tool calling to answer questions about a local codebase

#### Week 8 — RAG Systems + Vector Databases

- Embeddings: what they represent, how to generate them, cosine similarity vs dot product vs Euclidean
- Vector databases: Pinecone, Weaviate, pgvector, Chroma — indexing strategies (HNSW, IVF)
- RAG architecture: document ingestion, chunking strategies, embedding storage, retrieval, generation
- Chunking strategies: fixed-size, sentence-based, recursive, semantic chunking, overlapping windows
- Hybrid search: combining dense vector search with sparse BM25/keyword search
- Evaluation: faithfulness, answer relevance, context recall — using RAGAS or custom eval harnesses
- Reranking: cross-encoders, Cohere Rerank, reciprocal rank fusion
- Build a RAG pipeline: ingest a set of technical documents, retrieve relevant chunks, generate cited answers

**Milestone: Phase 4 Complete when you can:**
- [ ] Explain the difference between RAG and fine-tuning and when to use each
- [ ] Implement a chunking strategy and explain the trade-offs of your choice
- [ ] Set up a vector store, embed documents, and run semantic search
- [ ] Evaluate a RAG pipeline's retrieval quality with at least two metrics
- [ ] Build a tool-calling agent that can read files and answer questions about them

---

### Weeks 9-10: System Design

**Goal:** Develop the ability to design large-scale distributed systems under realistic constraints, and communicate trade-offs clearly during 45-minute design interviews.

#### Week 9 — Distributed Systems Fundamentals

- CAP theorem and PACELC: what consistency and availability trade-offs look like in practice
- Consistency models: eventual consistency, strong consistency, causal consistency, read-your-writes
- Consensus algorithms: Raft and Paxos at a conceptual level, leader election, log replication
- Distributed transactions: two-phase commit, saga pattern, outbox pattern
- Message queues: Kafka (partitions, offsets, consumer groups, compaction) vs RabbitMQ vs SQS
- Service discovery, load balancing: round-robin, least connections, consistent hashing
- Circuit breaker, retry with exponential backoff, bulkhead pattern

#### Week 10 — System Design Practice

- Horizontal scaling patterns: stateless services, sticky sessions, sharding strategies
- Content delivery: CDNs, edge caching, cache invalidation, origin pull vs push
- Observability stack: structured logging, distributed tracing (OpenTelemetry), metrics (Prometheus/Grafana)
- Design patterns for common systems:
  - URL shortener (hashing, redirection, analytics)
  - Rate limiter (token bucket in Redis, distributed rate limiting)
  - Notification system (fan-out, push vs pull, priority queues)
  - Search autocomplete (trie, prefix indexing, ranking)
  - Distributed job scheduler (leader election, at-least-once delivery)
- Practice full 45-minute design sessions with a partner for: Twitter feed, YouTube, Slack, Uber

**Milestone: Phase 5 Complete when you can:**
- [ ] Design a system that handles 100K requests/second with specific latency SLAs
- [ ] Explain the trade-offs between Kafka and a traditional message queue
- [ ] Whiteboard a notification system with fan-out, deduplication, and delivery guarantees
- [ ] Describe how consistent hashing minimizes key remapping when nodes are added
- [ ] Complete a system design question in 45 minutes with clear trade-off discussion

---

### Weeks 11-12: DSA Intensive

**Goal:** Reach the level where you can solve medium LeetCode problems fluently and hard problems with hints, covering the patterns that appear most frequently in senior-level interviews.

#### Week 11 — Core Patterns

- Arrays and strings: sliding window, two pointers, prefix sums, in-place manipulation
- Hashing: frequency counting, anagram detection, two-sum variants, grouping
- Stacks and queues: monotonic stack, next greater element, valid parentheses, queue via stacks
- Binary search: on sorted arrays, on answer space, rotated arrays, first/last occurrence
- Trees: DFS (pre/in/post-order), BFS, level-order traversal, LCA, path problems
- Heaps: top-k elements, merge k sorted lists, median of data stream
- Linked lists: reversal, cycle detection (Floyd's), merge, remove nth from end

Solve 3-5 problems per pattern, focusing on recognizing the pattern before coding.

#### Week 12 — Advanced Patterns + Mock Interviews

- Graphs: BFS/DFS on adjacency list, topological sort (Kahn's + DFS), union-find/DSU
- Dynamic programming: top-down memoization, bottom-up tabulation, 1D DP, 2D DP on grids
- DP patterns: 0/1 knapsack, unbounded knapsack, LCS, LIS, interval DP
- Backtracking: permutations, combinations, subsets, constraint satisfaction (N-Queens, Sudoku)
- Greedy algorithms: interval scheduling, activity selection, Huffman encoding
- String algorithms: KMP pattern matching, rolling hash (Rabin-Karp), trie operations
- Mock interview week: 2 timed coding sessions per day, one medium + one hard, with verbal explanation

**Milestone: Phase 6 Complete when you can:**
- [ ] Identify the correct DSA pattern within 2 minutes of reading a problem
- [ ] Solve sliding window, binary search, and monotonic stack problems without hints
- [ ] Implement Dijkstra's and BFS on a graph from memory
- [ ] Write a clean DP solution with time/space complexity analysis
- [ ] Complete 2 medium LeetCode problems in under 35 minutes total in mock conditions

---

## Specialized Tracks

After completing the 12-week core program, or in parallel if you have a specific role in mind, follow one of these focused tracks to deepen expertise.

---

### Backend Engineer Track

**Duration:** 4 weeks post-core  
**Focus:** High-throughput services, infrastructure, and production engineering

#### Track Curriculum

**Backend Track Week 1 — API Design and Performance**
- gRPC: protobuf schema design, unary vs streaming RPCs, interceptors, deadlines
- GraphQL: schema definition, resolvers, N+1 problem and DataLoader, subscriptions
- API versioning strategies: URL versioning, header versioning, schema evolution
- Performance profiling: CPU/memory profiling in Python and Node.js, flame graphs
- Connection pooling, keep-alive, and HTTP persistent connections

**Backend Track Week 2 — Queueing and Async Patterns**
- Event-driven architecture: event sourcing, CQRS, domain events
- Kafka deep dive: exactly-once semantics, transactional producers, Kafka Streams
- Background job systems: Celery, BullMQ, Sidekiq — job retries, dead letter queues, concurrency
- Webhooks: delivery guarantees, signature verification, fan-out, idempotency keys

**Backend Track Week 3 — Infrastructure and Deployment**
- Docker: multi-stage builds, layer caching, image hardening, non-root users
- Kubernetes: pods, deployments, services, ingress, ConfigMaps, Secrets, HPA
- CI/CD pipelines: GitHub Actions, test parallelism, deployment strategies (blue/green, canary)
- Terraform basics: resource definitions, state management, modules, remote backends

**Backend Track Week 4 — Production Reliability**
- SLIs, SLOs, SLAs: defining and measuring error budgets
- Incident response: runbooks, postmortems, on-call practices
- Chaos engineering: fault injection, dependency failure simulation
- Database migrations in production: zero-downtime migrations, expand/contract pattern

**Track Capstone:** Build a high-throughput event processing service that ingests events via Kafka, processes them with idempotency guarantees, persists to PostgreSQL, exposes a gRPC API, and deploys to Kubernetes with autoscaling and health checks.

---

### AI/ML Engineer Track

**Duration:** 4 weeks post-core  
**Focus:** Production AI systems, model serving, and advanced RAG

#### Track Curriculum

**AI/ML Track Week 1 — ML Fundamentals for Engineers**
- Supervised learning concepts: loss functions, gradient descent, overfitting, regularization
- Model evaluation: precision/recall/F1, ROC-AUC, confusion matrix interpretation
- Feature engineering: encoding, normalization, handling missing data
- Scikit-learn pipelines: preprocessing, cross-validation, hyperparameter tuning

**AI/ML Track Week 2 — LLM Engineering**
- Fine-tuning vs prompt engineering vs RAG: decision framework
- Parameter-efficient fine-tuning: LoRA, QLoRA, adapter layers
- Prompt engineering patterns: chain-of-thought, self-consistency, ReAct, least-to-most
- LLM observability: LangSmith, tracing LLM calls, latency and cost dashboards
- Guardrails: input/output validation, topic restriction, PII detection

**AI/ML Track Week 3 — Advanced RAG**
- Advanced chunking: propositional chunking, late chunking, contextual retrieval
- Multi-vector retrieval: parent document retrieval, hypothetical document embeddings (HyDE)
- Agentic RAG: tool-using agents, self-correcting retrieval, query decomposition
- Knowledge graphs as retrieval layer: graph-based RAG, entity extraction, relationship linking
- RAG evaluation frameworks: RAGAS, TruLens, custom LLM-as-judge pipelines

**AI/ML Track Week 4 — Model Serving and MLOps**
- Model serving infrastructure: vLLM, TGI, Triton Inference Server, batching strategies
- Embedding service design: batch embedding, caching embeddings, versioned embedding indexes
- A/B testing for AI features: traffic splitting, shadow mode, evaluation metrics
- Vector index management: incremental updates, index versioning, deletion handling
- Cost optimization: caching LLM responses, prompt compression, smaller model routing

**Track Capstone:** Build a production-grade RAG system with: a document ingestion pipeline, hybrid search (dense + sparse), an agentic query layer, an LLM-as-judge evaluation harness, and a monitoring dashboard tracking retrieval quality over time.

---

### Full-Stack Track

**Duration:** 4 weeks post-core  
**Focus:** Frontend engineering, end-to-end product development, and modern web performance

#### Track Curriculum

**Full-Stack Track Week 1 — Modern Frontend Architecture**
- React in depth: reconciliation, fiber, hooks internals (useEffect dependency array, closures), concurrent rendering
- State management: Redux Toolkit, Zustand, Jotai — when each is appropriate
- React Server Components and Next.js App Router: server vs client components, streaming, suspense
- TypeScript: generics, conditional types, mapped types, discriminated unions, utility types

**Full-Stack Track Week 2 — Web Performance**
- Core Web Vitals: LCP, FID/INP, CLS — measurement and optimization strategies
- Rendering strategies: SSR, SSG, ISR, CSR — trade-offs per use case
- Bundle optimization: code splitting, tree shaking, dynamic imports, bundle analysis
- Image optimization: formats (WebP, AVIF), responsive images, lazy loading, CDN delivery
- Caching on the frontend: service workers, Cache API, stale-while-revalidate, HTTP cache headers

**Full-Stack Track Week 3 — API Integration Patterns**
- tRPC: end-to-end type safety, router definition, client integration
- React Query / SWR: caching, stale data, background refetch, optimistic updates
- WebSockets and SSE: real-time UI updates, reconnection logic, backpressure
- Form handling: React Hook Form, Zod schema validation, server-side validation mirroring

**Full-Stack Track Week 4 — Full-Stack Production**
- Authentication on the frontend: NextAuth / Clerk, session hydration, protected routes
- End-to-end testing: Playwright test setup, page object model, CI integration
- Accessibility: WCAG 2.1 AA compliance, ARIA roles, keyboard navigation, screen reader testing
- Deployment: Vercel/Netlify edge functions, CDN configuration, environment variable management

**Track Capstone:** Build and deploy a full-stack AI-powered application with: a Next.js frontend with server components, a tRPC API, PostgreSQL persistence, streaming LLM responses, proper auth, and a 90+ Lighthouse performance score.

---

## Resources

### Weeks 1-2: Foundations + HTTP

**Books**
- "Computer Networks: A Top-Down Approach" — Kurose & Ross (Chapters 1-4 for networking fundamentals)
- "High Performance Browser Networking" — Ilya Grigorik (free at hpbn.co) — definitive HTTP/2 and QUIC resource

**Documentation and Guides**
- MDN HTTP documentation: developer.mozilla.org/en-US/docs/Web/HTTP
- RFC 7230-7235 (HTTP/1.1) and RFC 9110 (HTTP Semantics) for protocol-level detail
- FastAPI official docs for modern Python API design patterns

**Practice**
- HTTPie or Insomnia for manual API testing
- Wireshark for inspecting raw TCP and HTTP traffic

---

### Weeks 3-4: Databases

**Books**
- "Designing Data-Intensive Applications" — Martin Kleppmann (essential; covers storage engines, replication, distributed systems)
- "PostgreSQL: Up and Running" — Regina Obe & Leo Hsu (practical PostgreSQL)
- "Redis in Action" — Josiah Carlson

**Documentation and Courses**
- PostgreSQL official documentation, especially the chapter on query planning
- CMU Database Group lectures (YouTube): 15-445/645 Intro to Database Systems by Andy Pavlo
- Use The Index, Luke (use-the-index-luke.com): deep guide to SQL indexing

**Practice**
- LeetCode Database problems (50 problems covering JOINs, subqueries, window functions)
- pgexercises.com for PostgreSQL-specific exercises

---

### Weeks 5-6: Auth + Security

**Books**
- "The Web Application Hacker's Handbook" — Stuttard & Pinto
- "OAuth 2 in Action" — Justin Richer & Antonio Sanso

**Documentation and Guides**
- OWASP Top 10: owasp.org/www-project-top-ten
- OWASP Authentication Cheat Sheet and JWT Security Cheat Sheet
- auth0.com/blog has well-written articles on JWTs, OAuth, and OIDC

**Practice**
- OWASP WebGoat: deliberately vulnerable app for hands-on security practice
- JWT.io debugger for inspecting and crafting tokens

---

### Weeks 7-8: AI/ML/RAG

**Books**
- "Hands-On Large Language Models" — Jay Alammar & Maarten Grootendorst (2024)
- "Building LLMs for Production" — Louis-Francois Bouchard & Loubna Ben Allal

**Documentation and Courses**
- Anthropic Claude API documentation and prompt engineering guide
- LangChain and LlamaIndex documentation for RAG pipeline patterns
- DeepLearning.AI short courses: "Building Systems with the ChatGPT API", "LangChain for LLM Application Development"
- RAGAS documentation (ragas.io) for RAG evaluation

**Papers**
- "Attention Is All You Need" (Vaswani et al., 2017) — the original transformer paper
- "Retrieval-Augmented Generation for Knowledge-Intensive NLP Tasks" (Lewis et al., 2020)
- "REALM: Retrieval-Augmented Language Model Pre-Training" (Guu et al., 2020)

---

### Weeks 9-10: System Design

**Books**
- "Designing Data-Intensive Applications" — Martin Kleppmann (also covers system design)
- "System Design Interview" Volumes 1 and 2 — Alex Xu (interview-focused walkthroughs)
- "Building Microservices" — Sam Newman (2nd edition)

**Resources**
- High Scalability blog (highscalability.com): real-world architecture case studies
- AWS Architecture Center and Google Cloud Architecture Framework
- ByteByteGo newsletter and YouTube channel by Alex Xu

**Practice**
- Pramp and Interviewing.io for live system design mock interviews
- ExcaliDraw or Miro for diagramming during practice sessions

---

### Weeks 11-12: DSA Intensive

**Books**
- "Introduction to Algorithms" (CLRS) — Cormen, Leiserson, Rivest, Stein (reference, not cover-to-cover)
- "Grokking Algorithms" — Aditya Bhargava (approachable intro with visuals)
- "Elements of Programming Interviews" — Aziz, Lee, Prakash (Python, Java, or C++ editions)

**Practice Platforms**
- LeetCode: focus on NeetCode 150 or Blind 75 curated lists
- NeetCode.io: free video solutions and structured roadmap
- AlgoExpert for video walkthroughs of 160 questions

**Interview Prep**
- "Cracking the Coding Interview" — Gayle Laakmann McDowell (behavioral + technical prep)
- Pramp for free peer mock interviews

---

## Milestone Checklist

Use this consolidated checklist to track readiness for technical interviews.

### Technical Knowledge

- [ ] Can explain the full HTTP request lifecycle from URL to response
- [ ] Can write complex SQL with CTEs, window functions, and subqueries
- [ ] Can explain ACID, MVCC, and isolation levels
- [ ] Can implement JWT-based auth with refresh token rotation
- [ ] Can identify and fix OWASP Top 10 vulnerabilities in code review
- [ ] Can build and evaluate a RAG pipeline end-to-end
- [ ] Can use LLM tool calling to build a simple agent
- [ ] Can design a distributed system handling 100K+ RPS with clear trade-offs
- [ ] Can explain CAP theorem with real-world database examples
- [ ] Can identify DSA patterns and select the correct approach within 2 minutes

### Coding Proficiency

- [ ] Sliding window: solve 5 medium problems without hints
- [ ] Binary search: solve on sorted arrays and on answer space
- [ ] Tree DFS/BFS: implement all traversals from memory
- [ ] Graph algorithms: BFS, DFS, topological sort, Dijkstra's
- [ ] Dynamic programming: solve 10 problems across knapsack, LCS, LIS patterns
- [ ] Backtracking: solve subsets, permutations, combinations problems

### Portfolio Projects

- [ ] REST API with auth, rate limiting, and structured logging (Weeks 1-5)
- [ ] PostgreSQL-backed service with Redis caching and query optimization (Weeks 3-4)
- [ ] RAG pipeline with evaluation harness (Weeks 7-8)
- [ ] Track capstone project (Weeks 13-16)

### Interview Readiness

- [ ] Completed 5+ mock coding interviews (timed, explained out loud)
- [ ] Completed 3+ mock system design sessions (45 minutes each)
- [ ] Prepared STAR-format answers for 10 behavioral questions
- [ ] Can discuss trade-offs in all major architectural decisions without prompting

---

*Last updated: August 2026. This roadmap reflects current industry expectations for senior backend, AI/ML, and full-stack engineering roles.*
