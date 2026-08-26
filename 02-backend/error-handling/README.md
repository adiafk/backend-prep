# Error Handling Patterns

**Related:** [Logging](../logging/README.md) | [HTTP](../http/README.md)

---

## Error Classification

Understanding what kind of error you have determines how you handle it.

### Operational vs Programmer Errors

| Type | Definition | Response |
|------|-----------|----------|
| **Operational** | Expected failures in a correct program | Handle gracefully, return appropriate response |
| **Programmer** | Bugs — code does something it should not | Fix the code; crash loudly in development |

```typescript
// Operational: network timeout, DB connection refused, invalid user input
// These are normal. Your code handles them.
try {
  await db.users.findById(id);
} catch (err) {
  if (err instanceof DatabaseConnectionError) {
    return res.status(503).json({ error: "Database unavailable" });
  }
  throw err; // Unexpected — let it propagate
}

// Programmer: accessing property on undefined, passing wrong type
// These should crash and alert you, not be silently swallowed
const user = null;
user.name; // TypeError — this is a bug, not an operational error
```

### Transient vs Permanent

- **Transient:** Will likely succeed on retry (network blip, rate limit, lock timeout). Use retry with backoff.
- **Permanent:** Retrying will not help (invalid input, not found, permission denied). Return immediately.

---

## Typed Errors in TypeScript

### Custom Error Classes

Extend `Error` to create specific, catchable error types with structured data.

```typescript
// Base class — adds a code and HTTP status
abstract class AppError extends Error {
  abstract readonly statusCode: number;
  abstract readonly code: string;
  readonly isOperational = true;

  constructor(message: string, public readonly context?: Record<string, unknown>) {
    super(message);
    this.name = this.constructor.name;
    // Restore prototype chain (needed when targeting ES5)
    Object.setPrototypeOf(this, new.target.prototype);
  }
}

class NotFoundError extends AppError {
  readonly statusCode = 404;
  readonly code = "NOT_FOUND";
}

class ValidationError extends AppError {
  readonly statusCode = 400;
  readonly code = "VALIDATION_ERROR";
  constructor(message: string, public readonly fields: Record<string, string>) {
    super(message);
  }
}

class ConflictError extends AppError {
  readonly statusCode = 409;
  readonly code = "CONFLICT";
}

class ForbiddenError extends AppError {
  readonly statusCode = 403;
  readonly code = "FORBIDDEN";
}

class InfrastructureError extends AppError {
  readonly statusCode = 503;
  readonly code = "SERVICE_UNAVAILABLE";
}

// Usage
throw new NotFoundError("User not found", { userId: id });
throw new ValidationError("Invalid input", { email: "Must be a valid email" });
```

### The Result / Either Pattern

An alternative to `throw` that makes error cases explicit in the type signature. The function cannot silently fail — callers must handle both branches.

```typescript
type Ok<T> = { ok: true; value: T };
type Err<E> = { ok: false; error: E };
type Result<T, E = AppError> = Ok<T> | Err<E>;

function ok<T>(value: T): Ok<T> {
  return { ok: true, value };
}

function err<E>(error: E): Err<E> {
  return { ok: false, error };
}

// Function signature makes it clear this can fail
async function findUser(id: string): Promise<Result<User, NotFoundError | InfrastructureError>> {
  try {
    const user = await db.users.findById(id);
    if (!user) return err(new NotFoundError("User not found"));
    return ok(user);
  } catch (e) {
    return err(new InfrastructureError("Database query failed"));
  }
}

// Caller is forced to handle both cases
const result = await findUser(userId);
if (!result.ok) {
  if (result.error instanceof NotFoundError) {
    return res.status(404).json({ error: result.error.message });
  }
  throw result.error; // Let infrastructure errors propagate
}
const user = result.value; // TypeScript knows this is User
```

### Discriminated Union Error Types

For APIs that return multiple different error shapes:

```typescript
type CreateUserError =
  | { type: "EMAIL_TAKEN"; email: string }
  | { type: "INVALID_INPUT"; fields: Record<string, string> }
  | { type: "RATE_LIMITED"; retryAfter: number };

async function createUser(
  input: CreateUserInput
): Promise<Result<User, CreateUserError>> {
  const existing = await db.users.findByEmail(input.email);
  if (existing) {
    return err({ type: "EMAIL_TAKEN", email: input.email });
  }
  // ...
}

// Exhaustive handling — TypeScript will error if a case is missing
const result = await createUser(input);
if (!result.ok) {
  switch (result.error.type) {
    case "EMAIL_TAKEN":
      return res.status(409).json({ message: `${result.error.email} is already registered` });
    case "INVALID_INPUT":
      return res.status(400).json({ fields: result.error.fields });
    case "RATE_LIMITED":
      return res.status(429).json({ retryAfter: result.error.retryAfter });
  }
}
```

---

## HTTP Error Response Structure (RFC 7807)

RFC 7807 "Problem Details for HTTP APIs" defines a standard error response format. Using it makes errors machine-readable and consistent across your API.

```json
{
  "type": "https://api.example.com/errors/validation-error",
  "title": "Your request data was invalid",
  "status": 400,
  "detail": "The email field must be a valid email address",
  "instance": "/users/register",
  "requestId": "req_01H8X9K2P3Q4R5S6T7",
  "fields": {
    "email": "Must be a valid email address",
    "password": "Must be at least 8 characters"
  }
}
```

```typescript
interface ProblemDetails {
  type: string;
  title: string;
  status: number;
  detail?: string;
  instance?: string;
  [key: string]: unknown; // Extension members allowed
}

function toProblemDetails(err: AppError, requestId: string, path: string): ProblemDetails {
  return {
    type: `https://api.example.com/errors/${err.code.toLowerCase().replace(/_/g, "-")}`,
    title: err.message,
    status: err.statusCode,
    instance: path,
    requestId,
    ...(err instanceof ValidationError ? { fields: err.fields } : {}),
  };
}
```

---

## Async Error Propagation

The most common Node.js footgun: a Promise rejection that nothing catches.

```typescript
// BAD: fire-and-forget — rejection goes unhandled, crashes Node
async function processQueue() {
  const jobs = await queue.getJobs();
  jobs.forEach(async (job) => {
    await processJob(job); // If this throws, nothing catches it
  });
}

// GOOD: use Promise.allSettled to process all and collect failures
async function processQueue() {
  const jobs = await queue.getJobs();
  const results = await Promise.allSettled(jobs.map(processJob));

  results.forEach((result, i) => {
    if (result.status === "rejected") {
      logger.error({ jobId: jobs[i].id, err: result.reason }, "Job failed");
    }
  });
}

// GOOD: use for...of so async/await propagates naturally
async function processQueue() {
  const jobs = await queue.getJobs();
  for (const job of jobs) {
    try {
      await processJob(job);
    } catch (err) {
      logger.error({ jobId: job.id, err }, "Job failed");
    }
  }
}
```

---

## Global Error Handlers

### Express Error Middleware

Express error middleware has exactly four parameters — omitting the fourth means Express treats it as a regular middleware, not an error handler.

```typescript
import { Request, Response, NextFunction } from "express";

function globalErrorHandler(
  err: unknown,
  req: Request,
  res: Response,
  _next: NextFunction  // Must be present even if unused
): void {
  const requestId = req.headers["x-request-id"] as string ?? crypto.randomUUID();

  if (err instanceof AppError && err.isOperational) {
    const body = toProblemDetails(err, requestId, req.path);
    res.status(err.statusCode).json(body);
    return;
  }

  // Programmer error or unexpected exception
  logger.error({ err, requestId, path: req.path }, "Unhandled error");
  res.status(500).json({
    type: "https://api.example.com/errors/internal-server-error",
    title: "An unexpected error occurred",
    status: 500,
    requestId,
  });
}

app.use(globalErrorHandler); // Must be registered AFTER all routes
```

### NestJS Exception Filters

```typescript
import { ExceptionFilter, Catch, ArgumentsHost, HttpException } from "@nestjs/common";
import { Response } from "express";

@Catch()
export class GlobalExceptionFilter implements ExceptionFilter {
  catch(exception: unknown, host: ArgumentsHost) {
    const ctx = host.switchToHttp();
    const response = ctx.getResponse<Response>();
    const request = ctx.getRequest();

    if (exception instanceof HttpException) {
      const status = exception.getStatus();
      response.status(status).json({
        type: `https://api.example.com/errors/${status}`,
        title: exception.message,
        status,
        instance: request.url,
      });
      return;
    }

    logger.error({ err: exception }, "Unhandled exception");
    response.status(500).json({ title: "Internal server error", status: 500 });
  }
}
```

### Process-Level Handlers

```typescript
// Unhandled promise rejection — catches forgotten async errors
process.on("unhandledRejection", (reason, promise) => {
  logger.error({ reason }, "Unhandled promise rejection");
  // Let the process exit — a process manager (PM2, Kubernetes) restarts it
  // Running in an unknown state is worse than crashing
  process.exit(1);
});

// Uncaught synchronous exception — truly unexpected
process.on("uncaughtException", (err) => {
  logger.error({ err }, "Uncaught exception — shutting down");
  process.exit(1);
});
```

---

## HTTP Status Codes for Different Error Types

```typescript
// Validation error (client sent bad data) → 400 Bad Request
throw new ValidationError("Invalid email format", { email: "Must be a valid email" });

// Authentication missing → 401 Unauthorized
throw new AuthenticationError("Bearer token required");

// Authenticated but not allowed → 403 Forbidden
throw new ForbiddenError("You do not own this resource");

// Resource does not exist → 404 Not Found
throw new NotFoundError(`Post ${id} not found`);

// State conflict (duplicate, version mismatch) → 409 Conflict
throw new ConflictError("Email address is already registered");

// Unprocessable input (valid format, wrong semantics) → 422 Unprocessable Entity
throw new UnprocessableError("Cannot transfer to the same account");

// Rate limited → 429 Too Many Requests
throw new RateLimitError("Too many requests", { retryAfter: 60 });

// Dependency down → 503 Service Unavailable
throw new InfrastructureError("Payment service is unavailable");
```

---

## Database Constraint Violations

Map database errors to appropriate HTTP responses:

```typescript
import { PrismaClientKnownRequestError } from "@prisma/client/runtime";

async function createUser(input: CreateUserInput): Promise<User> {
  try {
    return await db.user.create({ data: input });
  } catch (err) {
    if (err instanceof PrismaClientKnownRequestError) {
      switch (err.code) {
        case "P2002":
          // Unique constraint violation
          const field = (err.meta?.target as string[])?.[0] ?? "field";
          throw new ConflictError(`${field} is already taken`);

        case "P2003":
          // Foreign key constraint violation
          throw new ValidationError("Referenced resource does not exist", {});

        case "P2025":
          // Record not found (e.g., in update/delete)
          throw new NotFoundError("Record not found");
      }
    }
    throw err; // Re-throw unknown errors
  }
}
```

---

## Error Recovery Patterns

### Retry with Exponential Backoff

```typescript
interface RetryOptions {
  maxAttempts: number;
  initialDelayMs: number;
  maxDelayMs: number;
  isRetryable: (err: unknown) => boolean;
}

async function withRetry<T>(
  operation: () => Promise<T>,
  options: RetryOptions
): Promise<T> {
  let lastError: unknown;

  for (let attempt = 1; attempt <= options.maxAttempts; attempt++) {
    try {
      return await operation();
    } catch (err) {
      lastError = err;

      if (!options.isRetryable(err) || attempt === options.maxAttempts) {
        throw err;
      }

      const delay = Math.min(
        options.initialDelayMs * Math.pow(2, attempt - 1) +
          Math.random() * 100, // Jitter prevents thundering herd
        options.maxDelayMs
      );

      logger.warn({ attempt, delay, err }, "Retrying after error");
      await new Promise((resolve) => setTimeout(resolve, delay));
    }
  }

  throw lastError;
}

// Usage
const data = await withRetry(
  () => externalApi.getData(),
  {
    maxAttempts: 3,
    initialDelayMs: 100,
    maxDelayMs: 5000,
    isRetryable: (err) =>
      err instanceof NetworkError ||
      (err instanceof HttpError && err.statusCode >= 500),
  }
);
```

### Fallback Values

```typescript
async function getUserPreferences(userId: string): Promise<UserPreferences> {
  try {
    return await cache.get(`prefs:${userId}`)
      ?? await db.preferences.findByUserId(userId)
      ?? DEFAULT_PREFERENCES;
  } catch (err) {
    // Preferences being unavailable should not break the page
    logger.warn({ userId, err }, "Failed to load user preferences, using defaults");
    return DEFAULT_PREFERENCES;
  }
}
```

---

## Logging Errors with Context

```typescript
// WRONG: swallow the error silently
try {
  await sendEmail(user.email, template);
} catch (err) {
  // nothing
}

// WRONG: log without context — impossible to debug
} catch (err) {
  console.log("Error");
}

// WRONG: log PII
} catch (err) {
  logger.error({ password: input.password, err }, "Login failed");
}

// CORRECT: log structured context, no PII, with requestId for correlation
} catch (err) {
  logger.error({
    requestId: ctx.requestId,
    userId: ctx.currentUser?.id,  // ID is fine; email/name is not
    operation: "sendWelcomeEmail",
    err: {
      message: err.message,
      stack: err.stack,
      code: err.code,
    },
  }, "Failed to send welcome email");
}
```

---

## Graceful Shutdown

When a fatal error is received, drain in-flight requests before exiting rather than terminating abruptly.

```typescript
let isShuttingDown = false;

function initiateGracefulShutdown(reason: string) {
  if (isShuttingDown) return;
  isShuttingDown = true;

  logger.info({ reason }, "Initiating graceful shutdown");

  const server = app.listen();

  // Stop accepting new connections
  server.close(async () => {
    try {
      await db.$disconnect();
      await queue.close();
      logger.info("Graceful shutdown complete");
      process.exit(0);
    } catch (err) {
      logger.error({ err }, "Error during shutdown");
      process.exit(1);
    }
  });

  // Force exit if drain takes too long
  setTimeout(() => {
    logger.error("Graceful shutdown timed out — forcing exit");
    process.exit(1);
  }, 30_000);
}

process.on("SIGTERM", () => initiateGracefulShutdown("SIGTERM"));
process.on("SIGINT", () => initiateGracefulShutdown("SIGINT"));
```

---

## Interview Q&A

**Q: What is the difference between an operational error and a programmer error?**

An operational error is something that can go wrong in a correctly written program — a user sends invalid input, a database connection times out, an external API returns a 503. These are expected and handled: you return an appropriate HTTP response and log a warning.

A programmer error is a bug — a null pointer dereference, a type error, calling a function with the wrong number of arguments. These should never be silently caught and swallowed. In development, they should crash immediately so you find and fix them. In production, they should crash and restart via a process manager, because continuing to run in an undefined state is more dangerous than a brief restart.

The mistake people make is writing a catch-all try/catch at the top of every function that swallows both kinds. You end up with silent bugs that are nearly impossible to track down.

---

**Q: How should you handle a database unique constraint violation?**

Map it at the database client layer to a typed application error, then let the global error handler render the correct HTTP response. For a unique constraint on `email`, return HTTP 409 Conflict with a message like "Email is already registered." Do not return 500 — a 500 tells the client there is something wrong with your server, when actually the client sent data that conflicts with existing state. Most database clients expose error codes that identify the constraint type (Prisma: P2002, pg: error code 23505, MySQL: 1062).

---

**Q: Why is `forEach` with an async callback dangerous?**

`Array.prototype.forEach` does not await its callback. If you pass an `async` function to it, it launches promises but does not wait for them and does not catch their rejections. Any error thrown inside the async callback becomes an unhandled rejection. Use `for...of` with `await` when you need sequential execution, or `Promise.all`/`Promise.allSettled` when you want concurrency with proper error handling.

---

**Q: What is RFC 7807 and why should you use it?**

RFC 7807 defines a standard JSON format for HTTP error responses with fields: `type` (a URI that identifies the error class), `title` (human-readable summary), `status` (the HTTP status code), `detail` (specific detail about this occurrence), and `instance` (the URI of the specific request that failed). You can add custom extension fields.

Using it means every error your API returns has the same shape, so clients can reliably parse error responses regardless of which endpoint failed. It also makes errors machine-readable — a client can check `type` to handle specific error classes programmatically. Without a standard, every endpoint invents its own `{ error: "..." }` or `{ message: "..." }` format, and clients have to handle them all differently.
