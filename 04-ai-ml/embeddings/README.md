# Embeddings

Embeddings are one of the most practically useful ideas in modern AI. If you're building RAG, semantic search, or any system that needs to find "related" content, you need to understand this deeply.

---

## 1. What Embeddings Are

An embedding is the transformation of text (or any data) into a fixed-size vector of numbers.

```
"The cat sat on the mat"  →  [0.023, -0.841, 0.156, 0.392, ...]
                                        ↑ 1536 floats (for OpenAI text-embedding-3-small)
```

That's it mechanically. The crucial property is that **the numbers encode meaning**. Two pieces of text that mean similar things produce vectors that are close to each other in the high-dimensional space. Two pieces of text that mean different things produce vectors that are far apart.

```
"I love dogs"       →  [0.023, -0.841, 0.156, ...]
"I adore canines"   →  [0.019, -0.838, 0.161, ...]  ← very similar
"The stock rose 5%" →  [-0.312, 0.445, -0.891, ...]  ← very different
```

The model that produces embeddings has learned — from hundreds of millions of text examples — to place semantically similar text near each other. You don't teach it rules; it learns the geometry of meaning from patterns in data.

### How Embedding Models Work (Briefly)

An embedding model is typically an encoder-only transformer (like BERT). It processes the entire input text simultaneously (no left-to-right generation). The final layer produces a vector representation. The model is trained using a contrastive objective: pairs of semantically similar sentences should produce close vectors; dissimilar sentences should produce distant vectors.

The resulting vector space is a learned compression of the semantic content of the text into a fixed number of dimensions.

---

## 2. Why Geometry Encodes Semantics

This is the non-obvious part. Why does mapping text to points in space allow semantic comparison?

### The Intuition

Think of a 2D map of a city. Places that are functionally similar tend to cluster: restaurants are near other restaurants, parks near parks. If you drop a pin for every business in the city, geography encodes some information about purpose.

Now scale that to 1536 dimensions with a model trained on all of human text. The model learns that certain directions in this space correspond to semantic relationships.

### What the Space Looks Like

**Clusters** — similar concepts cluster together. Medical terminology clusters near other medical terminology. Python code clusters near other Python code. Questions cluster near other questions.

**Directions encode relationships** — famously, word2vec showed that vector(king) - vector(man) + vector(woman) ≈ vector(queen). This arithmetic works because the direction from "man" to "woman" encodes the concept of gender, and the direction from "king" to "queen" runs parallel to it.

Modern sentence embedding spaces show similar structure:
- The direction from "I like X" to "I hate X" encodes sentiment
- The direction from a question to its answer is consistent across topics
- Formal and informal versions of the same idea are close together

**Distance encodes similarity** — the closer two vectors, the more semantically related the underlying texts. "What is machine learning?" and "Explain ML to me" land near each other. "What is machine learning?" and "How do I bake bread?" land far apart.

### Why This Happens

The model was trained to pull semantically similar pairs together and push dissimilar pairs apart. After training on enough examples, the model discovers that it can efficiently organize meaning by encoding certain structural properties:

- Related concepts need similar internal representations to make similar predictions
- The model generalizes: it's never seen every possible sentence, but it has learned the structure of meaning well enough to correctly place novel sentences

The geometry isn't hand-designed. It emerges from the training objective. This is why embeddings generalize to text the model has never seen — they capture the underlying structure of language.

### Practical Implication

You can compare any two pieces of text for semantic similarity without any explicit rules, ontologies, or synonym lists. The geometry does the work. This is why embeddings are the foundation of semantic search, RAG, and clustering.

---

## 3. Cosine Similarity

### The Formula

Cosine similarity measures the cosine of the angle between two vectors. It ranges from -1 to 1.

```
cosine_similarity(A, B) = (A · B) / (‖A‖ × ‖B‖)
```

Where:
- `A · B` is the dot product: sum of element-wise products
- `‖A‖` is the magnitude (L2 norm) of vector A: square root of sum of squares

Expanded:

```
A · B    = A[0]×B[0] + A[1]×B[1] + ... + A[n]×B[n]
‖A‖      = √(A[0]² + A[1]² + ... + A[n]²)

cosine_similarity(A, B) = Σ(A[i]×B[i]) / (√Σ(A[i]²) × √Σ(B[i]²))
```

### The Range and What Values Mean

| Value | Meaning |
|---|---|
| 1.0 | Identical direction — semantically equivalent |
| 0.8 - 0.99 | Very similar — same topic, possibly different phrasing |
| 0.6 - 0.79 | Related — overlapping subject matter |
| 0.4 - 0.59 | Loosely related |
| 0.0 - 0.39 | Unrelated |
| < 0 | Semantically opposite (rare in practice with modern models) |

These thresholds are guidelines, not absolutes — they vary by model and use case. Calibrate with your actual data.

### Why Cosine vs Euclidean Distance

**Euclidean distance** measures the straight-line distance between two points in space. It works for some applications but has a critical flaw for text embeddings: **it is sensitive to vector magnitude**.

Two sentences that mean the same thing but have different lengths might produce vectors pointing in the same direction but at different magnitudes (distances from the origin). Euclidean distance would call them different; cosine similarity (which only cares about direction) would call them similar.

```
Short: "dogs are great"     → vector with small magnitude
Long:  "dogs are great animals and I love them very much" → larger magnitude

Euclidean distance: large (they're far apart)
Cosine similarity: high (they point in the same direction)
```

**Cosine similarity is scale-invariant** — it only measures the angle between vectors, ignoring their magnitude. For meaning comparison, direction is what matters.

A secondary benefit: when vectors are normalized (magnitude = 1.0), cosine similarity equals the dot product, which is extremely fast to compute. Most vector databases store normalized embeddings for this reason.

**When Euclidean distance is acceptable**: when vectors are already normalized (then cosine and Euclidean distance are monotonically equivalent), or when you explicitly care about magnitude.

---

## 4. TypeScript Implementation of Cosine Similarity

Here is a from-scratch implementation with no dependencies, including utility functions for batch comparison:

```typescript
/**
 * Compute the dot product of two vectors.
 * Throws if vectors have different dimensions.
 */
function dotProduct(a: number[], b: number[]): number {
  if (a.length !== b.length) {
    throw new Error(
      `Vector dimension mismatch: ${a.length} vs ${b.length}`
    );
  }
  let sum = 0;
  for (let i = 0; i < a.length; i++) {
    sum += a[i] * b[i];
  }
  return sum;
}

/**
 * Compute the L2 norm (magnitude) of a vector.
 */
function magnitude(v: number[]): number {
  let sumOfSquares = 0;
  for (const x of v) {
    sumOfSquares += x * x;
  }
  return Math.sqrt(sumOfSquares);
}

/**
 * Compute cosine similarity between two vectors.
 * Returns a value in [-1, 1].
 * Returns 0 for zero vectors (avoids division by zero).
 */
function cosineSimilarity(a: number[], b: number[]): number {
  const magA = magnitude(a);
  const magB = magnitude(b);

  if (magA === 0 || magB === 0) {
    return 0; // zero vector has no direction — similarity is undefined
  }

  return dotProduct(a, b) / (magA * magB);
}

/**
 * Normalize a vector to unit length (magnitude = 1).
 * After normalization, dot product == cosine similarity.
 */
function normalize(v: number[]): number[] {
  const mag = magnitude(v);
  if (mag === 0) return v.slice(); // return copy of zero vector
  return v.map((x) => x / mag);
}

/**
 * Find the top-k most similar vectors to a query vector.
 * Returns results sorted by similarity (highest first).
 */
function topKSimilar(
  query: number[],
  candidates: Array<{ id: string; vector: number[] }>,
  k: number
): Array<{ id: string; similarity: number }> {
  const scores = candidates.map((candidate) => ({
    id: candidate.id,
    similarity: cosineSimilarity(query, candidate.vector),
  }));

  scores.sort((a, b) => b.similarity - a.similarity);
  return scores.slice(0, k);
}

// --- Example usage ---

const queryVector = [0.1, 0.8, -0.3, 0.5, 0.2];
const docA = [0.11, 0.79, -0.28, 0.51, 0.19]; // nearly identical
const docB = [-0.4, 0.1, 0.9, -0.2, 0.7];    // very different
const docC = [0.08, 0.75, -0.31, 0.48, 0.21]; // similar

console.log(cosineSimilarity(queryVector, docA)); // ~0.999
console.log(cosineSimilarity(queryVector, docB)); // ~0.1
console.log(cosineSimilarity(queryVector, docC)); // ~0.998

const results = topKSimilar(
  queryVector,
  [
    { id: "doc-a", vector: docA },
    { id: "doc-b", vector: docB },
    { id: "doc-c", vector: docC },
  ],
  2
);
// results: [{ id: "doc-a", similarity: ~0.999 }, { id: "doc-c", similarity: ~0.998 }]
```

**Performance note** — this naive O(n) scan works for hundreds or low thousands of vectors. For millions of vectors, use a vector database (Pinecone, Weaviate, pgvector, Qdrant) which implements approximate nearest neighbor (ANN) algorithms (HNSW, IVF) that achieve sub-linear search time.

**Optimized version using pre-normalized vectors** — if you store normalized embeddings, cosine similarity reduces to a dot product:

```typescript
function dotProductSimilarity(
  normalizedA: number[],
  normalizedB: number[]
): number {
  // When both vectors are normalized, dot product == cosine similarity
  // This is faster — avoids computing magnitudes
  return dotProduct(normalizedA, normalizedB);
}

// Pre-normalize at index time (pay the cost once, not at every query)
const normalizedEmbeddings = rawEmbeddings.map((e) => ({
  id: e.id,
  vector: normalize(e.vector),
}));
```

---

## 5. When to Use Embeddings

### Semantic Search

**Problem** — keyword search fails when users phrase queries differently from how documents are written. "cheap accommodation" won't match "affordable lodging" with BM25.

**Solution** — embed both the query and all documents. Find documents whose vectors are closest to the query vector. Matches on meaning, not keywords.

```typescript
async function semanticSearch(
  query: string,
  documents: Document[],
  topK: number = 5
): Promise<Document[]> {
  const queryEmbedding = await embedText(query);
  const docEmbeddings = await Promise.all(
    documents.map((d) => embedText(d.content))
  );

  const scores = docEmbeddings.map((vec, i) => ({
    doc: documents[i],
    score: cosineSimilarity(queryEmbedding, vec),
  }));

  return scores
    .sort((a, b) => b.score - a.score)
    .slice(0, topK)
    .map((s) => s.doc);
}
```

### RAG (Retrieval-Augmented Generation)

The primary use case for embeddings in LLM applications. Rather than putting your entire knowledge base in the prompt (context limit, cost), embed it all and retrieve only the relevant chunks per query.

Architecture:
1. Chunk documents → embed each chunk → store in vector DB
2. At query time: embed the user's question → find top-k similar chunks → pass chunks + question to LLM

This is covered in depth in the chunking section.

### Clustering

Group similar documents together without predefined categories. Useful for:
- Topic modeling across a corpus
- Grouping customer support tickets by issue type
- Discovering patterns in unstructured data

k-means and DBSCAN work directly on embedding vectors.

### Classification

Instead of training a text classifier from scratch, embed text and train a simple classifier (logistic regression, SVM, k-NN) on the embeddings. With high-quality embeddings, you often need far fewer labeled examples because the embedding model has already encoded semantic structure.

```typescript
// k-NN classification using embeddings
function classifyByNearestNeighbor(
  queryEmbedding: number[],
  labeledExamples: Array<{ embedding: number[]; label: string }>,
  k: number = 3
): string {
  const scored = labeledExamples.map((ex) => ({
    label: ex.label,
    similarity: cosineSimilarity(queryEmbedding, ex.embedding),
  }));

  scored.sort((a, b) => b.similarity - a.similarity);
  const topK = scored.slice(0, k);

  // Majority vote
  const votes: Record<string, number> = {};
  for (const { label } of topK) {
    votes[label] = (votes[label] ?? 0) + 1;
  }

  return Object.entries(votes).sort((a, b) => b[1] - a[1])[0][0];
}
```

### Duplicate Detection

Find near-duplicate content — helpful for deduplication before indexing, plagiarism detection, or identifying when a user is asking the same question again.

```typescript
function findNearDuplicates(
  embeddings: Array<{ id: string; vector: number[] }>,
  threshold: number = 0.95
): Array<[string, string]> {
  const pairs: Array<[string, string]> = [];

  for (let i = 0; i < embeddings.length; i++) {
    for (let j = i + 1; j < embeddings.length; j++) {
      const sim = cosineSimilarity(
        embeddings[i].vector,
        embeddings[j].vector
      );
      if (sim >= threshold) {
        pairs.push([embeddings[i].id, embeddings[j].id]);
      }
    }
  }

  return pairs;
}
```

---

## 6. Embedding Model Selection

Choosing the right embedding model affects quality, cost, speed, and operational complexity. The dimensions of the output vector are one factor among many.

### Dimensions

Embedding models output vectors of fixed size, typically:
- 384 dimensions — small, fast, cheap (e.g., `all-MiniLM-L6-v2`)
- 768 dimensions — medium (e.g., `nomic-embed-text`)
- 1024 dimensions — larger, better quality
- 1536 dimensions — `text-embedding-3-small` (OpenAI)
- 3072 dimensions — `text-embedding-3-large` (OpenAI)

More dimensions does not automatically mean better quality. A well-trained 768-dimension model can outperform a poorly trained 3072-dimension model. Dimensions affect storage size and computation time for similarity search.

Higher dimensions also mean larger vector databases, more memory usage, and slower ANN index builds.

### Quality

The only reliable measure of quality is evaluation on your specific task and data. General benchmarks (MTEB) are useful starting points but don't always predict performance on domain-specific content.

For evaluating candidates:
1. Take 100-200 representative query/document pairs from your actual data
2. Embed with each candidate model
3. Measure recall@k: for each query, does the correct document appear in the top-k results?
4. Pick the model with the best recall@k at your target k

MTEB (Massive Text Embedding Benchmark) leaderboard at `huggingface.co/spaces/mteb/leaderboard` is the standard reference for comparing models across tasks.

### Current Models Worth Knowing (2025)

| Model | Dimensions | Notes |
|---|---|---|
| `text-embedding-3-small` (OpenAI) | 1536 | Cheap, solid quality. $0.02/M tokens. Default choice for OpenAI stack. |
| `text-embedding-3-large` (OpenAI) | 3072 | Higher quality, more expensive. $0.13/M tokens. |
| `voyage-3` (Voyage AI) | 1024 | Strong performance, especially on code and technical content. Recommended by Anthropic for use with Claude. |
| `voyage-3-lite` (Voyage AI) | 512 | Cheaper, faster version. Good for high-volume applications. |
| `nomic-embed-text` (open source) | 768 | Free, self-hostable. Competitive quality. |
| `bge-large-en-v1.5` (open source) | 1024 | Strong MTEB scores, free, self-hostable. |
| `all-MiniLM-L6-v2` (open source) | 384 | Tiny, fast, surprisingly good for general use. |

### Matryoshka Embeddings

Some modern models (including `text-embedding-3-small` and `nomic-embed-text`) support "matryoshka" truncation: you can truncate the vector to fewer dimensions and retain most of the quality. This lets you trade off storage/cost against quality dynamically.

```typescript
// OpenAI supports dimensions parameter
const embedding = await openai.embeddings.create({
  model: "text-embedding-3-small",
  input: "Hello world",
  dimensions: 256, // truncate from 1536 to 256
});
```

### Key Selection Factors

**API vs self-hosted**
- API (OpenAI, Voyage): no infrastructure to manage, pay per token, consistent latency, vendor dependency.
- Self-hosted (nomic, bge): pay once for compute, no per-token cost at scale, need to manage updates and infrastructure.

Break-even point for self-hosting: at high volume (hundreds of millions of tokens/month), self-hosting typically becomes cheaper. At lower volume, API wins on simplicity.

**Max input tokens** — embedding models have their own context limits. `text-embedding-3-small` supports 8191 tokens. Chunks larger than this must be truncated or handled specially. This is a key input to chunking strategy decisions.

**Consistency requirement** — you must embed queries and documents with the same model. Switching models requires re-embedding your entire corpus. Build in a model version field in your metadata from day one.

```typescript
// Always tag embeddings with the model that produced them
interface StoredEmbedding {
  id: string;
  textHash: string;       // to detect if source changed
  embeddingModel: string; // e.g., "text-embedding-3-small"
  dimensions: number;
  vector: number[];
  createdAt: Date;
}
```

**Multilingual** — if you need to support non-English content, select a multilingual model (`multilingual-e5-large`, `paraphrase-multilingual-mpnet-base-v2`). English-only models perform poorly on other languages.

### Cost Estimation

```
text-embedding-3-small: $0.02 per million input tokens

1 million tokens ≈ 750,000 words ≈ 1,500 pages of text

Indexing 10,000 documents of 500 tokens average:
  5,000,000 tokens × $0.02/M = $0.10

At query time (1 query = ~50 tokens):
  1,000,000 queries × 50 tokens = 50,000,000 tokens × $0.02/M = $1.00
```

Embedding is cheap relative to generative model costs. Optimize for quality and convenience first; cost is rarely the bottleneck for embedding.

---

## Summary

| Concept | Key Point |
|---|---|
| What embeddings are | Text → fixed-size vector where semantic similarity = geometric proximity |
| Why geometry encodes semantics | Learned from millions of (similar/dissimilar) text pairs; direction in space encodes meaning |
| Cosine similarity | Measures angle between vectors; scale-invariant; range -1 to 1; dot product when normalized |
| Why cosine vs Euclidean | Scale-invariant — same meaning at different lengths scores correctly |
| Implementation | Dot product divided by product of magnitudes; normalize first for speed |
| Use cases | Semantic search, RAG retrieval, clustering, classification, dedup |
| Model selection | Quality via MTEB + own evaluation; dimensions affect storage not just quality; tag embeddings with model version |
