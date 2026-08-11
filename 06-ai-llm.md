# 06 — AI & LLM Systems

## LLM fundamentals

Know:

- tokens
- context window
- temperature
- top-p
- system/user/assistant messages
- tool/function calling
- structured output
- streaming
- latency
- token cost
- hallucination

For backend interviews, focus less on model trivia and more on how you build reliable systems around models.

---

## Deterministic evaluation / test harnesses

An evaluation harness runs a fixed test suite against model behavior and produces reproducible scores.

```text
Test cases
   |
Model execution
   |
Normalized output
   |
Evaluator
   |
Pass / fail / score
   |
Regression report
```

Determinism requires controlling inputs, test data, evaluation rules, provider/model versions where possible, and nondeterministic factors.

### Resume connection

You built deterministic eval/test harnesses that grade AI model outputs against fixed check suites so behavior validation is repeatable and regression-safe.

### Follow-ups

- How do you evaluate nondeterministic LLM output?
- How do you avoid flaky tests?
- What belongs in a fixed check suite?
- How do you compare two model versions?
- How do you evaluate tool calls?

---

## Embeddings

An embedding maps text or other data to a numerical vector intended to capture semantic relationships.

```text
text -> embedding model -> [0.12, -0.31, 0.82, ...]
```

Know dimensions, normalization, semantic similarity, nearest-neighbor retrieval, and model selection.

---

## Chunking

RAG systems split source material into retrieval units.

Common strategies:

- fixed-size
- token-based
- paragraph/section-based
- semantic
- overlap

Small chunks can improve precision but lose context. Large chunks preserve context but may introduce irrelevant information.

---

## RAG

Retrieval-Augmented Generation combines retrieval with generation:

```text
Question
  |
Query embedding / keyword query
  |
Retriever
  |
Relevant chunks
  |
Prompt/context construction
  |
LLM
  |
Answer
```

### Ingestion

```text
Documents -> clean -> chunk -> embed -> store
```

### Query

```text
Question -> retrieve -> rerank/filter -> context -> generate
```

---

## Hybrid retrieval

Hybrid retrieval combines semantic/vector search with lexical/keyword search.

Why?

- semantic search handles conceptual similarity
- keyword search handles exact terms, IDs, names, and rare tokens

### Resume connection

You implemented hybrid RAG over PostgreSQL/pgvector. Be ready to explain how the vector and keyword result sets are combined/ranked and why this can outperform either method alone.

---

## Cosine similarity

```text
cos(A,B) = (A · B) / (||A|| ||B||)
```

It compares vector direction rather than raw magnitude.

Know the difference between cosine similarity, dot product, and Euclidean distance at a practical level.

---

## Retrieval recall

Recall asks:

> Did the retriever return the relevant information that existed in the corpus?

If retrieval misses the relevant chunk, the generator cannot reliably recover it.

Improve recall through:

- better chunking
- better embeddings
- query rewriting
- hybrid search
- reranking
- metadata filters
- multi-query retrieval

Do not confuse retrieval recall with answer correctness.

---

## Prompt injection and sanitization

Prompt injection occurs when untrusted content influences an agent/model to violate intended instructions.

Examples:

- malicious retrieved documents
- tool output containing instructions
- user input attempting to override system constraints

Defenses should include architectural controls, not just string sanitization:

- least-privilege tools
- allowlists
- schema validation
- output validation
- isolated credentials
- sandboxing
- human approval for high-risk actions
- treat external content as untrusted data

---

## MCP

Model Context Protocol provides a standardized way for AI applications to discover and interact with external tools/resources.

Concepts:

- MCP client
- MCP server
- tools
- resources
- prompts
- schemas

Security questions matter as much as protocol knowledge:

- Who can invoke a tool?
- What credentials can it access?
- Can tool output contain malicious instructions?
- How is authorization enforced?

---

## Tool-call orchestration

An agent can select tools and combine their results:

```text
User
 |
Agent
 |
 +--> Search
 +--> Database
 +--> External API
 +--> Tool B
 |
Final response
```

Reliable orchestration needs:

- typed tool schemas
- timeouts
- retries
- permission checks
- result validation
- state management
- tracing
- idempotency for side effects

### Resume connection

You built tool-call orchestration APIs driving agent workflows. Be ready to explain how tools are selected, how arguments are validated, how failures are recovered, and how you prevent an agent from performing unauthorized actions.

---

## Multi-provider LLM routing

A provider abstraction can normalize different model APIs:

```text
Agent
 |
Model Router
 |
 +--> OpenAI
 +--> Fireworks
 +--> Groq
 +--> DeepSeek
```

The router can choose a provider/model based on capability, cost, latency, availability, or policy.

### Failover

```text
Provider A
   |
timeout/error
   v
Provider B
   |
response
```

Important concerns:

- timeout budgets
- retry policy
- duplicate side effects from tool calls
- model capability differences
- output-format differences
- rate limits
- cost
- observability

### Resume connection

You implemented multi-provider routing with automatic failover. Expect questions about when failover is safe and when retrying can duplicate a tool action.

---

## AI provider gateways

Know the engineering concepts behind providers/gateways such as OpenRouter, DeepSeek, Gemini, Fireworks, Groq, and Alibaba Cloud:

- provider abstraction
- model routing
- fallback
- rate limits
- token cost
- latency
- streaming
- structured outputs
- capability differences

Do not memorize provider names without understanding the routing problem.

---

## Voice AI

Typical pipeline:

```text
Audio -> speech-to-text -> LLM -> text-to-speech -> audio
```

Know the role of Whisper, ElevenLabs, and meeting/voice integrations such as Recall.ai at a conceptual level.

Voice systems are latency-sensitive, so streaming and partial results matter.

---

## Integration platforms

Tools such as Composio abstract external SaaS/tool integrations. Understand the underlying engineering regardless of vendor:

- OAuth
- credentials
- token refresh
- permissions
- API normalization
- rate limits
- retries
- webhooks

---

## AI interview questions

- How would you make an LLM evaluation deterministic?
- What causes flaky AI tests?
- How does RAG work?
- How would you improve retrieval recall?
- Why hybrid search?
- Explain cosine similarity.
- How do you choose chunk size?
- What is reranking?
- How do you defend an agent against prompt injection?
- How would you design an MCP-based tool system?
- How would you implement model failover?
- When is retrying an LLM call unsafe?
- How would you reduce model latency and cost?
