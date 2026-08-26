# Hybrid Search

## Why Hybrid Search?

Pure dense vector search handles conceptual similarity well — "car" and "automobile" map close together in embedding space. But it fails on:

- **Exact terms**: product IDs (`SKU-9821`), error codes (`ECONNREFUSED`), model names (`gpt-4o-mini`)
- **Proper nouns**: "Aditya Lowanshi", "pgvector", "LangGraph"
- **Rare technical jargon**: terms not seen often enough in training to have strong embeddings
- **Precise queries**: "what is the return policy for order #48291?"

Pure keyword search (BM25) handles exact terms but misses paraphrasing and conceptual matches — "car troubles" won't match "automobile issues."

Hybrid search combines both: you get recall on exact terms from BM25 and semantic understanding from dense vectors. Benchmarks consistently show 10–15% better recall on named entities, IDs, and technical terms compared to pure vector search.

---

## BM25 In Depth

BM25 (Best Match 25) is the canonical keyword ranking function. It improves on TF-IDF with two key additions: term frequency saturation and document length normalization.

### Term Frequency (TF)

Raw TF counts how many times term `t` appears in document `d`. The problem: doubling occurrences shouldn't double relevance. BM25 saturates TF:

```
tf_saturated = (tf * (k1 + 1)) / (tf + k1 * (1 - b + b * (|d| / avgdl)))
```

Where:
- `tf` = raw term frequency in document
- `k1` = saturation parameter (typical value: 1.2–2.0). Higher = slower saturation
- `b` = length normalization strength (0–1, typical: 0.75). `b=1` = full normalization, `b=0` = none
- `|d|` = document length in tokens
- `avgdl` = average document length across corpus

### Inverse Document Frequency (IDF)

IDF penalizes common words. "the" appearing in every document carries no signal.

```
idf = log((N - df + 0.5) / (df + 0.5) + 1)
```

Where:
- `N` = total number of documents
- `df` = number of documents containing term `t`

If a term appears in every document, `df = N`, so IDF → 0. Rare terms get high IDF.

### Full BM25 Score

```
BM25(d, q) = Σ idf(t) * tf_saturated(t, d)   for each term t in query q
```

The sum is over all query terms. Each term's contribution is its IDF weight times its saturated TF.

```typescript
function bm25Score(
  termFreqs: Map<string, number>,  // term -> count in this doc
  docLength: number,
  avgDocLength: number,
  idfMap: Map<string, number>,     // term -> precomputed IDF
  queryTerms: string[],
  k1 = 1.2,
  b = 0.75
): number {
  let score = 0;
  for (const term of queryTerms) {
    const tf = termFreqs.get(term) ?? 0;
    const idf = idfMap.get(term) ?? 0;
    const tfSaturated =
      (tf * (k1 + 1)) /
      (tf + k1 * (1 - b + b * (docLength / avgDocLength)));
    score += idf * tfSaturated;
  }
  return score;
}
```

In practice you use a search engine (Elasticsearch, PostgreSQL `tsvector`, Typesense) that computes BM25 for you — but knowing the formula explains why "the quick brown fox" doesn't rank highly for "quick" if the corpus is about foxes.

---

## Dense Retrieval

Dense retrieval embeds the query and all documents into a shared vector space using a transformer encoder (e.g., `text-embedding-3-small`, `nomic-embed-text`). At query time:

```
similarity(q, d) = cosine(embed(q), embed(d))
                 = (q · d) / (||q|| * ||d||)
```

With normalized vectors (||v|| = 1), cosine similarity reduces to dot product, which is what ANN indexes (HNSW, IVFFlat) compute efficiently.

Dense retrieval captures:
- Paraphrasing: "how do I cancel?" matches "subscription termination steps"
- Cross-lingual: if embeddings are multilingual
- Concept proximity: "headache" near "migraine"

See [../basic-rag/README.md](../basic-rag/README.md) for embedding setup and [../../04-ai-ml/similarity-search/README.md](../../04-ai-ml/similarity-search/README.md) for ANN algorithms.

---

## Merging Results: Reciprocal Rank Fusion (RRF)

You have two ranked lists — one from BM25, one from dense retrieval. The scores are on incompatible scales (BM25 is unbounded; cosine is [-1, 1]). You need to merge them without recalibrating.

RRF is the standard solution. For each document, sum its reciprocal ranks across all result lists:

```
RRF_score(d) = Σ 1 / (k + rank_i(d))
```

Where:
- `rank_i(d)` = rank of document `d` in list `i` (1-indexed)
- `k` = smoothing constant, default **60** (chosen empirically by the original paper; dampens the impact of top-ranked documents and makes the formula robust to rank differences)
- Sum is over all result lists containing the document

Documents not in a given list contribute 0 from that list.

**Why k=60?** The original Cormack et al. paper found k=60 maximized performance across diverse retrieval tasks. It's robust: in practice, k=40–80 all perform similarly.

```typescript
function reciprocalRankFusion(
  rankings: Array<string[]>,  // each inner array is an ordered list of doc IDs
  k = 60
): Map<string, number> {
  const scores = new Map<string, number>();

  for (const ranking of rankings) {
    ranking.forEach((docId, index) => {
      const rank = index + 1;  // 1-indexed
      const prev = scores.get(docId) ?? 0;
      scores.set(docId, prev + 1 / (k + rank));
    });
  }

  return scores;
}

function mergeWithRRF(
  denseResults: string[],   // doc IDs ordered by cosine similarity desc
  sparseResults: string[],  // doc IDs ordered by BM25 score desc
  topK = 10
): string[] {
  const scores = reciprocalRankFusion([denseResults, sparseResults]);
  return [...scores.entries()]
    .sort((a, b) => b[1] - a[1])
    .slice(0, topK)
    .map(([id]) => id);
}
```

RRF is preferred over weighted combination because:
- No parameter tuning needed (k=60 works everywhere)
- Score scales don't matter — only ranks do
- Robust to one retriever returning noisy scores

---

## Alternative: Weighted Linear Combination

If you want explicit control over the dense/sparse balance:

```
hybrid_score(d) = α * normalize(dense_score(d)) + (1 - α) * normalize(sparse_score(d))
```

Where normalization maps scores to [0, 1] within each result set:
```
normalized(s) = (s - min) / (max - min)
```

```typescript
function weightedHybrid(
  denseScores: Map<string, number>,
  sparseScores: Map<string, number>,
  alpha = 0.7  // weight for dense; 1-alpha for sparse
): Map<string, number> {
  const normalize = (scores: Map<string, number>): Map<string, number> => {
    const values = [...scores.values()];
    const min = Math.min(...values);
    const max = Math.max(...values);
    const range = max - min || 1;
    return new Map([...scores.entries()].map(([id, s]) => [id, (s - min) / range]));
  };

  const normDense = normalize(denseScores);
  const normSparse = normalize(sparseScores);

  const allIds = new Set([...denseScores.keys(), ...sparseScores.keys()]);
  const combined = new Map<string, number>();

  for (const id of allIds) {
    const d = normDense.get(id) ?? 0;
    const s = normSparse.get(id) ?? 0;
    combined.set(id, alpha * d + (1 - alpha) * s);
  }

  return combined;
}
```

Downside: requires calibrating `alpha` per domain and re-calibrating when either retriever changes. Use RRF unless you have a strong reason to tune weights.

---

## Advanced Sparse: SPLADE

SPLADE (SParse Lexical AnD Expansion) is learned sparse retrieval. Instead of raw BM25 term counts, a transformer produces a sparse vector over the vocabulary — each dimension is a vocabulary term, most are zero.

Key differences from BM25:
- **Term expansion**: query "car" may activate "automobile", "vehicle", "sedan" — implicit query expansion
- **Learned weights**: importance of each term is learned from relevance data, not formula-derived
- **Sparse storage**: stored like an inverted index, not a dense vector — enables exact match lookup

The output is a sparse vector `{term_id: weight}` for both query and document. Retrieval is dot product over the sparse vectors. Efficient because most weights are zero.

SPLADE outperforms BM25 on most benchmarks while retaining keyword-search efficiency. It's available via HuggingFace (`naver/splade-cocondenser-ensembledistil`) but adds model inference overhead at index time.

---

## ColBERT: Late Interaction

ColBERT embeds each **token** in the query and each token in the document separately, then scores via MaxSim:

```
score(q, d) = Σ max_j(cosine(E_q[i], E_d[j]))   for each query token i
```

For each query token, find the most similar document token, sum those similarities. This is more expressive than single-vector cosine similarity but requires storing per-token embeddings (~128 dims × num_tokens per document).

ColBERT is used when retrieval quality matters more than storage cost. Not common in production RAG but available via Ragatouille library.

---

## Implementation: PostgreSQL

PostgreSQL supports both retrieval modes natively:

```sql
-- Enable extensions
CREATE EXTENSION IF NOT EXISTS vector;      -- pgvector for dense
CREATE EXTENSION IF NOT EXISTS pg_trgm;    -- trigram similarity

-- Table with both dense and full-text search support
CREATE TABLE documents (
  id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  content     TEXT NOT NULL,
  embedding   vector(1536),                           -- dense vector
  ts_content  TSVECTOR GENERATED ALWAYS AS (to_tsvector('english', content)) STORED
);

CREATE INDEX ON documents USING hnsw (embedding vector_cosine_ops);
CREATE INDEX ON documents USING gin (ts_content);

-- Hybrid search: union of dense and sparse, merged by RRF in application
WITH dense AS (
  SELECT id, row_number() OVER (ORDER BY embedding <=> $1::vector) AS rank
  FROM documents
  ORDER BY embedding <=> $1::vector
  LIMIT 20
),
sparse AS (
  SELECT id, row_number() OVER (ORDER BY ts_rank(ts_content, query) DESC) AS rank
  FROM documents, websearch_to_tsquery('english', $2) query
  WHERE ts_content @@ query
  ORDER BY ts_rank(ts_content, query) DESC
  LIMIT 20
),
rrf AS (
  SELECT
    COALESCE(d.id, s.id) AS id,
    COALESCE(1.0 / (60 + d.rank), 0) + COALESCE(1.0 / (60 + s.rank), 0) AS score
  FROM dense d
  FULL OUTER JOIN sparse s ON d.id = s.id
)
SELECT id, score FROM rrf ORDER BY score DESC LIMIT 10;
```

See [../../03-databases/pgvector/README.md](../../03-databases/pgvector/README.md) for pgvector setup and index configuration.

---

## Implementation: Elasticsearch

Elasticsearch 8.x supports hybrid search natively via `knn` + `query` combined with RRF:

```typescript
import { Client } from "@elastic/elasticsearch";

const client = new Client({ node: process.env.ELASTICSEARCH_URL });

async function hybridSearch(
  queryText: string,
  queryEmbedding: number[],
  topK = 10
) {
  const response = await client.search({
    index: "documents",
    body: {
      // Reciprocal Rank Fusion built into ES 8.8+
      retriever: {
        rrf: {
          retrievers: [
            // Dense retrieval
            {
              knn: {
                field: "embedding",
                query_vector: queryEmbedding,
                num_candidates: 50,
                k: 20,
              },
            },
            // Sparse BM25 retrieval
            {
              standard: {
                query: {
                  multi_match: {
                    query: queryText,
                    fields: ["content", "title^2"],
                  },
                },
              },
            },
          ],
          rank_constant: 60,
          window_size: 20,
        },
      },
      size: topK,
    },
  });

  return response.hits.hits.map((hit) => ({
    id: hit._id,
    score: hit._score,
    content: (hit._source as any).content,
  }));
}
```

---

## Full TypeScript Implementation

```typescript
import OpenAI from "openai";

const openai = new OpenAI();

interface SearchResult {
  id: string;
  content: string;
  score: number;
  source: "dense" | "sparse" | "hybrid";
}

// Simulates your vector DB returning ordered doc IDs
async function denseSearch(queryEmbedding: number[], topK: number): Promise<string[]> {
  // Replace with actual pgvector / Pinecone / Qdrant call
  throw new Error("Implement with your vector store");
}

// Simulates your keyword index returning ordered doc IDs
async function sparseSearch(queryText: string, topK: number): Promise<string[]> {
  // Replace with actual Elasticsearch / PostgreSQL tsquery call
  throw new Error("Implement with your search backend");
}

async function hybridSearch(
  queryText: string,
  topK = 10,
  rrfK = 60
): Promise<string[]> {
  // Embed query once, run both searches in parallel
  const [embeddingResponse, sparseIds] = await Promise.all([
    openai.embeddings.create({ model: "text-embedding-3-small", input: queryText }),
    sparseSearch(queryText, topK * 2),
  ]);

  const queryEmbedding = embeddingResponse.data[0].embedding;
  const denseIds = await denseSearch(queryEmbedding, topK * 2);

  // RRF merge
  const scores = new Map<string, number>();

  const addRanking = (ids: string[]) => {
    ids.forEach((id, index) => {
      const rank = index + 1;
      scores.set(id, (scores.get(id) ?? 0) + 1 / (rrfK + rank));
    });
  };

  addRanking(denseIds);
  addRanking(sparseIds);

  return [...scores.entries()]
    .sort((a, b) => b[1] - a[1])
    .slice(0, topK)
    .map(([id]) => id);
}
```

---

## Tuning Dense/Sparse Balance

| Scenario | Recommended approach |
|---|---|
| Query contains IDs, codes, names | Increase BM25 weight or lower k (raises sparse contribution) |
| Queries are conceptual / paraphrased | Increase dense weight |
| Domain with lots of jargon | Consider SPLADE instead of raw BM25 |
| Mixed queries (most real apps) | Start with RRF k=60, measure recall on golden set, adjust only if needed |
| Low latency required | Drop one retriever; dense-only is usually better than sparse-only |

Run A/B experiments comparing pure dense vs hybrid on your actual query distribution before optimizing. Hybrid adds latency (two parallel searches + merge) and complexity. If your queries are all conceptual, pure dense may be sufficient.

---

## When Hybrid Beats Pure Vector

Benchmark results (BEIR dataset, various studies):
- Named entities and proper nouns: +12–18% recall@5
- Product/document IDs: +20–30% recall@5
- Technical abbreviations: +8–15% recall@5
- General knowledge questions: +2–5% recall@5 (smaller gain)

The asymmetry explains why hybrid is the default in production RAG: the cost is small (parallel query + merge), and the upside on exact-term queries is large.

---

## Interview Q&A

**Q: Why can't you just use a better embedding model instead of adding BM25?**

Better embeddings help but don't fully solve exact-match failures. Embeddings compress meaning into a fixed vector — a 1536-dimensional vector can't distinguish "SKU-9821" from "SKU-9822" the way an inverted index can. Keyword search is exact; embeddings are approximate.

**Q: What is the k parameter in RRF and how do you choose it?**

k=60 is the empirically derived default from the original paper. It dampens the influence of top-ranked documents, making the formula robust when one retriever's top result is clearly better than the other's. Values 40–80 all perform similarly in practice. Only tune it if you have a labeled evaluation set and evidence that a different value helps.

**Q: When would you use weighted linear combination instead of RRF?**

When you have domain-specific evidence (via evaluation on a labeled test set) that one retriever consistently outperforms the other. For example, a legal document search system might weight BM25 heavily because queries use precise legal citations. But you need the labeled data to tune alpha — without it, default to RRF.

**Q: How does SPLADE differ from BM25?**

BM25 scores terms that literally appear in the document using a formula. SPLADE uses a transformer to produce a sparse weight vector over the entire vocabulary — it can assign weight to terms that aren't literally present (query expansion) and the weights are learned from relevance labels rather than derived from term statistics. SPLADE typically outperforms BM25 but requires running inference at index time.

**Q: What's the main failure mode of hybrid search?**

The merge step assumes both retrievers return the same document set for the same underlying corpus. If the keyword index and vector index are out of sync (e.g., one is stale after an update), the merge can surface stale results from one and fresh from the other. Ensure both indexes are updated atomically on document ingestion.

---

## Related

- [../basic-rag/README.md](../basic-rag/README.md) — dense-only RAG baseline
- [../../03-databases/pgvector/README.md](../../03-databases/pgvector/README.md) — pgvector setup and HNSW indexing
- [../../04-ai-ml/similarity-search/README.md](../../04-ai-ml/similarity-search/README.md) — ANN algorithms (HNSW, IVFFlat, LSH)
