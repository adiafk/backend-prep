# Model Routing

## Why Route?

Not every request needs GPT-4. A classification prompt or embedding lookup can run on a smaller, cheaper, faster model. Routing sends each request to the most appropriate model based on complexity, cost, or capability requirements.

---

## Intent-Based Routing

Classify the request first, then pick the model.

```typescript
import { ChatOpenAI } from "@langchain/openai";
import { z } from "zod";

const classificationModel = new ChatOpenAI({ model: "gpt-4o-mini" });

const intentSchema = z.object({
  complexity: z.enum(["simple", "moderate", "complex"]),
  requires_code: z.boolean(),
  requires_reasoning: z.boolean(),
});

async function classifyRequest(userMessage: string) {
  const structured = classificationModel.withStructuredOutput(intentSchema);
  return structured.invoke([
    {
      role: "system",
      content: "Classify the complexity and requirements of this user request.",
    },
    { role: "user", content: userMessage },
  ]);
}

async function routedChat(userMessage: string, systemPrompt?: string) {
  const intent = await classifyRequest(userMessage);

  let modelName: string;
  if (intent.complexity === "simple" && !intent.requires_reasoning) {
    modelName = "gpt-4o-mini";      // Fast, cheap
  } else if (intent.requires_code || intent.requires_reasoning) {
    modelName = "gpt-4o";           // Full capability
  } else {
    modelName = "gpt-4o-mini";
  }

  const model = new ChatOpenAI({ model: modelName });
  return model.invoke([
    ...(systemPrompt ? [{ role: "system" as const, content: systemPrompt }] : []),
    { role: "user" as const, content: userMessage },
  ]);
}
```

---

## Cost-Based Routing with OpenRouter

OpenRouter exposes many models via a single API. Route by task type to minimize cost.

```typescript
import { ChatOpenAI } from "@langchain/openai";

type ModelTier = "fast" | "balanced" | "powerful";

const OPENROUTER_MODELS: Record<ModelTier, string> = {
  fast: "google/gemini-flash-1.5",      // Cheapest, fast
  balanced: "anthropic/claude-3-haiku", // Good quality, low cost
  powerful: "anthropic/claude-sonnet-4-6", // Best quality
};

function getOpenRouterModel(tier: ModelTier) {
  return new ChatOpenAI({
    modelName: OPENROUTER_MODELS[tier],
    openAIApiKey: process.env.OPENROUTER_API_KEY,
    configuration: {
      baseURL: "https://openrouter.ai/api/v1",
      defaultHeaders: {
        "HTTP-Referer": "https://your-app.com",
        "X-Title": "Your App",
      },
    },
  });
}

// Route by task type
function selectTier(task: string): ModelTier {
  if (task === "summarize" || task === "classify" || task === "translate") {
    return "fast";
  }
  if (task === "qa" || task === "rewrite") {
    return "balanced";
  }
  return "powerful"; // code generation, analysis, reasoning
}
```

---

## Fallback Routing

Try primary model, fall back on failure or timeout.

```typescript
import { ChatOpenAI } from "@langchain/openai";
import { ChatAnthropic } from "@langchain/anthropic";
import { BaseMessage } from "@langchain/core/messages";

async function withFallback(messages: BaseMessage[]) {
  const primary = new ChatOpenAI({ model: "gpt-4o", timeout: 10_000 });
  const fallback = new ChatAnthropic({ model: "claude-haiku-4-5-20251001" });

  try {
    return await primary.invoke(messages);
  } catch (err) {
    console.warn("Primary model failed, using fallback:", (err as Error).message);
    return fallback.invoke(messages);
  }
}
```

LangChain has a built-in `.withFallbacks()` method:

```typescript
const modelWithFallback = new ChatOpenAI({ model: "gpt-4o" })
  .withFallbacks({
    fallbacks: [new ChatAnthropic({ model: "claude-haiku-4-5-20251001" })],
  });
```

---

## LangGraph Router Node

In an agent graph, a router node reads state and returns the next node name.

```typescript
import { StateGraph, Annotation, START, END } from "@langchain/langgraph";

type TaskType = "code" | "math" | "general";

const State = Annotation.Root({
  userMessage: Annotation<string>(),
  taskType: Annotation<TaskType>(),
  response: Annotation<string>(),
});

// Router: classifies the task and sets taskType
async function routerNode(state: typeof State.State) {
  const intent = await classifyRequest(state.userMessage);
  return {
    taskType: intent.requires_code ? "code" : "general" as TaskType,
  };
}

const graph = new StateGraph(State)
  .addNode("router", routerNode)
  .addNode("code_agent", codeAgentNode)
  .addNode("general_agent", generalAgentNode)
  .addEdge(START, "router")
  .addConditionalEdges("router", (state) => state.taskType, {
    code: "code_agent",
    general: "general_agent",
  })
  .addEdge("code_agent", END)
  .addEdge("general_agent", END);
```

---

## When to Route

| Scenario | Recommendation |
|----------|---------------|
| Mix of simple and complex queries | Route by complexity — save 80-90% on simple queries |
| High QPS with cost constraints | Route short queries to fast models, reserve powerful for complex ones |
| Multi-modal inputs | Route image/audio to vision/audio models |
| Reliability requirements | Always configure a fallback for critical paths |
