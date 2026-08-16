# Docker

## Images vs Containers

An **image** is a read-only template (the blueprint). A **container** is a running instance of an image.

```
Dockerfile → docker build → Image → docker run → Container
```

---

## Production Dockerfile (Node.js TypeScript)

```dockerfile
# ── Stage 1: build ─────────────────────────────────────────────
FROM node:22-alpine AS builder

WORKDIR /app

# Copy package files first — leverages layer cache
# If package.json doesn't change, npm install is cached
COPY package*.json ./
RUN npm ci --only=production=false

COPY tsconfig.json ./
COPY src ./src
RUN npm run build

# ── Stage 2: production runtime ────────────────────────────────
FROM node:22-alpine AS runtime

# Security: run as non-root
RUN addgroup -S appgroup && adduser -S appuser -G appgroup

WORKDIR /app

# Copy only production dependencies from builder
COPY package*.json ./
RUN npm ci --only=production && npm cache clean --force

# Copy compiled output from builder stage
COPY --from=builder /app/dist ./dist

# Set ownership
RUN chown -R appuser:appgroup /app
USER appuser

ENV NODE_ENV=production
EXPOSE 3000

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
  CMD wget -qO- http://localhost:3000/health || exit 1

CMD ["node", "dist/index.js"]
```

Key practices:
- **Multi-stage build**: build stage has dev tools; runtime stage is lean (~150MB vs ~800MB)
- **Layer cache order**: `COPY package.json` before `COPY src` — source changes don't bust the install cache
- **Non-root user**: containers run as root by default — a security risk
- **HEALTHCHECK**: tells the orchestrator when the container is ready and when to restart it

---

## .dockerignore

```
node_modules
dist
.env
.env.*
.git
*.log
coverage
```

---

## Essential Commands

```bash
# Build
docker build -t myapp:1.0 .
docker build -t myapp:latest --target runtime .  # specific stage

# Run
docker run -p 3000:3000 --env-file .env myapp:latest
docker run -d --name api myapp:latest             # -d = detached (background)

# Inspect
docker ps                   # running containers
docker ps -a                # all containers including stopped
docker logs api -f          # follow logs
docker exec -it api sh      # shell into container

# Cleanup
docker rm api               # remove container
docker rmi myapp:latest     # remove image
docker system prune -af     # remove all unused (careful)
```

---

## Docker Compose

```yaml
# docker-compose.yml
services:
  api:
    build: .
    ports:
      - "3000:3000"
    environment:
      - NODE_ENV=production
      - DATABASE_URL=postgresql://postgres:secret@postgres:5432/myapp
      - REDIS_URL=redis://redis:6379
    depends_on:
      postgres:
        condition: service_healthy
      redis:
        condition: service_healthy
    restart: unless-stopped

  postgres:
    image: postgres:16-alpine
    environment:
      POSTGRES_DB: myapp
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: secret
    volumes:
      - postgres_data:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 10s
      timeout: 5s
      retries: 5

  redis:
    image: redis:7-alpine
    command: redis-server --appendonly yes
    volumes:
      - redis_data:/data
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 10s
      timeout: 5s
      retries: 5

volumes:
  postgres_data:
  redis_data:
```

```bash
docker compose up -d          # start all services in background
docker compose logs api -f    # follow api logs
docker compose down           # stop and remove containers
docker compose down -v        # also remove volumes (destroys data)
```

---

## Volumes

```bash
# Named volume (managed by Docker, persists across container restarts)
docker run -v postgres_data:/var/lib/postgresql/data postgres

# Bind mount (host path → container path, for development)
docker run -v $(pwd)/src:/app/src myapp:dev

# Read-only bind mount
docker run -v $(pwd)/config:/app/config:ro myapp
```

---

## Networks

Containers on the same Compose network can reach each other by service name.

```bash
# In docker-compose.yml, 'api' can reach 'postgres' at hostname 'postgres'
DATABASE_URL=postgresql://postgres:5432/myapp  # no localhost — use service name
```

---

## Interview Questions

**Q: What's the difference between CMD and ENTRYPOINT?**
`ENTRYPOINT` sets the executable that always runs. `CMD` provides default arguments that can be overridden at `docker run`. Pattern: `ENTRYPOINT ["node"]`, `CMD ["dist/index.js"]` — you can override the file but not node. If only CMD is set, the whole command is overridable.

**Q: Why does layer cache order matter?**
Docker rebuilds a layer and all subsequent layers when the layer's content changes. Put frequently changing layers (source code) after rarely changing layers (dependencies). If you `COPY . .` before `npm install`, every source change busts the install cache.

**Q: What is a multi-stage build and why use it?**
Build stages let you use a full build environment (with compilers, dev dependencies) in stage 1, then copy only the compiled output into a minimal runtime image in stage 2. The final image doesn't contain TypeScript, tsc, test frameworks, or build tooling — significantly smaller and smaller attack surface.
