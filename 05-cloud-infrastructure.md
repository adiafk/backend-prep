# 05 — Cloud, Linux & Infrastructure

## Linux

Know processes, signals, file descriptors, permissions, users/groups, environment variables, networking, disk/memory/CPU inspection, SSH, systemd, cron, and logs.

Useful commands:

```bash
ps
htop
free -h
df -h
du -sh *
lsof
ss -lntp
curl
grep
tail -f
journalctl
systemctl
chmod
chown
```

### Interview question

**How would you debug a server whose API suddenly becomes slow?**

Check CPU, memory, disk, network, process health, logs, recent deployments, dependency latency, database performance, connection pools, and traffic patterns. Use metrics first to narrow the problem before changing code.

---

## Docker

Know:

- image
- container
- Dockerfile
- layers
- registry
- volumes
- networks
- port mapping
- environment variables
- multi-stage builds
- Compose

A container packages an application and its userspace dependencies while sharing the host kernel; a VM virtualizes a full guest operating system.

---

## Nginx / Gateway

A reverse proxy sits between clients and backend services.

```text
Internet -> Nginx/Gateway -> Application
```

Common responsibilities:

- TLS termination
- routing
- load balancing
- rate limiting
- compression
- static files
- security headers

---

## AWS EC2 / S3

### EC2

Virtual compute instances. Know CPU, memory, disk, networking, security groups, SSH, autoscaling, and deployment strategies.

### S3

Object storage. Know buckets, objects, permissions, lifecycle policies, and presigned URLs at a high level.

---

## Regions and Availability Zones

A region is a geographic cloud location; availability zones are isolated infrastructure locations within a region.

High availability commonly means spreading application capacity across multiple AZs behind a load balancer.

---

## Load Balancing

Know:

- Layer 4 vs Layer 7
- round robin
- health checks
- weighted routing
- least connections
- sticky sessions
- failover

AWS concepts to know:

- ELB as the family of load-balancing services
- ALB for HTTP/HTTPS Layer-7 routing
- NLB for high-performance Layer-4 networking

---

## Autoscaling

Horizontal autoscaling adds/removes instances based on demand or metrics.

Think about:

- CPU
- memory
- request count
- latency
- queue depth
- cooldown/stabilization
- minimum/maximum capacity

Queue depth can be a better scaling signal than CPU for worker systems.

---

## Kubernetes

Know:

- cluster
- node
- pod
- deployment
- ReplicaSet
- service
- ingress
- ConfigMap
- Secret
- namespace
- HPA
- readiness/liveness probes
- resource requests/limits

Typical path:

```text
Client -> Ingress/Gateway -> Service -> Pods -> Containers
```

---

## EKS

Amazon EKS is managed Kubernetes control-plane infrastructure. You still need to understand worker capacity, networking, IAM, deployments, services, ingress/load balancing, secrets, observability, and scaling.

---

## PM2

PM2 is a Node.js process manager.

Know:

- process supervision
- restart on crash
- logs
- environment configuration
- cluster mode
- graceful shutdown

Cluster mode does not automatically solve distributed state. Sessions, caches, queues, and other shared state still need an appropriate shared system.

---

## GitHub Actions / CI/CD

Typical pipeline:

```text
Push -> Lint -> Test -> Build -> Package -> Deploy -> Verify
```

Know:

- workflows
- jobs
- steps
- runners
- secrets
- artifacts
- caching
- matrix builds
- deployment environments
- rollback

### Resume connection

You ran deployments and infrastructure automation through GitHub Actions and PM2 on AWS EC2/S3 and Linode, supporting rolling releases across environments.

Be ready to explain:

- how a deployment starts
- how secrets are supplied
- how failures are detected
- how a rolling release avoids taking the whole service down
- how you would rollback

---

## Secrets and configuration

Never commit secrets to source control.

Separate:

- application configuration
- credentials
- signing keys
- provider API keys

Use environment-specific secret stores and rotate credentials when necessary.
