# Model Routing

## Why Route?

Not every request needs GPT-4. A classification prompt or embedding lookup can run on a smaller, cheaper, faster model. Routing sends each request to the most appropriate model based on complexity, cost, or capability requirements.

Cost impact is large: a typical production workload has 70–80% simple queries (summarize, classify, translate). Routing those to a cheap model while reserving frontier models for complex reasoning cuts cost by 60–80% with negligible quality loss.

---

## Model Tiers and Capabilities

### Cost Tiers (rough 2025 pricing, input/output per million tokens)

| Tier | Example Models | Input cost | Output cost | Use for |
|---|---|---|---|---|
| Cheap | GPT-4o mini, Claude Haiku | ~$0.15/M | ~$0.60/M | Classification, extraction, simple Q&A |
| Mid-tier | GPT-4o, Claude Sonnet | ~$2.50/M | ~$10/M | Code gen, analysis, multi-step reasoning |
| Frontier | o3, Claude Opus, Gemini 2.5 Pro | ~$10–15/M | ~$30–60/M | Complex reasoning, long context, highest quality |

Check provider pricing pages before building cost models — prices change frequently.

### Latency Tiers (typical TTFT at moderate load)

| Tier | Example Models | TTFT P50 |
|---|---|---|
| Fast | GPT-4o mini, Claude Haiku | ~0.3s |
| Standard | GPT-4o, Claude Sonnet | ~0.8s |
| Slow | o3, Claude Opus | ~2–5s |

### Capability Matrix

| Capability | Check before routing |
|---|---|
| Context window | Gemini 2.5 Pro (1M), GPT-4o (128K), Haiku (200K) |
| Function/tool calling | All major models; confirm schema compliance |
| Vision (image input) | GPT-4o, Claude Sonnet/Opus, Gemini — not all mini models |
| Structured JSON output | Verify JSON mode / `response_format` support |
| Streaming | All major models; confirm via provider docs |

---

## Model Selection Decision Tree

```
Request arrives
    │
    ├─ Has image/audio input?
    │       └─ YES → route to vision-capable model (GPT-4o, Claude Sonnet)
    │
    ├─ Requires structured JSON output?
    │       └─ YES → route to model with confirmed JSON mode
    │
    ├─ Context > 100K tokens?
    │       └─ YES → Gemini 2.5 Pro (1M ctx) or Claude (200K ctx)
    │
    ├─ Task type is simple? (classify / translate / summarize / extract)
    │       └─ YES → cheap tier (Haiku / GPT-4o mini)
    │
    ├─ Requires code generation or multi-step reasoning?
    │       └─ YES → mid-tier (Claude Sonnet / GPT-4o)
    │
    └─ Requires best possible quality (compliance, legal, critical path)?
            └─ YES → frontier (o3 / Claude Opus)
```

---

## Capability-Based Routing

```typescript
interface ModelCapabilities {
  contextLength: number;
  supportsVision: boolean;
  supportsFunctionCalling: boolean;
  supportsStructuredOutput: boolean;
  supportsStreaming: boolean;
  costTier: "cheap" | "mid" | "frontier";
  latencyTier: "fast" | "standard" | "slow";
  provider: "openai" | "anthropic" | "google";
}

const MODEL_REGISTRY: Record<string, ModelCapabilities> = {
  "gpt-4o-mini": {
    contextLength: 128_000,
    supportsVision: true,
    supportsFunctionCalling: true,
    supportsStructuredOutput: true,
    supportsStreaming: true,
    costTier: "cheap",
    latencyTier: "fast",
    provider: "openai",
  },
  "gpt-4o": {
    contextLength: 128_000,
    supportsVision: true,
    supportsFunctionCalling: true,
    supportsStructuredOutput: true,
    supportsStreaming: true,
    costTier: "mid",
    latencyTier: "standard",
    provider: "openai",
  },
  "claude-haiku-4-5": {
    contextLength: 200_000,
    supportsVision: true,
    supportsFunctionCalling: true,
    supportsStructuredOutput: true,
    supportsStreaming: true,
    costTier: "cheap",
    latencyTier: "fast",
    provider: "anthropic",
  },
  "claude-sonnet-4-6": {
    contextLength: 200_000,
    supportsVision: true,
    supportsFunctionCalling: true,
    supportsStructuredOutput: true,
    supportsStreaming: true,
    costTier: "mid",
    latencyTier: "standard",
    provider: "anthropic",
  },
  "gemini-2.5-pro": {
    contextLength: 1_000_000,
    supportsVision: true,
    supportsFunctionCalling: true,
    supportsStructuredOutput: true,
    supportsStreaming: true,
    costTier: "frontier",
    latencyTier: "slow",
    provider: "google",
  },
};

interface RoutingRequest {
  hasImageInput: boolean;
  requiresStructuredOutput: boolean;
  estimatedInputTokens: number;
  taskType: "classify" | "translate" | "summarize" | "extract" | "code" | "reasoning" | "general";
  latencyBudgetMs?: number;
  preferCheap: boolean;
}

function selectModel(req: RoutingRequest): string {
  const candidates = Object.entries(MODEL_REGISTRY)
    .filter(([, cap]) => {
      if (req.hasImageInput && !cap.supportsVision) return false;
      if (req.requiresStructuredOutput && !cap.supportsStructuredOutput) return false;
      if (req.estimatedInputTokens > cap.contextLength) return false;
      if (req.latencyBudgetMs && req.latencyBudgetMs < 1000 && cap.latencyTier === "slow") return false;
      return true;
    })
    .map(([name]) => name);

  if (candidates.length === 0) throw new Error("No model satisfies constraints");

  const simpleTasks = new Set(["classify", "translate", "summarize", "extract"]);
  const complexTasks = new Set(["code", "reasoning"]);

  if (simpleTasks.has(req.taskType) || req.preferCheap) {
    // Prefer cheap tier
    return (
      candidates.find((m) => MODEL_REGISTRY[m].costTier === "cheap") ??
      candidates[0]
    );
  }

  if (complexTasks.has(req.taskType)) {
    // Prefer mid tier; fall back to frontier if no mid available
    return (
      candidates.find((m) => MODEL_REGISTRY[m].costTier === "mid") ??
      candidates.find((m) => MODEL_REGISTRY[m].costTier === "frontier") ??
      candidates[0]
    );
  }

  return candidates.find((m) => MODEL_REGISTRY[m].costTier === "mid") ?? candidates[0];
}
```

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
    modelName = "gpt-4o-mini";
  } else if (intent.requires_code || intent.requires_reasoning) {
    modelName = "gpt-4o";
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
  fast: "google/gemini-flash-1.5",
  balanced: "anthropic/claude-3-haiku",
  powerful: "anthropic/claude-sonnet-4-6",
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

function selectTier(task: string): ModelTier {
  if (task === "summarize" || task === "classify" || task === "translate") {
    return "fast";
  }
  if (task === "qa" || task === "rewrite") {
    return "balanced";
  }
  return "powerful";
}
```

---

## Fallback Chain with Timeout Budget

The core pattern: try the primary model within a time budget; if it fails or times out, fall back to a secondary model with whatever budget remains.

```typescript
import { ChatOpenAI } from "@langchain/openai";
import { ChatAnthropic } from "@langchain/anthropic";
import { BaseMessage } from "@langchain/core/messages";

interface FallbackOptions {
  totalBudgetMs: number;       // total time budget for the request
  primaryTimeoutMs: number;    // how long to wait for primary before falling back
}

async function withFallback(
  messages: BaseMessage[],
  options: FallbackOptions = { totalBudgetMs: 15_000, primaryTimeoutMs: 5_000 }
) {
  const start = Date.now();
  const primary = new ChatOpenAI({ model: "gpt-4o", timeout: options.primaryTimeoutMs });

  try {
    return await primary.invoke(messages);
  } catch (err) {
    const elapsed = Date.now() - start;
    const remaining = options.totalBudgetMs - elapsed;

    if (remaining < 500) {
      throw new Error("Total budget exhausted before fallback could run");
    }

    console.warn("Primary model failed, using fallback:", (err as Error).message);
    const fallback = new ChatAnthropic({
      model: "claude-haiku-4-5-20251001",
      timeout: remaining,
    });
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

## Circuit Breaker for Provider Outages

Don't keep retrying a failing provider. Track failure rate and open the circuit when it exceeds a threshold.

```typescript
type CircuitState = "closed" | "open" | "half-open";

interface CircuitBreakerConfig {
  failureThreshold: number;   // open circuit after this many failures
  resetTimeoutMs: number;     // try again after this duration
  halfOpenRequests: number;   // probe requests in half-open state
}

class CircuitBreaker {
  private state: CircuitState = "closed";
  private failures = 0;
  private successes = 0;
  private lastFailureTime = 0;

  constructor(
    private readonly name: string,
    private readonly config: CircuitBreakerConfig
  ) {}

  isAvailable(): boolean {
    if (this.state === "closed") return true;
    if (this.state === "open") {
      const elapsed = Date.now() - this.lastFailureTime;
      if (elapsed > this.config.resetTimeoutMs) {
        this.state = "half-open";
        this.successes = 0;
        return true;
      }
      return false;
    }
    // half-open: allow limited probing
    return this.successes < this.config.halfOpenRequests;
  }

  recordSuccess() {
    if (this.state === "half-open") {
      this.successes++;
      if (this.successes >= this.config.halfOpenRequests) {
        this.state = "closed";
        this.failures = 0;
      }
    } else {
      this.failures = 0;
    }
  }

  recordFailure() {
    this.failures++;
    this.lastFailureTime = Date.now();
    if (this.failures >= this.config.failureThreshold) {
      console.warn(`Circuit breaker [${this.name}] opened after ${this.failures} failures`);
      this.state = "open";
    }
  }
}

const openaiCircuit = new CircuitBreaker("openai", {
  failureThreshold: 5,
  resetTimeoutMs: 30_000,
  halfOpenRequests: 2,
});

const anthropicCircuit = new CircuitBreaker("anthropic", {
  failureThreshold: 5,
  resetTimeoutMs: 30_000,
  halfOpenRequests: 2,
});
```

---

## Rate Limit Management

Each provider has per-model rate limits (requests per minute, tokens per minute). Spreading across multiple API keys is the standard scaling approach.

```typescript
interface RateLimitState {
  requestsThisMinute: number;
  tokensThisMinute: number;
  windowStart: number;
  maxRequestsPerMinute: number;
  maxTokensPerMinute: number;
}

class RateLimiter {
  private state: RateLimitState;

  constructor(maxRPM: number, maxTPM: number) {
    this.state = {
      requestsThisMinute: 0,
      tokensThisMinute: 0,
      windowStart: Date.now(),
      maxRequestsPerMinute: maxRPM,
      maxTokensPerMinute: maxTPM,
    };
  }

  private resetIfNewWindow() {
    if (Date.now() - this.state.windowStart >= 60_000) {
      this.state.requestsThisMinute = 0;
      this.state.tokensThisMinute = 0;
      this.state.windowStart = Date.now();
    }
  }

  canMakeRequest(estimatedTokens: number): boolean {
    this.resetIfNewWindow();
    return (
      this.state.requestsThisMinute < this.state.maxRequestsPerMinute &&
      this.state.tokensThisMinute + estimatedTokens < this.state.maxTokensPerMinute
    );
  }

  recordRequest(tokensUsed: number) {
    this.resetIfNewWindow();
    this.state.requestsThisMinute++;
    this.state.tokensThisMinute += tokensUsed;
  }
}
```

On 429 (rate limit) responses, use exponential backoff with jitter:

```typescript
async function withBackoff<T>(
  fn: () => Promise<T>,
  maxRetries = 3,
  baseDelayMs = 1000
): Promise<T> {
  for (let attempt = 0; attempt <= maxRetries; attempt++) {
    try {
      return await fn();
    } catch (err: any) {
      if (attempt === maxRetries || err?.status !== 429) throw err;
      const delay = baseDelayMs * 2 ** attempt + Math.random() * 500;
      await new Promise((r) => setTimeout(r, delay));
    }
  }
  throw new Error("Unreachable");
}
```

---

## Observability for Routing Decisions

Every routed request should log enough to reconstruct why a model was selected and what it cost.

```typescript
interface RoutingLog {
  request_id: string;
  timestamp: string;
  selected_model: string;
  routing_reason: string;         // "task_type:classify" | "fallback:primary_timeout" | "cost_tier:cheap"
  fallback_triggered: boolean;
  input_tokens: number;
  output_tokens: number;
  estimated_cost_usd: number;
  latency_ms: number;
  circuit_breaker_state: Record<string, CircuitState>;
}

function logRoutingDecision(log: RoutingLog) {
  // Use structured logging — this feeds dashboards and cost alerts
  console.log(JSON.stringify({ level: "info", type: "routing", ...log }));
}
```

Track in your monitoring system:
- Cost per model per day (alert on unexpected spikes)
- Fallback rate (spike indicates primary provider degradation)
- Model distribution over time (sanity check that routing logic is working)
- P95 latency per model (validate tier assumptions)

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
|---|---|
| Mix of simple and complex queries | Route by complexity — save 80-90% on simple queries |
| High QPS with cost constraints | Route short queries to fast models, reserve powerful for complex ones |
| Multi-modal inputs | Route image/audio to vision/audio models |
| Reliability requirements | Configure fallback chain with circuit breaker |
| Long context inputs (> 100K tokens) | Route to Gemini 2.5 Pro or Claude with 200K context |
| Provider outage | Circuit breaker routes all traffic to secondary provider automatically |

---

## Interview Q&A

**Q: How do you decide where to put the routing logic — client side or a dedicated routing service?**

A dedicated routing service (or gateway) is better for anything beyond a single application. It centralizes cost tracking, rate limit state, circuit breaker state, and logging. Client-side routing duplicates that state across instances and makes it impossible to enforce organization-wide rate limits. The tradeoff: the routing service adds a network hop. For latency-sensitive applications, co-locate the router with the calling application.

**Q: How do you prevent the classifier in intent-based routing from becoming a bottleneck?**

Use the cheapest possible classifier (GPT-4o mini, ~0.3s TTFT). Cache classifications for common queries (exact-match or semantic cache). Set a tight timeout — if the classifier doesn't respond in 500ms, fall back to a default model. Avoid classification chains; a single LLM call with structured output is enough for most routing decisions.

**Q: What's the failure mode of a circuit breaker that opens too aggressively?**

All traffic routes to the secondary provider, which may also have limits. If both providers degrade simultaneously, an overly aggressive circuit breaker magnifies the outage. Tune `failureThreshold` against your actual error rate baseline — if your error rate is normally 1%, a threshold of 3 errors will cause false opens. Use error rate over a time window rather than raw error count.

**Q: How do you handle model deprecations in a routing system?**

The model registry pattern solves this: update the registry entry, not all call sites. Add a `deprecated_at` field and a `replacement` field to each model entry. Write a startup check that warns (or errors) if any configured model is past its deprecation date. Log the selected model name in every request so you can audit usage before a deadline.

---

## Related

- [../function-calling/README.md](../function-calling/README.md) — function calling that routing must support
- [../structured-output/README.md](../structured-output/README.md) — structured output capability affects routing
- [../prompting/README.md](../prompting/README.md) — prompt design affects which tier is needed
