# AI/ML Fundamentals

A practical grounding in the concepts that matter when building AI-powered backend systems. No PhD required — but you need to know enough to make good decisions in production.

---

## 1. What LLMs Are

### The One-Sentence Version

A Large Language Model (LLM) is a neural network trained to predict the next token in a sequence. That's it. Everything else — reasoning, code generation, summarization, conversation — emerges from doing that one thing extremely well at enormous scale.

### Transformer Architecture (Conceptual)

Before transformers (2017), language models processed text sequentially — word by word, left to right. The fundamental problem: by the time you were predicting word 100, you'd almost forgotten word 1.

Transformers solve this with **attention**: every token in the input can "look at" every other token simultaneously. The model learns which tokens are relevant to which others, regardless of distance.

Key components to understand conceptually:

**Tokenization** — raw text is split into tokens (roughly word-pieces). "unbelievable" might become ["un", "believ", "able"]. The model never sees characters or words directly.

**Embeddings** — each token is converted into a dense vector of numbers (e.g., 768 or 4096 dimensions). This vector encodes the token's meaning in a high-dimensional space.

**Attention layers** — the core innovation. Each layer allows every token to gather information from other tokens, weighted by learned relevance scores. "The bank by the river" vs "the bank that holds money" — attention learns that "bank" should pay attention to different context words in each case.

**Feed-forward layers** — after attention aggregates context, feed-forward networks process each token's representation to update it.

**Stacking** — transformers stack many of these attention + feed-forward blocks. GPT-4 likely has 96+ layers. Each layer builds on the last, constructing progressively more abstract representations.

**Output** — the final layer produces a probability distribution over the entire vocabulary. The model samples from this distribution to pick the next token.

Why this matters for engineers: the attention mechanism is why LLMs are so good at long-range dependencies, but it's also why context windows are computationally expensive — attention is O(n²) with sequence length.

### Pre-training vs Fine-tuning

These are two distinct training phases with fundamentally different purposes.

**Pre-training**

The model is trained on a massive corpus (trillions of tokens from the internet, books, code, etc.) to predict the next token. No labels, no human feedback — just "given these tokens, what comes next?"

This is where the model builds its world model: grammar, facts, reasoning patterns, code syntax, how arguments are structured. Pre-training costs tens of millions of dollars and takes months on thousands of GPUs. You will never do this.

What you get: a base model that can continue any text, but isn't particularly useful as an assistant.

**Fine-tuning**

Starting from a pre-trained base, the model is further trained on a smaller, curated dataset to shape its behavior.

Types you'll encounter:

- **Instruction fine-tuning (SFT)** — train on (instruction, response) pairs so the model follows instructions rather than just continuing text. This is what turns a base model into a useful assistant.

- **RLHF (Reinforcement Learning from Human Feedback)** — human raters score outputs; the model is trained to produce higher-rated outputs. This is how you get models that refuse harmful requests and produce helpful, well-structured responses.

- **Domain fine-tuning** — train on domain-specific data (medical records, legal documents, your company's codebase) to improve performance in that domain.

- **LoRA / parameter-efficient fine-tuning** — rather than updating all billions of parameters, train small adapter layers. Much cheaper. Widely used for customization.

When you should consider fine-tuning:
- You have thousands of high-quality labeled examples
- You need consistent output format that prompting alone can't reliably produce
- You need a smaller, faster, cheaper model to match a larger model's behavior on a specific task
- Latency or cost makes using frontier models impractical at your scale

When you should not fine-tune:
- You have fewer than ~1,000 examples (try few-shot prompting first)
- You want to inject new knowledge (fine-tuning doesn't reliably add facts — use RAG)
- You just want the model to follow specific instructions (prompting is faster and cheaper to iterate)

---

## 2. Tokens

### What They Are

Tokens are the units that LLMs process. They are not characters, not words — they sit somewhere between the two. Modern models use byte-pair encoding (BPE), which builds a vocabulary of common subword pieces.

Rules of thumb for English text:
- 1 token ≈ 4 characters
- 1 token ≈ 0.75 words
- 100 tokens ≈ 75 words ≈ a short paragraph
- 1,000 tokens ≈ 750 words ≈ a page of text

Code tokenizes differently — often more tokens per "meaningful unit" because variable names and syntax characters each become separate tokens.

Non-English languages tokenize less efficiently. A character in Chinese or Arabic might require 2-3 tokens whereas the equivalent meaning in English takes 1.

You can explore tokenization directly at [platform.openai.com/tokenizer](https://platform.openai.com/tokenizer).

### Why Tokens Matter for Cost

Most LLM APIs charge per token — separately for input (prompt) tokens and output (completion) tokens. Output tokens typically cost 3-5x more than input tokens.

Concrete example with approximate 2025 pricing:
```
claude-sonnet-4-5: $3/million input, $15/million output
claude-haiku-3:   $0.25/million input, $1.25/million output

A RAG query with 2,000 token context + 500 token response:
  Sonnet: (2000 × $3 + 500 × $15) / 1,000,000 = $0.0135 per query
  Haiku:  (2000 × $0.25 + 500 × $1.25) / 1,000,000 = $0.00113 per query
```

At 100,000 queries/day, that's $1,350/day vs $113/day. Model selection and prompt efficiency are real engineering decisions.

### Why Tokens Matter for Limits

Every model has a context window — a maximum number of tokens it can process in a single request (input + output combined). Exceeding this limit causes an error. You need to count tokens before sending requests.

### How to Estimate Token Counts

**Quick estimation** — divide character count by 4, or word count by 0.75.

**Exact counting** — use the tokenizer library:

```typescript
import Anthropic from "@anthropic-ai/sdk";

const client = new Anthropic();

// Count tokens before sending
const response = await client.messages.countTokens({
  model: "claude-sonnet-4-5",
  messages: [{ role: "user", content: "Hello, world!" }],
});
console.log(response.input_tokens); // exact count
```

For OpenAI models, use the `tiktoken` library:

```typescript
import { encoding_for_model } from "tiktoken";

const enc = encoding_for_model("gpt-4");
const tokens = enc.encode("Hello, world!");
console.log(tokens.length);
enc.free();
```

**Rule for production systems**: always count tokens for dynamic content (user messages, retrieved documents) and set hard limits before sending to the API. Never let an unbounded user input hit the API unchecked.

---

## 3. Context Window

### What It Is

The context window is the total number of tokens the model can "see" at once — both input and output combined. Think of it as the model's working memory. The model attends to everything in the window; it has no access to anything outside it.

Current context windows (2025):
- GPT-4o: 128K tokens
- Claude 3.5 Sonnet: 200K tokens
- Gemini 1.5 Pro: 1M tokens
- GPT-4 Turbo: 128K tokens

128K tokens sounds enormous (~96,000 words, ~192 pages). But in agentic applications it fills up faster than you expect:

- System prompt: 500-2,000 tokens
- Tool definitions: 200-500 tokens each
- Conversation history: grows with every turn
- Retrieved documents: 500-2,000 tokens each
- Structured output schemas: 200-1,000 tokens
- Reasoning steps / chain-of-thought: can be thousands of tokens

A multi-turn agentic workflow retrieving several documents can consume 50K-100K tokens per request.

### Why Context Windows Limit Agents

**Memory is transient** — the model has no memory between separate API calls. Every call starts fresh. Conversation history must be explicitly re-sent each time, which consumes tokens.

**Compounding costs** — in a conversation, turn N requires re-sending turns 1 through N-1. A 20-turn conversation costs quadratically more than 20 single-turn queries.

**Attention degradation** — empirically, models perform worse on information buried in the middle of very long contexts than on information near the beginning or end. This is called the "lost in the middle" problem. A 200K context window does not mean uniform perfect recall across 200K tokens.

**Throughput limits** — large context requests are slower and consume more compute. Providers often rate-limit differently for large-context requests.

**Tool call accumulation** — agentic loops where the model calls tools repeatedly accumulate function call results in the context. A loop that runs 20 tool calls can easily consume 40K tokens just in tool call/response pairs.

### Strategies to Work Around Context Limits

**Sliding window / truncation** — keep the most recent N tokens of conversation. Simple but loses early context. Best for chat applications where recent context matters more.

```typescript
function truncateMessages(
  messages: Message[],
  maxTokens: number
): Message[] {
  let totalTokens = 0;
  const result: Message[] = [];

  // Walk backwards — keep most recent
  for (let i = messages.length - 1; i >= 0; i--) {
    const msgTokens = estimateTokens(messages[i].content);
    if (totalTokens + msgTokens > maxTokens) break;
    result.unshift(messages[i]);
    totalTokens += msgTokens;
  }
  return result;
}
```

**Summarization** — when context approaches the limit, ask the model to summarize earlier portions of the conversation, then replace those messages with the summary. Loses detail but preserves gist.

**RAG (Retrieval-Augmented Generation)** — instead of putting all documents in the context, store them in a vector database and retrieve only the relevant chunks per query. This is the primary architectural response to context limits.

**Caching** — if your system prompt and tool definitions are static, use prompt caching (supported by Claude and others). Cached tokens are cheaper and faster. A 2,000-token system prompt sent with every request costs real money at scale.

```typescript
// Claude prompt caching
const response = await client.messages.create({
  model: "claude-sonnet-4-5",
  messages: [{
    role: "user",
    content: [{
      type: "text",
      text: systemPromptContent,
      cache_control: { type: "ephemeral" }, // cache this
    }],
  }],
});
```

**Selective tool inclusion** — don't include all tool definitions in every request. Dynamically include only the tools relevant to the current task.

**External memory** — use a database as the model's memory. Summarize and store important information externally; retrieve it as needed rather than keeping everything in context.

---

## 4. Temperature, Top-p, Top-k

These parameters control how the model samples from the probability distribution over tokens. Understanding them helps you tune outputs for different use cases.

### The Mechanics

After computing probabilities for all ~100,000 vocabulary tokens, the model doesn't always pick the most probable one. These parameters control the sampling process.

### Temperature

**What it does** — scales the probability distribution before sampling. Higher temperature flattens the distribution (more uniform, more random); lower temperature sharpens it (concentrates probability on the top tokens).

**Range** — typically 0.0 to 2.0. Default is usually 1.0.

- `temperature: 0.0` — deterministic (always picks the highest-probability token). Reproducible outputs.
- `temperature: 0.2-0.5` — focused, consistent, low creativity. Good for extraction, classification, structured output.
- `temperature: 0.7-1.0` — balanced. Good for general-purpose chat and writing.
- `temperature: 1.2-2.0` — highly varied, creative, sometimes incoherent. Use sparingly.

**Analogy** — temperature is like the thermostat on brainstorming. Low: everyone agrees on the safe answer. High: everyone shouts random ideas.

### Top-p (Nucleus Sampling)

**What it does** — rather than sampling from the full vocabulary, top-p restricts sampling to the smallest set of tokens whose cumulative probability exceeds p.

`top_p: 0.9` means: take the most probable tokens until their probabilities add up to 90%, then sample only from those.

**Why this is useful** — it adapts dynamically. When the model is confident (one token has 95% probability), top-p = 0.9 effectively picks that token. When the model is uncertain (probabilities are spread), top-p allows more diversity.

**Range** — 0.0 to 1.0. `top_p: 1.0` means no restriction (use all tokens).

### Top-k

**What it does** — restricts sampling to only the k most probable tokens, regardless of their actual probabilities.

`top_k: 50` means: sample only from the 50 highest-probability tokens.

**Difference from top-p** — top-k always considers exactly k tokens; top-p considers however many tokens are needed to reach the probability threshold. Top-p is generally preferred because it adapts to the model's confidence level.

### Practical Settings by Use Case

| Use Case | Temperature | Top-p | Notes |
|---|---|---|---|
| Data extraction (JSON, structured) | 0.0 - 0.2 | 0.9 | Deterministic, consistent format |
| Code generation | 0.2 - 0.4 | 0.9 | Correct over creative |
| Classification / routing | 0.0 | 1.0 | Should always give same answer |
| Q&A / factual responses | 0.3 - 0.5 | 0.9 | Accurate with some flexibility |
| Conversational assistant | 0.7 | 0.9 | Natural, varied responses |
| Creative writing / brainstorming | 0.9 - 1.2 | 0.95 | High variety |
| Marketing copy variations | 1.0 - 1.4 | 0.95 | Diverse options |

**The interaction problem** — temperature and top-p interact. Using both set to non-default values makes behavior harder to predict. Common advice: use temperature alone, or use top-p alone. Don't tune both simultaneously unless you have a specific reason.

**For agents** — use low temperature (0.1-0.3) for tool selection and structured decisions. The model should reliably pick the right tool, not randomly vary its choices. Higher temperature is only appropriate for the final text generation step.

---

## 5. Hallucination

### What Hallucination Is

Hallucination is when an LLM generates text that is factually incorrect, fabricated, or not grounded in the provided context — but states it with full confidence.

Examples:
- Citing papers that don't exist
- Stating a function signature that was deprecated two versions ago
- Inventing details about a person that are plausible but false
- Claiming to have performed actions it didn't perform
- Agreeing that a code snippet "looks right" when it has a bug

Hallucination is not a bug that will be fixed — it is a fundamental property of how LLMs work. The model generates the most statistically likely continuation, not necessarily the true one.

### Why It Happens

LLMs don't have a fact database and a retrieval mechanism. They have learned statistical associations between tokens. "The capital of France" strongly associates with "Paris" — that's a reliable pattern. "The phone number of the CEO of Acme Corp" has no strong association in training data, so the model invents a plausible-sounding number.

Key causes:

**Training data gaps** — the model doesn't know what it doesn't know. If a fact wasn't well-represented in training data, the model will confabulate rather than say "I don't know."

**Sycophancy** — models trained with RLHF learn that agreeable, confident responses get better ratings. This can make models confirm incorrect premises rather than push back.

**Instruction following** — if you ask "what are five examples of X" and there are only three real examples, the model will often fabricate two more to satisfy the format requirement.

**Knowledge cutoff** — training data has a cutoff date. Anything after that cutoff is unknown, but the model may still attempt to answer.

**Long context degradation** — in very long contexts, the model may lose track of constraints stated early in the prompt.

### How to Detect Hallucination

**Self-consistency** — ask the model the same question multiple times with different phrasing or different seeds. If the answers disagree significantly, the model is uncertain. Confident, consistent answers are more likely to be correct (though not guaranteed).

**Citation checking** — if the model cites sources, check them. Fabricated citations are a common form of hallucination. Ask for DOIs, URLs, exact quotes — these are hard to fabricate accurately.

**Grounding verification** — for RAG systems, check whether the model's claims are actually supported by the retrieved documents. You can do this with another LLM call:

```
System: You are a fact-checker. Given the provided source documents and a response,
identify any claims in the response that are not supported by the source documents.

User: [documents]
[response to check]
```

**Confidence calibration** — models often use hedging language when uncertain ("I believe", "I think", "approximately"). Train yourself to notice when confident language doesn't match the certainty of the domain.

**Unit tests for critical outputs** — for structured outputs (JSON, function calls), validate against a schema. For factual outputs, maintain a test set of known ground-truth answers and measure accuracy.

### How to Reduce Hallucination

**RAG (Retrieval-Augmented Generation)** — the most effective intervention. Instead of relying on the model's parametric memory, provide the relevant information in the prompt. "Based only on the following documents, answer the question. If the answer is not in the documents, say so."

**Explicit grounding instructions** — tell the model to cite specific passages, quote the source, and say "I don't know" when uncertain. This shifts the failure mode from hallucination to abstention, which is much more debuggable.

```
Answer the question using only the provided context.
If the context does not contain sufficient information to answer,
respond with: "I don't have enough information to answer this question."
Do not use your general knowledge.
```

**Lower temperature** — reduces randomness in generation. Doesn't eliminate hallucination but reduces the variance in outputs.

**Chain-of-thought** — asking the model to reason step by step ("think through this before answering") can improve accuracy by forcing more deliberate generation, catching errors before they become the final answer.

**Structured output with validation** — for factual extraction, define a JSON schema and validate the output. Add fields like `confidence: low | medium | high` and `source_quote: string` to make uncertainty explicit and checkable.

**Small, focused prompts** — the model is more reliable on focused, well-scoped questions than on broad, open-ended ones. Break complex queries into smaller, verifiable steps.

**Model selection** — newer, larger models generally hallucinate less on common topics. Use the most capable model you can afford for high-stakes factual tasks.

---

## 6. Embedding Models vs Generative Models

These are fundamentally different kinds of models, designed for different jobs. Confusing them leads to architectural mistakes.

### Generative Models

**Purpose** — generate text. Given a prompt, produce a continuation.

**Output** — tokens (text), one at a time.

**Examples** — Claude, GPT-4, Gemini, Llama, Mistral.

**Architecture** — decoder-only transformer (for most modern models). The model autoregressively generates tokens left to right.

**Use cases**:
- Chat and conversation
- Code generation
- Summarization
- Question answering
- Instruction following
- Reasoning and planning

**Cost model** — charged per input + output token. Output is expensive because it requires sequential generation (each token requires a forward pass).

### Embedding Models

**Purpose** — encode meaning. Given a piece of text, produce a fixed-size numerical vector that captures its semantic content.

**Output** — a single vector (e.g., 1536 floats). Not text.

**Examples** — `text-embedding-3-small` (OpenAI), `text-embedding-3-large` (OpenAI), `voyage-3` (Anthropic/Voyage), `nomic-embed-text` (open source), `bge-large` (open source).

**Architecture** — typically encoder-only transformer (like BERT), or encoder part of an encoder-decoder. The entire input is processed simultaneously; there is no sequential generation.

**Use cases**:
- Semantic search — find documents similar in meaning to a query
- RAG — convert documents and queries to vectors for retrieval
- Clustering — group similar documents together
- Classification — feed embeddings to a classifier
- Duplicate detection — identify near-duplicate content
- Recommendation — find items similar to what a user liked

**Cost model** — charged per input token only. Much cheaper than generative models because there's no token-by-token generation. OpenAI's `text-embedding-3-small` costs $0.02/million tokens — roughly 150x cheaper than GPT-4o input.

### Key Differences

| Dimension | Generative Model | Embedding Model |
|---|---|---|
| Output | Text (tokens) | Vector (floats) |
| Purpose | Produce content | Encode meaning |
| Latency | Slow (sequential generation) | Fast (single forward pass) |
| Cost | High | Low |
| Context window | Matters for quality | Matters for what fits |
| Can reason? | Yes | No |
| Can compare documents? | Via prompt | Via vector similarity |

### The Architectural Pattern

In production RAG systems, both types are used together:

1. **Offline (indexing)** — use an embedding model to encode all your documents into vectors. Store in a vector database.
2. **Online (query)** — use the same embedding model to encode the user's query. Find similar document vectors. Pass the matching document text to a generative model with the query. Return the generative model's response.

The embedding model is a fast, cheap lookup mechanism. The generative model is the expensive reasoning engine. Keep them separate and use each for what it's good at.

### Choosing Between Them

Use a generative model when you need text output — responses, summaries, generated code.

Use an embedding model when you need to compare or retrieve — similarity search, clustering, finding relevant context.

Use both when you need to retrieve relevant information and then reason about it — which is most RAG and agentic applications.

---

## Summary

| Concept | The Key Insight |
|---|---|
| LLMs | Next-token prediction at scale. Transformers enable parallel attention over full context. |
| Pre-training vs fine-tuning | Pre-training builds world knowledge; fine-tuning shapes behavior. Use RAG for facts, fine-tuning for format. |
| Tokens | The unit of cost and limit. 1 token ≈ 4 chars. Count before sending. |
| Context window | Working memory. Limited and expensive. RAG and caching are the main mitigations. |
| Temperature | Controls randomness. Low for structured tasks, higher for creative work. |
| Top-p / top-k | Shape the sampling distribution. Prefer top-p over top-k. Don't stack with temperature without reason. |
| Hallucination | Fundamental, not fixable. Mitigate with RAG, grounding instructions, and validation. |
| Embeddings vs generative | Embeddings encode meaning cheaply; generative models reason expensively. Use both together in RAG. |
