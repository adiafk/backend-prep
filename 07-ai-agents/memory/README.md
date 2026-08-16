# Agent Memory

## 1. Types of Memory

Cognitive science distinguishes four types of memory that map directly onto agent architectures. Understanding the analogy clarifies what you need to build.

```
┌─────────────────────────────────────────────────────────────────┐
│                     AGENT MEMORY TAXONOMY                       │
├──────────────┬──────────────────────────────────────────────────┤
│ Working      │ Active context window — what the agent "sees"    │
│              │ right now during a single run                    │
├──────────────┼──────────────────────────────────────────────────┤
│ Episodic     │ Memories of past conversations and past runs —   │
│              │ "what happened before"                           │
├──────────────┼──────────────────────────────────────────────────┤
│ Semantic     │ General knowledge — facts, domain knowledge,     │
│              │ documentation, entity relationships              │
├──────────────┼──────────────────────────────────────────────────┤
│ Procedural   │ How to do things — system prompt instructions,   │
│              │ tool usage patterns, behavioral guidelines       │
└──────────────┴──────────────────────────────────────────────────┘
```

### Working memory (in-context)

Everything currently in the model's context window: the system prompt, the conversation history, tool call results, and any injected context. This is fast (no retrieval needed), but finite and ephemeral.

**Scope:** Single agent run  
**Capacity:** Model-dependent (e.g., 200k tokens for Claude 3.5)  
**Persistence:** Gone when the run ends  
**Access speed:** Zero latency — already in context

### Episodic memory (past conversations)

Records of prior interactions, past runs, and historical observations. "Last Tuesday, the user mentioned they prefer TypeScript over Python." Enables continuity across sessions.

**Scope:** Cross-session  
**Capacity:** Unlimited (stored externally)  
**Persistence:** Persistent between runs  
**Access speed:** Retrieval latency (vector similarity search)

### Semantic memory (knowledge)

Factual knowledge the agent can look up: documentation, product catalogs, codebases, research papers. Typically stored in a vector database and retrieved via similarity search.

**Scope:** Shared across all agent instances  
**Capacity:** Unlimited  
**Persistence:** Updated on a schedule or event-driven  
**Access speed:** Retrieval latency

### Procedural memory (instructions)

How the agent should behave: system prompt rules, step-by-step workflows, tool usage guidelines, output format requirements. Usually injected via the system prompt or as part of the agent's initial context.

**Scope:** Per-agent configuration  
**Capacity:** Limited by system prompt length  
**Persistence:** Static (changes require deployment)  
**Access speed:** Zero latency — in system prompt

---

## 2. Short-Term Buffer Memory — How It Works, When It Hits Limits

Short-term (buffer) memory is the simplest memory strategy: append every message to the conversation history and pass the full history on every call.

### How it works:

```typescript
// Conceptual representation
const history: Message[] = [];

async function chat(userMessage: string): Promise<string> {
  history.push({ role: "user", content: userMessage });

  const response = await llm.invoke(history);

  history.push({ role: "assistant", content: response.content });

  return response.content;
}
```

LangChain's `ConversationBufferMemory` does exactly this. LangGraph persists the full `messages` state list across graph invocations, which is buffer memory by default.

### What "buffer" means in practice:

Each agent step adds:
- The assistant's reasoning/response (maybe 200–500 tokens)
- One or more tool calls (50–200 tokens each)
- Tool results — this is where it balloons (can be 500–5,000 tokens per result)

An agent that runs 20 steps with 3 tool calls each, where each tool result averages 1,000 tokens, accumulates ~60,000 tokens of tool results alone — before reasoning.

### When buffer memory hits limits:

**Hard limit:** The model has a context window (e.g., 200k tokens for Claude). Exceed it and the API call fails with a context length error.

**Soft limit:** Even before the hard limit, performance degrades. The model's attention is spread across the full context. Important information from step 1 competes with noise from steps 10–18. Instructions in the system prompt get "forgotten" in practice.

**Rule of thumb:** If an agent run exceeds 30 steps or if any individual tool result exceeds 2,000 tokens, buffer memory alone is not sufficient. You need windowing or summarization.

### Buffer windowing:

Keep only the last N messages instead of the full history:

```typescript
import { BufferWindowMemory } from "langchain/memory";

const memory = new BufferWindowMemory({
  k: 10, // Keep last 10 exchanges (20 messages)
  returnMessages: true,
});
```

**Tradeoff:** Recent context is preserved at the cost of losing earlier context entirely. Acceptable for conversational agents; risky for agents that need to reference early decisions.

---

## 3. Long-Term Vector Recall — Similarity-Based Retrieval of Past Turns

Vector recall stores past interactions (or knowledge) as embeddings and retrieves the most relevant ones when needed, rather than including everything in context.

### How embedding-based retrieval works:

```
Storage phase:
  Past message → Embed (model) → Vector [0.2, 0.8, ...] → Store in vector DB

Retrieval phase:
  New query → Embed → Query vector → Similarity search → Top-K results → Inject into context
```

Similarity is measured by cosine distance between vectors. The assumption: if two texts have similar embeddings, they are semantically related.

### Implementing vector memory with LangChain:

```typescript
import { MemoryVectorStore } from "langchain/vectorstores/memory";
import { OpenAIEmbeddings } from "@langchain/openai";
import { VectorStoreRetrieverMemory } from "langchain/memory";

const vectorStore = new MemoryVectorStore(new OpenAIEmbeddings());

const memory = new VectorStoreRetrieverMemory({
  vectorStoreRetriever: vectorStore.asRetriever(3), // retrieve top 3
  memoryKey: "relevant_history",
});

// After each interaction, save to vector store
await memory.saveContext(
  { input: "What's the project deadline?" },
  { output: "The deadline is December 15th, 2025." }
);

// On next interaction, retrieve relevant past context
const retrieved = await memory.loadMemoryVariables({
  input: "When do we need to finish?",
});
// retrieved.relevant_history contains the deadline conversation
```

### What to store:

- Full conversation turns (user + assistant pairs)
- Summaries of past sessions
- Key facts extracted from conversations ("user prefers Python", "project deadline is X")
- Observations from tool calls that are likely to be referenced again

### What vector recall is not good for:

- Exact lookups (use a regular database, not a vector store, for IDs and structured data)
- Ordered sequences (vector search doesn't preserve order)
- Very recent context (the last 5 messages should stay in buffer, not be retrieved — retrieval adds latency and may miss them due to similarity thresholds)

### Hybrid approach (recommended for production):

```
Context assembly for each step:
1. System prompt (procedural memory — always included)
2. Last N messages (buffer memory — always included)
3. Retrieved relevant history (vector recall — top-K by similarity)
4. Retrieved relevant knowledge (RAG — domain documents if needed)
```

---

## 4. External Memory — Database-Backed, When Needed

When vector similarity is not the right retrieval mechanism, use structured external storage.

### When you need external (database-backed) memory:

| Scenario | Memory Type |
|---|---|
| Look up a user by ID | SQL / key-value store |
| Store structured facts with exact lookup | SQL / document DB |
| Track task completion state across runs | SQL or Redis |
| Store large files (images, documents) | Object storage (S3) |
| Cache expensive computation results | Redis with TTL |
| Audit trail of all agent actions | Append-only SQL / event store |

### Example: Persistent state across sessions using Redis

```typescript
import { Redis } from "ioredis";

const redis = new Redis(process.env.REDIS_URL);

interface AgentSession {
  userId: string;
  conversationHistory: Message[];
  collectedFacts: Record<string, string>;
  currentGoal: string | null;
}

async function loadSession(sessionId: string): Promise<AgentSession | null> {
  const raw = await redis.get(`agent:session:${sessionId}`);
  return raw ? JSON.parse(raw) : null;
}

async function saveSession(
  sessionId: string,
  session: AgentSession
): Promise<void> {
  await redis.set(
    `agent:session:${sessionId}`,
    JSON.stringify(session),
    "EX",
    86400 // 24-hour TTL
  );
}
```

### Example: Structured fact storage (what the agent has "learned")

```typescript
// Store discrete facts as structured rows rather than in the context window
interface AgentFact {
  agentId: string;
  userId: string;
  factKey: string;   // e.g., "preferred_language"
  factValue: string; // e.g., "TypeScript"
  source: string;    // e.g., "user stated in session 2024-12-01"
  confidence: number;
  createdAt: Date;
  updatedAt: Date;
}

// When the agent needs to know about the user, query facts first:
async function getUserContext(userId: string): Promise<string> {
  const facts = await db.query(
    "SELECT fact_key, fact_value FROM agent_facts WHERE user_id = $1",
    [userId]
  );
  return facts.map(f => `${f.fact_key}: ${f.fact_value}`).join("\n");
}
```

### Checkpointing agent state

For long-running agents, you need to be able to resume a run after a failure without starting over. LangGraph has built-in checkpointing:

```typescript
import { PostgresSaver } from "@langchain/langgraph-checkpoint-postgres";

const checkpointer = PostgresSaver.fromConnString(process.env.DATABASE_URL);

const agent = createReactAgent({
  llm: model,
  tools,
  checkpointSaver: checkpointer,
});

// Resume a prior run by providing the thread_id
const result = await agent.invoke(
  { messages: [{ role: "user", content: "Continue where we left off" }] },
  { configurable: { thread_id: "session-abc123" } }
);
```

With checkpointing, if the agent crashes at step 14 of 20, it can resume from step 14 rather than step 1.

---

## 5. Memory Architecture Patterns for Production Agents

### Pattern 1: The minimal viable memory stack

For most production agents, this covers 90% of use cases:

```
┌─────────────────────────────────────────────────────────┐
│ System prompt        (procedural — static)              │
│ Last 20 messages     (buffer — rolling window)          │
│ Session state        (key-value — Redis or Postgres)    │
└─────────────────────────────────────────────────────────┘
```

Add vector recall only when users explicitly reference past conversations and you need semantic search to find the relevant one.

### Pattern 2: The summarization pipeline

When buffer memory grows too large, compress it:

```
Every N steps (or when tokens > threshold):
  1. Take the oldest M messages
  2. Call LLM: "Summarize these messages into key facts and decisions"
  3. Replace the M messages with a single summary message
  4. Continue appending new messages as normal
```

```typescript
async function compressHistory(
  messages: Message[],
  keepLast: number = 10
): Promise<Message[]> {
  if (messages.length <= keepLast) return messages;

  const toSummarize = messages.slice(0, -keepLast);
  const toKeep = messages.slice(-keepLast);

  const summary = await llm.invoke([
    {
      role: "user",
      content: `Summarize these conversation messages into key facts. Be concise.\n\n${JSON.stringify(toSummarize)}`,
    },
  ]);

  return [
    {
      role: "system",
      content: `[Earlier conversation summary]: ${summary.content}`,
    },
    ...toKeep,
  ];
}
```

### Pattern 3: Memory-augmented RAG agent

The agent retrieves from both a knowledge base and past conversations simultaneously:

```
At each step, context = [
  system_prompt,
  retrieved_docs (top 3 from knowledge base),
  retrieved_history (top 3 from past conversations),
  last 10 messages (buffer)
]
```

This keeps the context window bounded regardless of how many past conversations exist.

### Pattern 4: Hierarchical memory with explicit fact extraction

After each session, extract and store structured facts:

```
Session ends →
  LLM: "Extract key facts from this conversation" →
  Parse into structured facts →
  Store in SQL (for exact lookup) AND embed in vector store (for semantic search)

Next session begins →
  Load user's stored facts into system prompt
  Enable semantic search over past sessions
```

### Common production mistakes:

**Mistake 1: Storing raw messages instead of summaries.** A year of daily conversations is thousands of messages. Store summaries of sessions plus a rolling buffer of recent full messages.

**Mistake 2: Over-retrieving.** Retrieving top-20 similar memories and stuffing them into context wastes tokens. Top 3–5 is usually sufficient; quality beats quantity.

**Mistake 3: Not indexing by user.** Memory retrieved for user A must never appear in user B's context. Always filter by userId at the database and vector store level.

**Mistake 4: Trusting retrieved memory without staleness checks.** A fact retrieved from vector memory may be outdated. Store timestamps and inject them: "As of 2024-11-01, the user's goal was X."

**Mistake 5: No memory eviction strategy.** Without a TTL or cleanup policy, memory stores grow indefinitely. Define retention policies: keep last 90 days of episodic memory, archive or delete older records.
