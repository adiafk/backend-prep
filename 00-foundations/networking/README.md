# Networking Fundamentals

## OSI Model — What Matters

You rarely need all 7 layers. Focus on what breaks in practice:

| Layer | Name | What it does | Relevant tech |
|-------|------|-------------|---------------|
| 7 | Application | User-facing protocols | HTTP, HTTPS, WebSocket, DNS, SMTP |
| 4 | Transport | End-to-end delivery, ordering | TCP (reliable), UDP (fast) |
| 3 | Network | IP addressing, routing | IPv4, IPv6, ICMP |
| 2 | Data Link | MAC addressing, local delivery | Ethernet, Wi-Fi |

---

## TCP vs UDP

**TCP** (Transmission Control Protocol):
- Connection-oriented (3-way handshake: SYN → SYN-ACK → ACK)
- Guaranteed delivery, ordered packets, retransmission on loss
- Congestion and flow control
- Use for: HTTP, HTTPS, databases, email, file transfer

**UDP** (User Datagram Protocol):
- Connectionless, no handshake
- Fire-and-forget — no retransmission, no ordering
- Lower latency, higher throughput
- Use for: video/audio streaming, gaming, DNS, WebRTC

```
TCP Handshake:
  Client → SYN             (I want to connect)
  Server → SYN-ACK         (OK, I hear you)
  Client → ACK             (Great, let's go)

TCP Teardown:
  Client → FIN             (Done sending)
  Server → ACK + FIN       (Got it, also done)
  Client → ACK             (Acknowledged)
```

---

## DNS Resolution

```
Browser → OS cache → /etc/hosts → Recursive resolver (ISP)
         → Root nameserver (.) → TLD nameserver (.com)
         → Authoritative nameserver (example.com)
         → Returns IP address
```

- **A record**: domain → IPv4 address
- **AAAA record**: domain → IPv6 address
- **CNAME**: alias to another domain
- **MX**: mail server
- **TXT**: arbitrary text (SPF, DKIM, domain verification)

TTL (Time To Live): how long resolvers cache the record. Low TTL = faster propagation during changes, more DNS traffic.

---

## TLS / HTTPS

TLS provides: confidentiality (encryption), integrity (MACs), authenticity (certificates).

**TLS 1.3 Handshake** (simplified):
```
Client → ClientHello (supported ciphers, random)
Server → ServerHello (chosen cipher, certificate, random)
Client → Verifies certificate against CA
Both   → Derive session keys from shared secret (via ECDHE)
Client → Finished (encrypted)
Server → Finished (encrypted)
→ Application data flows
```

**Certificates**: signed by a Certificate Authority (CA). Chain: Root CA → Intermediate CA → Leaf cert. Browser has a list of trusted root CAs.

**Common issues**:
- `CERT_HAS_EXPIRED`: certificate past its validity date
- `ERR_CERT_AUTHORITY_INVALID`: self-signed or unknown CA
- `ERR_SSL_PROTOCOL_ERROR`: TLS version mismatch

---

## HTTP/1.1, HTTP/2, HTTP/3

| Feature | HTTP/1.1 | HTTP/2 | HTTP/3 |
|---------|---------|--------|--------|
| Transport | TCP | TCP | QUIC (UDP-based) |
| Multiplexing | No (one req per connection) | Yes (streams) | Yes |
| Head-of-line blocking | Yes (text protocol) | Yes (TCP level) | No |
| Header compression | No | HPACK | QPACK |
| Server push | No | Yes | Yes |

HTTP/2 eliminates multiple connections but still suffers TCP head-of-line blocking (one lost packet stalls all streams). HTTP/3 / QUIC solves this with per-stream reliability.

---

## Latency Numbers (Memorize These)

| Operation | Approx. latency |
|-----------|---------------|
| L1 cache reference | 1 ns |
| L2 cache reference | 4 ns |
| RAM access | 100 ns |
| SSD sequential read (1 KB) | 1 µs |
| Network round trip (same DC) | 0.5 ms |
| Network round trip (NYC → Europe) | 100 ms |
| HDD seek | 10 ms |
| Packet: CA → Netherlands → CA | 150 ms |

These numbers justify: cache everything hot, avoid cross-region synchronous calls, use async for anything crossing DC boundaries.

---

## Ports to Know

| Port | Protocol |
|------|---------|
| 22 | SSH |
| 25 | SMTP |
| 53 | DNS (UDP/TCP) |
| 80 | HTTP |
| 443 | HTTPS |
| 3306 | MySQL |
| 5432 | PostgreSQL |
| 6379 | Redis |
| 27017 | MongoDB |

---

## CORS (Cross-Origin Resource Sharing)

Browser security policy: a webpage at `app.com` cannot read responses from `api.different.com` unless the server explicitly allows it.

```
Origin: https://app.com
→ Browser sends preflight OPTIONS request
← Server responds: Access-Control-Allow-Origin: https://app.com
← Server responds: Access-Control-Allow-Methods: GET, POST
→ Browser proceeds with actual request
```

Common mistake: CORS is enforced by the **browser**, not the server. Curl and server-to-server calls are not affected.

```typescript
// Express CORS setup
import cors from "cors";

app.use(cors({
  origin: process.env.FRONTEND_URL,  // or "*" for public APIs
  methods: ["GET", "POST", "PUT", "DELETE"],
  allowedHeaders: ["Content-Type", "Authorization"],
  credentials: true,  // required for cookies/auth headers
}));
```

---

## Load Balancing Algorithms

| Algorithm | How it works | Best for |
|-----------|------------|---------|
| Round-robin | Rotate through servers in sequence | Uniform request sizes |
| Least connections | Route to server with fewest active connections | Variable request duration |
| IP hash | Hash client IP → always same server | Session affinity |
| Weighted round-robin | Servers with higher weight get more traffic | Heterogeneous hardware |
| Random | Pick random server | Simple, low overhead |

---

## TCP vs WebSocket vs SSE

| | HTTP Request | WebSockets | Server-Sent Events |
|--|------------|-----------|-----------------|
| Direction | Request/response | Bidirectional | Server → client only |
| Protocol | HTTP | WS (upgraded from HTTP) | HTTP |
| Overhead | Per-request headers | Low (framed) | HTTP chunked encoding |
| Use case | APIs | Chat, gaming, collab | Live feeds, notifications |
| Reconnect | N/A | Manual | Automatic (`EventSource`) |
