# pgvector

pgvector is a PostgreSQL extension that adds a `vector` data type and nearest-neighbor search. It lets you store embeddings alongside your relational data and query both in a single SQL statement.

---

## Setup

```sql
CREATE EXTENSION IF NOT EXISTS vector;

-- Add an embedding column to an existing table
ALTER TABLE documents ADD COLUMN embedding vector(1536);

-- Or create a table with embeddings from the start
CREATE TABLE documents (
  id         BIGSERIAL PRIMARY KEY,
  content    TEXT NOT NULL,
  metadata   JSONB,
  embedding  vector(1536),           -- OpenAI text-embedding-3-small dimension
  model_version TEXT NOT NULL DEFAULT 'text-embedding-3-small',
  created_at TIMESTAMPTZ DEFAULT NOW()
);
```

---

## Distance Operators

pgvector provides three distance operators:

| Operator | Distance Metric | Use When |
|---|---|---|
| `<->` | L2 / Euclidean | Embeddings NOT normalized; absolute magnitude matters |
| `<=>` | Cosine distance | Embeddings of different lengths; semantic similarity |
| `<#>` | Negative inner product | Embeddings ARE normalized (unit vectors); fastest computation |

```sql
-- L2 distance: find 5 nearest by Euclidean distance
SELECT id, content, embedding <-> $1 AS distance
FROM documents
ORDER BY embedding <-> $1
LIMIT 5;

-- Cosine distance: most common for text embeddings
SELECT id, content, 1 - (embedding <=> $1) AS similarity
FROM documents
ORDER BY embedding <=> $1
LIMIT 5;

-- Negative inner product (use when vectors are L2-normalized)
SELECT id, content, (embedding <#> $1) * -1 AS score
FROM documents
ORDER BY embedding <#> $1
LIMIT 5;
```

**Rule of thumb**: OpenAI and most embedding APIs return L2-normalized vectors — use `<=>` (cosine) or `<#>` (inner product). If you're not sure, cosine is the safe default for semantic search.

---

## Exact Nearest Neighbor (No Index)

Without an index, pgvector does a **sequential scan** — computes distance to every row. Correct but O(n).

```sql
-- Exact result: guaranteed to find the true nearest neighbors
-- O(n) — acceptable for < 100k vectors, slow above that
SELECT id, content, embedding <=> $1 AS distance
FROM documents
ORDER BY embedding <=> $1
LIMIT 10;
```

Use exact search for: small datasets (< 100k), correctness-critical pipelines (evaluation, offline jobs), or as a baseline to measure recall of approximate indexes.

---

## IVFFlat Index (Approximate)

**Inverted File Flat** divides the vector space into `lists` (Voronoi cells) during index build. At query time, it searches only the `probes` nearest cells.

```sql
-- Build IVFFlat index
-- Rule of thumb: lists = rows / 1000 (for up to 1M rows)
CREATE INDEX ON documents USING ivfflat (embedding vector_cosine_ops)
WITH (lists = 100);

-- For L2 distance:
CREATE INDEX ON documents USING ivfflat (embedding vector_l2_ops)
WITH (lists = 100);

-- Query-time probe count (higher = more recall, slower)
SET ivfflat.probes = 10;  -- default 1; set 10–50% of lists for high recall

SELECT id, content, embedding <=> $1 AS distance
FROM documents
ORDER BY embedding <=> $1
LIMIT 10;
```

### IVFFlat Parameters

| Parameter | Effect |
|---|---|
| `lists` | Number of partitions. More lists = faster per-query scan (fewer vectors per list) but requires more probes for same recall. |
| `ivfflat.probes` | Lists searched at query time. `probes = lists` = exact search. `probes = 1` = fastest, lowest recall. |

**Trade-off**: with `lists = 100` and `probes = 10`, you search 10% of the vector space. Recall is typically 95–99% — meaning you might miss 1–5% of true nearest neighbors.

**Critical**: IVFFlat requires data to exist at index build time. Building on an empty table produces a useless index. Insert data first, then build.

---

## HNSW Index (Approximate, Preferred)

**Hierarchical Navigable Small World** builds a proximity graph: vectors are nodes, edges connect nearby vectors. Multiple layers form a hierarchy — upper layers have long-range connections for fast navigation, lower layers have short-range connections for precision.

```sql
-- HNSW index (recommended for most production use)
CREATE INDEX ON documents USING hnsw (embedding vector_cosine_ops)
WITH (m = 16, ef_construction = 64);
```

### HNSW Parameters

| Parameter | Default | Effect |
|---|---|---|
| `m` | 16 | Max connections per node per layer. Higher = better recall, more memory/build time. Range: 4–64. |
| `ef_construction` | 64 | Candidate list size during index build. Higher = better quality index, slower build. Range: 4–400. |
| `hnsw.ef_search` | 40 | Candidate list size at query time. Higher = better recall, slower query. Set per session. |

```sql
-- Increase ef_search for better recall at query time
SET hnsw.ef_search = 100;

SELECT id, content, embedding <=> $1 AS distance
FROM documents
ORDER BY embedding <=> $1
LIMIT 10;
```

### IVFFlat vs HNSW

| | IVFFlat | HNSW |
|---|---|---|
| Query speed | Moderate | Faster |
| Recall at same speed | Lower | Higher |
| Build time | Fast | Slower |
| Memory usage | Lower | Higher (stores graph) |
| Needs pre-existing data | Yes | No |
| Best for | Cost-constrained, batch rebuild OK | Production, best recall/speed |

For new projects, default to HNSW. IVFFlat is worth considering only when memory is tight or you rebuild the index frequently on bulk loads.

---

## TypeScript Integration

### With node-postgres (pg)

```typescript
import { Pool } from 'pg'

const pool = new Pool({ connectionString: process.env.DATABASE_URL })

interface SearchResult {
  id: number
  content: string
  similarity: number
  metadata: Record<string, unknown>
}

async function semanticSearch(
  embedding: number[],
  topK = 10,
  minSimilarity = 0.7
): Promise<SearchResult[]> {
  // Format as PostgreSQL vector literal
  const vectorLiteral = `[${embedding.join(',')}]`

  const result = await pool.query<SearchResult>(
    `SELECT
       id,
       content,
       metadata,
       1 - (embedding <=> $1::vector) AS similarity
     FROM documents
     WHERE 1 - (embedding <=> $1::vector) >= $2
     ORDER BY embedding <=> $1::vector
     LIMIT $3`,
    [vectorLiteral, minSimilarity, topK]
  )

  return result.rows
}

async function upsertDocument(
  id: number,
  content: string,
  embedding: number[],
  metadata: Record<string, unknown>
): Promise<void> {
  const vectorLiteral = `[${embedding.join(',')}]`

  await pool.query(
    `INSERT INTO documents (id, content, embedding, metadata)
     VALUES ($1, $2, $3::vector, $4)
     ON CONFLICT (id)
     DO UPDATE SET
       content = EXCLUDED.content,
       embedding = EXCLUDED.embedding,
       metadata = EXCLUDED.metadata`,
    [id, content, vectorLiteral, JSON.stringify(metadata)]
  )
}
```

### With Drizzle ORM

```typescript
import { pgTable, serial, text, jsonb, customType } from 'drizzle-orm/pg-core'
import { sql } from 'drizzle-orm'
import { db } from './db'

// Custom vector type for Drizzle
const vector = (dimensions: number) =>
  customType<{ data: number[]; driverData: string }>({
    dataType() {
      return `vector(${dimensions})`
    },
    toDriver(value: number[]): string {
      return `[${value.join(',')}]`
    },
    fromDriver(value: string): number[] {
      return value.slice(1, -1).split(',').map(Number)
    },
  })

const documents = pgTable('documents', {
  id: serial('id').primaryKey(),
  content: text('content').notNull(),
  metadata: jsonb('metadata'),
  embedding: vector(1536)('embedding'),
})

// Query using raw SQL for the distance operator
async function searchDocuments(queryEmbedding: number[], topK = 10) {
  const vectorLiteral = `[${queryEmbedding.join(',')}]`

  return db.execute(sql`
    SELECT
      id,
      content,
      metadata,
      1 - (embedding <=> ${sql.raw(`'${vectorLiteral}'::vector`)}) AS similarity
    FROM documents
    ORDER BY embedding <=> ${sql.raw(`'${vectorLiteral}'::vector`)}
    LIMIT ${topK}
  `)
}
```

---

## Metadata Filtering

pgvector does **post-filtering** by default: it finds the top-K nearest vectors from the index, then applies WHERE conditions. If most results are filtered out, you may get fewer than K results.

```sql
-- Post-filter: finds nearest 10 embeddings, then checks user_id
-- If user has few documents, result count < 10
SELECT id, content, embedding <=> $1 AS distance
FROM documents
WHERE user_id = $2          -- applied AFTER vector search
ORDER BY embedding <=> $1
LIMIT 10;
```

### Strategies for Filtered Search

**Option 1: Increase LIMIT** — fetch more candidates to compensate for post-filter attrition:
```sql
-- Fetch 100 nearest, filter, return top 10
SELECT id, content, embedding <=> $1 AS distance
FROM (
  SELECT id, content, user_id, embedding
  FROM documents
  ORDER BY embedding <=> $1
  LIMIT 100
) candidates
WHERE user_id = $2
LIMIT 10;
```

**Option 2: Partial index per tenant** — for high-cardinality filters with stable values:
```sql
-- Each user gets their own index partition
CREATE INDEX ON documents USING hnsw (embedding vector_cosine_ops)
WHERE user_id = 1;
-- Not practical for many users; better for category-level partitioning
```

**Option 3: Partition the table** — range partition on user_id or org_id:
```sql
CREATE TABLE documents_p1 PARTITION OF documents
FOR VALUES FROM (1) TO (1000);
-- Queries with user_id in range only scan relevant partition + its index
```

**Option 4: Use a dedicated vector DB** — Pinecone, Weaviate, and Qdrant support pre-filter (filter first, search within filtered set). pgvector's post-filter is a fundamental limitation for high-cardinality metadata filtering.

---

## Hybrid Search: Dense + Keyword

Combine pgvector similarity with PostgreSQL full-text search to get the best of both worlds — semantic understanding from embeddings, keyword precision from FTS.

```sql
-- Add a tsvector column for full-text search
ALTER TABLE documents ADD COLUMN content_tsv tsvector
  GENERATED ALWAYS AS (to_tsvector('english', content)) STORED;

CREATE INDEX ON documents USING GIN (content_tsv);

-- Hybrid search: reciprocal rank fusion
WITH semantic AS (
  SELECT id, ROW_NUMBER() OVER (ORDER BY embedding <=> $1) AS sem_rank
  FROM documents
  ORDER BY embedding <=> $1
  LIMIT 60
),
keyword AS (
  SELECT id, ROW_NUMBER() OVER (ORDER BY ts_rank(content_tsv, query) DESC) AS kw_rank
  FROM documents,
       to_tsquery('english', $2) query
  WHERE content_tsv @@ query
  LIMIT 60
)
SELECT
  COALESCE(s.id, k.id) AS id,
  -- RRF score: 1/(k + rank) summed across retrievers
  COALESCE(1.0 / (60 + s.sem_rank), 0) + COALESCE(1.0 / (60 + k.kw_rank), 0) AS rrf_score
FROM semantic s
FULL OUTER JOIN keyword k ON k.id = s.id
ORDER BY rrf_score DESC
LIMIT 10;
```

---

## Embedding Model Versioning

When you change embedding models (e.g., from `text-embedding-3-small` to `text-embedding-3-large`, or from 1536 to 3072 dimensions), old and new embeddings are incompatible — comparing them produces meaningless distances.

```sql
-- Track which model generated each embedding
ALTER TABLE documents ADD COLUMN model_version TEXT NOT NULL DEFAULT 'text-embedding-3-small';

-- When migrating to a new model, add a new column
ALTER TABLE documents ADD COLUMN embedding_v2 vector(3072);

-- Backfill in batches (never one giant transaction)
UPDATE documents
SET embedding_v2 = $1, model_version = 'text-embedding-3-large'
WHERE id IN (
  SELECT id FROM documents WHERE embedding_v2 IS NULL LIMIT 1000
);

-- Build index on new column before switching
CREATE INDEX CONCURRENTLY ON documents USING hnsw (embedding_v2 vector_cosine_ops);

-- Once fully backfilled: drop old column
ALTER TABLE documents DROP COLUMN embedding;
ALTER TABLE documents RENAME COLUMN embedding_v2 TO embedding;
```

```typescript
// Always tag embeddings with model version at write time
async function embedAndStore(content: string, docId: number): Promise<void> {
  const MODEL = 'text-embedding-3-small'
  const DIMENSIONS = 1536

  const response = await openai.embeddings.create({
    model: MODEL,
    input: content,
    dimensions: DIMENSIONS,
  })

  const embedding = response.data[0].embedding

  await pool.query(
    `UPDATE documents
     SET embedding = $1::vector, model_version = $2
     WHERE id = $3`,
    [`[${embedding.join(',')}]`, MODEL, docId]
  )
}
```

---

## pgvector vs Dedicated Vector Databases

| Factor | pgvector | Pinecone / Weaviate / Qdrant |
|---|---|---|
| Setup complexity | Low (extension) | Higher (separate service) |
| Relational JOINs | Native SQL | Not available |
| Pre-filter support | No (post-filter only) | Yes |
| Scale | Up to ~10M vectors comfortably | Billions of vectors |
| Operational overhead | Unified with Postgres | Additional infra |
| Cost | Postgres hosting cost | Additional SaaS cost |
| Ecosystem | Any Postgres client | Dedicated SDKs |

**Use pgvector when**: < 10M vectors, you need relational JOINs (e.g., filter by user → join users table), team already runs PostgreSQL, metadata filtering needs are simple.

**Use Pinecone/Weaviate/Qdrant when**: > 50M vectors, complex metadata pre-filtering, multi-tenant with per-tenant isolation, latency SLAs that pgvector can't meet.

---

## Index Maintenance and Monitoring

```sql
-- Check index size and usage
SELECT
  indexrelid::regclass AS index,
  pg_size_pretty(pg_relation_size(indexrelid)) AS size,
  idx_scan AS scans,
  idx_tup_read AS tuples_read
FROM pg_stat_user_indexes
WHERE tablename = 'documents';

-- Rebuild HNSW after significant data churn
-- (HNSW degrades slightly as deleted vectors aren't cleaned up)
REINDEX INDEX CONCURRENTLY idx_documents_embedding;

-- Monitor query performance with pg_stat_statements
SELECT
  left(query, 80) AS query,
  calls,
  mean_exec_time,
  total_exec_time
FROM pg_stat_statements
WHERE query ILIKE '%<=>%'
ORDER BY mean_exec_time DESC;
```

---

## Interview Questions

**Q: What is the difference between IVFFlat and HNSW and when would you choose each?**
IVFFlat partitions the vector space into clusters at build time and searches the nearest N clusters at query time. It requires data to exist before building, and recall depends heavily on the `probes` setting. HNSW builds a layered proximity graph that supports fast greedy navigation from coarse to fine resolution. HNSW has better recall/speed trade-offs and doesn't require pre-existing data, but uses more memory. Default to HNSW; use IVFFlat only when memory is the constraint.

**Q: What is the "post-filter problem" in pgvector and how do you work around it?**
pgvector finds the K nearest vectors first, then applies WHERE conditions. If a WHERE clause filters out most results, you get fewer than K results back — the semantic search didn't "know" about the filter. Work-arounds: (1) increase the LIMIT of the inner scan to fetch more candidates, (2) partition the table by the filter key so the index only covers relevant rows, (3) switch to a vector DB that supports pre-filtering (Qdrant, Weaviate).

**Q: How do you handle an embedding model migration without downtime?**
Add a second vector column for the new model. Backfill asynchronously in small batches. Build the index on the new column with `CREATE INDEX CONCURRENTLY`. Update the application to write to both columns (dual-write). Once backfill and index are complete, flip the application to read from the new column. Drop the old column. Never do the backfill in one transaction — it locks the table and may exceed memory limits.

**Q: When would you use cosine vs L2 distance?**
Use cosine (`<=>`) for text embeddings where you care about semantic direction, not magnitude — standard for search and RAG. Use L2 (`<->`) when the magnitude of the vector carries meaning (rare in NLP; more common in vision embeddings, recommendation systems). Use inner product (`<#>`) when vectors are already L2-normalized — it is mathematically equivalent to cosine similarity but faster to compute.

---

## Related

- [Indexing](../indexing/README.md) — B-tree, GIN, BRIN, EXPLAIN ANALYZE
- [PostgreSQL](../postgresql/README.md) — MVCC, connection pooling, partitioning
- [Vector Databases](../../04-ai-ml/vector-databases/README.md) — Pinecone, Weaviate, Qdrant
- [Embeddings](../../04-ai-ml/embeddings/README.md) — generating and chunking embeddings
- [Similarity Search](../../04-ai-ml/similarity-search/README.md) — ANN algorithms
- [Hybrid Search](../../06-rag/hybrid-search/README.md) — combining dense + sparse retrieval
- [Basic RAG](../../06-rag/basic-rag/README.md) — end-to-end retrieval pipeline
