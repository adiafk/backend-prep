# Streaming LLM Responses

## Why Stream?

LLMs generate tokens one at a time. Without streaming:
- User stares at a blank screen for 5-30 seconds
- Time to first token (TTFT) is the dominant latency perception factor
- Network buffers the entire response before delivery

With streaming, the user sees tokens as they're generated — feels instant. TTFT and total latency are separate SLOs: a request might have 400ms TTFT (good) and 12s total (expected for a long response). Track them independently.

---

## Server-Sent Events Protocol

SSE is the standard transport for streaming LLM responses to browsers. It's one-directional (server to client), text-based, and works over regular HTTP.

### Wire Format

```
HTTP/1.1 200 OK
Content-Type: text/event-stream
Cache-Control: no-cache
Connection: keep-alive

data: {"token": "Hello"}\n\n
data: {"token": " world"}\n\n
data: [DONE]\n\n
```

Each SSE message:
- Prefixed with `data: `
- Terminated with **two newlines** (`\n\n`)
- Optional `id:` field for reconnection: `id: 42\n data: ...\n\n`
- Optional `retry:` field tells the client how long to wait before reconnecting: `retry: 3000\n\n` (milliseconds)

### SSE Fields

```
event: token          ← named event type (optional; default is "message")
id: 42                ← last event ID; sent as "Last-Event-ID" header on reconnect
retry: 3000           ← reconnect interval in ms if connection drops
data: {"t": "Hello"}  ← payload; multi-line: repeat "data:" prefix per line
                      ← blank line terminates this event
```

### Proxy and Nginx Buffering

Proxies buffer responses by default. SSE events accumulate in the proxy buffer and get delivered in bursts, defeating the purpose. Fix:

**Nginx**:
```nginx
location /api/chat {
    proxy_pass http://backend;
    proxy_buffering off;           # disable response buffering
    proxy_cache off;
    proxy_read_timeout 300s;       # SSE connections are long-lived
    proxy_set_header X-Accel-Buffering no;  # alternative: set per-response
}
```

**Express** (set header per response):
```typescript
res.setHeader("X-Accel-Buffering", "no");  // tells Nginx not to buffer this response
```

### Browser EventSource API

The browser's built-in `EventSource` handles reconnection automatically:

```typescript
const source = new EventSource("/api/chat?sessionId=abc");

source.onmessage = (event) => {
  if (event.data === "[DONE]") {
    source.close();
    return;
  }
  const { token } = JSON.parse(event.data);
  appendToken(token);
};

source.onerror = (err) => {
  // EventSource reconnects automatically after retry interval
  // Close explicitly if you don't want reconnection
  source.close();
};
```

`EventSource` only supports GET and cannot send a body. For POST-based LLM APIs, use `fetch` with a `ReadableStream` instead.

---

## Chunked Transfer Encoding

The HTTP mechanism underlying streaming. When the server doesn't know the response length upfront, it uses `Transfer-Encoding: chunked`:

```
HTTP/1.1 200 OK
Transfer-Encoding: chunked

1a\r\n                    ← chunk size in hexadecimal (26 bytes)
Hello this is a chunk\r\n ← chunk data + CRLF
0e\r\n                    ← next chunk (14 bytes)
and another one\r\n
0\r\n                     ← terminal chunk (size 0 = end of body)
\r\n
```

You don't implement this directly — `res.write()` in Node.js uses chunked encoding automatically when `Content-Length` is not set. SSE is built on top of chunked transfer encoding.

---

## ReadableStream and Async Iteration in Node.js

With native `fetch` in Node 18+, response bodies are `ReadableStream`:

```typescript
async function streamFromAPI(url: string, body: object): Promise<void> {
  const response = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });

  if (!response.ok) throw new Error(`HTTP ${response.status}`);
  if (!response.body) throw new Error("No response body");

  const reader = response.body.getReader();
  const decoder = new TextDecoder();

  try {
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      const text = decoder.decode(value, { stream: true });  // stream:true handles multi-byte chars split across chunks
      process.stdout.write(text);
    }
  } finally {
    reader.releaseLock();  // always release, even on error
  }
}
```

With `for await...of` (Node 22+ or with ReadableStream async iterator):

```typescript
for await (const chunk of response.body) {
  process.stdout.write(new TextDecoder().decode(chunk));
}
```

---

## How OpenAI and Anthropic Streaming APIs Work

### OpenAI Delta Format

OpenAI sends a stream of `chat.completion.chunk` objects:

```json
{"choices":[{"delta":{"role":"assistant"},"finish_reason":null}]}
{"choices":[{"delta":{"content":"Hello"},"finish_reason":null}]}
{"choices":[{"delta":{"content":" world"},"finish_reason":null}]}
{"choices":[{"delta":{},"finish_reason":"stop"}]}
```

The first chunk carries the role. Subsequent chunks carry content deltas. The final chunk has `finish_reason` set. Tool call streaming adds `tool_calls` deltas with partial JSON that must be accumulated.

### Anthropic Delta Format

Anthropic uses typed events:

```json
{"type":"message_start","message":{"id":"msg_01","role":"assistant",...}}
{"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}
{"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hello"}}
{"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":" world"}}
{"type":"content_block_stop","index":0}
{"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":12}}
{"type":"message_stop"}
```

---

## Streaming with LangChain

```typescript
import { ChatOpenAI } from "@langchain/openai";
import { HumanMessage } from "@langchain/core/messages";

const llm = new ChatOpenAI({ model: "gpt-4o", streaming: true });

// Method 1: stream() — async iterable of chunks
const stream = await llm.stream([new HumanMessage("Explain RAG in 3 sentences")]);

for await (const chunk of stream) {
  process.stdout.write(chunk.content as string);
}

// Method 2: streamEvents() — fine-grained events
const eventStream = llm.streamEvents(
  [new HumanMessage("Explain RAG")],
  { version: "v2" }
);

for await (const event of eventStream) {
  if (event.event === "on_chat_model_stream") {
    process.stdout.write(event.data.chunk.content);
  }
}
```

---

## Streaming in Express (SSE Endpoint)

```typescript
import express from "express";
import { ChatOpenAI } from "@langchain/openai";

const app = express();
app.use(express.json());

app.post("/api/chat", async (req, res) => {
  const { message } = req.body;

  // SSE headers — set before any write
  res.setHeader("Content-Type", "text/event-stream");
  res.setHeader("Cache-Control", "no-cache");
  res.setHeader("Connection", "keep-alive");
  res.setHeader("X-Accel-Buffering", "no");  // disable Nginx buffering
  res.flushHeaders();  // CRITICAL: flush headers immediately so client can start reading
                       // Without this, headers are buffered until first write

  const llm = new ChatOpenAI({ model: "gpt-4o", streaming: true });

  // Handle client disconnect — stop generating when client is gone
  const abortController = new AbortController();
  req.on("close", () => abortController.abort());

  try {
    const stream = await llm.stream(
      [{ role: "user", content: message }],
      { signal: abortController.signal }
    );

    for await (const chunk of stream) {
      if (abortController.signal.aborted) break;
      const text = chunk.content as string;
      if (text) {
        res.write(`data: ${JSON.stringify({ token: text })}\n\n`);
      }
    }

    res.write("data: [DONE]\n\n");
  } catch (err: any) {
    if (err?.name !== "AbortError") {
      res.write(`data: ${JSON.stringify({ error: "Stream failed" })}\n\n`);
    }
  } finally {
    res.end();
  }
});
```

**Common mistake**: omitting `res.flushHeaders()`. Without it, the `Content-Type: text/event-stream` header isn't sent until the first `res.write()`, and some proxies use the content-type to decide whether to buffer — so they buffer the first chunk before seeing the header, causing a delay.

**Gzip middleware**: if you have compression middleware (`express.compress()`, `compression` package), it buffers chunks to compress them, which defeats streaming. Disable compression for SSE routes:

```typescript
import compression from "compression";

// Apply compression to all routes EXCEPT SSE endpoints
app.use(compression({
  filter: (req, res) => {
    if (req.headers.accept?.includes("text/event-stream")) return false;
    return compression.filter(req, res);
  },
}));
```

---

## Streaming in Next.js (App Router)

```typescript
// app/api/chat/route.ts
import { StreamingTextResponse, LangChainStream } from "ai";
import { ChatOpenAI } from "@langchain/openai";
import { HumanMessage } from "@langchain/core/messages";

export async function POST(req: Request) {
  const { messages } = await req.json();

  const { stream, handlers } = LangChainStream();

  const llm = new ChatOpenAI({
    model: "gpt-4o",
    streaming: true,
    callbacks: [handlers],
  });

  llm.call(messages.map((m: any) => new HumanMessage(m.content)));

  return new StreamingTextResponse(stream);
}
```

```typescript
// Frontend: app/page.tsx
"use client";
import { useChat } from "ai/react";

export default function ChatPage() {
  const { messages, input, handleInputChange, handleSubmit, isLoading } = useChat({
    api: "/api/chat",
  });

  return (
    <div>
      {messages.map((m) => (
        <div key={m.id}>
          <b>{m.role}</b>: {m.content}
        </div>
      ))}
      <form onSubmit={handleSubmit}>
        <input value={input} onChange={handleInputChange} />
        <button type="submit" disabled={isLoading}>Send</button>
      </form>
    </div>
  );
}
```

---

## Consuming Streams in React (Manual)

When you need more control than `useChat`:

```typescript
import { useState, useCallback } from "react";

function useLLMStream(apiUrl: string) {
  const [tokens, setTokens] = useState<string[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const send = useCallback(async (message: string) => {
    setTokens([]);
    setLoading(true);
    setError(null);

    const controller = new AbortController();

    try {
      const res = await fetch(apiUrl, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ message }),
        signal: controller.signal,
      });

      if (!res.ok || !res.body) throw new Error(`HTTP ${res.status}`);

      const reader = res.body.getReader();
      const decoder = new TextDecoder();
      let buffer = "";

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        buffer += decoder.decode(value, { stream: true });

        // Process complete SSE lines from buffer
        const lines = buffer.split("\n");
        buffer = lines.pop() ?? "";  // keep incomplete last line in buffer

        for (const line of lines) {
          if (!line.startsWith("data: ")) continue;
          const data = line.slice(6);
          if (data === "[DONE]") return;
          try {
            const { token } = JSON.parse(data);
            setTokens((prev) => [...prev, token]);
          } catch {
            // malformed chunk; skip
          }
        }
      }
    } catch (err: any) {
      if (err.name !== "AbortError") {
        setError(err.message);
      }
    } finally {
      setLoading(false);
    }

    return () => controller.abort();
  }, [apiUrl]);

  return { text: tokens.join(""), loading, error, send };
}
```

**Note**: calling `setTokens((prev) => [...prev, token])` on every token causes a React render per token. For high-frequency updates, batch with `useRef` and flush on `requestAnimationFrame`:

```typescript
const tokenBuffer = useRef<string[]>([]);
const [displayText, setDisplayText] = useState("");

// In the reader loop:
tokenBuffer.current.push(token);

// Flush at animation frame rate instead of per-token
requestAnimationFrame(() => {
  if (tokenBuffer.current.length > 0) {
    setDisplayText((prev) => prev + tokenBuffer.current.join(""));
    tokenBuffer.current = [];
  }
});
```

---

## Backpressure

What happens when the client reads slower than the server writes:

1. Server writes to socket → kernel TCP send buffer fills
2. TCP flow control kicks in — client advertises zero receive window
3. Server `res.write()` blocks (or returns false in Node.js streams)
4. If server ignores backpressure and keeps writing, kernel buffers overflow and data is dropped

In Node.js streams, `res.write()` returns `false` when the internal buffer is full. You should pause the source and wait for the `drain` event:

```typescript
for await (const chunk of llmStream) {
  const canContinue = res.write(`data: ${JSON.stringify({ token: chunk.content })}\n\n`);
  if (!canContinue) {
    // Buffer full — wait until drained before writing more
    await new Promise<void>((resolve) => res.once("drain", resolve));
  }
}
```

The `AbortSignal` approach handles this more practically: when the client disconnects (slow client eventually gives up), the abort signal fires and you stop generating, avoiding wasted LLM compute.

---

## Streaming Tool Calls

Tool call arguments arrive as partial JSON deltas. You must accumulate them before parsing.

```typescript
import OpenAI from "openai";

const openai = new OpenAI();

async function streamWithTools(userMessage: string) {
  const stream = await openai.chat.completions.create({
    model: "gpt-4o",
    stream: true,
    messages: [{ role: "user", content: userMessage }],
    tools: [{ type: "function", function: { name: "search", parameters: { type: "object", properties: { query: { type: "string" } } } } }],
  });

  // Accumulate partial tool call JSON
  const toolCallAccumulators: Record<number, { id: string; name: string; arguments: string }> = {};
  let textContent = "";

  for await (const chunk of stream) {
    const choice = chunk.choices[0];
    if (!choice) continue;

    // Accumulate text content
    if (choice.delta.content) {
      textContent += choice.delta.content;
      process.stdout.write(choice.delta.content);
    }

    // Accumulate tool call argument deltas
    for (const toolCall of choice.delta.tool_calls ?? []) {
      const idx = toolCall.index;
      if (!toolCallAccumulators[idx]) {
        toolCallAccumulators[idx] = { id: toolCall.id ?? "", name: toolCall.function?.name ?? "", arguments: "" };
      }
      toolCallAccumulators[idx].arguments += toolCall.function?.arguments ?? "";
    }

    if (choice.finish_reason === "tool_calls") {
      // All tool call JSON is now accumulated — parse and execute
      for (const accumulated of Object.values(toolCallAccumulators)) {
        const args = JSON.parse(accumulated.arguments);  // safe to parse now; it's complete
        console.log(`\nTool call: ${accumulated.name}(${JSON.stringify(args)})`);
        // Execute tool, stream continuation...
      }
    }
  }
}
```

---

## Streaming with Chains / Pipelines

```typescript
import { PromptTemplate } from "@langchain/core/prompts";
import { StringOutputParser } from "@langchain/core/output_parsers";

const chain = PromptTemplate.fromTemplate("Explain {topic} in simple terms")
  .pipe(new ChatOpenAI({ model: "gpt-4o", streaming: true }))
  .pipe(new StringOutputParser());

const stream = await chain.stream({ topic: "transformer attention" });
for await (const chunk of stream) {
  process.stdout.write(chunk);
}
```

---

## TTFT vs Total Latency as Separate SLOs

```
                time →
[request sent]──[first token received]──────────[last token received]
               └── TTFT ──────────────────────────── total latency ─┘
```

**TTFT** (time to first token): what users perceive as "responsiveness." Driven by model processing time before generation starts — prompt tokenization, attention computation over the input. For large prompts (RAG context), TTFT is higher because more input is processed.

**Total latency**: TTFT + (tokens generated × time per token). For a 500-token response at 50 tokens/second, add ~10s to TTFT.

**SLO examples**:
- TTFT P95: < 1.5s (user perception threshold)
- Total latency P95: < 15s (acceptable for long-form generation)

Track both separately in your observability stack. A regression in TTFT with unchanged total latency usually indicates a prompt got longer. A regression in total latency with unchanged TTFT indicates more tokens are being generated.

```typescript
interface StreamingMetrics {
  request_id: string;
  model: string;
  input_tokens: number;
  output_tokens: number;
  ttft_ms: number;          // time to first token
  total_latency_ms: number;
  tokens_per_second: number; // output_tokens / ((total_latency_ms - ttft_ms) / 1000)
}
```

---

## Common Mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Missing `res.flushHeaders()` | Client waits for first chunk before getting headers; some clients timeout | Call `res.flushHeaders()` immediately after setting SSE headers |
| Gzip middleware buffering | Chunks arrive in bursts, not one at a time | Disable compression for `text/event-stream` routes |
| Not handling client disconnect | LLM generates tokens for a gone client; wasted cost | Listen for `req.on("close")` and abort the stream |
| Parsing SSE chunks immediately without buffering | Crashes on chunks split across TCP segments | Buffer `decoder.decode(value, {stream:true})`, split on `\n`, keep incomplete lines |
| Returning token usage mid-stream | `usage` is only available after `finish_reason` | Collect usage from the final chunk or `on_llm_end` callback |
| SSE on a route behind session middleware | Session cookies can't be set after headers are flushed | Authenticate before SSE begins |
| Nginx default buffering on SSE route | Events delayed until proxy buffer fills | Set `proxy_buffering off` for SSE location blocks |

---

## Notes

- Always handle client disconnects — if the client closes the connection mid-stream, the `for await` loop should detect the abort signal to avoid wasted compute
- Token usage is only available after the stream completes (in `on_llm_end` callback or the final chunk's `usage` field)
- For long-running pipelines, stream intermediate results too (e.g., "Searching documents..." before the final answer starts streaming)
- `TextDecoder` with `{stream: true}` is required to correctly handle multi-byte UTF-8 characters split across chunk boundaries

---

## Related

- [../model-routing/README.md](../model-routing/README.md) — routing affects TTFT because different models have different latency tiers
- [../structured-output/README.md](../structured-output/README.md) — structured output and streaming have interaction: JSON mode often can't stream until the full object is valid
- [../../02-backend/websockets/README.md](../../02-backend/websockets/README.md) — alternative to SSE for bidirectional streaming
