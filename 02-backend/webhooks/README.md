# Webhooks

## 1. Webhooks vs Polling vs WebSockets — When to Use Each

### Polling
The client repeatedly requests the server on a schedule to check for updates.

```
Client → GET /events?since=1234  →  Server
Client ← 200 { events: [] }      ←  Server   (empty most of the time)
```

**Use polling when:**
- You have no control over the data source (no webhook/push capability)
- Update frequency is low and latency requirements are loose
- Simplicity trumps efficiency

**Drawbacks:** Wasted requests when nothing changed; latency equals poll interval; scales poorly at high frequency.

### WebSockets
A persistent, bidirectional TCP connection. Both client and server can push frames at any time.

**Use WebSockets when:**
- You own both the client and server
- Low latency is required (chat, live collaboration, games)
- The communication is bidirectional

### Webhooks
HTTP callbacks. You register a URL with a third-party service. When an event occurs on their side, they POST a JSON payload to your URL. You are the HTTP server; they are the HTTP client.

```
Stripe  →  POST /webhooks/stripe  →  Your server
            { type: "payment_intent.succeeded", ... }
```

**Use webhooks when:**
- Integrating with external services (Stripe, GitHub, Twilio, Shopify)
- You cannot maintain a persistent connection to the third party
- Events are infrequent relative to polling frequency
- Push is better than pull (the third party knows when events happen)

### Decision matrix

| Scenario | Best fit |
|---|---|
| "Did my payment succeed?" — third party | Webhook |
| Live chat between two users | WebSocket |
| Refresh a dashboard every 60 s | Polling |
| Tail a build log in real time | SSE or WebSocket |
| GitHub notifies you of a push | Webhook |
| Trading app: live order book | WebSocket |

---

## 2. Webhook Security — HMAC Signature Verification

Without verification, anyone who discovers your endpoint URL can POST arbitrary payloads to it. HMAC signature verification proves the payload came from the expected sender and was not tampered with in transit.

### How Stripe and GitHub do it

**Stripe:**
1. Stripe computes `HMAC-SHA256(secret, "v1:" + timestamp + "." + rawBody)`
2. Stripe sends the signature in the `Stripe-Signature` header: `t=<timestamp>,v1=<hex_digest>`
3. Your server recomputes the HMAC and compares using a timing-safe comparison

**GitHub:**
1. GitHub computes `HMAC-SHA256(secret, rawBody)`
2. GitHub sends it in `X-Hub-Signature-256: sha256=<hex_digest>`
3. Your server recomputes and compares

### Why raw body matters

You must compare against the **raw bytes** of the request body, not a re-serialized version. JSON serialization is not deterministic across libraries (key ordering, spacing). Parse only after verification.

### Why timing-safe comparison matters

A naive string equality `a === b` short-circuits on the first differing character. An attacker can measure response timing to learn how many bytes of their forged signature match — eventually reconstructing a valid signature. `crypto.timingSafeEqual` always compares all bytes.

---

## 3. Delivery Guarantees — At-Least-Once and Idempotency

### At-least-once delivery

Webhook providers guarantee **at-least-once** delivery: if they do not receive a 2xx response within a timeout window, they retry. This means your handler may receive the same event multiple times, especially during:
- Network timeouts (your server received it but the ACK was lost)
- Your server returning a 5xx
- Retry storms after an outage

### Idempotency key pattern

An idempotency key is a stable, unique identifier for each event. You store it after processing; before processing you check if you have already handled this event.

Stripe uses `event.id` (e.g., `evt_1Nxyz123`). GitHub uses the `X-GitHub-Delivery` header (a UUID).

```
+------------------+
| incoming event   |
| id: evt_abc123   |
+------------------+
         |
         v
+------------------+       already exists?
| check processed  | -----> YES → return 200 (no-op)
| events table     |
+------------------+
         |
         NO
         v
+------------------+
| process event    |
| (charge, email,  |
|  update DB, ...) |
+------------------+
         |
         v
+------------------+
| mark event id    |
| as processed     |
+------------------+
         |
         v
       200 OK
```

The check and mark should ideally be in the same database transaction as the business logic, or use a distributed lock if spanning systems.

---

## 4. Retry Strategies — Exponential Backoff and Dead Letter

### Provider-side retry schedules

Most webhook providers use exponential backoff:

| Provider | Schedule |
|---|---|
| Stripe | Immediately, then 5 min, 30 min, 2 h, 5 h, 10 h, 24 h (7 attempts over 3 days) |
| GitHub | 3 attempts within minutes |
| Shopify | 19 attempts over 48 hours |
| Twilio | Up to 3 times within seconds |

Your server should return `2xx` quickly (see section 5). Returning `5xx` or timing out triggers retries.

### Dead letter queue

After exhausting retries, many systems move the event to a **dead letter queue (DLQ)** — a separate queue for events that could not be delivered. This prevents permanent data loss and allows manual inspection and replay.

```
Main queue → handler fails repeatedly → DLQ
                                          |
                                          v
                                   Alert + manual review
                                   Replay after fix
```

### Self-hosted retry logic (if you are the webhook sender)

If you build a webhook system yourself, implement retry on your side:

```typescript
interface WebhookJob {
  id: string;
  url: string;
  payload: unknown;
  attempt: number;
  nextAttemptAt: Date;
}

const MAX_ATTEMPTS = 10;
const BASE_DELAY_MS = 1_000;
const MAX_DELAY_MS  = 3_600_000; // 1 hour

function scheduleNextAttempt(job: WebhookJob): Date | null {
  if (job.attempt >= MAX_ATTEMPTS) return null; // move to DLQ

  const delay = Math.min(BASE_DELAY_MS * 2 ** job.attempt, MAX_DELAY_MS);
  const jitter = Math.random() * delay * 0.2;
  return new Date(Date.now() + delay + jitter);
}
```

---

## 5. Webhook Receiver Best Practices

### Respond fast — queue the work

Webhook providers have short timeout windows (Stripe: 30 s, GitHub: 10 s). If your processing takes longer (sending emails, charging cards, updating multiple services), you will time out, receive a 5xx, and get retried.

**Pattern: accept → enqueue → process asynchronously**

```
POST /webhooks/stripe
  1. Verify signature       (< 1 ms)
  2. Enqueue job            (< 5 ms, Redis/SQS/BullMQ)
  3. Return 200 OK          (total < 50 ms)

Worker process:
  4. Dequeue job
  5. Do actual work (charge, email, etc.)
  6. Mark event as processed
```

### Verify the signature before anything else

Never touch the database or external services before verification. An unverified webhook could be an attack.

### Use HTTPS

Plain HTTP exposes the payload and the signature secret in transit.

### Log raw payloads for debugging

Store the raw body and all headers before processing. Debugging a missed event is much easier with the original payload.

---

## Full Example — Stripe-Style HMAC-SHA256 Signature Verification

```typescript
import { createHmac, timingSafeEqual } from 'crypto';
import express, { Request, Response, NextFunction } from 'express';

// ─── Types ────────────────────────────────────────────────────────────────────

interface StripeSignatureHeader {
  timestamp: number;     // seconds since epoch
  signatures: string[];  // may include multiple (key rotation)
}

interface StripeEvent {
  id: string;
  type: string;
  data: { object: Record<string, unknown> };
  created: number;
}

// ─── Signature parsing ────────────────────────────────────────────────────────

function parseStripeSignature(header: string): StripeSignatureHeader {
  const parts = header.split(',');
  let timestamp = 0;
  const signatures: string[] = [];

  for (const part of parts) {
    const [key, value] = part.split('=');
    if (key === 't') timestamp = parseInt(value, 10);
    if (key === 'v1') signatures.push(value);
  }

  if (!timestamp || signatures.length === 0) {
    throw new Error('Malformed Stripe-Signature header');
  }

  return { timestamp, signatures };
}

// ─── Signature verification ───────────────────────────────────────────────────

const STRIPE_TOLERANCE_SECONDS = 300; // reject events older than 5 minutes

function verifyStripeSignature(
  rawBody: Buffer,
  signatureHeader: string,
  secret: string,
): void {
  const { timestamp, signatures } = parseStripeSignature(signatureHeader);

  // 1. Reject stale events — prevents replay attacks
  const now = Math.floor(Date.now() / 1000);
  if (Math.abs(now - timestamp) > STRIPE_TOLERANCE_SECONDS) {
    throw new Error(`Webhook timestamp too old: ${now - timestamp}s drift`);
  }

  // 2. Compute expected signature
  // Stripe signs: "<timestamp>.<rawBody>"
  const signedPayload = `${timestamp}.${rawBody.toString('utf8')}`;
  const expectedHmac = createHmac('sha256', secret)
    .update(signedPayload)
    .digest('hex');

  const expectedBuffer = Buffer.from(expectedHmac, 'hex');

  // 3. Compare using timing-safe equality
  // Check all provided signatures (Stripe may send multiple during key rotation)
  const isValid = signatures.some((sig) => {
    const sigBuffer = Buffer.from(sig, 'hex');
    // timingSafeEqual requires same-length buffers
    if (sigBuffer.length !== expectedBuffer.length) return false;
    return timingSafeEqual(sigBuffer, expectedBuffer);
  });

  if (!isValid) {
    throw new Error('Webhook signature verification failed');
  }
}

// ─── GitHub-style verification (simpler — no timestamp) ───────────────────────

function verifyGitHubSignature(
  rawBody: Buffer,
  signatureHeader: string,
  secret: string,
): void {
  // Header format: "sha256=<hex_digest>"
  if (!signatureHeader.startsWith('sha256=')) {
    throw new Error('Missing sha256 prefix in X-Hub-Signature-256');
  }

  const receivedSig = signatureHeader.slice('sha256='.length);
  const expectedSig = createHmac('sha256', secret)
    .update(rawBody)
    .digest('hex');

  const received = Buffer.from(receivedSig, 'hex');
  const expected = Buffer.from(expectedSig, 'hex');

  if (received.length !== expected.length || !timingSafeEqual(received, expected)) {
    throw new Error('GitHub signature verification failed');
  }
}

// ─── Idempotency store (in-memory for illustration; use Redis/DB in production) ──

const processedEvents = new Set<string>();

async function isAlreadyProcessed(eventId: string): Promise<boolean> {
  return processedEvents.has(eventId);
}

async function markAsProcessed(eventId: string): Promise<void> {
  processedEvents.add(eventId);
}

// ─── Express webhook handler ──────────────────────────────────────────────────

const app = express();

// IMPORTANT: use express.raw() — NOT express.json() — before this route.
// We need the raw body bytes for HMAC computation.
app.post(
  '/webhooks/stripe',
  express.raw({ type: 'application/json' }),
  async (req: Request, res: Response, next: NextFunction) => {
    const signatureHeader = req.headers['stripe-signature'] as string | undefined;

    if (!signatureHeader) {
      res.status(400).json({ error: 'Missing Stripe-Signature header' });
      return;
    }

    // Step 1: Verify signature against raw bytes
    try {
      verifyStripeSignature(
        req.body as Buffer,
        signatureHeader,
        process.env.STRIPE_WEBHOOK_SECRET!,
      );
    } catch (err) {
      console.warn('Webhook signature verification failed:', (err as Error).message);
      res.status(400).json({ error: 'Invalid signature' });
      return;
    }

    // Step 2: Parse event now that it is verified
    let event: StripeEvent;
    try {
      event = JSON.parse((req.body as Buffer).toString('utf8'));
    } catch {
      res.status(400).json({ error: 'Invalid JSON body' });
      return;
    }

    // Step 3: Idempotency check
    if (await isAlreadyProcessed(event.id)) {
      console.log(`Duplicate event ignored: ${event.id}`);
      res.status(200).json({ received: true }); // return 200 so Stripe stops retrying
      return;
    }

    // Step 4: Respond immediately — queue the actual work
    res.status(200).json({ received: true });

    // Step 5: Process asynchronously (do not await before responding)
    setImmediate(async () => {
      try {
        await processStripeEvent(event);
        await markAsProcessed(event.id);
      } catch (err) {
        console.error(`Failed to process event ${event.id}:`, err);
        // In production: push to DLQ for manual review / retry
      }
    });
  },
);

// ─── GitHub webhook handler ───────────────────────────────────────────────────

app.post(
  '/webhooks/github',
  express.raw({ type: 'application/json' }),
  async (req: Request, res: Response) => {
    const signatureHeader = req.headers['x-hub-signature-256'] as string | undefined;
    const deliveryId      = req.headers['x-github-delivery']  as string | undefined;
    const eventType       = req.headers['x-github-event']     as string | undefined;

    if (!signatureHeader) {
      res.status(400).json({ error: 'Missing X-Hub-Signature-256 header' });
      return;
    }

    try {
      verifyGitHubSignature(
        req.body as Buffer,
        signatureHeader,
        process.env.GITHUB_WEBHOOK_SECRET!,
      );
    } catch (err) {
      console.warn('GitHub signature verification failed:', (err as Error).message);
      res.status(400).json({ error: 'Invalid signature' });
      return;
    }

    if (deliveryId && await isAlreadyProcessed(deliveryId)) {
      res.status(200).json({ received: true });
      return;
    }

    res.status(200).json({ received: true });

    setImmediate(async () => {
      const payload = JSON.parse((req.body as Buffer).toString('utf8'));
      console.log(`GitHub event: ${eventType}, delivery: ${deliveryId}`);
      // handle push, pull_request, etc.
      if (deliveryId) await markAsProcessed(deliveryId);
    });
  },
);

// ─── Event processing ─────────────────────────────────────────────────────────

async function processStripeEvent(event: StripeEvent): Promise<void> {
  switch (event.type) {
    case 'payment_intent.succeeded':
      console.log('Payment succeeded:', event.data.object['id']);
      // fulfillOrder(event.data.object);
      break;

    case 'customer.subscription.deleted':
      console.log('Subscription cancelled:', event.data.object['id']);
      // downgradeAccount(event.data.object);
      break;

    default:
      console.log(`Unhandled event type: ${event.type}`);
  }
}

// ─── Start ────────────────────────────────────────────────────────────────────

app.listen(3000, () => console.log('Webhook server listening on :3000'));
```

---

## Quick Reference

| Concept | Key point |
|---|---|
| Webhooks vs polling | Webhooks push on event; polling pulls on schedule — webhooks win for low-latency, event-driven integrations |
| HMAC verification | `HMAC-SHA256(secret, signedPayload)`; use `timingSafeEqual`; verify before parsing |
| Raw body | Buffer must be untouched before HMAC; use `express.raw()` not `express.json()` |
| Timestamp tolerance | Reject events older than 5 minutes to block replay attacks |
| At-least-once | Deduplicate using `event.id` or `X-GitHub-Delivery` stored in DB/Redis |
| Respond fast | Return 200 within seconds; do real work in a background queue |
| Dead letter queue | After N failed retries, move to DLQ for alerting and manual replay |
