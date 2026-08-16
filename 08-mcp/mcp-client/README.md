# Building an MCP Client

## Overview

An MCP client connects to one MCP server, discovers what tools and resources it exposes, and forwards tool calls from the LLM to the server. You build an MCP client when you want your application to use tools hosted in a separate MCP server process.

In practice, you often pair an MCP client with an LLM call:
1. Connect to the server and list available tools
2. Pass the tool schemas to the LLM (via Claude's tool use API)
3. When the LLM returns a tool call, forward it to the MCP server
4. Return the result to the LLM

---

## Installation

```bash
npm install @modelcontextprotocol/sdk @anthropic-ai/sdk
npm install -D typescript @types/node
```

---

## Discovering Tools from a Server

The `Client` class manages the connection. After connecting, call `listTools()` to get the server's tool definitions.

```typescript
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";

async function connectToServer(serverCommand: string, serverArgs: string[]) {
  // Spawn the server process and connect via stdio
  const transport = new StdioClientTransport({
    command: serverCommand,
    args: serverArgs,
  });

  const client = new Client(
    { name: "my-client", version: "1.0.0" },
    { capabilities: {} }
  );

  await client.connect(transport);
  return client;
}

async function discoverTools(client: Client) {
  const response = await client.listTools();
  // response.tools is Tool[]
  for (const tool of response.tools) {
    console.log(`Tool: ${tool.name}`);
    console.log(`  Description: ${tool.description}`);
    console.log(`  Schema: ${JSON.stringify(tool.inputSchema, null, 2)}`);
  }
  return response.tools;
}
```

The `inputSchema` returned is a JSON Schema object. This is exactly what Claude's tool use API expects.

---

## Calling Tools

Use `client.callTool()` with the tool name and input arguments:

```typescript
import { CallToolResultSchema } from "@modelcontextprotocol/sdk/types.js";

async function callTool(
  client: Client,
  toolName: string,
  toolInput: Record<string, unknown>
) {
  const result = await client.callTool({
    name: toolName,
    arguments: toolInput,
  });

  // result.content is an array of content blocks
  // result.isError is true if the tool reported an error
  if (result.isError) {
    throw new Error(
      `Tool ${toolName} returned an error: ${JSON.stringify(result.content)}`
    );
  }

  // Extract text from content blocks
  const textContent = result.content
    .filter((block): block is { type: "text"; text: string } => block.type === "text")
    .map((block) => block.text)
    .join("\n");

  return textContent;
}
```

---

## Complete TypeScript Client Example

This example:
1. Connects to the demo server from the `mcp-server` section
2. Discovers its tools and converts them to Anthropic's tool format
3. Sends a user message to Claude with those tools available
4. Handles Claude's tool calls by forwarding them to the MCP server
5. Continues the conversation until Claude finishes

```typescript
// client.ts
import Anthropic from "@anthropic-ai/sdk";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";
import type { Tool as MCPTool } from "@modelcontextprotocol/sdk/types.js";

// ────────────────────────────────────────────────────────────
// MCP connection helpers
// ────────────────────────────────────────────────────────────

async function createMCPClient(command: string, args: string[]): Promise<Client> {
  const transport = new StdioClientTransport({ command, args });
  const client = new Client(
    { name: "demo-client", version: "1.0.0" },
    { capabilities: {} }
  );
  await client.connect(transport);
  return client;
}

// Convert MCP tool definitions to Anthropic's tool format
function mcpToolsToAnthropic(mcpTools: MCPTool[]): Anthropic.Tool[] {
  return mcpTools.map((tool) => ({
    name: tool.name,
    description: tool.description ?? "",
    input_schema: tool.inputSchema as Anthropic.Tool["input_schema"],
  }));
}

// ────────────────────────────────────────────────────────────
// Main agent loop
// ────────────────────────────────────────────────────────────

async function runAgentLoop(
  anthropic: Anthropic,
  mcpClient: Client,
  userMessage: string
): Promise<void> {
  // 1. Discover tools from the MCP server
  const toolsResponse = await mcpClient.listTools();
  const anthropicTools = mcpToolsToAnthropic(toolsResponse.tools);

  console.log(`\nConnected to MCP server. Available tools:`);
  for (const tool of toolsResponse.tools) {
    console.log(`  - ${tool.name}: ${tool.description?.split("\n")[0]}`);
  }
  console.log();

  // 2. Build the initial messages array
  const messages: Anthropic.MessageParam[] = [
    { role: "user", content: userMessage },
  ];

  // 3. Agentic loop: call Claude, handle tool use, repeat
  while (true) {
    const response = await anthropic.messages.create({
      model: "claude-opus-4-8",
      max_tokens: 4096,
      tools: anthropicTools,
      messages,
    });

    // Append Claude's response to the conversation
    messages.push({ role: "assistant", content: response.content });

    // Print any text the model produced
    for (const block of response.content) {
      if (block.type === "text" && block.text.trim()) {
        console.log("Claude:", block.text);
      }
    }

    // If Claude is done, exit the loop
    if (response.stop_reason === "end_turn") {
      break;
    }

    // If Claude wants to use tools, execute them via MCP
    if (response.stop_reason === "tool_use") {
      const toolUseBlocks = response.content.filter(
        (b): b is Anthropic.ToolUseBlock => b.type === "tool_use"
      );

      const toolResults: Anthropic.ToolResultBlockParam[] = [];

      for (const toolUse of toolUseBlocks) {
        console.log(`\n[Tool call] ${toolUse.name}(${JSON.stringify(toolUse.input)})`);

        let resultText: string;
        let isError = false;

        try {
          // Forward the tool call to the MCP server
          const mcpResult = await mcpClient.callTool({
            name: toolUse.name,
            arguments: toolUse.input as Record<string, unknown>,
          });

          isError = mcpResult.isError === true;

          // Extract text from the result
          resultText = mcpResult.content
            .filter((b): b is { type: "text"; text: string } => b.type === "text")
            .map((b) => b.text)
            .join("\n");

          console.log(`[Tool result] ${resultText}`);
        } catch (err) {
          resultText = `Error calling tool: ${err instanceof Error ? err.message : String(err)}`;
          isError = true;
          console.error(`[Tool error] ${resultText}`);
        }

        toolResults.push({
          type: "tool_result",
          tool_use_id: toolUse.id,
          content: resultText,
          is_error: isError,
        });
      }

      // Add all tool results in a single user message
      messages.push({ role: "user", content: toolResults });
      // Continue the loop — Claude will process the results
    }
  }
}

// ────────────────────────────────────────────────────────────
// Entry point
// ────────────────────────────────────────────────────────────

async function main() {
  const anthropic = new Anthropic();

  // Connect to the demo server (adjust path to your compiled server)
  const mcpClient = await createMCPClient("node", [
    "./mcp-server/dist/server.js",
  ]);

  try {
    // Run a few test queries
    const queries = [
      "What is 847 divided by 7?",
      "What's the weather like in Tokyo and London?",
      "Calculate (15 * 4) + (100 / 5), then tell me the weather in Paris.",
    ];

    for (const query of queries) {
      console.log("\n" + "=".repeat(60));
      console.log(`User: ${query}`);
      console.log("=".repeat(60));
      await runAgentLoop(anthropic, mcpClient, query);
    }
  } finally {
    // Always close the MCP client
    await mcpClient.close();
  }
}

main().catch((err) => {
  console.error("Fatal error:", err);
  process.exit(1);
});
```

---

## Connecting to an HTTP/SSE Server

For remote servers using SSE transport instead of stdio:

```typescript
import { SSEClientTransport } from "@modelcontextprotocol/sdk/client/sse.js";

async function connectToRemoteServer(serverUrl: string): Promise<Client> {
  const transport = new SSEClientTransport(new URL(serverUrl));

  const client = new Client(
    { name: "my-client", version: "1.0.0" },
    { capabilities: {} }
  );

  await client.connect(transport);
  return client;
}

// Usage
const client = await connectToRemoteServer("http://localhost:3000/sse");
```

---

## Listing Resources

```typescript
async function listResources(client: Client) {
  const response = await client.listResources();
  for (const resource of response.resources) {
    console.log(`Resource: ${resource.uri}`);
    console.log(`  Name: ${resource.name}`);
    console.log(`  MIME: ${resource.mimeType}`);
  }
  return response.resources;
}

async function readResource(client: Client, uri: string) {
  const response = await client.readResource({ uri });
  for (const content of response.contents) {
    if ("text" in content) {
      console.log(content.text);
    }
  }
}
```

---

## Multi-Server Client

You can connect to multiple MCP servers and merge their tools:

```typescript
async function connectMultipleServers(
  servers: Array<{ name: string; command: string; args: string[] }>
) {
  const clients: Map<string, Client> = new Map();
  const allTools: Anthropic.Tool[] = [];
  // Track which server handles each tool name
  const toolToServer: Map<string, Client> = new Map();

  for (const server of servers) {
    const client = await createMCPClient(server.command, server.args);
    clients.set(server.name, client);

    const toolsResponse = await client.listTools();
    for (const tool of toolsResponse.tools) {
      allTools.push({
        name: tool.name,
        description: tool.description ?? "",
        input_schema: tool.inputSchema as Anthropic.Tool["input_schema"],
      });
      toolToServer.set(tool.name, client);
    }
  }

  return { clients, allTools, toolToServer };
}
```

---

## Error Handling

```typescript
import { McpError, ErrorCode } from "@modelcontextprotocol/sdk/types.js";

try {
  const result = await client.callTool({ name: "nonexistent_tool", arguments: {} });
} catch (err) {
  if (err instanceof McpError) {
    switch (err.code) {
      case ErrorCode.MethodNotFound:
        console.error("Tool does not exist on the server");
        break;
      case ErrorCode.InvalidParams:
        console.error("Invalid tool arguments:", err.message);
        break;
      default:
        console.error(`MCP error ${err.code}: ${err.message}`);
    }
  } else {
    throw err; // Re-throw unexpected errors
  }
}
```

---

## Cleanup

Always close the client when done to cleanly terminate the server process (for stdio transport):

```typescript
// In a try/finally block
try {
  await runAgentLoop(anthropic, mcpClient, userMessage);
} finally {
  await mcpClient.close();
}

// Or handle process signals
process.on("SIGINT", async () => {
  await mcpClient.close();
  process.exit(0);
});
```
