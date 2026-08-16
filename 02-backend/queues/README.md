# Queues and Async Processing

## Table of Contents
1. [Why Queues](#why-queues)
2. [Queue Patterns](#queue-patterns)
3. [BullMQ Fundamentals](#bullmq-fundamentals)
4. [Job Lifecycle](#job-lifecycle)
5. [Failure Handling](#failure-handling)
6. [At-Least-Once vs Exactly-Once Delivery](#at-least-once-vs-exactly-once-delivery)
7. [Queue vs Pub/Sub](#queue-vs-pubsub)
8. [Bull Dashboard — Observability](#bull-dashboard--observability)
9. [Complete TypeScript Example](#complete-typescript-example)

---

## Why Queues

### The Core Problem: Synchronous Coupling

Without queues, services call each other directly. A user uploads a video; the API synchronously transcodes it, sends a confirmation email, updates analytics, and generates thumbnails — all before returning HTTP 200. This breaks in multiple ways:

- The request times out after 30 seconds. Transcoding takes 10 minutes.
- The email service is down. The entire upload fails.
- A traffic spike slows transcoding. It backs up HTTP workers. The whole API slows down.

Queues solve this by separating the act of *accepting work* from *performing work*.

### Decoupling

The producer (the code that submits a job) does not know or care which consumer processes it, when it runs, or how many retries it takes. The producer just writes to the queue and moves on. This means:

- Producers and consumers can be deployed independently.
- Consumers can be written in a different language or run on different infrastructure.
- Consumers can be swapped without touching producer code.

### Backpressure

Queues act as a buffer. If consumers are slower than producers, jobs accumulate in the queue rather than crashing either side. Operators can observe queue depth and scale workers horizontally when depth grows. Without a queue, the consumer either rejects overflow requests (errors) or gets overwhelmed (OOM crash, latency spiral).

Backpressure is a first-class signal: a deep queue tells you exactly where to add capacity.

### Reliability

A synchronous call that fails loses the work. A queued job that fails can be retried, with the job safely persisted in Redis or a database. The work is not lost even if the worker crashes mid-execution. Reliable delivery is one of the primary reasons to introduce a queue even when latency is not a concern.

### Async Processing

Some work is inherently asynchronous from the user's perspective:
- Sending emails
- Generating reports
- Resizing images
- Syncing data to a third-party API
- Running ML inference

The user should not wait for these. Accept the request, enqueue the job, return immediately, and let the worker handle it in the background. The user gets a fast response; the work gets done.

---

## Queue Patterns

### Task Queue (Work Queue)

A task queue distributes jobs to a pool of workers. Each job is processed by exactly one worker. This is the most common pattern.

```
Producer --> [Queue] --> Worker A
                    --> Worker B
                    --> Worker C
```

Use cases: sending emails, processing uploads, running scheduled jobs, calling external APIs.

Key property: a job is consumed once. If you have three workers, they compete for jobs. Worker A taking a job means Workers B and C do not process that same job.

### Pub/Sub (Publish-Subscribe)

In pub/sub, a message published to a channel is delivered to *all* subscribers. There is no concept of a job being "consumed" — every subscriber gets every message.

```
Publisher --> [Topic] --> Subscriber A (gets copy)
                     --> Subscriber B (gets copy)
                     --> Subscriber C (gets copy)
```

Use cases: broadcasting events, cache invalidation across services, real-time notifications, audit logging alongside business logic.

Key property: fan-out. One publish creates N deliveries. Adding a new subscriber does not affect the publisher or other subscribers.

### Competing Consumers

This is the concurrency model inside a task queue. Multiple worker instances read from the same queue. They compete to claim jobs. The first to claim a job locks it; others do not see it until the lock expires.

This pattern enables horizontal scaling: add more worker processes to increase throughput. The queue coordinates between them automatically — no custom distributed locking required in application code.

### Priority Queues

Jobs are assigned a numeric priority. Higher-priority jobs are dequeued before lower-priority ones even if they arrived later. Useful when you need to differentiate between paid vs. free users, urgent vs. background tasks, or real-time vs. batch workloads on the same queue infrastructure.

### Delayed Jobs

A job is placed in the queue but should not be processed until a specific time. Useful for:
- Sending a welcome email 24 hours after signup
- Scheduling a reminder
- Retry backoff (retry after 30 seconds)

---

## BullMQ Fundamentals

BullMQ is a Node.js queue library built on Redis. It is the successor to Bull, with TypeScript-first design and a more robust architecture.

### Core Concepts

**Queue**: A named channel to which jobs are added. The Queue class is used by producers. Multiple producers can write to the same named queue.

**Worker**: Subscribes to a queue and executes jobs. Multiple Worker instances can run in the same process or across multiple processes/machines, all pointing at the same Redis and queue name.

**Job**: A unit of work. Contains a `name` (job type), `data` (arbitrary serializable payload), and `opts` (configuration like delay, priority, retry settings).

**QueueScheduler** (BullMQ v1) / built-in scheduler (BullMQ v2+): Handles delayed and repeatable jobs. In BullMQ v2+, this is built into the Worker.

### Concurrency

A Worker's `concurrency` option controls how many jobs it processes simultaneously. A single Worker instance with `concurrency: 5` processes up to 5 jobs in parallel. Across multiple Worker instances, total throughput multiplies.

```
// 3 worker processes, each with concurrency 4 = 12 parallel jobs
const worker = new Worker('email', processor, { connection, concurrency: 4 });
```

Setting concurrency too high on CPU-bound work causes context-switching overhead. For I/O-bound work (network calls, database queries), higher concurrency is fine.

### Priorities

Jobs accept a `priority` option (1 = highest, larger numbers = lower priority). Within a priority level, jobs are FIFO. BullMQ uses sorted sets in Redis to implement this efficiently.

```typescript
await queue.add('send-email', { to: 'vip@example.com' }, { priority: 1 });
await queue.add('send-email', { to: 'user@example.com' }, { priority: 10 });
```

### Connection

BullMQ uses `ioredis` under the hood. You pass a connection config or an existing `ioredis` instance.

```typescript
const connection = { host: 'localhost', port: 6379 };
```

---

## Job Lifecycle

```
             add()
               |
               v
          [waiting]  <----------- (retry after failure)
               |
         worker picks up
               |
               v
           [active]   <--- job is being processed, lock is held
               |
        +------+------+
        |             |
        v             v
  [completed]      [failed]
                      |
               retries exhausted?
                  yes |
                      v
               [dead letter / failed set]
```

### States

**waiting**: Job is in the queue, ready to be picked up by a worker. Also called "pending". Jobs are ordered by priority and arrival time.

**active**: A worker has claimed the job and is executing the processor function. BullMQ holds a Redis lock for the duration. If the worker crashes and the lock expires, the job moves back to waiting (stalled job recovery).

**completed**: The processor function resolved successfully. The job data is kept in the completed set for a configurable duration before being removed.

**failed**: The processor function threw an error. BullMQ checks the retry configuration. If retries remain, the job re-enters the waiting state after an optional delay. If retries are exhausted, it moves to the failed set permanently.

**delayed**: The job has a `delay` option set, or is waiting for a retry backoff. It sits in a sorted set keyed by its execution timestamp and moves to waiting when its time arrives.

**waiting-children** (BullMQ flows): A parent job waiting for all child jobs to complete before it becomes active. Part of the job flow/dependency system.

**repeat**: Not a state but a configuration. Repeatable jobs are scheduled to re-add themselves to the waiting queue on a cron or interval basis.

### Stalled Jobs

If a worker crashes while processing a job (the active lock expires), BullMQ detects the stalled job and moves it back to waiting for another worker to pick up. This provides at-least-once delivery guarantees even under worker failure.

---

## Failure Handling

### Retries

Configure retries in job options:

```typescript
await queue.add('process-payment', data, {
  attempts: 5,        // total attempts (1 initial + 4 retries)
  backoff: {
    type: 'exponential',
    delay: 1000,      // initial delay in ms
  },
});
```

The processor throws an error to signal failure. BullMQ catches it, checks remaining attempts, and re-queues the job.

### Exponential Backoff

Retrying immediately after failure often fails again for the same reason (rate limit, transient network error, dependent service restart). Exponential backoff increases the delay between each retry:

```
Attempt 1 fails  --> wait 1s
Attempt 2 fails  --> wait 2s
Attempt 3 fails  --> wait 4s
Attempt 4 fails  --> wait 8s
Attempt 5 fails  --> wait 16s
```

The formula: `delay * 2^(attemptsMade - 1)`

Add jitter to avoid thundering herd (many jobs retrying at exactly the same time):

```
actual_delay = base_delay * 2^attempt * random(0.5, 1.5)
```

BullMQ's built-in exponential backoff does not add jitter automatically — implement it with a custom backoff strategy if needed.

### Custom Backoff

```typescript
await queue.add('job', data, {
  attempts: 5,
  backoff: {
    type: 'custom',
  },
});

// In Worker options:
const worker = new Worker('queue', processor, {
  connection,
  settings: {
    backoffStrategy: (attemptsMade: number) => {
      // Custom jitter: base * 2^n * random multiplier
      const base = 1000;
      const exp = Math.pow(2, attemptsMade - 1);
      const jitter = 0.5 + Math.random();
      return Math.floor(base * exp * jitter);
    },
  },
});
```

### Dead Letter Queue (DLQ)

When a job exhausts all retry attempts, it moves to BullMQ's failed set. This is the functional equivalent of a dead letter queue — a holding area for jobs that could not be processed.

From the failed set you can:
- Inspect the error and stack trace
- Manually retry individual jobs after fixing a bug
- Alert on high failed counts
- Archive to a database for audit

BullMQ does not ship a separate DLQ concept — the failed set *is* your DLQ. You can implement explicit DLQ behavior by listening to the `failed` event and re-adding the job to a separate `*-dlq` queue with no retries.

```typescript
worker.on('failed', async (job, err) => {
  if (job && job.attemptsMade >= (job.opts.attempts ?? 1)) {
    await dlqQueue.add(job.name, {
      originalData: job.data,
      error: err.message,
      failedAt: new Date().toISOString(),
    });
  }
});
```

### Error Isolation

Do not let one bad job pattern starve the queue. Strategies:

- **Rate limiting**: BullMQ supports `RateLimiter` to cap how many jobs a worker processes per time window.
- **Separate queues by job type**: A broken image-processing job type should not block email sends. Use dedicated queues.
- **removeOnFail limits**: Cap how many failed jobs are retained to avoid Redis memory growth.

---

## At-Least-Once vs Exactly-Once Delivery

### At-Least-Once

A job may be processed more than once, but it will always be processed at least once. This is what BullMQ (and most queue systems) provide.

Why does this happen? The worker completes the job but crashes before acknowledging completion to the queue. The queue's lock expires, and another worker picks up the same job. Result: the job ran twice.

This is the safe default because the alternative — at-most-once — means jobs can be lost. At-least-once is recoverable; duplicate processing can be handled by the application. Lost work cannot be recovered.

**Implication**: Your job processors must be **idempotent** — running the same job twice with the same data must produce the same result as running it once.

Examples of idempotent operations:
- Sending an email with a unique message ID header (email servers deduplicate)
- `INSERT ... ON CONFLICT DO NOTHING`
- Setting a value (not incrementing)
- Checking "has this job run before" in a Redis set or database before doing work

Examples of non-idempotent operations that need guards:
- `INSERT` without dedup check (creates duplicate rows)
- Charging a credit card (charges twice)
- Incrementing a counter

### Exactly-Once

Exactly-once means every job is processed precisely once — no duplicates, no losses. This sounds like the obviously correct goal. It is extremely difficult to achieve in distributed systems.

**Why it is hard:**

The core problem is the **two-generals problem** / lack of atomic commit across two systems. Consider:

1. Worker processes the job (writes to database)
2. Worker tries to acknowledge to Redis that the job is done
3. Worker crashes before step 2

The job has been processed, but the queue thinks it has not. Whoever checks next will reprocess it. There is no way to make steps 1 and 2 atomic unless both operations target the same transactional system (e.g., a database-backed queue where job state and business data share the same transaction).

**Approaches that approximate exactly-once:**

1. **Idempotency keys**: Do not try to prevent duplicate delivery. Instead, make your processor detect and skip duplicate work. Store processed job IDs in a database/Redis set. Check before processing.

2. **Transactional outbox**: Write both the business operation and "mark job done" to the same database transaction. A separate poller confirms acknowledgment.

3. **Database-backed queues**: Some systems (like Postgres-backed queues: Graphile Worker, pg-boss) can participate in the same transaction as the business logic, giving true exactly-once within the database boundary.

4. **Kafka with idempotent producers and transactional consumers**: Kafka provides exactly-once semantics within the Kafka ecosystem with careful configuration, but this is complex and has performance costs.

**The practical answer**: Design for at-least-once and make processors idempotent. This is simpler, more reliable under failure, and performs better than trying to enforce exactly-once.

---

## Queue vs Pub/Sub

| Dimension | Task Queue (BullMQ) | Pub/Sub (Redis Pub/Sub, Kafka, SNS) |
|---|---|---|
| Delivery | One consumer per message | All subscribers get a copy |
| Persistence | Jobs persisted in Redis | Depends on system; often ephemeral |
| Consumer count | Scales workers horizontally | Each subscriber independent |
| Acknowledgment | Worker acks; retries on failure | Varies; often fire-and-forget |
| Use case | Background jobs, work distribution | Event broadcasting, fan-out |
| When consumer is down | Jobs wait in queue | Messages lost (unless durable) |

### When to Use a Task Queue

- You need **exactly one** consumer to process each item
- Work needs to be **retried** on failure
- You need **backpressure** — slow consumers should not crash producers
- You need **delayed execution** or **scheduled jobs**
- Work items are **stateful** (you care about success/failure of each one)
- Examples: send email, process payment, transcode video, run a report

### When to Use Pub/Sub

- Multiple independent services need to **react to the same event**
- You want **decoupled notification** without knowing who is listening
- Adding new subscribers should not require producer changes
- Events are **notifications**, not commands — you do not need to track their processing
- Examples: "user signed up" event consumed by email service, analytics service, and CRM simultaneously; cache invalidation broadcast; real-time UI updates via WebSocket

### Hybrid: Event Bus with Task Queues

A common architecture:
1. A domain event is published to a pub/sub topic ("order.placed")
2. Each subscriber service has its own task queue
3. The subscriber's event handler enqueues a job into its own queue
4. Workers process the job with retries and reliability

This gives you fan-out (pub/sub) plus reliable processing per subscriber (task queue).

---

## Bull Dashboard — Observability

Bull Board (and Arena, Taskforce.sh) are UI dashboards for monitoring BullMQ queues.

### What the Dashboard Shows

- **Queue metrics**: total waiting, active, completed, failed, delayed jobs
- **Job details**: payload, result, error message, stack trace, timestamps, attempt count
- **Job controls**: manually retry failed jobs, remove jobs, clean completed sets
- **Throughput graphs**: jobs processed per second over time
- **Worker status**: which workers are connected and active

### Setting Up Bull Board

```typescript
import { createBullBoard } from '@bull-board/api';
import { BullMQAdapter } from '@bull-board/api/bullMQAdapter';
import { ExpressAdapter } from '@bull-board/express';
import express from 'express';

const serverAdapter = new ExpressAdapter();
serverAdapter.setBasePath('/admin/queues');

createBullBoard({
  queues: [new BullMQAdapter(emailQueue), new BullMQAdapter(videoQueue)],
  serverAdapter,
});

const app = express();
app.use('/admin/queues', serverAdapter.getRouter());
```

### Metrics to Monitor

- **Queue depth (waiting count)**: primary scaling signal. If consistently growing, add workers.
- **Failed count**: alert when this exceeds a threshold. Indicates a systemic error.
- **Active count**: should be <= workers * concurrency. If maxed out, workers are saturated.
- **Job age in queue**: P50/P99 time from enqueue to active. Measures true latency.
- **Processing time**: how long active jobs take. Regressions indicate performance problems.

### Programmatic Metrics

```typescript
const counts = await queue.getJobCounts(
  'waiting', 'active', 'completed', 'failed', 'delayed'
);
// { waiting: 42, active: 5, completed: 1200, failed: 3, delayed: 10 }
```

Export these to Prometheus/Grafana for production alerting.

---

## Complete TypeScript Example

The following example demonstrates a production-ready BullMQ setup with:
- Queue setup and configuration
- A typed job producer
- A worker with error handling, retry logic, and dead letter queue
- Event listeners for observability

### Installation

```bash
npm install bullmq ioredis
npm install --save-dev @types/node
```

### Types

```typescript
// src/queues/types.ts

export interface EmailJobData {
  to: string;
  subject: string;
  body: string;
  messageId: string; // idempotency key
}

export interface EmailJobResult {
  accepted: string[];
  messageId: string;
}

export type JobName = 'send-welcome-email' | 'send-password-reset' | 'send-notification';
```

### Queue Setup

```typescript
// src/queues/emailQueue.ts

import { Queue, QueueEvents } from 'bullmq';
import { EmailJobData } from './types';

const connection = {
  host: process.env.REDIS_HOST ?? 'localhost',
  port: parseInt(process.env.REDIS_PORT ?? '6379', 10),
  password: process.env.REDIS_PASSWORD,
  // maxRetriesPerRequest must be null for BullMQ blocking commands
  maxRetriesPerRequest: null,
};

// Default job options applied to every job unless overridden
const defaultJobOptions = {
  attempts: 5,
  backoff: {
    type: 'exponential' as const,
    delay: 2000, // 2s, 4s, 8s, 16s, 32s
  },
  removeOnComplete: {
    age: 3600,    // keep completed jobs for 1 hour
    count: 1000,  // keep last 1000 completed jobs
  },
  removeOnFail: {
    age: 24 * 3600, // keep failed jobs for 24 hours
  },
};

export const emailQueue = new Queue<EmailJobData>('email', {
  connection,
  defaultJobOptions,
});

// QueueEvents lets you listen to job lifecycle events from any process
export const emailQueueEvents = new QueueEvents('email', { connection });

emailQueueEvents.on('completed', ({ jobId }) => {
  console.log(`[queue] job ${jobId} completed`);
});

emailQueueEvents.on('failed', ({ jobId, failedReason }) => {
  console.error(`[queue] job ${jobId} failed: ${failedReason}`);
});

emailQueueEvents.on('stalled', ({ jobId }) => {
  console.warn(`[queue] job ${jobId} stalled — will be retried`);
});
```

### Job Producer

```typescript
// src/queues/emailProducer.ts

import { emailQueue } from './emailQueue';
import { EmailJobData, JobName } from './types';
import crypto from 'crypto';

export async function enqueueWelcomeEmail(userId: string, email: string): Promise<string> {
  const messageId = `welcome-${userId}-${Date.now()}`;

  const data: EmailJobData = {
    to: email,
    subject: 'Welcome!',
    body: `Welcome to the platform, your account is ready.`,
    messageId,
  };

  const job = await emailQueue.add('send-welcome-email' satisfies JobName, data, {
    // Job-specific options override queue defaults
    priority: 5,
    // Unique option prevents duplicate jobs with the same key
    // If a job with this jobId already exists in waiting/active/delayed, skip it
    jobId: `welcome-${userId}`, // idempotency: only one welcome email per user in flight
  });

  console.log(`[producer] enqueued job ${job.id} for ${email}`);
  return job.id!;
}

export async function enqueuePasswordReset(
  userId: string,
  email: string,
  resetToken: string
): Promise<string> {
  const data: EmailJobData = {
    to: email,
    subject: 'Password Reset Request',
    body: `Your reset token: ${resetToken}`,
    messageId: `reset-${userId}-${crypto.randomUUID()}`,
  };

  const job = await emailQueue.add('send-password-reset' satisfies JobName, data, {
    priority: 1,   // high priority — user is waiting
    attempts: 3,   // override default 5 retries; fail faster for resets
    delay: 0,
  });

  return job.id!;
}

export async function enqueueDelayedNotification(
  email: string,
  message: string,
  delayMs: number
): Promise<string> {
  const data: EmailJobData = {
    to: email,
    subject: 'Notification',
    body: message,
    messageId: `notif-${Date.now()}-${Math.random()}`,
  };

  const job = await emailQueue.add('send-notification' satisfies JobName, data, {
    delay: delayMs, // process after delayMs milliseconds
  });

  return job.id!;
}
```

### Worker

```typescript
// src/queues/emailWorker.ts

import { Worker, Job } from 'bullmq';
import { EmailJobData, EmailJobResult, JobName } from './types';
import { emailQueue } from './emailQueue';

const connection = {
  host: process.env.REDIS_HOST ?? 'localhost',
  port: parseInt(process.env.REDIS_PORT ?? '6379', 10),
  password: process.env.REDIS_PASSWORD,
  maxRetriesPerRequest: null,
};

// Deduplicated message ID store — in production, use Redis or your database
const processedMessageIds = new Set<string>();

// Dead letter queue for jobs that exhaust all retries
const dlqQueue = new Queue('email-dlq', { connection });

// --- Processor function ---
// This is called for every job. It must return a result or throw an error.
async function processEmailJob(job: Job<EmailJobData, EmailJobResult, JobName>): Promise<EmailJobResult> {
  const { to, subject, body, messageId } = job.data;

  // Idempotency check: skip if we already processed this message
  if (processedMessageIds.has(messageId)) {
    console.log(`[worker] skipping duplicate job ${job.id}, messageId ${messageId}`);
    return { accepted: [to], messageId };
  }

  job.log(`Processing email to ${to}, attempt ${job.attemptsMade + 1}`);

  try {
    // Simulate sending email via SMTP/SES
    await sendEmail({ to, subject, body });

    // Mark as processed only after confirmed send
    processedMessageIds.add(messageId);

    // Update job progress (visible in dashboard)
    await job.updateProgress(100);

    console.log(`[worker] sent email to ${to} (job ${job.id})`);
    return { accepted: [to], messageId };

  } catch (error) {
    const err = error as Error;

    // Distinguish retriable from non-retriable errors
    if (isNonRetriableError(err)) {
      // Throw UnrecoverableError to skip remaining retries
      throw new UnrecoverableError(`Non-retriable error: ${err.message}`);
    }

    // Throw normal error — BullMQ will retry according to job options
    throw new Error(`Failed to send email to ${to}: ${err.message}`);
  }
}

// Simulated email send — replace with nodemailer/SES/Postmark
async function sendEmail(params: { to: string; subject: string; body: string }): Promise<void> {
  // Simulate random transient failure for demonstration
  if (Math.random() < 0.1) {
    throw new Error('SMTP connection timeout');
  }
  // Actual send logic here
  await new Promise((resolve) => setTimeout(resolve, 100));
}

// Classify errors that should not be retried
function isNonRetriableError(err: Error): boolean {
  const nonRetriable = [
    'invalid email address',
    'domain not found',
    'user does not exist',
  ];
  return nonRetriable.some((msg) => err.message.toLowerCase().includes(msg));
}

// --- Worker ---
import { UnrecoverableError } from 'bullmq';
import { Queue } from 'bullmq';

const worker = new Worker<EmailJobData, EmailJobResult, JobName>(
  'email',
  processEmailJob,
  {
    connection,
    concurrency: 10,           // process up to 10 jobs in parallel
    limiter: {
      max: 100,                // max 100 jobs per duration
      duration: 60_000,        // per 60 seconds (rate limit)
    },
  }
);

// --- Lifecycle event handlers ---

worker.on('completed', (job, result) => {
  console.log(`[worker] completed job ${job.id} → accepted: ${result.accepted.join(', ')}`);
});

worker.on('failed', async (job, err) => {
  if (!job) return;

  const attemptsExhausted = job.attemptsMade >= (job.opts.attempts ?? 1);

  console.error(
    `[worker] job ${job.id} failed (attempt ${job.attemptsMade}): ${err.message}`
  );

  if (attemptsExhausted) {
    console.error(`[worker] job ${job.id} exhausted retries — sending to DLQ`);

    // Move to dead letter queue with failure metadata
    await dlqQueue.add(
      'dead-letter',
      {
        originalQueue: 'email',
        originalJobId: job.id,
        originalData: job.data,
        error: err.message,
        stack: err.stack,
        failedAt: new Date().toISOString(),
        attemptsMade: job.attemptsMade,
      },
      {
        removeOnComplete: false,
        removeOnFail: false,
        attempts: 1, // DLQ jobs do not retry
      }
    );
  }
});

worker.on('error', (err) => {
  // Worker-level errors (Redis connection issues, etc.)
  console.error('[worker] worker error:', err);
});

worker.on('stalled', (jobId) => {
  console.warn(`[worker] job ${jobId} stalled`);
});

// --- Graceful shutdown ---
async function shutdown(): Promise<void> {
  console.log('[worker] shutting down...');
  // close(true) waits for active jobs to finish before closing
  await worker.close();
  await emailQueue.close();
  await dlqQueue.close();
  console.log('[worker] shutdown complete');
  process.exit(0);
}

process.on('SIGTERM', shutdown);
process.on('SIGINT', shutdown);

console.log('[worker] email worker started with concurrency 10');
```

### Entry Point

```typescript
// src/index.ts — example wiring everything together

import './queues/emailWorker';  // starts the worker
import { enqueueWelcomeEmail, enqueuePasswordReset, enqueueDelayedNotification } from './queues/emailProducer';

async function main() {
  // Enqueue some test jobs
  await enqueueWelcomeEmail('user-123', 'alice@example.com');
  await enqueuePasswordReset('user-456', 'bob@example.com', 'reset-token-xyz');
  await enqueueDelayedNotification(
    'charlie@example.com',
    'Your report is ready.',
    5 * 60 * 1000  // send in 5 minutes
  );
}

main().catch(console.error);
```

### Key Design Decisions in the Example

**Idempotency via `messageId`**: The processor checks `processedMessageIds` before sending. In production this would be a Redis `SETNX` or a database unique index on `message_id`. This handles the case where a job is picked up twice due to stalling.

**`UnrecoverableError`**: Imported from BullMQ. When thrown, BullMQ skips all remaining retries and moves the job directly to failed. Use for validation errors, malformed data, or any error that retrying will never fix.

**DLQ via `failed` event**: When attempts are exhausted, the job is written to `email-dlq` with full diagnostic context. This queue can be monitored separately and jobs can be reprocessed after a bug fix.

**Graceful shutdown**: `worker.close()` waits for in-flight jobs to complete. This prevents data corruption when deploying new versions. Always handle `SIGTERM` in production.

**`maxRetriesPerRequest: null`**: Required by BullMQ for the blocking Redis commands it uses internally. Without this, ioredis throws an error on long-running blocking reads.

**`removeOnComplete` / `removeOnFail` caps**: Without these, completed and failed jobs accumulate in Redis indefinitely. The caps keep Redis memory bounded while retaining enough history for debugging.
