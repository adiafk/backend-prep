# Function Calling (Tool Use)

Function calling — also called tool use — is the mechanism that lets language models interact with external systems. The model does not execute code. It emits a structured description of what it wants to do, your application does it, and you return the result.

---

## 1. How Function Calling Works

The mental model: the model is a decision-maker, not an executor.

### The Loop

```
1. You send: messages + tool definitions
2. Model responds: "I want to call tool X with arguments Y"
3. You execute: actually call X(Y) in your code
4. You return: the result back to the model as a tool result
5. Model responds: final answer incorporating the result
```

The model never directly calls your functions. It emits JSON that describes a function call. Your code runs the function and hands the result back.

### The Wire Format (Anthropic)

**Request** — you provide tools and a user message:
```json
{
  "model": "claude-sonnet-4-5",
  "tools": [{
    "name": "get_weather",
    "description": "Get current weather for a location",
    "input_schema": {
      "type": "object",
      "properties": {
        "location": { "type": "string", "description": "City and state, e.g. Austin, TX" },
        "unit": { "type": "string", "enum": ["celsius", "fahrenheit"] }
      },
      "required": ["location"]
    }
  }],
  "messages": [
    { "role": "user", "content": "What's the weather in Austin?" }
  ]
}
```

**Response** — the model emits a `tool_use` block:
```json
{
  "stop_reason": "tool_use",
  "content": [{
    "type": "tool_use",
    "id": "toolu_01XPq...",
    "name": "get_weather",
    "input": { "location": "Austin, TX", "unit": "fahrenheit" }
  }]
}
```

**You execute**, then continue the conversation with a `tool_result`:
```json
{
  "role": "user",
  "content": [{
    "type": "tool_result",
    "tool_use_id": "toolu_01XPq...",
    "content": "72°F, partly cloudy"
  }]
}
```

**Final response** — the model incorporates the result:
```json
{
  "content": [{
    "type": "text",
    "text": "The current weather in Austin, TX is 72°F and partly cloudy."
  }]
}
```

---

## 2. Tool Definition Best Practices

The quality of your tool descriptions directly determines how reliably the model uses them correctly. This is not incidental — the model reasons about which tool to use and what arguments to pass based entirely on the text you provide.

### Description Quality Is Everything

**Poor description**:
```json
{
  "name": "search",
  "description": "Search for things",
  "input_schema": {
    "type": "object",
    "properties": {
      "q": { "type": "string" }
    }
  }
}
```

**Good description**:
```json
{
  "name": "search_products",
  "description": "Search the product catalog by keyword. Use this when the user asks about specific products, wants to find items by name or category, or needs to check if a product exists. Returns a list of matching products with prices and stock status.",
  "input_schema": {
    "type": "object",
    "properties": {
      "query": {
        "type": "string",
        "description": "Search keywords. Use the most specific terms from the user's request. Example: 'blue wireless headphones' or 'USB-C laptop charger 65W'."
      },
      "category": {
        "type": "string",
        "description": "Optional category filter. Valid values: 'electronics', 'clothing', 'home', 'sports'. Omit if not specified by the user.",
        "enum": ["electronics", "clothing", "home", "sports"]
      },
      "max_results": {
        "type": "integer",
        "description": "Maximum number of results to return. Default 10, max 50.",
        "default": 10
      }
    },
    "required": ["query"]
  }
}
```

### Naming Conventions

- Use `snake_case` for tool names and parameter names
- Name tools as verb_noun: `get_user`, `search_products`, `send_email`, `delete_record`
- Be specific: `search_products` over `search`; `get_order_by_id` over `get_order` if ID is always the lookup key

### When to Use Which Tool

The description should include:
1. **What it does** — the action it performs
2. **When to use it** — conditions that should trigger this tool (especially important when multiple tools have similar purposes)
3. **What it returns** — shape and meaning of the result
4. **Constraints** — rate limits, side effects, required preconditions

### Parameter Design

- Use `required` accurately — only list fields that are truly required
- Prefer enums over free strings when the domain is bounded
- Add `description` to every parameter, especially non-obvious ones
- Include examples in descriptions for format-sensitive fields (dates, IDs, etc.)

---

## 3. Parallel Tool Calls

The model can request multiple tool calls in a single response. This happens when it determines that multiple pieces of information are needed and they can be fetched concurrently.

### How It Looks

The model returns multiple `tool_use` blocks in one response:

```json
{
  "stop_reason": "tool_use",
  "content": [
    {
      "type": "tool_use",
      "id": "toolu_01A...",
      "name": "get_weather",
      "input": { "location": "Austin, TX" }
    },
    {
      "type": "tool_use",
      "id": "toolu_01B...",
      "name": "get_weather",
      "input": { "location": "Denver, CO" }
    }
  ]
}
```

### Executing Parallel Calls

Execute all tool calls concurrently, then return all results:

```typescript
const toolUseBlocks = response.content.filter(b => b.type === "tool_use");

// Execute in parallel
const results = await Promise.all(
  toolUseBlocks.map(async (block) => {
    if (block.type !== "tool_use") return null;
    const result = await executeTool(block.name, block.input);
    return {
      type: "tool_result" as const,
      tool_use_id: block.id,
      content: JSON.stringify(result)
    };
  })
);

// Continue the conversation with all results
const nextResponse = await client.messages.create({
  model: "claude-sonnet-4-5",
  max_tokens: 1024,
  tools: tools,
  messages: [
    ...previousMessages,
    { role: "assistant", content: response.content },
    { role: "user", content: results.filter(Boolean) }
  ]
});
```

### Controlling Parallel Calls

- **`tool_choice: { type: "auto" }`** (default): Model decides when and whether to call tools
- **`tool_choice: { type: "any" }`**: Model must call at least one tool
- **`tool_choice: { type: "tool", name: "specific_tool" }`**: Model must call this specific tool
- **`disable_parallel_tool_use: true`**: Force sequential single calls (useful if your tools have ordering dependencies)

---

## 4. Tool Result Handling

### Success

Return a string or array of content blocks. JSON strings work well for structured data:

```typescript
{
  type: "tool_result",
  tool_use_id: block.id,
  content: JSON.stringify({ temperature: 72, condition: "partly cloudy" })
}
```

### Errors

Use `is_error: true` to tell the model the call failed. It will typically try again or inform the user:

```typescript
{
  type: "tool_result",
  tool_use_id: block.id,
  content: "Error: Location not found. The API returned 404 for 'Atlantis, GA'.",
  is_error: true
}
```

The model will usually:
- Ask the user for clarification
- Try a different approach or argument
- Acknowledge the failure in its final response

### Result Size

Keep tool results concise. The model processes them as tokens — a 50,000-character database dump is expensive and may exceed context limits. Truncate or summarize large results before returning them.

```typescript
function formatSearchResults(results: Product[]): string {
  if (results.length === 0) return "No products found.";

  // Limit to top 5, include only essential fields
  return results.slice(0, 5).map(p =>
    `- ${p.name} (ID: ${p.id}): $${p.price}, ${p.inStock ? "in stock" : "out of stock"}`
  ).join("\n");
}
```

---

## 5. TypeScript Example: Complete Tool-Calling Loop

A complete, production-ready tool-calling loop with multiple tools, parallel execution, and error handling.

```typescript
import Anthropic from "@anthropic-ai/sdk";

const client = new Anthropic();

// Tool implementations
const toolImplementations: Record<string, (input: any) => Promise<string>> = {
  async get_product(input: { product_id: string }): Promise<string> {
    // Simulate DB lookup
    const products: Record<string, object> = {
      "PROD-001": { name: "Mechanical Keyboard", price: 129.99, stock: 45 },
      "PROD-002": { name: "USB Hub", price: 34.99, stock: 0 }
    };
    const product = products[input.product_id];
    if (!product) throw new Error(`Product ${input.product_id} not found`);
    return JSON.stringify(product);
  },

  async check_inventory(input: { product_id: string; warehouse: string }): Promise<string> {
    return JSON.stringify({
      product_id: input.product_id,
      warehouse: input.warehouse,
      quantity: Math.floor(Math.random() * 100)
    });
  },

  async create_order(input: {
    product_id: string;
    quantity: number;
    customer_email: string;
  }): Promise<string> {
    const orderId = `ORD-${Date.now()}`;
    return JSON.stringify({ order_id: orderId, status: "created", ...input });
  }
};

// Tool definitions
const tools: Anthropic.Tool[] = [
  {
    name: "get_product",
    description: "Look up a product by its ID. Returns product name, price, and current stock level. Use this before creating an order to verify the product exists.",
    input_schema: {
      type: "object",
      properties: {
        product_id: {
          type: "string",
          description: "Product ID in the format PROD-XXX, e.g. PROD-001"
        }
      },
      required: ["product_id"]
    }
  },
  {
    name: "check_inventory",
    description: "Check inventory levels at a specific warehouse. Use when the user asks about availability at a particular location.",
    input_schema: {
      type: "object",
      properties: {
        product_id: { type: "string" },
        warehouse: {
          type: "string",
          enum: ["east", "west", "central"],
          description: "Warehouse region"
        }
      },
      required: ["product_id", "warehouse"]
    }
  },
  {
    name: "create_order",
    description: "Create a new order for a product. Only call this when the user explicitly confirms they want to place an order. Requires product ID, quantity, and customer email.",
    input_schema: {
      type: "object",
      properties: {
        product_id: { type: "string" },
        quantity: { type: "integer", minimum: 1 },
        customer_email: { type: "string", description: "Customer email address" }
      },
      required: ["product_id", "quantity", "customer_email"]
    }
  }
];

// Execute a single tool call
async function executeTool(
  name: string,
  input: unknown
): Promise<{ content: string; isError: boolean }> {
  const impl = toolImplementations[name];
  if (!impl) {
    return { content: `Unknown tool: ${name}`, isError: true };
  }

  try {
    const result = await impl(input);
    return { content: result, isError: false };
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    return { content: `Tool execution failed: ${message}`, isError: true };
  }
}

// The main agent loop
async function runAgentLoop(userMessage: string): Promise<string> {
  const messages: Anthropic.MessageParam[] = [
    { role: "user", content: userMessage }
  ];

  const MAX_ITERATIONS = 10; // Prevent infinite loops

  for (let iteration = 0; iteration < MAX_ITERATIONS; iteration++) {
    const response = await client.messages.create({
      model: "claude-sonnet-4-5",
      max_tokens: 4096,
      temperature: 0,
      tools,
      messages
    });

    // If the model is done, return the final text
    if (response.stop_reason === "end_turn") {
      const textBlock = response.content.find(b => b.type === "text");
      return textBlock?.type === "text" ? textBlock.text : "";
    }

    // Collect all tool calls from this response
    const toolUseBlocks = response.content.filter(
      (b): b is Anthropic.ToolUseBlock => b.type === "tool_use"
    );

    if (toolUseBlocks.length === 0) {
      // stop_reason is tool_use but no tool blocks — unexpected
      throw new Error("Expected tool calls but found none");
    }

    // Execute all tool calls in parallel
    const toolResults = await Promise.all(
      toolUseBlocks.map(async (block) => {
        console.log(`Calling tool: ${block.name}`, block.input);
        const { content, isError } = await executeTool(block.name, block.input);
        console.log(`Tool result (${block.name}):`, content);

        return {
          type: "tool_result" as const,
          tool_use_id: block.id,
          content,
          ...(isError && { is_error: true })
        };
      })
    );

    // Append assistant response and tool results to the conversation
    messages.push(
      { role: "assistant", content: response.content },
      { role: "user", content: toolResults }
    );
  }

  throw new Error(`Agent loop exceeded ${MAX_ITERATIONS} iterations`);
}

// Example usage
async function main() {
  const result = await runAgentLoop(
    "I need to order 2 units of product PROD-001 for customer@example.com. " +
    "Can you check if it's in stock first?"
  );
  console.log("\nFinal response:", result);
}

main().catch(console.error);
```

### Key Design Decisions in This Example

**Iteration cap**: Always cap the loop. A model that calls tools repeatedly can loop indefinitely if a tool keeps returning errors it misinterprets.

**Parallel execution**: `Promise.all` on all tool calls in a response. Never serialize parallel tool calls — it adds latency for no benefit.

**Error signaling**: Use `is_error: true` rather than throwing. The model handles errors more gracefully than an empty/missing tool result.

**Message accumulation**: The full conversation history is preserved. The model sees its previous tool calls and results, which prevents it from calling the same tool twice with the same arguments.

**Temperature 0**: For agentic loops, use temperature 0. Randomness in tool selection or argument generation leads to flaky behavior.
