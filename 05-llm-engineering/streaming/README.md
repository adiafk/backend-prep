# Streaming LLM Responses

## Why Stream?

LLMs generate tokens one at a time. Without streaming:
- User stares at a blank screen for 5-30 seconds
- First token latency (TTFT) is poor UX
- Network buffers the entire response before delivery

With streaming, the user sees tokens as they're generated — feels instant.

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
// Prints tokens as they arrive

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

## Streaming in Express (Server-Sent Events)

The standard pattern for web apps: Express sends SSE, frontend reads with `EventSource` or `fetch` with streaming body.

```typescript
import express from "express";
import { ChatOpenAI } from "@langchain/openai";

const app = express();
app.use(express.json());

app.post("/api/chat", async (req, res) => {
  const { message } = req.body;

  // SSE headers
  res.setHeader("Content-Type", "text/event-stream");
  res.setHeader("Cache-Control", "no-cache");
  res.setHeader("Connection", "keep-alive");

  const llm = new ChatOpenAI({ model: "gpt-4o", streaming: true });

  try {
    const stream = await llm.stream([{ role: "user", content: message }]);

    for await (const chunk of stream) {
      const text = chunk.content as string;
      if (text) {
        // SSE format: "data: <payload>\n\n"
        res.write(`data: ${JSON.stringify({ token: text })}\n\n`);
      }
    }

    res.write("data: [DONE]\n\n");
    res.end();
  } catch (err) {
    res.write(`data: ${JSON.stringify({ error: "Stream failed" })}\n\n`);
    res.end();
  }
});
```

---

## Streaming in Next.js (App Router)

```typescript
// app/api/chat/route.ts
import { StreamingTextResponse, LangChainStream } from "ai";  // Vercel AI SDK
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

  // Don't await — fire and forget so we can return the stream
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

## Consuming Streams on the Client

```typescript
// Using fetch with streaming body
async function streamChat(message: string, onToken: (t: string) => void) {
  const res = await fetch("/api/chat", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ message }),
  });

  const reader = res.body!.getReader();
  const decoder = new TextDecoder();

  while (true) {
    const { done, value } = await reader.read();
    if (done) break;

    const text = decoder.decode(value);
    // Parse SSE lines
    for (const line of text.split("\n")) {
      if (line.startsWith("data: ")) {
        const data = line.slice(6);
        if (data === "[DONE]") return;
        try {
          const { token } = JSON.parse(data);
          onToken(token);
        } catch {}
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

// stream() works on the full chain
const stream = await chain.stream({ topic: "transformer attention" });
for await (const chunk of stream) {
  process.stdout.write(chunk);
}
```

---

## Notes

- Always handle client disconnects — if the client closes the connection mid-stream, the `for await` loop should detect the abort signal to avoid wasted compute
- Token usage is only available after the stream completes (in `on_llm_end` callback)
- For long-running pipelines, stream intermediate results too (e.g., "Searching documents..." before the final answer starts streaming)
