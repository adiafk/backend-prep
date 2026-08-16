# Agent Tools

## 1. Tool Design Principles

Tools are the hands of an agent. A poorly designed tool is worse than no tool — it consumes tokens, produces unexpected errors, and causes the agent to hallucinate corrective actions. A well-designed tool is so clear and predictable that the model rarely misuses it.

### Principle 1: Clear description

The model decides which tool to call based on the tool's description. The description is not documentation — it is a behavioral specification that the model reads at inference time.

**Bad:**
```
name: "db_query"
description: "Queries the database"
```

**Good:**
```
name: "search_users"
description: "Search for users in the database by name, email, or username.
Returns up to 10 matching users. Use this when you need to find a user's
ID before performing operations on their account. Do NOT use this to check
if a user exists — use get_user_by_id for that."
```

The good description tells the model:
- What it does specifically
- What it returns
- When to use it
- When NOT to use it (prevents misuse)

### Principle 2: Narrow scope

One tool should do one thing. Avoid "Swiss army knife" tools with a `mode` parameter that changes behavior entirely.

**Bad:**
```
manage_user(action: "create" | "delete" | "update" | "list", ...variousArgs)
```

**Good:**
```
create_user(name, email)
delete_user(userId)
update_user(userId, changes)
list_users(filters)
```

Narrow tools are easier to describe accurately. They produce predictable outputs. They are easier to test, mock, and audit.

### Principle 3: Predictable output

The model must be able to interpret tool results without additional explanation. Structure outputs consistently.

**Bad output:** Different shapes depending on success/failure, missing fields, variable types:
```
// Sometimes returns an array, sometimes a single object, sometimes null
```

**Good output:** Consistent structure regardless of outcome:
```typescript
type ToolResult<T> = {
  success: boolean;
  data?: T;
  error?: {
    code: string;
    message: string;
    retryable: boolean;
  };
};
```

### Principle 4: Fail informatively

When a tool fails, the error message must give the model enough information to decide what to do next. A generic "Error: 500" teaches the model nothing. A specific error tells it whether to retry, try a different approach, or ask the user.

**Bad:** `"Internal server error"`

**Good:** `"User with ID 'abc123' was not found. The ID may be incorrect. Use search_users() to find the correct user ID."`

### Principle 5: Minimize side effects on read operations

If a tool is read-only, make that clear. The model is more likely to freely use a tool labeled as safe than one that might cause side effects. Separate read tools from write tools explicitly.

---

## 2. Tool Schema Design — Zod, Required vs Optional Params

Tool schemas are what the model receives to understand what arguments a tool accepts. A well-structured schema significantly reduces hallucinated arguments.

### Defining tools with Zod in LangChain:

```typescript
import { tool } from "@langchain/core/tools";
import { z } from "zod";

const searchUsersTool = tool(
  async ({ query, limit, includeInactive }) => {
    // implementation
  },
  {
    name: "search_users",
    description:
      "Search for users by name, email, or username. Returns matching users. " +
      "Use this to find a user's ID before performing account operations.",
    schema: z.object({
      query: z
        .string()
        .min(2)
        .describe("Search term — name, email address, or username fragment"),
      limit: z
        .number()
        .int()
        .min(1)
        .max(50)
        .default(10)
        .optional()
        .describe("Maximum number of results to return. Defaults to 10."),
      includeInactive: z
        .boolean()
        .default(false)
        .optional()
        .describe(
          "Include deactivated accounts in results. Defaults to false."
        ),
    }),
  }
);
```

### Required vs optional parameters:

**Required:** Parameters the tool cannot function without. The model will always provide them.
- Make required only what is truly necessary
- If a parameter has a sensible default, make it optional with a default

**Optional with defaults:** Most behavioral modifiers (limit, format, includeX flags) should be optional with defaults. This reduces the cognitive load on the model — it only needs to specify what differs from the default.

**Avoid:** Optional parameters with no default that silently change behavior when absent. This causes unpredictable behavior and confuses the model.

### Schema design tactics:

**Use enums to constrain choices:**
```typescript
status: z.enum(["active", "inactive", "pending"]).describe(
  "Filter by account status. Use 'active' for current users."
)
```
This prevents the model from guessing invalid values like `"enabled"` or `"disabled"`.

**Use `.describe()` on every field:**
The field description is injected into the model's context alongside the schema. Every field should explain:
- What value is expected
- What format (if not obvious)
- What it affects

**Validate ranges explicitly:**
```typescript
pageSize: z.number().int().min(1).max(100).default(20)
```
Without `.max(100)`, the model might request `pageSize: 10000`, which would be valid TypeScript but catastrophic for a database query.

**Prefer flat schemas:**
Nested objects work, but deeply nested schemas are harder for models to fill correctly. If possible, flatten.

---

## 3. Tool Error Handling — What to Return on Failure

The golden rule: **never throw an unhandled exception from a tool**. An unhandled exception terminates the agent run. Instead, catch all errors and return structured error objects that the model can reason about.

### Why structured errors matter:

The model's next reasoning step depends on the tool result. If the tool returns an error with actionable information, the model can adapt. If the tool throws and the run crashes, there's nothing to reason about.

### The error return contract:

```typescript
type ToolError = {
  success: false;
  error: {
    code: string;        // Machine-readable, stable identifier
    message: string;     // Human-readable, actionable explanation
    retryable: boolean;  // Can the model try again?
    suggestion?: string; // What should the model do instead?
  };
};
```

### Error categories and how to handle them:

**Not found errors:**
```typescript
{
  success: false,
  error: {
    code: "USER_NOT_FOUND",
    message: "No user found with ID 'abc123'.",
    retryable: false,
    suggestion: "Use search_users() to find the correct user ID."
  }
}
```

**Validation errors:**
```typescript
{
  success: false,
  error: {
    code: "INVALID_ARGUMENT",
    message: "The 'email' field must be a valid email address. Received: 'not-an-email'.",
    retryable: false,
    suggestion: "Confirm the email format with the user before retrying."
  }
}
```

**Rate limit / transient errors:**
```typescript
{
  success: false,
  error: {
    code: "RATE_LIMITED",
    message: "API rate limit reached. Limit resets in 30 seconds.",
    retryable: true,
    suggestion: "Wait briefly and retry, or reduce the frequency of requests."
  }
}
```

**Permissions errors:**
```typescript
{
  success: false,
  error: {
    code: "FORBIDDEN",
    message: "The current user does not have permission to delete accounts.",
    retryable: false,
    suggestion: "Inform the user that this action requires admin privileges."
  }
}
```

### Implementation wrapper:

```typescript
function withErrorHandling<TInput, TOutput>(
  fn: (input: TInput) => Promise<TOutput>
): (input: TInput) => Promise<TOutput | ToolError> {
  return async (input: TInput) => {
    try {
      return await fn(input);
    } catch (err) {
      if (err instanceof NotFoundError) {
        return {
          success: false,
          error: {
            code: "NOT_FOUND",
            message: err.message,
            retryable: false,
            suggestion: err.suggestion,
          },
        };
      }

      if (err instanceof ValidationError) {
        return {
          success: false,
          error: {
            code: "INVALID_ARGUMENT",
            message: err.message,
            retryable: false,
          },
        };
      }

      // Log unexpected errors but don't expose internals to the model
      logger.error("Unexpected tool error", { err, input });
      return {
        success: false,
        error: {
          code: "INTERNAL_ERROR",
          message:
            "An unexpected error occurred. The operation could not be completed.",
          retryable: true,
          suggestion: "Try again. If the error persists, report it to support.",
        },
      };
    }
  };
}
```

---

## 4. Tool Result Truncation — Handling Large Outputs

Tool results go directly into the model's context window. Large results consume tokens that could be used for reasoning. Worse, they can push important earlier context out of the window.

### Where large outputs come from:

- Web search results (full page HTML or long article text)
- Database queries returning many rows
- File reads (log files, large documents)
- API responses with verbose JSON payloads
- Code execution output (especially if the code prints extensively)

### Truncation strategies:

**Strategy 1: Truncate with a notice**

The simplest approach. Truncate the result and tell the model that truncation occurred.

```typescript
function truncateResult(
  content: string,
  maxChars: number = 8000
): string {
  if (content.length <= maxChars) return content;

  const truncated = content.slice(0, maxChars);
  const omitted = content.length - maxChars;

  return (
    truncated +
    `\n\n[Output truncated. ${omitted} characters omitted. ` +
    `Use a more specific query to retrieve less data, or request a specific section.]`
  );
}
```

**Strategy 2: Summarize before returning**

For search results and documents, summarize the content before injecting it into context.

```typescript
async function fetchAndSummarize(url: string): Promise<string> {
  const fullContent = await fetchWebPage(url);

  if (estimateTokens(fullContent) < 1000) {
    return fullContent; // Small enough to return directly
  }

  const summary = await summarizationLlm.invoke([
    {
      role: "user",
      content:
        `Summarize the following web page content in 3-5 sentences, ` +
        `preserving key facts, dates, and figures.\n\n${fullContent.slice(0, 20000)}`,
    },
  ]);

  return summary.content as string;
}
```

**Strategy 3: Paginated results**

Return a page of results at a time, with metadata that lets the model request the next page if needed.

```typescript
type PaginatedResult<T> = {
  success: true;
  items: T[];
  pagination: {
    page: number;
    pageSize: number;
    totalItems: number;
    hasMore: boolean;
    nextPageHint: string; // e.g., "Call with page: 2 to see more results"
  };
};
```

**Strategy 4: Field selection**

For API responses with many fields, allow the model to specify which fields it needs.

```typescript
const getOrderTool = tool(
  async ({ orderId, fields }) => {
    const order = await db.getOrder(orderId);

    if (fields && fields.length > 0) {
      return Object.fromEntries(
        fields.map((f) => [f, order[f]])
      );
    }

    return order;
  },
  {
    schema: z.object({
      orderId: z.string(),
      fields: z
        .array(z.enum(["id", "status", "total", "items", "customer", "createdAt"]))
        .optional()
        .describe("Specific fields to return. Omit to return all fields."),
    }),
  }
);
```

### Token budget monitoring:

Track how many tokens tool results consume and take action when approaching the limit:

```typescript
const TOKEN_BUDGET = 150_000; // Leave headroom in a 200k window
let tokensUsed = estimateTokens(systemPrompt);

for (const message of conversationHistory) {
  tokensUsed += estimateTokens(JSON.stringify(message));

  if (tokensUsed > TOKEN_BUDGET * 0.8) {
    // Compress old messages before adding more tool results
    conversationHistory = await compressHistory(conversationHistory);
  }
}
```

---

## 5. TypeScript Example: Well-Designed Tool with Error Handling

A complete example of a production-quality tool that applies all the principles above.

```typescript
import { tool } from "@langchain/core/tools";
import { z } from "zod";

// --- Types ---

interface Order {
  id: string;
  status: "pending" | "processing" | "shipped" | "delivered" | "cancelled";
  total: number;
  currency: string;
  items: Array<{ productId: string; name: string; quantity: number; price: number }>;
  customerId: string;
  createdAt: string;
  updatedAt: string;
}

type ToolSuccess<T> = { success: true; data: T };
type ToolError = {
  success: false;
  error: { code: string; message: string; retryable: boolean; suggestion?: string };
};
type ToolResult<T> = ToolSuccess<T> | ToolError;

// --- Tool implementation ---

export const getOrderTool = tool(
  async ({
    orderId,
    fields,
  }): Promise<ToolResult<Partial<Order>>> => {
    // Input sanitization — never trust model-provided IDs
    const sanitizedId = orderId.trim();
    if (!/^ord_[a-zA-Z0-9]{8,32}$/.test(sanitizedId)) {
      return {
        success: false,
        error: {
          code: "INVALID_ORDER_ID",
          message: `Order ID '${sanitizedId}' is not a valid format. Order IDs start with 'ord_' followed by 8-32 alphanumeric characters.`,
          retryable: false,
          suggestion:
            "Use list_orders() to find valid order IDs for this customer.",
        },
      };
    }

    let order: Order;
    try {
      order = await fetchOrderFromDatabase(sanitizedId);
    } catch (err) {
      if (err instanceof NotFoundError) {
        return {
          success: false,
          error: {
            code: "ORDER_NOT_FOUND",
            message: `No order found with ID '${sanitizedId}'.`,
            retryable: false,
            suggestion:
              "Verify the order ID with the customer, or use list_orders() to find recent orders.",
          },
        };
      }

      if (err instanceof DatabaseTimeoutError) {
        return {
          success: false,
          error: {
            code: "DATABASE_TIMEOUT",
            message: "The database did not respond in time.",
            retryable: true,
            suggestion: "Retry this request in a few seconds.",
          },
        };
      }

      // Unexpected error — log internally but return safe message
      console.error("[getOrderTool] Unexpected error", { err, orderId });
      return {
        success: false,
        error: {
          code: "INTERNAL_ERROR",
          message: "An unexpected error occurred while fetching the order.",
          retryable: true,
        },
      };
    }

    // Field selection — return only what was requested, or all fields
    const allowedFields: (keyof Order)[] = [
      "id", "status", "total", "currency", "items",
      "customerId", "createdAt", "updatedAt",
    ];

    let result: Partial<Order>;
    if (fields && fields.length > 0) {
      result = Object.fromEntries(
        (fields as (keyof Order)[]).filter((f) => allowedFields.includes(f))
          .map((f) => [f, order[f]])
      );
    } else {
      // Return everything except items (can be large); require explicit request
      const { items: _items, ...orderWithoutItems } = order;
      result = orderWithoutItems;
    }

    // Truncation guard — items array can be very large
    if (result.items && result.items.length > 20) {
      const total = result.items.length;
      result.items = result.items.slice(0, 20);
      return {
        success: true,
        data: {
          ...result,
          _truncationNotice: `Showing 20 of ${total} items. Request with fields=['items'] and a smaller order to see all items.`,
        } as Partial<Order>,
      };
    }

    return { success: true, data: result };
  },
  {
    name: "get_order",
    description:
      "Retrieve order details by order ID. Returns order status, total, items, and timestamps. " +
      "Use the 'fields' parameter to request only specific fields and reduce response size. " +
      "To find an order ID, use list_orders() first. " +
      "Do NOT use this to check whether an order exists — use list_orders() with a customer ID for that.",
    schema: z.object({
      orderId: z
        .string()
        .describe(
          "The order ID to retrieve. Must start with 'ord_'. Example: 'ord_a1b2c3d4'."
        ),
      fields: z
        .array(
          z.enum([
            "id",
            "status",
            "total",
            "currency",
            "items",
            "customerId",
            "createdAt",
            "updatedAt",
          ])
        )
        .optional()
        .describe(
          "Specific fields to include in the response. Omit to return all fields except items. " +
          "Include 'items' explicitly only when you need the line item details."
        ),
    }),
  }
);

// --- Placeholder implementations for the example ---

async function fetchOrderFromDatabase(orderId: string): Promise<Order> {
  // In production: query your database
  throw new NotFoundError(`Order ${orderId} not found`);
}

class NotFoundError extends Error {}
class DatabaseTimeoutError extends Error {}
```

### Why this implementation is production-quality:

1. **Input sanitization** before any database call — the model's provided ID is validated against a regex pattern before use.

2. **Typed error discrimination** — different error types produce different actionable messages.

3. **Unexpected error safety** — unknown errors log internally but return a safe, non-leaking message to the model.

4. **Field selection** — caller can request only what they need, keeping responses small.

5. **Truncation guard** — large item arrays are truncated with a notice before being returned.

6. **Description engineering** — the tool description tells the model when to use it AND when not to use it, which is equally important.
