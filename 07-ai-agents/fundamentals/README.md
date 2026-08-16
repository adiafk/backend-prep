# AI Agent Fundamentals

## 1. What an Agent Is — Perceive → Think → Act Loop

An AI agent is a system that autonomously pursues a goal by repeatedly observing its environment, reasoning about what to do next, and taking actions that change the environment — then repeating the cycle until the goal is achieved or the agent gives up.

```
┌─────────────────────────────────────────────────┐
│                    AGENT LOOP                   │
│                                                 │
│   ┌──────────┐    ┌──────────┐    ┌──────────┐ │
│   │ PERCEIVE │───▶│  THINK   │───▶│   ACT    │ │
│   │          │    │          │    │          │ │
│   │ Observe  │    │ Reason   │    │ Execute  │ │
│   │ inputs,  │    │ over     │    │ tool     │ │
│   │ tool     │    │ context  │    │ calls,   │ │
│   │ results, │    │ and      │    │ API      │ │
│   │ env state│    │ memory   │    │ calls    │ │
│   └──────────┘    └──────────┘    └──────────┘ │
│         ▲                               │       │
│         └───────────────────────────────┘       │
│                  Environment                    │
└─────────────────────────────────────────────────┘
```

### Key distinctions from a simple LLM call:

| Property | LLM Call | Agent |
|---|---|---|
| Turns | Single | Multiple |
| Actions | Text output only | Tools, API calls, code execution |
| Memory | Prompt context only | Persistent state across steps |
| Goal tracking | None | Explicit goal, measures completion |
| Decision-making | One-shot | Iterative, adaptive |

### The perceive-think-act breakdown:

**Perceive** — The agent receives inputs. This is not just the initial user message. It includes:
- Tool call results from the previous step
- Updated environment state (e.g., a file was written, a search returned results)
- Injected memory (retrieved from vector store or database)
- System context (date, user identity, available tools)

**Think** — The LLM reasons over the full context window to decide: What is the current state? What is the goal? What is the best next action? This is where planning happens. With ReAct, this reasoning is explicit and observable.

**Act** — The agent emits a structured action: a tool call, a message to the user, or a decision to stop. The action changes the environment, which becomes input for the next perceive step.

### Why this matters for backend systems:
- Agents are stateful across calls — you need to persist the message thread (the "scratchpad")
- Agent loops can run for many steps — you need timeouts, max-iteration guards, and async execution
- Each act step may hit external APIs — you need retry logic, rate limiting, and observability

---

## 2. ReAct Pattern — Reason + Act, Why It Works

ReAct (Reason + Act) is a prompting pattern where the model explicitly writes out its reasoning before deciding what action to take. Introduced in the 2022 paper "ReAct: Synergizing Reasoning and Acting in Language Models."

### The pattern:

```
Thought: I need to find the current weather in Paris.
Action: search_web(query="Paris weather today")
Observation: Temperature is 18°C, partly cloudy.

Thought: I now have the weather. The user also asked about the forecast.
Action: search_web(query="Paris 5-day forecast")
Observation: Monday 20°C, Tuesday 17°C, Wednesday 22°C...

Thought: I have all the information needed to answer.
Action: respond_to_user(message="Paris is currently 18°C...")
```

### Why ReAct works:

**1. Reduces hallucination in multi-step tasks.** Without explicit reasoning, the model jumps straight to action. The reasoning step forces it to check whether it actually has the information needed before acting.

**2. Creates an audit trail.** Each Thought is observable. When an agent fails, you can inspect the thought chain to see exactly where it went wrong.

**3. Enables self-correction.** When an action returns unexpected results, the next Thought step can recognize the failure: "The search returned no results. The query was too specific. I should broaden it."

**4. Grounds actions in stated goals.** The model must articulate why it is taking an action before taking it — this forces alignment between reasoning and behavior.

### ReAct in LangChain:

```typescript
import { createReactAgent } from "@langchain/langgraph/prebuilt";
import { ChatAnthropic } from "@langchain/anthropic";
import { TavilySearchResults } from "@langchain/community/tools/tavily_search";

const model = new ChatAnthropic({ model: "claude-sonnet-4-5" });
const tools = [new TavilySearchResults({ maxResults: 3 })];

const agent = createReactAgent({ llm: model, tools });

const result = await agent.invoke({
  messages: [{ role: "user", content: "What is the latest news about AI?" }],
});
```

### Structured tool calls vs. text-based ReAct:

Modern agents with function-calling models (Claude, GPT-4) don't literally write "Thought:" and "Action:" as text. Instead:
- The **Thought** is the model's internal chain-of-thought (or an explicit `<thinking>` block)
- The **Action** is a structured `tool_use` content block
- The **Observation** is a `tool_result` message injected back into the context

The underlying logic is identical — only the serialization format changes.

---

## 3. Agent vs Chain — When to Use Each

### What is a chain?

A chain is a deterministic, predefined sequence of LLM calls and transformations. The flow is fixed at design time. Input → Step 1 → Step 2 → Step 3 → Output. No branching, no tool calls mid-execution (beyond what is hardcoded), no adaptive behavior.

```
RAG Chain example:
User query → Embed query → Retrieve documents → Stuff into prompt → LLM → Answer
```

### What is an agent?

An agent is a dynamic loop where the model decides at runtime which steps to take, in what order, and when to stop.

```
Agent example:
User query → Agent reasons → calls search tool → reasons again →
calls calculator tool → reasons again → concludes → responds
```

### Decision framework:

| Use a Chain when... | Use an Agent when... |
|---|---|
| The steps are known and fixed | The steps depend on the input |
| The number of LLM calls is predictable | The number of calls is variable |
| Latency must be tightly bounded | Latency can vary with task complexity |
| You need deterministic behavior | You need adaptive problem-solving |
| Debugging must be simple | You can afford observability tooling |
| The task is retrieval + generation | The task requires tool use, search, computation |

### Practical examples:

**Use a chain:**
- Document summarization pipeline
- Q&A over a fixed knowledge base (RAG)
- Data extraction from structured documents
- Classification + routing

**Use an agent:**
- "Research this topic and write a report" (unknown number of searches)
- "Fix this bug in my codebase" (needs to read files, run tests, edit files)
- "Book a flight for me" (multi-step with external APIs and conditional logic)
- Any task where "it depends" is the honest answer to "how many steps?"

### The hybrid approach:

Many production systems use chains for predictable subtasks and agents for the top-level orchestration:

```
Agent (top-level)
├── calls: RAG Chain (retrieval is deterministic)
├── calls: Summarization Chain (fixed steps)
└── calls: Code Execution Tool (unpredictable, handled adaptively)
```

---

## 4. Planning Approaches

### Sequential (linear) planning

The simplest approach: execute steps in order, one after another. Each step's output feeds into the next.

```
Step 1 → Step 2 → Step 3 → Done
```

**When to use:** Tasks with clear linear dependencies. Writing a report: research → outline → draft → edit.

**Limitation:** Slow. No parallelism. A failure at step 2 blocks everything.

### DAG (Directed Acyclic Graph) planning

Steps are structured as a graph. Independent steps run in parallel. Dependent steps wait for their prerequisites.

```
        ┌─── Step B ───┐
Step A ──┤              ├──▶ Step D
        └─── Step C ───┘
```

**When to use:** Tasks with independent subtasks that can run concurrently. Researching 5 topics simultaneously before synthesizing a report.

**Implementation:** LangGraph makes this explicit with nodes and edges. Each node is a step; edges define dependencies. Parallel branches run concurrently.

**Limitation:** The DAG must be known ahead of time (or generated by a planner step). Cannot handle truly dynamic branching.

### Self-correcting loops

The agent attempts a task, evaluates the result, and retries with corrections if the result is unsatisfactory.

```
Execute → Evaluate → Satisfied? → Done
             │
             No → Reflect → Adjust → Execute again
```

**Example patterns:**
- **Reflexion:** After each action, the agent writes a "reflection" on what went wrong and how to improve before retrying.
- **Self-RAG:** The agent critiques its own retrieval — decides whether retrieved documents are relevant, and retrieves again if not.
- **Code + test loops:** Generate code → run tests → if tests fail, read error → fix code → run tests again.

```typescript
// Self-correcting loop pseudocode
let attempts = 0;
const MAX_ATTEMPTS = 3;

while (attempts < MAX_ATTEMPTS) {
  const result = await agent.invoke({ messages, previousAttempts });
  const evaluation = await evaluator.invoke({ result, goal });

  if (evaluation.satisfied) {
    return result;
  }

  messages = addReflection(messages, evaluation.feedback);
  attempts++;
}

throw new Error("Agent failed to satisfy goal after max attempts");
```

**Critical implementation detail:** You must cap retries. Without a hard limit, self-correcting loops are infinite loops waiting to happen.

### Plan-and-execute

A separate planning step generates a structured task list before any execution begins. Then an executor works through the plan, updating it as needed.

```
Planner LLM → [Task 1, Task 2, Task 3, ...] → Executor → Results
                        ▲                            │
                        └────── Re-plan if needed ───┘
```

**Advantage:** The plan is inspectable. Users can review and approve before execution begins. Good for high-stakes tasks.

**Limitation:** The planner may generate a bad plan. The executor may encounter conditions the planner didn't anticipate, requiring mid-execution re-planning.

---

## 5. Failure Modes

### Tool loops

**What it is:** The agent calls the same tool repeatedly with the same or similar arguments, making no progress toward the goal.

**Why it happens:** The tool returns a result the model doesn't know how to interpret, so it calls the tool again. Or the model gets "stuck" in a reasoning pattern that always concludes with the same action.

**Example:**
```
Thought: I need to find the user's email.
Action: search_database(query="user email")
Observation: No results found.

Thought: I need to find the user's email.
Action: search_database(query="user email address")
Observation: No results found.

[repeats 8 more times]
```

**Mitigation:**
- Hard limit on total tool calls per run (e.g., `maxIterations: 15`)
- Track tool call history; if the same tool+args appear twice, inject a "you tried this already, try something different" message
- Monitor for identical observations in consecutive steps

```typescript
const agent = createReactAgent({
  llm: model,
  tools,
  // LangGraph's built-in recursion limit
  recursionLimit: 15,
});
```

### Hallucinated tool arguments

**What it is:** The agent calls a real tool with arguments that don't exist, are malformed, or reference entities that don't exist in the system.

**Why it happens:** The model doesn't have ground truth about valid IDs, enum values, or schema constraints. It generates plausible-looking arguments that fail validation.

**Example:**
```
Action: get_user(userId="user_abc123")
// userId "user_abc123" does not exist — model invented it
```

**Mitigation:**
- Validate all tool inputs against Zod schemas before execution; return structured errors
- Never pass raw model output directly to databases or APIs
- Design tools to return the valid options when the agent asks a vague question: `list_users()` before `get_user(id)`
- Use enum fields in tool schemas to constrain choices to valid values

### Context overflow

**What it is:** The agent's message history grows until it exceeds the model's context window, causing truncation or errors.

**Why it happens:** Each tool result is added to the context. Long tool outputs (e.g., a 10,000-token search result) fill the window in a few steps. Long-running agents accumulate dozens of steps.

**Example scenario:** An agent that searches the web 10 times, each returning 2,000 tokens of content, consumes 20,000 tokens just in observations — before the model has generated any synthesis.

**Mitigation strategies:**

1. **Truncate tool outputs at the source.** Summarize or truncate search results before adding them to context. Return only the top N results.

2. **Compress old context.** Periodically summarize earlier steps into a compact summary and discard the raw messages.

3. **Use a memory system.** Store intermediate results in a vector store and retrieve only relevant pieces in later steps.

4. **Track token usage.** Count tokens at each step. If approaching the limit, trigger a summarization step or gracefully end the run.

```typescript
// Truncate tool output before it enters context
function truncateToolResult(result: string, maxTokens = 2000): string {
  const tokens = estimateTokens(result);
  if (tokens <= maxTokens) return result;

  const truncated = result.slice(0, maxTokens * 4); // rough char estimate
  return `${truncated}\n\n[Output truncated. ${tokens - maxTokens} tokens omitted.]`;
}
```

### Compounding errors

**What it is:** An error in step 2 causes incorrect context for step 3, which causes a worse error in step 4. Each step's mistake is built on the previous one.

**Why it happens:** The agent treats its own prior outputs as ground truth. If it retrieves the wrong document, it reasons from it confidently.

**Mitigation:** Use self-correcting loops with explicit verification steps. After each major action, have the agent verify the result before proceeding. Build in checkpoints where a human or evaluator LLM can review progress.

### Instruction following drift

**What it is:** Over a long agent run, the agent gradually loses track of the original goal and starts optimizing for an intermediate subgoal.

**Why it happens:** As the message history grows, the original system prompt and user request are further from the end of the context. Recent tool results and thoughts dominate the model's attention.

**Mitigation:** Re-inject the original goal into the context at regular intervals. LangGraph allows injecting messages at any point in the graph — use this to prepend a "current goal" reminder before each reasoning step.
