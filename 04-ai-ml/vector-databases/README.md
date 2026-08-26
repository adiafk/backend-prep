# Vector Databases

## What a Vector Database Does

A vector database stores high-dimensional embedding vectors alongside metadata, indexes them for fast approximate nearest neighbor (ANN) search, and supports filtering on metadata fields before or after vector search. This is distinct from a general-purpose database that happens to have a vector column — a purpose-built vector database organizes its entire storage and query engine around vector operations.

The key operations:
- **Upsert**: store a vector with an ID and metadata payload
- **Vector search**: given a query vector, return the k most similar stored vectors
- **Filtered search**: vector search restricted to documents matching a metadata predicate (e.g., `source = "wiki"` AND `year >= 2023`)
- **Delete / update**: remove or replace vectors by ID

---

## Major Options Compared

| System | Type | ANN Index | Metadata Filtering | Hybrid Search | Scale | Notes |
|--------|------|-----------|-------------------|---------------|-------|-------|
| **Pinecone** | Managed SaaS | Proprietary (HNSW-based) | Yes, post-filter | Yes (sparse+dense) | 10B+ | No infra management; most expensive |
| **Weaviate** | Open-source / managed | HNSW | Yes, pre-filter | Yes, built-in BM25+vector | 1B+ | GraphQL API; module ecosystem for auto-vectorization |
| **Qdrant** | Open-source / managed | HNSW (Rust) | Yes, pre-filter | Yes, sparse+dense | 1B+ | High performance; filterable payload; self-host or Qdrant Cloud |
| **Chroma** | Open-source | HNSW (hnswlib) | Yes | No (manual) | <10M | Python-first; simplest setup; default for prototyping |
| **pgvector** | PostgreSQL extension | HNSW, IVF | Yes, full SQL | Manual (BM25 via pg_search) | <50M (practical) | Transactional; SQL joins with relational data; no new infra |
| **Milvus** | Open-source / cloud | HNSW, IVF, DiskANN | Yes | Yes | 10B+ | Cloud-native; Kubernetes-native; complex deployment |
| **Redis VSS** | Add-on to Redis | HNSW, FLAT | Yes | No | <100M | Good if Redis already in stack; co-locate vectors with other data |

---

## Decision Framework

### Use pgvector when:

- Vectors live alongside relational data you join frequently (e.g., `JOIN products ON products.id = vectors.doc_id`)
- Your team already operates PostgreSQL — no new infrastructure, same connection pool, same backup procedures
- Vector corpus is under 10M vectors (50M is possible but requires careful index tuning)
- You need ACID transactions across vector inserts and relational updates
- Cost matters — pgvector has no separate licensing cost beyond your PostgreSQL hosting

```sql
-- pgvector: create table, index, query in plain SQL
CREATE TABLE documents (
    id BIGSERIAL PRIMARY KEY,
    content TEXT,
    source TEXT,
    embedding VECTOR(1536),
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX ON documents USING hnsw (embedding vector_cosine_ops)
    WITH (m = 16, ef_construction = 64);

-- Filtered vector search: only documents from a specific source
SELECT id, content, 1 - (embedding <=> $1::vector) AS similarity
FROM documents
WHERE source = 'product-docs'
  AND created_at > NOW() - INTERVAL '90 days'
ORDER BY embedding <=> $1::vector
LIMIT 10;
```

### Use a dedicated vector database when:

- Corpus exceeds 50M–100M vectors and you need sub-100ms p99 latency
- Complex metadata filtering is a first-class requirement (pre-filtering before vector search, not post-filtering)
- You need managed infrastructure with auto-scaling and no operational burden
- Hybrid search (combining dense vector scores with sparse BM25 keyword scores) is required out of the box
- Multi-tenancy with strict data isolation between customers is required

### Use Chroma when:

- Local development, prototyping, or a proof-of-concept
- Team is Python-first and wants zero infrastructure setup
- Corpus is small (<1M vectors) and you do not need production SLAs

---

## Metadata Filtering: Pre-filter vs. Post-filter

This distinction matters for both correctness and performance.

**Post-filtering**: retrieve the top-k by vector similarity, then discard results that do not match the metadata predicate. Simple but wrong when the filter is selective — if only 1% of documents match your filter, you may need to retrieve 100× more candidates than your final k to get enough matches through.

**Pre-filtering**: restrict the candidate set to matching documents before running ANN search. Correct and efficient but harder to implement — the ANN index must support filtering within a subset of the corpus. Qdrant and Weaviate implement this. pgvector relies on PostgreSQL's query planner to push predicates before the index scan.

For RAG pipelines with selective filters (e.g., tenant isolation, document type restrictions), pre-filtering is required. With post-filtering, you risk returning too few results or incorrect recall guarantees.

---

## Multi-tenancy

When building a SaaS product where each customer's data must be isolated:

**Option 1: Namespace / collection per tenant**
- Create a separate collection or namespace for each tenant
- No cross-tenant data leakage possible at the database level
- Management overhead scales with number of tenants
- Best for: strict compliance requirements, large enterprise tenants with high query volumes

**Option 2: Metadata filter per tenant**
- Store all tenants in one collection with a `tenant_id` metadata field
- Filter every query by `tenant_id = X`
- Simpler operationally; one collection to maintain
- Risk: filter bugs expose cross-tenant data; pre-filtering required for correctness
- Best for: large numbers of small tenants (SaaS with thousands of customers)

**Option 3: Separate database instances**
- Maximum isolation; allows independent scaling, billing, and SLA per tenant
- Operationally expensive
- Best for: enterprise customers with dedicated infrastructure requirements

Weaviate has first-class multi-tenancy support where tenant data is stored in separate physical shards while appearing as a single logical collection. Qdrant supports named collections with separate HNSW indexes.

---

## Hybrid Search Support

Hybrid search combines dense vector similarity (semantic) with sparse keyword matching (BM25/TF-IDF). This matters because:
- Dense search finds semantically similar documents but misses exact keyword matches
- Sparse search finds exact keyword matches but misses paraphrases and synonyms
- Combining both outperforms either alone on most retrieval benchmarks

```
hybrid_score = α × dense_score + (1 - α) × sparse_score
```

| System | Native Hybrid Search |
|--------|---------------------|
| Pinecone | Yes — sparse+dense in one query |
| Weaviate | Yes — BM25 built-in, combine with vector search via `hybrid` operator |
| Qdrant | Yes — sparse vector support since v1.7 |
| Milvus | Yes — sparse+dense index types |
| pgvector | Manual — combine with `pg_search` (BM25) or `tsvector` full-text search |
| Chroma | No — requires manual implementation |

See [Hybrid Search](../../06-rag/hybrid-search/README.md) for implementation details.

---

## Embedding Model Versioning

When you upgrade the embedding model (e.g., from `text-embedding-ada-002` to `text-embedding-3-large`), all stored vectors become incompatible — you cannot query new embeddings against old vectors because the vector spaces are different.

**Migration strategy:**

1. **Store `model_version` in metadata** on every document at index time:
   ```python
   metadata = {
       "source": "docs",
       "model_version": "text-embedding-3-large-v1",
       "embedded_at": "2024-11-01"
   }
   ```

2. **Create a new collection** (or namespace) for the new model. Do not overwrite in place — you need both models operational during migration.

3. **Re-embed in the background**: process the corpus asynchronously, writing to the new collection.

4. **Dual-read period**: route all queries to both collections, merge results, until re-embedding is complete.

5. **Cutover**: once 100% of documents are re-embedded, route all queries to the new collection.

6. **Delete old collection** after a monitoring period.

Never mix vectors from different models in the same collection without explicit versioning in metadata — the results will be silently wrong.

---

## Index Tuning for Production

### HNSW parameters (applicable to Qdrant, Weaviate, pgvector)

```python
# Qdrant example
client.create_collection(
    collection_name="documents",
    vectors_config=VectorParams(
        size=1536,
        distance=Distance.COSINE,
    ),
    hnsw_config=HnswConfigDiff(
        m=16,                  # edges per node; increase for better recall at cost of memory
        ef_construct=100,      # build quality; increase for better graph, slower inserts
        full_scan_threshold=10_000,  # below this, do brute-force (no index overhead)
    ),
    optimizers_config=OptimizersConfigDiff(
        indexing_threshold=20_000,  # build index after this many vectors
    ),
)

# Query with dynamic ef_search
results = client.search(
    collection_name="documents",
    query_vector=query_embedding,
    limit=10,
    search_params=SearchParams(hnsw_ef=128),  # higher ef = better recall, more latency
)
```

### Measuring recall in production

Always maintain a held-out evaluation set with ground-truth nearest neighbors (computed by exact search offline). Run your ANN index against this set periodically and alert if recall@10 drops below your SLA.

---

## Interview Q&A

**Q: When would you use pgvector instead of Pinecone?**
A: When vectors need to be queried together with relational data via SQL joins, the team already operates PostgreSQL, the corpus is under 10M vectors, and the cost of a managed vector database is not justified. pgvector adds ANN capability to a system you already understand operationally. For billion-scale, dedicated vector DBs win on performance.

**Q: What is the difference between pre-filtering and post-filtering in vector search?**
A: Post-filtering retrieves the top-k by vector similarity and then discards non-matching results — it fails when the filter is selective because most retrieved candidates get discarded. Pre-filtering restricts the ANN search to the matching subset upfront, giving correct recall regardless of filter selectivity. Purpose-built vector databases like Qdrant and Weaviate implement pre-filtering; pgvector relies on the PostgreSQL query planner.

**Q: What happens when you change your embedding model and how do you handle it?**
A: You must re-embed the entire corpus because vectors from different models live in different spaces — you cannot search across them. The safe migration path is: add model_version to metadata, create a new collection for the new model, re-embed in the background, dual-read during migration, then cut over. Never mix model versions in one collection.

**Q: How would you architect multi-tenancy in a vector database?**
A: Depends on the isolation requirement and tenant count. For strict data isolation (compliance, large enterprise), use separate collections or namespaces per tenant. For large numbers of small tenants, use a single collection with tenant_id metadata and pre-filter every query — pre-filtering is critical; post-filtering can leak data on bugs. Some systems like Weaviate have native multi-tenancy with physical shard isolation per tenant.

---

## Related Topics

- [Similarity Search](../similarity-search/README.md) — HNSW, IVF, PQ algorithms
- [pgvector](../../03-databases/pgvector/README.md) — PostgreSQL vector extension setup and tuning
- [Hybrid Search](../../06-rag/hybrid-search/README.md) — combining dense and sparse retrieval
