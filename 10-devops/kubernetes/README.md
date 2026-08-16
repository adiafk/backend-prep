# Kubernetes

## Core Concepts

Kubernetes (K8s) orchestrates containers across a cluster of machines.

| Object | What It Is |
|--------|-----------|
| **Pod** | Smallest deployable unit — one or more containers sharing network/storage |
| **Deployment** | Manages a set of identical pods, handles rolling updates and rollback |
| **Service** | Stable network endpoint for a set of pods (pods are ephemeral; Services are not) |
| **Ingress** | HTTP/HTTPS routing rules — maps URLs to Services |
| **ConfigMap** | Non-secret configuration data (env vars, config files) |
| **Secret** | Sensitive data (passwords, tokens) — base64 encoded, not encrypted by default |
| **Namespace** | Virtual cluster within a cluster — used for isolation (dev/staging/prod) |

---

## Deployment

```yaml
# deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: api
  namespace: production
spec:
  replicas: 3
  selector:
    matchLabels:
      app: api
  template:
    metadata:
      labels:
        app: api
    spec:
      containers:
        - name: api
          image: myrepo/api:1.0.0  # always pin a tag, never use :latest in prod
          ports:
            - containerPort: 3000
          env:
            - name: NODE_ENV
              value: production
            - name: DATABASE_URL
              valueFrom:
                secretKeyRef:
                  name: api-secrets
                  key: database-url
          resources:
            requests:
              cpu: "100m"
              memory: "128Mi"
            limits:
              cpu: "500m"
              memory: "512Mi"
          readinessProbe:
            httpGet:
              path: /health
              port: 3000
            initialDelaySeconds: 5
            periodSeconds: 10
          livenessProbe:
            httpGet:
              path: /health
              port: 3000
            initialDelaySeconds: 15
            periodSeconds: 20
      terminationGracePeriodSeconds: 30
```

---

## Service

```yaml
# service.yaml
apiVersion: v1
kind: Service
metadata:
  name: api-service
spec:
  selector:
    app: api       # routes to pods with this label
  ports:
    - port: 80
      targetPort: 3000
  type: ClusterIP  # internal only
```

Service types:
- **ClusterIP**: accessible only within the cluster (default)
- **NodePort**: accessible on a port on every node (dev/testing)
- **LoadBalancer**: provisions a cloud load balancer (production)

---

## Ingress

```yaml
# ingress.yaml
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: api-ingress
  annotations:
    nginx.ingress.kubernetes.io/rewrite-target: /
spec:
  rules:
    - host: api.example.com
      http:
        paths:
          - path: /
            pathType: Prefix
            backend:
              service:
                name: api-service
                port:
                  number: 80
  tls:
    - hosts:
        - api.example.com
      secretName: api-tls-cert
```

---

## Health Probes

| Probe | Purpose | Failure Action |
|-------|---------|---------------|
| `readinessProbe` | Is the container ready to receive traffic? | Remove from Service endpoints (no traffic sent) |
| `livenessProbe` | Is the container still running correctly? | Restart the container |
| `startupProbe` | Has the container finished starting? | Gates readiness/liveness probes until passing |

Always define readiness probes. Liveness probes prevent zombie containers. Startup probes prevent killing slow-starting containers.

---

## Rolling Updates and Rollback

```bash
# Update image — triggers rolling deployment
kubectl set image deployment/api api=myrepo/api:1.1.0

# Watch rollout progress
kubectl rollout status deployment/api

# Roll back to previous version
kubectl rollout undo deployment/api

# Roll back to specific revision
kubectl rollout history deployment/api
kubectl rollout undo deployment/api --to-revision=2
```

---

## Horizontal Pod Autoscaler

```yaml
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: api-hpa
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: api
  minReplicas: 2
  maxReplicas: 10
  metrics:
    - type: Resource
      resource:
        name: cpu
        target:
          type: Utilization
          averageUtilization: 70
```

---

## Essential kubectl Commands

```bash
# Context
kubectl config get-contexts
kubectl config use-context production

# Overview
kubectl get pods -n production
kubectl get deployments
kubectl get services
kubectl get ingress

# Inspect
kubectl describe pod api-5d9f6b-xyz
kubectl logs api-5d9f6b-xyz -f
kubectl logs api-5d9f6b-xyz --previous  # logs from crashed container

# Shell into a pod
kubectl exec -it api-5d9f6b-xyz -- sh

# Apply configuration
kubectl apply -f deployment.yaml
kubectl delete -f deployment.yaml

# Scale manually
kubectl scale deployment api --replicas=5

# Port-forward for local debugging
kubectl port-forward service/api-service 3000:80
```

---

## Secrets and ConfigMaps

```yaml
# secret.yaml — values must be base64 encoded
apiVersion: v1
kind: Secret
metadata:
  name: api-secrets
type: Opaque
data:
  database-url: cG9zdGdyZXNxbDovLy4uLg==  # echo -n "postgresql://..." | base64
```

In production, use a secrets manager (AWS Secrets Manager, HashiCorp Vault, Sealed Secrets) — plain K8s Secrets are only base64, not encrypted at rest by default.

---

## Interview Questions

**Q: What is the difference between liveness and readiness probes?**
Readiness: is this pod ready to serve traffic? Failing readiness removes the pod from the Service's endpoint list — no traffic is sent to it, but it's not restarted. Liveness: is the process healthy? Failing liveness restarts the container. Use readiness for startup and temporary unavailability (e.g. during a DB migration). Use liveness for detecting hung/deadlocked processes.

**Q: Why should you never use the `:latest` tag in production?**
`:latest` doesn't guarantee idempotency — the same tag can point to different images over time. If a new version is pushed with bugs, any pod restart (crash, scale-out) will pull the broken image. Always pin to a specific tag or SHA digest.

**Q: What happens during a rolling deployment?**
K8s incrementally replaces old pods with new ones. By default, it brings up new pods before removing old ones (maxSurge=1, maxUnavailable=0). The new pods must pass readiness probes before the old pods are terminated. If the new version fails readiness checks, the rollout stalls and old pods continue serving traffic — the cluster stays healthy.
