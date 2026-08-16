# Multi-Agent Systems

## Why Multi-Agent?

Single agents are bottlenecked by context length and have no parallelism. Multi-agent systems:
- Split large tasks across specialized agents
- Run subtasks in parallel
- Allow one agent to verify another's output
- Recover from failures independently

---

## Patterns

### Orchestrator / Worker
A supervisor agent breaks a task into steps and delegates to worker agents.

```typescript
import { ChatOpenAI } from "@langchain/openai";
import { createReactAgent } from "@langchain/langgraph/prebuilt";

// Worker agent with specialized tools
const researchAgent = createReactAgent({
  llm: new ChatOpenAI({ model: "gpt-4o" }),
  tools: [webSearchTool, fetchURLTool],
});

const writerAgent = createReactAgent({
  llm: new ChatOpenAI({ model: "gpt-4o" }),
  tools: [formatMarkdownTool],
});

// Orchestrator decides which worker to call
async function orchestrate(task: string) {
  const research = await researchAgent.invoke({
    messages: [{ role: "user", content: `Research: ${task}` }],
  });
  
  const article = await writerAgent.invoke({
    messages: [
      { role: "user", content: `Write an article using this research: ${research.messages.at(-1)?.content}` }
    ],
  });
  
  return article.messages.at(-1)?.content;
}
```

### LangGraph Multi-Agent with Handoffs
Agents transfer control to each other via tool calls.

```typescript
import { StateGraph, Annotation, START, END } from "@langchain/langgraph";
import { HumanMessage } from "@langchain/core/messages";

const GraphState = Annotation.Root({
  messages: Annotation<HumanMessage[]>({
    reducer: (a, b) => [...a, ...b],
    default: () => [],
  }),
  nextAgent: Annotation<string>({ default: () => "researcher" }),
});

const graph = new StateGraph(GraphState)
  .addNode("researcher", researcherNode)
  .addNode("writer", writerNode)
  .addNode("reviewer", reviewerNode)
  .addEdge(START, "researcher")
  .addConditionalEdges("researcher", (state) => state.nextAgent, {
    writer: "writer",
    end: END,
  })
  .addEdge("writer", "reviewer")
  .addConditionalEdges("reviewer", (state) => state.nextAgent, {
    writer: "writer",  // rewrite if quality is low
    end: END,
  });
```

---

## Communication Patterns

### Shared State
All agents read/write to a shared state object (LangGraph's default model).

```typescript
type SharedState = {
  task: string;
  researchResults: string[];
  draft: string;
  reviewFeedback: string;
  approved: boolean;
};
```

### Message Passing
Agents communicate via a message queue. Useful for async / distributed setups.

```typescript
// Agent A produces work
await messageQueue.publish("write-task", {
  topic: "distributed systems",
  sources: researchResults,
});

// Agent B consumes it
messageQueue.subscribe("write-task", async (msg) => {
  const draft = await writerAgent.run(msg);
  await messageQueue.publish("review-task", { draft });
});
```

---

## Avoiding Common Pitfalls

| Problem | Cause | Fix |
|---------|-------|-----|
| Infinite loops | Agents keep handing off to each other | Add max steps / turn counter |
| Context overflow | Each agent appends to a growing message list | Summarize before handoff |
| Conflicting outputs | Two agents write to the same state field | Use reducer functions (LangGraph) |
| No error recovery | Worker fails, orchestrator halts | Each node should try/catch and return error state |

---

## When to Use

- Task is too long for one context window
- Subtasks can run in parallel (research + code + diagrams at once)
- You need specialized behavior (one model for code, one for prose)
- You want adversarial verification (agent A generates, agent B checks)

**When NOT to use**: simple single-turn tasks, low-latency requirements (multi-agent adds round-trip overhead), or when the added coordination complexity outweighs the benefits.
