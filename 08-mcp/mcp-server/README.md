# Building an MCP Server

## Overview

An MCP server is a process that exposes tools, resources, and prompts over the Model Context Protocol. Clients (Claude Desktop, custom agents, VS Code extensions) connect to your server and can then use whatever capabilities you expose.

This guide uses the official TypeScript SDK: `@modelcontextprotocol/sdk`.

---

## Installation

```bash
npm install @modelcontextprotocol/sdk zod
npm install -D typescript @types/node
```

The SDK ships its own types. `zod` is used for runtime input validation alongside the JSON Schema that MCP requires for tool definitions.

---

## Defining Tools with Schemas

Each tool requires:
1. A name — used when calling it
2. A description — the LLM reads this to decide when to use the tool
3. An `inputSchema` — JSON Schema object describing accepted parameters

The description is more important than it might seem. Write it to answer: "When should the LLM call this tool, and what exactly does it do?" Vague descriptions lead to the LLM misusing or ignoring your tool.

```typescript
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";

const server = new McpServer({
  name: "my-server",
  version: "1.0.0",
});

// Register a tool using server.tool()
// The Zod schema is converted to JSON Schema automatically by the SDK
server.tool(
  "add",                              // tool name
  "Add two numbers together",          // description
  {                                    // parameter schema (Zod)
    a: z.number().describe("First number"),
    b: z.number().describe("Second number"),
  },
  async ({ a, b }) => {               // handler
    return {
      content: [
        {
          type: "text",
          text: String(a + b),
        },
      ],
    };
  },
);
```

---

## Handling Tool Calls

Tool handlers must return a `CallToolResult` object:

```typescript
{
  content: Array<TextContent | ImageContent | EmbeddedResource>;
  isError?: boolean;
}
```

For text results, use `{ type: "text", text: string }`. Set `isError: true` when the tool encountered an error — the LLM will see the error message and can decide how to respond.

```typescript
server.tool(
  "divide",
  "Divide two numbers. Returns an error if dividing by zero.",
  {
    numerator: z.number(),
    denominator: z.number(),
  },
  async ({ numerator, denominator }) => {
    if (denominator === 0) {
      return {
        content: [{ type: "text", text: "Error: cannot divide by zero" }],
        isError: true,
      };
    }
    return {
      content: [{ type: "text", text: String(numerator / denominator) }],
    };
  },
);
```

---

## Defining Resources

Resources are read-only data sources the LLM can read. Use `server.resource()` to register them.

```typescript
import { ResourceTemplate } from "@modelcontextprotocol/sdk/server/mcp.js";

// Static resource — fixed URI
server.resource(
  "config",
  "app://config",                     // URI
  { mimeType: "application/json" },
  async (uri) => {
    return {
      contents: [
        {
          uri: uri.href,
          mimeType: "application/json",
          text: JSON.stringify({ version: "1.0.0", environment: "production" }),
        },
      ],
    };
  },
);

// Template resource — URI with variables
server.resource(
  "user-profile",
  new ResourceTemplate("users://{userId}/profile", { list: undefined }),
  { mimeType: "application/json" },
  async (uri, { userId }) => {
    // Fetch real data here
    const profile = { id: userId, name: "Alice", role: "admin" };
    return {
      contents: [
        {
          uri: uri.href,
          mimeType: "application/json",
          text: JSON.stringify(profile),
        },
      ],
    };
  },
);
```

---

## Complete Working Example: Calculator + Weather Mock Server

This is a complete, runnable MCP server exposing two tools:
- `calculator` — performs arithmetic operations
- `get_weather` — returns mock weather data for a city

```typescript
// server.ts
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";

// ────────────────────────────────────────────────────────────
// Server definition
// ────────────────────────────────────────────────────────────

const server = new McpServer({
  name: "demo-server",
  version: "1.0.0",
});

// ────────────────────────────────────────────────────────────
// Tool 1: Calculator
// ────────────────────────────────────────────────────────────

const OperationSchema = z.enum(["add", "subtract", "multiply", "divide"]);

server.tool(
  "calculator",
  `Perform arithmetic on two numbers.
Operations: add, subtract, multiply, divide.
Returns the numeric result as a string.
Returns an error if you attempt to divide by zero.`,
  {
    operation: OperationSchema.describe(
      "Arithmetic operation: add | subtract | multiply | divide"
    ),
    a: z.number().describe("Left operand"),
    b: z.number().describe("Right operand"),
  },
  async ({ operation, a, b }) => {
    let result: number;

    switch (operation) {
      case "add":
        result = a + b;
        break;
      case "subtract":
        result = a - b;
        break;
      case "multiply":
        result = a * b;
        break;
      case "divide":
        if (b === 0) {
          return {
            content: [{ type: "text", text: "Error: division by zero" }],
            isError: true,
          };
        }
        result = a / b;
        break;
    }

    return {
      content: [
        {
          type: "text",
          text: `${a} ${operation} ${b} = ${result}`,
        },
      ],
    };
  }
);

// ────────────────────────────────────────────────────────────
// Tool 2: Weather (mock)
// ────────────────────────────────────────────────────────────

// Mock data — replace with a real weather API call in production
const MOCK_WEATHER: Record<string, { temp_c: number; condition: string; humidity: number }> = {
  "new york":    { temp_c: 22, condition: "Partly cloudy", humidity: 65 },
  "london":      { temp_c: 14, condition: "Overcast",      humidity: 80 },
  "tokyo":       { temp_c: 28, condition: "Sunny",         humidity: 55 },
  "sydney":      { temp_c: 18, condition: "Clear",         humidity: 70 },
  "paris":       { temp_c: 19, condition: "Light rain",    humidity: 75 },
  "san francisco": { temp_c: 16, condition: "Foggy",       humidity: 85 },
};

server.tool(
  "get_weather",
  `Get the current weather for a city.
Returns temperature in Celsius and Fahrenheit, weather condition, and humidity.
Use this when the user asks about weather, temperature, or climate in a specific city.
Supported cities: New York, London, Tokyo, Sydney, Paris, San Francisco.
Returns an error for unknown cities.`,
  {
    city: z.string().describe("Name of the city, e.g. 'London' or 'New York'"),
    unit: z
      .enum(["celsius", "fahrenheit"])
      .optional()
      .default("celsius")
      .describe("Temperature unit (default: celsius)"),
  },
  async ({ city, unit }) => {
    const key = city.toLowerCase().trim();
    const data = MOCK_WEATHER[key];

    if (!data) {
      const supported = Object.keys(MOCK_WEATHER)
        .map((c) => c.replace(/\b\w/g, (l) => l.toUpperCase()))
        .join(", ");
      return {
        content: [
          {
            type: "text",
            text: `Error: no weather data for "${city}". Supported cities: ${supported}.`,
          },
        ],
        isError: true,
      };
    }

    const tempF = Math.round((data.temp_c * 9) / 5 + 32);
    const displayTemp =
      unit === "fahrenheit"
        ? `${tempF}°F (${data.temp_c}°C)`
        : `${data.temp_c}°C (${tempF}°F)`;

    const report = [
      `Weather for ${city.replace(/\b\w/g, (l) => l.toUpperCase())}:`,
      `  Temperature: ${displayTemp}`,
      `  Condition:   ${data.condition}`,
      `  Humidity:    ${data.humidity}%`,
    ].join("\n");

    return {
      content: [{ type: "text", text: report }],
    };
  }
);

// ────────────────────────────────────────────────────────────
// Resource: server info
// ────────────────────────────────────────────────────────────

server.resource(
  "server-info",
  "info://server",
  { mimeType: "application/json" },
  async (uri) => {
    return {
      contents: [
        {
          uri: uri.href,
          mimeType: "application/json",
          text: JSON.stringify({
            name: "demo-server",
            version: "1.0.0",
            tools: ["calculator", "get_weather"],
            description: "Demo MCP server with calculator and weather tools",
          }),
        },
      ],
    };
  }
);

// ────────────────────────────────────────────────────────────
// Start: stdio transport (used by Claude Desktop and most clients)
// ────────────────────────────────────────────────────────────

async function main() {
  const transport = new StdioServerTransport();
  await server.connect(transport);
  // Server is now running; it communicates over stdin/stdout
  // Do not write to stdout directly after this point — it breaks the protocol
  console.error("Demo MCP server running on stdio"); // stderr is safe
}

main().catch((err) => {
  console.error("Server error:", err);
  process.exit(1);
});
```

### Running the Server

```bash
# Compile
npx tsc

# Run directly (for testing)
node dist/server.js

# Or via ts-node (development)
npx ts-node server.ts
```

### Connecting to Claude Desktop

Add to `~/Library/Application Support/Claude/claude_desktop_config.json` (macOS):

```json
{
  "mcpServers": {
    "demo": {
      "command": "node",
      "args": ["/absolute/path/to/dist/server.js"]
    }
  }
}
```

### tsconfig.json

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "Node16",
    "moduleResolution": "Node16",
    "outDir": "./dist",
    "strict": true,
    "esModuleInterop": true
  },
  "include": ["*.ts"]
}
```

### package.json

```json
{
  "name": "demo-mcp-server",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "build": "tsc",
    "start": "node dist/server.js",
    "dev": "ts-node server.ts"
  },
  "dependencies": {
    "@modelcontextprotocol/sdk": "^1.0.0",
    "zod": "^3.22.0"
  },
  "devDependencies": {
    "typescript": "^5.0.0",
    "@types/node": "^20.0.0",
    "ts-node": "^10.9.0"
  }
}
```

---

## HTTP Transport (Remote Servers)

To serve over HTTP instead of stdio, swap the transport:

```typescript
import { SSEServerTransport } from "@modelcontextprotocol/sdk/server/sse.js";
import express from "express";

const app = express();
let transport: SSEServerTransport;

app.get("/sse", async (req, res) => {
  transport = new SSEServerTransport("/message", res);
  await server.connect(transport);
});

app.post("/message", async (req, res) => {
  await transport.handlePostMessage(req, res);
});

app.listen(3000, () => {
  console.error("MCP server running on http://localhost:3000");
});
```

---

## Key Rules for Server Authors

1. **Never write to stdout** when using stdio transport — all stdout is the protocol channel. Use `console.error()` for logs.
2. **Tool descriptions drive LLM behavior** — spend time writing precise, accurate descriptions that explain when and why to call each tool.
3. **Return `isError: true`** for tool failures rather than throwing — the LLM can respond to error results; thrown errors crash the handler.
4. **Validate inputs** — the SDK validates against your Zod schema before calling your handler, but validate business logic constraints yourself (e.g., file path restrictions).
5. **Keep handlers fast** — the LLM is waiting; for slow operations consider returning a job ID and polling resource.
