# Structured Output

Getting reliable structured data out of language models is one of the core challenges of LLM engineering. Without enforcement, you are parsing human-readable text and hoping the model cooperates. With the right approach, you get typed, validated data you can pass directly to your application.

---

## 1. Why Structured Output

### The Problem with Free-Text Parsing

Naive approach:
```typescript
const response = await client.messages.create({ ... });
const text = response.content[0].text;
// Now what? The model might return:
// "The sentiment is positive."
// "Positive."
// "Sentiment: POSITIVE"
// "Based on my analysis, the sentiment appears to be positive with high confidence."
```

Regex and string parsing on LLM output breaks constantly. Models are inconsistent about formatting, especially across:
- Different input lengths
- Edge cases in the data
- Minor prompt changes
- Model updates

### What Structured Output Gives You

- Type-safe data you can pass directly to downstream functions
- Validation at the boundary — catch model errors before they propagate
- Predictable schemas — your TypeScript types match what comes out
- Easier testing — mock the schema, not raw text

Structured output shifts the reliability problem from "will the model format correctly" to "does the output match the schema" — the second is testable and retryable.

---

## 2. JSON Mode vs Function Calling vs withStructuredOutput

### JSON Mode

JSON mode tells the model to produce valid JSON. It does not enforce a specific schema — just syntactic validity.

Available in OpenAI-compatible APIs:
```typescript
const response = await openai.chat.completions.create({
  model: "gpt-4o",
  response_format: { type: "json_object" },
  messages: [...]
});
```

**Limitations**:
- No schema enforcement — the model decides what fields to include
- You still need to validate and parse the output yourself
- Does not work well if you forget to mention JSON in the prompt

**When to use it**: Quick prototyping, or when your schema varies and you want freeform JSON.

### Function Calling (Tool Use)

Function/tool calling lets you define a schema using JSON Schema. The model is constrained to emit an object matching that schema.

```typescript
const response = await openai.chat.completions.create({
  model: "gpt-4o",
  tools: [{
    type: "function",
    function: {
      name: "extract_contact",
      description: "Extract contact information from text",
      parameters: {
        type: "object",
        properties: {
          name: { type: "string" },
          email: { type: "string", format: "email" },
          phone: { type: "string" }
        },
        required: ["name"]
      }
    }
  }],
  tool_choice: { type: "function", function: { name: "extract_contact" } },
  messages: [...]
});
```

The model emits a `tool_calls` array with the JSON arguments. You parse `tool_calls[0].function.arguments`.

**Advantages over JSON mode**:
- Schema is enforced by the API, not just the prompt
- `required` fields are guaranteed to be present
- Works well with OpenAI's structured outputs feature (stricter enforcement)

**When to use it**: Any time you need a specific schema. This is the standard approach.

### withStructuredOutput (LangChain)

LangChain's `withStructuredOutput` wraps a model to return a typed object directly. It uses function calling under the hood when available and falls back to prompt-based parsing.

```typescript
import { ChatOpenAI } from "@langchain/openai";
import { z } from "zod";

const schema = z.object({
  sentiment: z.enum(["positive", "negative", "neutral"]),
  confidence: z.number().min(0).max(1),
  reasoning: z.string()
});

const model = new ChatOpenAI({ model: "gpt-4o" });
const structuredModel = model.withStructuredOutput(schema);

const result = await structuredModel.invoke("Analyze: 'Best purchase I've made all year'");
// result is typed as { sentiment: "positive" | "negative" | "neutral", confidence: number, reasoning: string }
```

**Advantages**:
- Returns a typed TypeScript object, not a string
- Integrates Zod validation automatically
- Works across providers — same API for OpenAI, Anthropic, Google
- Handles the parsing boilerplate

**When to use it**: LangChain projects, multi-provider support, or when you want the cleanest developer experience.

### Comparison

| | JSON Mode | Function Calling | withStructuredOutput |
|---|---|---|---|
| Schema enforced | No | Yes | Yes (via Zod) |
| Type-safe output | No | No (string → parse) | Yes |
| Multi-provider | N/A | Varies | Yes |
| Boilerplate | Medium | Medium | Low |
| Best for | Quick prototyping | Direct API use | LangChain apps |

---

## 3. Zod Schema Patterns for Common Use Cases

### Classification

```typescript
import { z } from "zod";

const ClassificationSchema = z.object({
  category: z.enum(["bug", "feature", "question", "spam"]),
  confidence: z.number().min(0).max(1),
  reasoning: z.string().optional()
});

type Classification = z.infer<typeof ClassificationSchema>;
```

### Extraction with Optional Fields

```typescript
const ContactSchema = z.object({
  name: z.string(),
  email: z.string().email().optional(),
  phone: z.string().optional(),
  company: z.string().optional(),
  // Use nullable when the model might explicitly say "not found"
  title: z.string().nullable()
});
```

### Lists

```typescript
const DocumentSummarySchema = z.object({
  title: z.string(),
  keyPoints: z.array(z.string()).min(1).max(10),
  entities: z.array(z.object({
    name: z.string(),
    type: z.enum(["person", "organization", "location", "date"])
  }))
});
```

### Nested Structures

```typescript
const CodeReviewSchema = z.object({
  findings: z.array(z.object({
    file: z.string(),
    line: z.number().int().positive(),
    severity: z.enum(["critical", "warning", "info"]),
    message: z.string(),
    suggestion: z.string().optional()
  })),
  summary: z.string(),
  approved: z.boolean()
});
```

### Discriminated Unions

```typescript
const ActionSchema = z.discriminatedUnion("type", [
  z.object({
    type: z.literal("search"),
    query: z.string(),
    maxResults: z.number().default(10)
  }),
  z.object({
    type: z.literal("create"),
    title: z.string(),
    content: z.string()
  }),
  z.object({
    type: z.literal("delete"),
    id: z.string()
  })
]);
```

### Schema Design Tips

- Use `.describe()` on fields to provide hints that improve extraction quality:
  ```typescript
  const schema = z.object({
    sentiment: z.enum(["positive", "negative", "neutral"])
      .describe("Overall emotional tone of the text"),
    score: z.number()
      .describe("Sentiment score from -1.0 (most negative) to 1.0 (most positive)")
  });
  ```
- Prefer enums over free strings wherever the domain is bounded
- Use `.optional()` for fields that may genuinely not be present in the source text
- Add `.min()` and `.max()` constraints on arrays and numbers to catch runaway outputs

---

## 4. TypeScript Example: Extracting Structured Data from Unstructured Text

Full pipeline extracting invoice data from a raw text document.

```typescript
import Anthropic from "@anthropic-ai/sdk";
import { z } from "zod";

const client = new Anthropic();

// Schema definition
const InvoiceSchema = z.object({
  invoiceNumber: z.string(),
  date: z.string().describe("ISO 8601 date string, e.g. 2024-03-15"),
  vendor: z.object({
    name: z.string(),
    address: z.string().optional()
  }),
  lineItems: z.array(z.object({
    description: z.string(),
    quantity: z.number(),
    unitPrice: z.number().describe("Price in dollars, e.g. 49.99"),
    total: z.number()
  })),
  subtotal: z.number(),
  tax: z.number().nullable(),
  total: z.number()
});

type Invoice = z.infer<typeof InvoiceSchema>;

// Convert Zod schema to JSON Schema for function calling
function zodToJsonSchema(schema: z.ZodObject<any>): object {
  // In practice, use the zod-to-json-schema package:
  // import { zodToJsonSchema } from "zod-to-json-schema";
  // Return zodToJsonSchema(schema, { target: "openApi3" });

  // Simplified inline version for illustration:
  return {
    type: "object",
    properties: {
      invoiceNumber: { type: "string" },
      date: { type: "string", description: "ISO 8601 date string" },
      vendor: {
        type: "object",
        properties: {
          name: { type: "string" },
          address: { type: "string" }
        },
        required: ["name"]
      },
      lineItems: {
        type: "array",
        items: {
          type: "object",
          properties: {
            description: { type: "string" },
            quantity: { type: "number" },
            unitPrice: { type: "number" },
            total: { type: "number" }
          },
          required: ["description", "quantity", "unitPrice", "total"]
        }
      },
      subtotal: { type: "number" },
      tax: { type: ["number", "null"] },
      total: { type: "number" }
    },
    required: ["invoiceNumber", "date", "vendor", "lineItems", "subtotal", "total"]
  };
}

async function extractInvoice(rawText: string): Promise<Invoice> {
  const response = await client.messages.create({
    model: "claude-sonnet-4-5",
    max_tokens: 2048,
    temperature: 0,
    tools: [{
      name: "extract_invoice",
      description: "Extract structured invoice data from raw text. Extract all line items, amounts, and vendor information present in the text.",
      input_schema: zodToJsonSchema(InvoiceSchema) as any
    }],
    tool_choice: { type: "tool", name: "extract_invoice" },
    messages: [{
      role: "user",
      content: `Extract the invoice data from the following text:\n\n${rawText}`
    }]
  });

  // Find the tool use block
  const toolUseBlock = response.content.find(
    block => block.type === "tool_use"
  );

  if (!toolUseBlock || toolUseBlock.type !== "tool_use") {
    throw new Error("Model did not call the extraction tool");
  }

  // Parse and validate with Zod
  const parsed = InvoiceSchema.parse(toolUseBlock.input);
  return parsed;
}

// Usage
const rawInvoiceText = `
  INVOICE #INV-2024-0312
  Date: March 15, 2024

  From: Acme Software Inc.
  123 Tech Street, San Francisco, CA 94102

  Web development services - 40 hours @ $150.00/hr ... $6,000.00
  Cloud hosting - March 2024 .......................... $299.00
  SSL certificate renewal .............................. $99.00

  Subtotal: $6,398.00
  Tax (8.5%): $543.83
  TOTAL: $6,941.83
`;

const invoice = await extractInvoice(rawInvoiceText);
console.log(invoice.total); // 6941.83 (typed as number)
```

---

## 5. Handling Validation Failures — Retries and Fallbacks

### Why Validation Fails

Even with function calling and schema enforcement, validation can fail because:
- The model fills a required field with a placeholder ("N/A", "unknown")
- Numeric fields come back as strings
- The model omits optional-looking fields that are actually required
- Date formats are inconsistent despite instructions

### Retry Strategy

The most effective approach: on validation failure, send the error back to the model.

```typescript
import { ZodError } from "zod";

async function extractWithRetry(
  text: string,
  maxRetries: number = 2
): Promise<Invoice> {
  let lastError: Error | null = null;

  for (let attempt = 0; attempt <= maxRetries; attempt++) {
    try {
      const raw = await callModel(text, attempt === 0 ? [] : [
        {
          role: "user" as const,
          content: `Previous attempt failed validation. Error:\n${lastError?.message}\n\nPlease try again, paying attention to the schema requirements.`
        }
      ]);

      return InvoiceSchema.parse(raw);
    } catch (error) {
      if (error instanceof ZodError) {
        // Format Zod errors into a readable message for the retry prompt
        lastError = new Error(
          error.errors.map(e => `${e.path.join(".")}: ${e.message}`).join("; ")
        );
      } else {
        throw error; // Non-validation errors, rethrow
      }
    }
  }

  throw new Error(`Extraction failed after ${maxRetries + 1} attempts: ${lastError?.message}`);
}
```

### Retry with Context Preservation

Keep the full message history across retries so the model sees its previous (failed) attempt:

```typescript
async function extractWithContext(rawText: string): Promise<Invoice> {
  const messages: Anthropic.MessageParam[] = [
    {
      role: "user",
      content: `Extract invoice data from:\n\n${rawText}`
    }
  ];

  for (let attempt = 0; attempt < 3; attempt++) {
    const response = await client.messages.create({
      model: "claude-sonnet-4-5",
      max_tokens: 2048,
      temperature: 0,
      tools: [invoiceTool],
      tool_choice: { type: "tool", name: "extract_invoice" },
      messages
    });

    const toolBlock = response.content.find(b => b.type === "tool_use");
    if (!toolBlock || toolBlock.type !== "tool_use") {
      throw new Error("No tool call in response");
    }

    const result = InvoiceSchema.safeParse(toolBlock.input);

    if (result.success) {
      return result.data;
    }

    // Append the failed attempt and the error to the conversation
    messages.push(
      { role: "assistant", content: response.content },
      {
        role: "user",
        content: [
          {
            type: "tool_result",
            tool_use_id: toolBlock.id,
            content: `Validation failed: ${result.error.errors.map(e =>
              `${e.path.join(".")}: ${e.message}`
            ).join("; ")}. Please correct these fields.`,
            is_error: true
          }
        ]
      }
    );
  }

  throw new Error("Extraction failed after 3 attempts");
}
```

### Fallback Strategies

When retries are exhausted:

**Partial extraction**: Return what validated successfully, mark failed fields as `null`.

```typescript
const PartialInvoiceSchema = InvoiceSchema.partial();
const partial = PartialInvoiceSchema.safeParse(raw);
if (partial.success) {
  return { ...partial.data, _partial: true };
}
```

**Escalate to human review**: Log the failure and queue the document for manual processing. For financial data, this is often the right answer.

**Downgrade to a more capable model**: If you were using a fast/cheap model, retry with a more capable one.

```typescript
async function extractWithFallback(text: string): Promise<Invoice> {
  try {
    return await extractWithModel(text, "claude-haiku-4-5");
  } catch {
    // Escalate to a more capable model
    return await extractWithModel(text, "claude-sonnet-4-5");
  }
}
```

### Logging for Improvement

Always log validation failures with the raw model output. This data is invaluable for improving your prompts and understanding systematic failures.

```typescript
if (!result.success) {
  logger.warn("structured_output_validation_failure", {
    model: "claude-sonnet-4-5",
    rawOutput: toolBlock.input,
    errors: result.error.errors,
    inputLength: rawText.length,
    attempt
  });
}
```
