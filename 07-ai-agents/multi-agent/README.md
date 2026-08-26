# Multi-Agent Systems

**Related:** [Fundamentals](../fundamentals/README.md) | [Tools](../tools/README.md) | [Guardrails](../guardrails/README.md)

---

## Why Multi-Agent?

Single agents hit hard ceilings that multi-agent architecture is designed to break through.

**Context window exhaustion.** A research-then-write task might require reading 50 documents, drafting a 5,000-word article, and then revising based on review feedback. A single agent accumulates all of that in one context. When the window fills, the model starts dropping earlier content — it forgets the sources it read. Breaking the task across agents gives each one a clean, focused context.

**No parallelism.** A single agent is sequential by definition. If a task has three independent subtasks (research three separate topics), a single agent does them one by one. Three specialized agents working in parallel cut wall-clock time to one-third.

**Single model capability limit.** Different subtasks suit different models. A small, fast model is appropriate for classification and routing. A large reasoning model is appropriate for synthesis and judgment. A single-agent system is forced to use one model for everything — either overpaying for simple steps or under-serving complex ones.

**No adversarial verification.** An agent cannot reliably check its own output — the same biases that caused an error in generation will cause it to miss the error in review. A separate agent with a fresh context can catch errors the generating agent cannot.

---

## Decomposition Strategies

How you split a task determines system complexity, latency, and cost. Three main patterns:

### Horizontal (Voting / Ensemble)

N agents tackle the same problem independently. Results are merged via voting, averaging, or a judge agent selecting the best.

Use when correctness confidence matters more than speed or cost. Common for: fact extraction, classification with uncertain labels, code generation where multiple solutions are synthesized.

```typescript
async function ensembleAgents<T>(
  agents: Agent[],
  input: string,
  mergeFn: (results: T[]) => T
): Promise<T> {
  const results = await Promise.all(
    agents.map((agent) =>
      agent.invoke({ messages: [{ role: "user", content: input }] })
    )
  );

  const outputs = results.map((r) => r.messages.at(-1)?.content as T);
  return mergeFn(outputs);
}

// Majority vote for classification
function majorityVote(labels: string[]): string {
  const counts = labels.reduce<Record<string, number>>((acc, label) => {
    acc[label] = (acc[label] ?? 0) + 1;
    return acc;
  }, {});

  return Object.entries(counts).sort((a, b) => b[1] - a[1])[0][0];
}
```

### Vertical (Pipeline)

Agents form a sequential pipeline. Each specialist hands off to the next: researcher → writer → reviewer → publisher. Output quality compounds across stages because each agent receives already-processed input.

Use when each stage genuinely requires different capabilities or context. Common for: content production, software development cycles, data transformation pipelines.

### Hybrid

A vertical pipeline where some stages fan out horizontally. A researcher decomposes the question into sub-questions, N research agents work them in parallel, a synthesizer merges their findings, a reviewer checks the synthesis.

```
Decomposer
    ├── Researcher A (sub-question 1)
    ├── Researcher B (sub-question 2)  ← parallel
    └── Researcher C (sub-question 3)
         ↓
    Synthesizer
         ↓
    Reviewer ── (if rejected) ── Synthesizer (retry)
         ↓
    Publisher
```

---

## Orchestrator / Worker Pattern

An orchestrator agent breaks a task into steps and delegates to worker agents. Workers are stateless and focused.

```typescript
import { ChatAnthropic } from "@anthropic-ai/sdk";
import { createReactAgent } from "@langchain/langgraph/prebuilt";

// Workers: each has a narrow tool set
const researchAgent = createReactAgent({
  llm: new ChatAnthropic({ model: "claude-sonnet-4-5" }),
  tools: [webSearchTool, fetchURLTool],
});

const writerAgent = createReactAgent({
  llm: new ChatAnthropic({ model: "claude-opus-4-5" }), // stronger model for prose
  tools: [formatMarkdownTool],
});

const reviewerAgent = createReactAgent({
  llm: new ChatAnthropic({ model: "claude-sonnet-4-5" }),
  tools: [readDraftTool, postCommentTool],
});

// Orchestrator: coordinates without doing the work itself
async function orchestrate(task: string): Promise<string> {
  const researchResult = await researchAgent.invoke({
    messages: [{ role: "user", content: `Research this topic thoroughly: ${task}` }],
  });

  const researchSummary = researchResult.messages.at(-1)?.content ?? "";

  const draftResult = await writerAgent.invoke({
    messages: [{
      role: "user",
      content: `Write a 1000-word article on "${task}" using this research:\n\n${researchSummary}`,
    }],
  });

  const draft = draftResult.messages.at(-1)?.content ?? "";

  const reviewResult = await reviewerAgent.invoke({
    messages: [{
      role: "user",
      content: `Review this draft for factual accuracy and clarity. Return "APPROVED" or list specific issues:\n\n${draft}`,
    }],
  });

  return reviewResult.messages.at(-1)?.content ?? "";
}
```

---

## LangGraph Multi-Agent with Handoffs

Agents transfer control via conditional edges in a state graph. The graph enforces valid transitions — an agent cannot hand off to an invalid next agent.

```typescript
import { StateGraph, Annotation, START, END } from "@langchain/langgraph";
import { BaseMessage, HumanMessage } from "@langchain/core/messages";

const GraphState = Annotation.Root({
  messages: Annotation<BaseMessage[]>({
    reducer: (existing, incoming) => [...existing, ...incoming],
    default: () => [],
  }),
  // Typed state fields prevent agents from writing arbitrary keys
  researchFindings: Annotation<string>({ default: () => "" }),
  draft: Annotation<string>({ default: () => "" }),
  reviewFeedback: Annotation<string>({ default: () => "" }),
  revisionCount: Annotation<number>({ default: () => 0 }),
  nextAgent: Annotation<string>({ default: () => "researcher" }),
  done: Annotation<boolean>({ default: () => false }),
});

type GraphStateType = typeof GraphState.State;

async function researcherNode(state: GraphStateType): Promise<Partial<GraphStateType>> {
  const result = await researchAgent.invoke({ messages: state.messages });
  const findings = result.messages.at(-1)?.content ?? "";
  return {
    researchFindings: findings,
    messages: result.messages,
    nextAgent: "writer",
  };
}

async function writerNode(state: GraphStateType): Promise<Partial<GraphStateType>> {
  const result = await writerAgent.invoke({
    messages: [
      ...state.messages,
      new HumanMessage(`Research findings:\n${state.researchFindings}`),
    ],
  });
  const draft = result.messages.at(-1)?.content ?? "";
  return { draft, messages: result.messages, nextAgent: "reviewer" };
}

async function reviewerNode(state: GraphStateType): Promise<Partial<GraphStateType>> {
  if (state.revisionCount >= 2) {
    // Hard limit: publish after 2 revisions regardless of feedback
    return { nextAgent: "publisher", done: true };
  }

  const result = await reviewerAgent.invoke({
    messages: [new HumanMessage(`Review this draft:\n${state.draft}`)],
  });

  const feedback = result.messages.at(-1)?.content ?? "";
  const approved = feedback.toUpperCase().includes("APPROVED");

  return {
    reviewFeedback: feedback,
    messages: result.messages,
    nextAgent: approved ? "publisher" : "writer",
    revisionCount: state.revisionCount + 1,
  };
}

const graph = new StateGraph(GraphState)
  .addNode("researcher", researcherNode)
  .addNode("writer", writerNode)
  .addNode("reviewer", reviewerNode)
  .addEdge(START, "researcher")
  .addEdge("researcher", "writer")
  .addEdge("writer", "reviewer")
  .addConditionalEdges("reviewer", (state) => state.nextAgent, {
    writer: "writer",   // revision cycle
    publisher: END,
  })
  .compile();
```

---

## Communication Patterns

### Shared State Graph

All agents read from and write to a typed state object. LangGraph's default model. Reducer functions control how concurrent writes merge — without reducers, the last write wins and data is silently lost.

```typescript
// Reducer for accumulating findings from multiple research agents
const ResearchState = Annotation.Root({
  findings: Annotation<string[]>({
    reducer: (existing, incoming) => [...existing, ...incoming], // append, never overwrite
    default: () => [],
  }),
  errors: Annotation<string[]>({
    reducer: (existing, incoming) => [...existing, ...incoming],
    default: () => [],
  }),
  finalSynthesis: Annotation<string>({ default: () => "" }),
});
```

**Trade-offs:** Simple to implement, natural for sequential pipelines, easy to checkpoint. Becomes a contention point for highly parallel systems writing to the same fields.

### Message Passing via Queue

Agents communicate through a message broker. No shared memory — each agent is a separate process or service. Natural fit for async workloads and distributed deployment.

```typescript
// Agent A: researcher publishes findings
await messageQueue.publish("research-complete", {
  taskId: "task-abc123",
  topic: "distributed consensus",
  findings: researchResults,
  timestamp: Date.now(),
});

// Agent B: writer subscribes and processes
messageQueue.subscribe("research-complete", async (message) => {
  const { taskId, topic, findings } = message;

  const draft = await writerAgent.invoke({
    messages: [{
      role: "user",
      content: `Write an article on "${topic}" using:\n\n${findings.join("\n---\n")}`,
    }],
  });

  await messageQueue.publish("draft-complete", {
    taskId,
    draft: draft.messages.at(-1)?.content,
  });
});
```

**Trade-offs:** Decoupled, independently scalable, supports async. Harder to debug — trace correlation IDs are essential. State is distributed across queue messages, not in one place.

### Blackboard Pattern

Agents post findings to a shared workspace. No direct agent-to-agent communication — agents read from and write to the blackboard independently. Useful when the number of agents or their dependencies are dynamic.

```typescript
type BlackboardEntry = {
  agentId: string;
  type: "finding" | "question" | "hypothesis" | "conclusion";
  content: string;
  confidence: number; // 0–1
  timestamp: number;
};

class Blackboard {
  private entries: BlackboardEntry[] = [];

  post(entry: Omit<BlackboardEntry, "timestamp">): void {
    this.entries.push({ ...entry, timestamp: Date.now() });
  }

  readByType(type: BlackboardEntry["type"]): BlackboardEntry[] {
    return this.entries.filter((e) => e.type === type);
  }

  highConfidenceFindings(threshold = 0.8): BlackboardEntry[] {
    return this.entries.filter(
      (e) => e.type === "finding" && e.confidence >= threshold
    );
  }
}
```

---

## State Persistence and Checkpointing

LangGraph checkpointers save state between steps, enabling agents to resume after failure or human interruption. Without checkpointing, a failure at step 15 of a 20-step task means restarting from step 1.

```typescript
import { MemorySaver } from "@langchain/langgraph";
// For production: PostgresSaver, RedisSaver from @langchain/langgraph-checkpoint-*

const checkpointer = new MemorySaver();

const graphWithCheckpointing = graph.compile({ checkpointer });

// Thread ID scopes checkpoints — same thread resumes from last saved state
const threadConfig = { configurable: { thread_id: "task-abc123" } };

// First invocation
const result = await graphWithCheckpointing.invoke(
  { messages: [new HumanMessage("Research quantum computing")] },
  threadConfig
);

// If interrupted, resume with same thread_id — picks up at last checkpoint
const resumed = await graphWithCheckpointing.invoke(
  { messages: [] }, // no new input needed — state is loaded from checkpoint
  threadConfig
);

// Inspect state at any point
const snapshot = await graphWithCheckpointing.getState(threadConfig);
console.log("Current node:", snapshot.next);
console.log("Draft so far:", snapshot.values.draft);
```

**Checkpoint storage options:**
- `MemorySaver`: in-process, lost on restart. Development only.
- `PostgresSaver`: durable, supports concurrent access. Use in production.
- `RedisSaver`: fast, TTL-based expiry for short-lived tasks.

---

## Agent Identity and Permissions

Each agent should have a distinct identity with its own scoped tool set. No agent should hold all permissions.

```typescript
type AgentRole = "researcher" | "writer" | "reviewer" | "publisher" | "admin";

const AGENT_TOOL_ALLOWLIST: Record<AgentRole, string[]> = {
  researcher: ["web_search", "fetch_url", "read_document"],
  writer:     ["read_document", "format_markdown", "save_draft"],
  reviewer:   ["read_draft", "read_document", "post_comment"],
  publisher:  ["read_draft", "publish_document", "send_notification"],
  admin:      ["read_draft", "publish_document", "send_notification", "delete_document"],
};

function createScopedAgent(role: AgentRole, allTools: Tool[]): Agent {
  const allowedNames = AGENT_TOOL_ALLOWLIST[role];
  const scopedTools = allTools.filter((t) => allowedNames.includes(t.name));

  if (scopedTools.length === 0) {
    throw new Error(`No tools matched allowlist for role "${role}". Check tool names.`);
  }

  return createReactAgent({ llm: modelForRole(role), tools: scopedTools });
}

function modelForRole(role: AgentRole): BaseChatModel {
  // Route roles to cost-appropriate models
  const expensiveRoles: AgentRole[] = ["writer", "admin"];
  return expensiveRoles.includes(role)
    ? new ChatAnthropic({ model: "claude-opus-4-5" })
    : new ChatAnthropic({ model: "claude-haiku-4-5" });
}
```

---

## Cost Management

Multi-agent systems multiply token costs. A 5-agent pipeline with 10k tokens per agent per step costs 5x a single-agent run. Measure and bound costs at design time.

```typescript
type AgentCostRecord = {
  agentId: string;
  role: AgentRole;
  inputTokens: number;
  outputTokens: number;
  steps: number;
};

class TaskCostTracker {
  private records: AgentCostRecord[] = [];
  private readonly budgetCents: number;
  private spentCents = 0;

  // Approximate pricing in cents per 1k tokens (varies by model)
  private static readonly INPUT_PRICE_PER_1K: Record<string, number> = {
    "claude-opus-4-5": 1.5,
    "claude-sonnet-4-5": 0.3,
    "claude-haiku-4-5": 0.025,
  };

  constructor(budgetCents: number) {
    this.budgetCents = budgetCents;
  }

  record(record: AgentCostRecord, model: string): void {
    const pricePerK = TaskCostTracker.INPUT_PRICE_PER_1K[model] ?? 0.3;
    const cost = (record.inputTokens / 1000) * pricePerK;
    this.spentCents += cost;
    this.records.push(record);

    if (this.spentCents > this.budgetCents) {
      throw new Error(
        `Task budget exceeded: spent ${this.spentCents.toFixed(2)}¢, limit is ${this.budgetCents}¢.`
      );
    }
  }

  summary(): { totalCents: number; byAgent: AgentCostRecord[] } {
    return { totalCents: this.spentCents, byAgent: this.records };
  }
}
```

**Cost reduction strategies:**
- Route simple subtasks (classification, formatting, extraction) to cheaper models
- Summarize research findings before passing to the writer — don't pass 50k tokens of raw search results
- Set per-agent step limits so one stuck agent does not exhaust the task budget
- Cache tool results that do not change (static documents, read-only DB queries)

---

## Debugging Multi-Agent Systems

Multi-agent bugs are hard to reproduce because they depend on message history, state at specific steps, and nondeterministic model outputs. Structured debugging from the start prevents hours of log archaeology.

### Correlation IDs for Tracing

Every task, every agent invocation, every tool call gets a correlation ID. Logs without correlation IDs are useless in a multi-agent system.

```typescript
import { randomUUID } from "crypto";

type AgentSpan = {
  traceId: string;      // one per top-level task
  spanId: string;       // one per agent invocation
  parentSpanId: string | null;
  agentRole: AgentRole;
  step: number;
  toolCalls: { toolName: string; durationMs: number; success: boolean }[];
  handoffTo: AgentRole | "end" | null;
  durationMs: number;
};

function startSpan(
  traceId: string,
  role: AgentRole,
  parentSpanId: string | null
): AgentSpan {
  return {
    traceId,
    spanId: randomUUID(),
    parentSpanId,
    agentRole: role,
    step: 0,
    toolCalls: [],
    handoffTo: null,
    durationMs: 0,
  };
}
```

### Visualizing Graph Execution

LangGraph exposes execution order via streaming events. Log state at each node transition.

```typescript
const stream = await graphWithCheckpointing.stream(
  { messages: [new HumanMessage(task)] },
  { ...threadConfig, streamMode: "values" }
);

for await (const state of stream) {
  // Log the current node and key state fields — not full message history
  console.log({
    nextAgent: state.nextAgent,
    revisionCount: state.revisionCount,
    draftLength: state.draft.length,
    findingsCount: state.researchFindings.length,
  });
}
```

---

## Convergence and Termination

Multi-agent systems must have deterministic termination conditions. A system that can run forever will eventually run forever.

```typescript
type TerminationCondition =
  | { type: "max_steps"; limit: number }
  | { type: "task_complete"; checkFn: (state: unknown) => boolean }
  | { type: "consensus"; requiredAgents: string[]; agreementField: string }
  | { type: "budget_exhausted"; tracker: TaskCostTracker };

function shouldTerminate(
  state: GraphStateType,
  conditions: TerminationCondition[],
  stepCount: number
): { terminate: boolean; reason: string } {
  for (const condition of conditions) {
    if (condition.type === "max_steps" && stepCount >= condition.limit) {
      return { terminate: true, reason: `Max steps (${condition.limit}) reached.` };
    }
    if (condition.type === "task_complete" && condition.checkFn(state)) {
      return { terminate: true, reason: "Task completion condition met." };
    }
  }
  return { terminate: false, reason: "" };
}
```

**Termination checklist:**
- Hard `max_steps` limit — non-negotiable, always present
- Per-agent revision limit (e.g., writer can be invoked at most 3 times per task)
- Budget limit that throws rather than logs
- Completion detection based on typed state fields, not model-generated text

---

## Common Failure Patterns

| Pattern | Cause | Detection | Fix |
|---------|-------|-----------|-----|
| Infinite handoff loop | Reviewer always rejects; sends back to writer forever | `revisionCount` field; alert when >3 | Hard revision limit; publish after N revisions regardless |
| Context overflow | Each handoff appends full message history | Monitor total token count at each node | Summarize before handoff; pass structured state, not raw messages |
| Conflicting state writes | Two parallel agents write to the same field without a reducer | Silent data loss — latest write survives | Reducer functions on every shared field |
| Orphaned tasks | Worker agent crashes; orchestrator does not detect it | No status update for task after timeout | Heartbeat mechanism; orchestrator re-queues after timeout |
| Cost explosion | One agent enters a subtask loop; keeps calling tools | Token spend tracking per agent per task | Per-agent tool call budget; circuit breaker on expensive tools |
| Stale checkpoint | State schema changes after checkpoints are saved | Deserialization errors on resume | Version checkpoint schemas; migrate or invalidate old checkpoints |

---

## Real-World Patterns

### Code Review Pipeline

```
write → test → review → fix → re-test → (merge or reject)
```

- **Writer agent**: generates code given a spec. Tools: `read_spec`, `write_file`.
- **Test agent**: runs the test suite against the generated code. Tools: `run_tests`, `read_file`.
- **Reviewer agent**: reads code and test results, posts structured feedback. Tools: `read_file`, `post_comment`.
- **Fix agent**: applies reviewer feedback. Tools: `read_file`, `write_file`, `read_comment`.
- Termination: merge when tests pass AND reviewer approves, or reject after 3 fix cycles.

### Parallel Research Pipeline

```
Decomposer → [Researcher A, B, C in parallel] → Synthesizer → Fact-checker → Publisher
```

- Decomposer breaks the question into 3 independent sub-questions.
- Three researcher agents run in parallel via `Promise.all`, each with their own search context.
- Synthesizer receives all three findings and merges them into one coherent answer.
- Fact-checker cross-references claims against the source documents.
- Cost: parallel stage costs 3x tokens but cuts wall time to 1/3.

### Content Production Pipeline

```
Brief → Outline → [Section A, B, C draft in parallel] → Editor → Publisher
```

- Outline agent produces a structured section breakdown.
- Three writer agents draft each section in parallel (separate contexts, no cross-contamination).
- Editor agent receives all sections, merges, and enforces consistent voice and style.
- Each parallel writer uses a cheaper model; the editor uses a stronger model.

---

## When to Use (and When Not To)

**Use multi-agent when:**
- The task has multiple independent subtasks that can run in parallel
- Different stages genuinely benefit from different model sizes or tool sets
- The total content exceeds one model's context window
- You need adversarial verification (generate + independently check)
- Human-in-the-loop is needed at specific stages, not throughout

**Do not use multi-agent when:**
- The task is a single-turn query with a short response
- Latency matters more than quality — every handoff adds round-trip time
- The coordination logic is more complex than the task itself
- You can fit everything in one context without quality loss
- Budget is tightly constrained — parallelism multiplies token costs
