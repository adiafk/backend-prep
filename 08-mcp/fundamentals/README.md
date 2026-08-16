# MCP Fundamentals

## What Is MCP?

Model Context Protocol (MCP) is an open standard that defines how LLMs communicate with external tools, data sources, and services. It creates a uniform wire protocol so that any MCP-compatible client (Claude, Cursor, VS Code Copilot, custom agents) can connect to any MCP-compatible server without custom glue code.

Before MCP, every AI application had to write bespoke integration code for every tool or data source it needed. MCP replaces that m-to-n problem with a single protocol: build one MCP server and any client can use it; build one MCP client and it can connect to any server.

The protocol was introduced by Anthropic in November 2024 and is now maintained as an open specification at https://modelcontextprotocol.io.

---

## Why MCP?

### The Problem It Solves

Without a standard, the integration landscape looks like this:

```
Claude ──── custom code ──── GitHub
Claude ──── custom code ──── Postgres
Claude ──── custom code ──── Slack
GPT-4 ──── different custom code ──── GitHub
GPT-4 ──── different custom code ──── Postgres
```

Every LLM provider needs separate integrations with every tool. Every tool needs separate integrations with every LLM provider. The cost scales as O(models × tools).

With MCP:

```
Claude ──── MCP ──── GitHub MCP Server
Claude ──── MCP ──── Postgres MCP Server
GPT-4  ──── MCP ──── GitHub MCP Server   (same server, reused)
```

The cost drops to O(models + tools).

### Key Benefits

**Interoperability** — A single MCP server works with any MCP client. You build the GitHub MCP server once; every AI product that supports MCP can use it.

**Separation of concerns** — Tool authors focus on tool logic. LLM application authors focus on application logic. Neither needs to know the internals of the other.

**Composability** — MCP clients can connect to multiple servers simultaneously. An agent can have access to a filesystem server, a database server, and a web search server all at once.

**Ecosystem** — A growing registry of pre-built MCP servers covers most common tools (GitHub, Slack, Postgres, filesystem, web search, etc.). You often don't need to write a server from scratch.

**Security boundary** — The server controls what capabilities it exposes. Secrets and credentials live in the server process, not in the LLM application.

---

## MCP Architecture

MCP uses a client-server architecture with three participants:

```
┌─────────────────────────────────────┐
│           MCP Host                  │
│  (Claude Desktop, VS Code, etc.)    │
│                                     │
│  ┌─────────────┐                    │
│  │  MCP Client │◄──── protocol ────► MCP Server
│  └─────────────┘                    │         (your tools/data)
└─────────────────────────────────────┘
```

### Host

The host is the application the user runs: Claude Desktop, a VS Code extension, a custom AI agent, or a web app. The host manages one or more MCP clients, handles user interaction, and decides which servers to connect to.

### Client

Each MCP client maintains a 1:1 connection with one MCP server. The client speaks the MCP protocol, discovers what the server offers (tools, resources, prompts), and forwards tool calls from the LLM to the server.

### Server

The MCP server exposes capabilities to clients. It can be a local process, a remote service, or anything in between. The server declares what tools it has, handles invocations, and returns results.

### Transports

MCP is transport-agnostic. The same JSON-RPC message format runs over different transports depending on the deployment scenario:

#### stdio (Standard I/O)

The client spawns the server as a child process and communicates over stdin/stdout.

```
MCP Client ──── stdin/stdout ──── MCP Server (child process)
```

Best for: local tools, command-line integration, Claude Desktop plugins. This is the most common transport for local development.

```json
// Messages sent over stdin as newline-delimited JSON
{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}
```

#### SSE (Server-Sent Events)

The server runs as an HTTP service. The client makes HTTP POST requests to send messages, and subscribes to an SSE endpoint to receive server-initiated messages.

```
MCP Client ──── HTTP POST /message ──── MCP Server (HTTP)
           ◄─── GET /sse (event stream) ─
```

Best for: remote servers, shared team infrastructure, cloud deployments.

#### HTTP (Streamable HTTP)

A newer transport that uses a single HTTP endpoint. Responses may be streamed via SSE or returned as plain JSON. This is the direction the protocol is moving for remote deployments.

```
MCP Client ──── POST /mcp ──── MCP Server (HTTP)
           ◄─── response (JSON or SSE stream) ─
```

Best for: production remote servers, services that need to handle multiple clients.

---

## Core Primitives

MCP defines three primitives that a server can expose:

### Tools

Tools are functions the LLM can call to take actions or retrieve information. They are the most commonly used primitive.

A tool has:
- A **name** — unique identifier used when calling it
- A **description** — natural language explanation of what it does and when to use it
- An **input schema** — JSON Schema describing expected parameters
- A **handler** — server-side function that executes when called

```json
{
  "name": "read_file",
  "description": "Read the contents of a file from the filesystem",
  "inputSchema": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "description": "Absolute or relative path to the file"
      }
    },
    "required": ["path"]
  }
}
```

When the LLM decides to use a tool, the client sends a `tools/call` request to the server and returns the result to the LLM.

### Resources

Resources are data sources the LLM can read. Unlike tools (which take actions), resources are read-only and URI-addressed.

A resource has:
- A **URI** — unique address (e.g., `file:///home/user/notes.txt`, `db://customers/42`)
- A **name** — human-readable label
- A **MIME type** — content type of the data
- **Contents** — text or binary data

```json
{
  "uri": "file:///home/user/project/README.md",
  "name": "Project README",
  "mimeType": "text/markdown"
}
```

Resources support listing (`resources/list`) and reading (`resources/read`). They can also be templated, where the URI is a pattern with variables.

Resources are appropriate when the content is relatively static, file-like, or document-like — things the LLM should read rather than interact with.

### Prompts

Prompts are reusable message templates the server exposes. They let servers package common workflows or instructions that clients can invoke.

A prompt has:
- A **name** — identifier
- A **description** — what the prompt does
- **Arguments** — parameterized inputs
- **Messages** — the actual prompt content, which can include dynamic data from resources or tools

```json
{
  "name": "summarize_pr",
  "description": "Summarize a GitHub pull request for review",
  "arguments": [
    {
      "name": "pr_number",
      "description": "The pull request number",
      "required": true
    }
  ]
}
```

Prompts are less commonly used than tools but useful for standardizing common workflows across teams.

---

## MCP vs Function Calling

MCP and function calling (tool use) solve overlapping but distinct problems. Understanding when to use each is important.

### Function Calling (Claude Tool Use)

Function calling is the mechanism by which a single LLM provider (Anthropic, OpenAI, etc.) lets an LLM request execution of functions defined in the API request.

```
Application ──── defines tools in API request ──── Claude API
                                                      │ tool_use block
Application ◄──── executes tool locally ──────────────┘
           ──── tool_result ──────────────────────────► Claude API
```

**Characteristics:**
- Tools are defined per-request in the API call
- The application executes tools and returns results
- Tightly coupled to a specific LLM provider
- No standard discovery mechanism
- Tools are defined ad-hoc in application code

**Use function calling when:**
- Building a self-contained application with a small, fixed set of tools
- You need fine-grained control over tool execution (approval gates, rate limiting, custom retry logic)
- You are not sharing tools across multiple LLM clients
- The tools are application-specific and not meant for reuse
- You want the simplest possible architecture

### MCP

MCP is a protocol for discovering and calling tools that live in a separate server process, potentially running remotely.

```
MCP Client ──── tools/list ──── MCP Server
           ◄─── tool definitions ───
           ──── tools/call ─────────► 
           ◄─── tool result ──────────
```

**Characteristics:**
- Tools are discovered dynamically from servers
- The server executes tools (not the client application)
- Provider-agnostic — any MCP client works with any MCP server
- Standard discovery, invocation, and result format
- Servers can be shared across teams and applications

**Use MCP when:**
- Building tools that should be reusable across multiple AI clients
- Integrating with external services (GitHub, Slack, databases)
- Sharing tool infrastructure across a team
- Building a platform where users bring their own tools
- You want to use the growing ecosystem of pre-built MCP servers

### Decision Guide

| Situation | Use |
|---|---|
| Quick prototype with 2-3 local tools | Function calling |
| Building a reusable GitHub integration | MCP server |
| Single-app, fixed tool set | Function calling |
| Multi-client tool platform | MCP |
| Need custom approval/retry logic per tool | Function calling |
| Connecting to existing MCP server ecosystem | MCP client |
| Secrets must stay in tool server | MCP (server holds secrets) |
| Simplest possible implementation | Function calling |

In practice, these often complement each other: a Claude application might use function calling for app-specific logic while also acting as an MCP client to connect to shared team infrastructure.
