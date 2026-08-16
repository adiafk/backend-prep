# System Design: Chat Application

## Requirements

**Functional**:
- 1:1 and group messaging (up to 500 members)
- Message delivery receipts (sent / delivered / read)
- Online presence
- Message history
- File/image sharing

**Non-functional**:
- 50 million DAU, each sends 40 messages/day
- Messages delivered in < 500ms
- 5 years of message history retained
- 99.99% availability

---

## Estimation

```
Messages/day = 50M users × 40 messages = 2 billion messages/day
Messages/sec = 2B / 86400 ≈ 23,000 msg/s (average)
Peak          = ~3-5× average ≈ 100,000 msg/s

Message size  = ~100 bytes average
Storage/day   = 2B × 100 bytes = 200 GB/day
Storage/5y    = 200 GB × 365 × 5 ≈ 365 TB
```

---

## High-Level Architecture

```
                        ┌─────────────┐
Clients (WS)  ──────►  │  WebSocket  │
                        │   Gateway   │
                        └──────┬──────┘
                               │
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
        ┌──────────┐   ┌──────────────┐  ┌──────────┐
        │  Presence│   │   Message    │  │  Group   │
        │  Service │   │   Service    │  │  Service │
        └────┬─────┘   └──────┬───────┘  └────┬─────┘
             │                │               │
        Redis Pub/Sub   ┌─────┴──────┐   Cassandra
        (online users)  │  Message   │   (group members)
                        │    DB      │
                        └────────────┘
                       Cassandra (chats)
```

---

## WebSocket Gateway

Clients maintain a persistent WebSocket connection to a gateway server. Each connection is registered with Redis so any gateway can route messages to the correct one.

```
Client A → Gateway-1
Client B → Gateway-2

Redis: { "user_A": "gateway-1", "user_B": "gateway-2" }

Message from A to B:
  Client A → Gateway-1
  Gateway-1 → Redis Pub/Sub channel "user_B"
  Gateway-2 (subscribed to "user_B") → Client B
```

```typescript
import { Server } from "socket.io";
import { createAdapter } from "@socket.io/redis-adapter";
import { createClient } from "redis";

const pubClient = createClient({ url: process.env.REDIS_URL });
const subClient = pubClient.duplicate();

io.adapter(createAdapter(pubClient, subClient));

io.on("connection", (socket) => {
  const userId = socket.data.userId;

  // Register user presence
  await redis.set(`presence:${userId}`, socket.id, { EX: 30 });

  socket.on("send_message", async ({ toUserId, content }) => {
    const msg = { from: userId, to: toUserId, content, ts: Date.now() };
    
    // Persist to Cassandra
    await db.insertMessage(msg);

    // Deliver via Redis pub/sub
    await pubClient.publish(`user:${toUserId}`, JSON.stringify(msg));
  });
});
```

---

## Message Storage: Cassandra

Cassandra is the industry standard for chat message storage (WhatsApp, Discord, Facebook Messenger all use it). Reasons:
- Write-optimized (append-only log)
- Linear horizontal scaling
- Designed for time-series data with TTL

```sql
CREATE TABLE messages (
  conversation_id  UUID,
  created_at       TIMESTAMP,
  message_id       UUID,
  sender_id        UUID,
  content          TEXT,
  media_url        TEXT,
  PRIMARY KEY (conversation_id, created_at, message_id)
) WITH CLUSTERING ORDER BY (created_at DESC)
  AND default_time_to_live = 157680000;  -- 5 years in seconds
```

Reads: `SELECT * FROM messages WHERE conversation_id = ? LIMIT 50` — fast, no JOINs.

---

## Presence Service

Show who's online. Redis sorted set with timestamp as score:

```typescript
// Heartbeat every 10s from client
async function updatePresence(userId: string) {
  await redis.zadd("online_users", Date.now(), userId);
}

// Check presence
async function getPresence(userIds: string[]): Promise<Record<string, boolean>> {
  const staleThreshold = Date.now() - 30_000;
  const result: Record<string, boolean> = {};
  
  for (const id of userIds) {
    const score = await redis.zscore("online_users", id);
    result[id] = score !== null && Number(score) > staleThreshold;
  }
  return result;
}

// Cleanup stale entries periodically
await redis.zremrangebyscore("online_users", 0, Date.now() - 30_000);
```

---

## Message Delivery Receipts

Three states: sent (stored in DB), delivered (client received it), read (client opened chat).

```typescript
// Client ACKs delivery
socket.on("message_delivered", async ({ messageId }) => {
  await db.updateMessageStatus(messageId, "delivered");
  
  // Notify sender
  const msg = await db.getMessage(messageId);
  io.to(msg.senderId).emit("delivery_receipt", { messageId, status: "delivered" });
});
```

---

## Group Messaging

Group messages are fan-out: one message → N deliveries. For large groups, do fan-out at read time (pull model) instead of write time (push model).

**Push model** (< 100 members): write one record per member to their inbox. Fast reads, slow writes.

**Pull model** (> 100 members): write one record to the group timeline. All members read from the group. Fast writes, slightly slower reads.

Discord uses the pull model with offset-based pagination.

---

## File Sharing

Upload files to S3 directly from client (presigned URL), store only the S3 URL in the message DB.

```
Client → POST /api/upload-url → S3 presigned URL
Client → PUT <presigned-url> (directly to S3)
Client → send_message { type: "image", media_url: "s3://..." }
```

---

## Trade-offs and Scale Points

| Component | Scale strategy |
|-----------|--------------|
| WebSocket gateways | Add more; stateless with Redis adapter |
| Message DB | Add Cassandra nodes (no downtime) |
| Presence | Redis cluster; pre-aggregate for large groups |
| File storage | S3 + CloudFront CDN |
| API servers | Horizontal; stateless |
