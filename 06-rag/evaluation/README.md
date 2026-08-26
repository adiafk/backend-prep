# RAG Evaluation

## Why Evaluation Is Hard

RAG has more failure modes than a standard ML model. The answer can be wrong because:
1. The retriever didn't fetch the relevant chunk (retrieval failure)
2. The retriever fetched it but it was ranked too low (ranking failure)
3. The retrieved context was correct but the LLM ignored it (faithfulness failure)
4. The LLM answered correctly but with irrelevant information added (hallucination)
5. The answer addressed a different question than what was asked (relevance failure)

You need metrics for each layer independently so you can debug which part broke.

---

## RAGAS Metrics

[RAGAS](https://docs.ragas.io) is the standard evaluation framework for RAG. It provides four complementary metrics that together cover the full pipeline.

### 1. Faithfulness

**Question**: Is every claim in the generated answer supported by the retrieved context?

**Process**:
1. Decompose the answer into atomic statements: "The capital is Paris." → statement: "The capital is Paris."
2. For each statement, ask an LLM: "Is this statement supported by the following context?"
3. Faithfulness = (supported statements) / (total statements)

Score range: 0–1. A faithfulness of 0.9 means 10% of answer claims are hallucinated (not grounded in retrieved context).

Low faithfulness → LLM is hallucinating or ignoring retrieved context. Fix: stronger grounding prompt, reduce max_tokens to prevent elaboration, or use a model that follows instructions better.

### 2. Answer Relevance

**Question**: Does the answer actually address the question asked?

**Process**: Reverse-generate N questions from the answer using an LLM. Embed each generated question and the original question. Answer relevance = mean cosine similarity between generated questions and the original question.

The intuition: if the answer is on-topic, questions generated from it should be similar to the original question. If the answer drifts, the generated questions diverge.

Score range: 0–1. Low score → answer is on a tangential topic or the question was mis-interpreted.

### 3. Context Precision

**Question**: Are the retrieved chunks ranked well? Do useful chunks appear first?

**Process**: For each retrieved chunk, an LLM judges whether it's relevant to the question. Context precision is a mean-precision-at-k:

```
context_precision = (1/K) * Σ (precision@k * relevance_k)
```

Where `relevance_k = 1` if chunk k is relevant, 0 otherwise.

Low context precision → relevant chunks exist in the retrieved set but are buried behind irrelevant ones. Fix: reranker, better BM25/dense weights, or reduce top_k.

### 4. Context Recall

**Question**: Does the retrieved context contain all information needed to answer the question?

**Process**: Decompose the ground-truth answer into statements. For each statement, check if it can be attributed to the retrieved context.

```
context_recall = (statements attributable to context) / (total ground-truth statements)
```

Requires a reference answer (ground truth). Low recall → relevant documents weren't retrieved at all. Fix: expand top_k, improve embeddings, add hybrid search.

---

## Computing RAGAS in TypeScript

```typescript
import { evaluate } from "ragas";  // Python; for TS, call via subprocess or REST
// In practice, RAGAS is a Python library. Wrap it as a service:

interface RAGASInput {
  question: string;
  answer: string;
  contexts: string[];    // retrieved chunks
  ground_truth?: string; // needed for context recall
}

interface RAGASScores {
  faithfulness: number;
  answer_relevancy: number;
  context_precision: number;
  context_recall?: number;  // only if ground_truth provided
}

// Call your Python RAGAS evaluation service
async function evaluateWithRAGAS(samples: RAGASInput[]): Promise<RAGASScores[]> {
  const response = await fetch(`${process.env.EVAL_SERVICE_URL}/evaluate`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ samples }),
  });
  if (!response.ok) throw new Error(`Eval service error: ${response.status}`);
  return response.json();
}
```

---

## LLM-as-Judge

When you don't want to build a full RAGAS pipeline, or you have custom criteria (tone, citation format, safety), use an LLM as evaluator directly.

```typescript
import Anthropic from "@anthropic-ai/sdk";

const anthropic = new Anthropic();

interface JudgeInput {
  question: string;
  answer: string;
  context: string;
}

interface JudgeResult {
  score: number;     // 1-5
  reasoning: string;
}

async function judgeAnswer(input: JudgeInput): Promise<JudgeResult> {
  const response = await anthropic.messages.create({
    model: "claude-sonnet-4-6",
    max_tokens: 512,
    messages: [
      {
        role: "user",
        content: `You are evaluating a RAG system's answer.

Question: ${input.question}

Retrieved context:
${input.context}

Generated answer:
${input.answer}

Score the answer on a scale of 1-5:
1 = Completely wrong or hallucinated
2 = Partially correct but major errors
3 = Mostly correct with minor issues
4 = Correct and grounded in context
5 = Correct, well-grounded, and complete

Respond with JSON: {"score": <number>, "reasoning": "<brief explanation>"}`,
      },
    ],
  });

  const text = response.content[0].type === "text" ? response.content[0].text : "";
  return JSON.parse(text);
}

// Run 3 judges, average to reduce variance
async function robustJudge(input: JudgeInput): Promise<number> {
  const [r1, r2, r3] = await Promise.all([
    judgeAnswer(input),
    judgeAnswer(input),
    judgeAnswer(input),
  ]);
  return (r1.score + r2.score + r3.score) / 3;
}
```

**Biases to watch for**:
- **Verbosity bias**: LLM judges prefer longer answers even when shorter is correct
- **Self-preference**: Claude judges may prefer Claude-style answers; GPT judges prefer GPT-style
- **Position bias**: answers placed first in a comparison get higher scores

Mitigate by: averaging multiple judge calls, randomizing answer order in comparisons, using a different model family as judge than the generator.

---

## Building a Golden Evaluation Dataset

A golden dataset is the foundation of offline evaluation. Without it, you're flying blind when you change chunking strategies or swap embedding models.

**What it contains**: (question, relevant context chunks, reference answer) triples.

**Minimum size**: 50–100 examples. Below 50, sampling variance dominates and score differences aren't meaningful.

**Stratify across**:
- Query types: factual lookup, comparison, multi-hop reasoning, summarization
- Domain areas: cover all major topics in your corpus proportionally
- Difficulty: easy (answer in one chunk), medium (two chunks needed), hard (requires inference)
- Query length: short keywords vs. full sentences

**Collection methods**:
1. **Human annotation** (highest quality): domain experts write questions from your corpus, annotate relevant chunks, write reference answers
2. **Synthetic generation** (fast, scalable): use an LLM to generate (question, answer) pairs from chunks, have humans spot-check 20%
3. **User query mining** (realistic): if you have production traffic, mine real user questions that received thumbs-up feedback

**Maintenance**: re-validate when the corpus changes significantly. A reference answer that was correct for v1 of a document may be wrong after an update.

```typescript
interface GoldenExample {
  id: string;
  question: string;
  relevant_chunk_ids: string[];   // ground-truth relevant chunks
  reference_answer: string;
  difficulty: "easy" | "medium" | "hard";
  query_type: "factual" | "comparison" | "multi_hop" | "summarization";
  created_at: string;
  created_by: string;
}
```

---

## Offline vs. Online Evaluation

### Offline Evaluation

Run before deployment. Batch evaluate your golden set against the new system version.

```typescript
interface OfflineEvalResult {
  version: string;
  timestamp: string;
  metrics: {
    faithfulness_mean: number;
    answer_relevancy_mean: number;
    context_precision_mean: number;
    context_recall_mean: number;
    retrieval_recall_at_3: number;
    retrieval_recall_at_5: number;
    latency_p50_ms: number;
    latency_p95_ms: number;
  };
  per_example: Array<{
    example_id: string;
    passed: boolean;
    scores: Record<string, number>;
  }>;
}

async function runOfflineEval(
  goldenSet: GoldenExample[],
  version: string
): Promise<OfflineEvalResult> {
  const results = await Promise.all(
    goldenSet.map(async (example) => {
      const start = Date.now();
      const { answer, contexts } = await runRAGPipeline(example.question);
      const latency = Date.now() - start;

      const scores = await evaluateWithRAGAS([{
        question: example.question,
        answer,
        contexts,
        ground_truth: example.reference_answer,
      }]);

      return { example, answer, contexts, scores: scores[0], latency };
    })
  );

  // Aggregate metrics
  const mean = (arr: number[]) => arr.reduce((a, b) => a + b, 0) / arr.length;
  return {
    version,
    timestamp: new Date().toISOString(),
    metrics: {
      faithfulness_mean: mean(results.map((r) => r.scores.faithfulness)),
      answer_relevancy_mean: mean(results.map((r) => r.scores.answer_relevancy)),
      context_precision_mean: mean(results.map((r) => r.scores.context_precision)),
      context_recall_mean: mean(results.map((r) => r.scores.context_recall ?? 0)),
      retrieval_recall_at_3: computeRetrievalRecall(results, 3),
      retrieval_recall_at_5: computeRetrievalRecall(results, 5),
      latency_p50_ms: percentile(results.map((r) => r.latency), 50),
      latency_p95_ms: percentile(results.map((r) => r.latency), 95),
    },
    per_example: results.map((r) => ({
      example_id: r.example.id,
      passed: r.scores.faithfulness > 0.8 && r.scores.answer_relevancy > 0.7,
      scores: r.scores as unknown as Record<string, number>,
    })),
  };
}
```

**Deployment gate**: require that no metric regresses more than 2% from baseline before deploying a change (chunking strategy, embedding model, reranker, prompt).

### Online Evaluation

Run in production. Tracks real user behavior.

| Signal | What it measures | How to collect |
|---|---|---|
| Thumbs up / down | Explicit answer quality | UI component |
| Message rephrasing rate | User confusion / answer failure | Track if user restates same question |
| Conversation end rate | Session-level satisfaction (high rate after short exchange = bad answer) | Session analytics |
| Citation click-through | Trust in sources | Track source link clicks |
| Follow-up question rate | Answer incompleteness | Track within-session question patterns |

Shadow traffic: before full rollout, run new system in parallel with old one (don't show to user), compare internal metrics.

A/B testing: split traffic, measure user satisfaction metrics across cohorts. Run for at least 1 week to avoid day-of-week confounds.

---

## Production Metrics to Track

Track these metrics broken down by stage so you know where time is spent:

```
total_latency = embed_latency + retrieve_latency + rerank_latency + generate_latency
```

```typescript
interface RAGTrace {
  request_id: string;
  question: string;
  timestamp: string;

  // Stage latencies (ms)
  embed_latency_ms: number;
  retrieve_latency_ms: number;
  rerank_latency_ms?: number;
  generate_latency_ms: number;
  total_latency_ms: number;

  // Retrieval quality
  chunks_retrieved: number;
  chunks_after_rerank?: number;

  // Generation quality (if online eval)
  tokens_generated: number;
  finish_reason: string;

  // User feedback (async, filled in later)
  user_rating?: 1 | -1;
  rephrased_within_3_turns?: boolean;
}
```

**SLO targets** (adjust to your application):
- TTFT (time to first token) P95: < 1.5s
- Total latency P95: < 8s
- Retrieval recall@3 on golden set: > 0.85
- Faithfulness on sampled production answers: > 0.80
- Answer acceptance rate (thumbs up): > 0.75

---

## Regression Testing When Changing Components

When you change chunking strategy, embedding model, or reranker, run the full golden set and compare:

```typescript
async function compareVersions(
  baseline: OfflineEvalResult,
  candidate: OfflineEvalResult,
  threshold = 0.02  // 2% regression threshold
): Promise<{ passed: boolean; regressions: string[] }> {
  const regressions: string[] = [];
  const metrics = Object.keys(baseline.metrics) as Array<keyof typeof baseline.metrics>;

  for (const metric of metrics) {
    const baseVal = baseline.metrics[metric];
    const candVal = candidate.metrics[metric];
    const isLatency = metric.includes("latency");

    // For latency: regression is increase; for quality: regression is decrease
    const regressed = isLatency
      ? candVal > baseVal * (1 + threshold)
      : candVal < baseVal * (1 - threshold);

    if (regressed) {
      regressions.push(
        `${metric}: ${baseVal.toFixed(3)} → ${candVal.toFixed(3)} (${isLatency ? "slower" : "worse"})`
      );
    }
  }

  return { passed: regressions.length === 0, regressions };
}
```

**Workflow**:
1. Commit change to feature branch
2. CI runs offline eval against golden set
3. Compare to main branch baseline
4. Block merge if any metric regresses > threshold
5. Manual review for borderline cases (e.g., quality improves but latency regresses)

---

## TruLens and DeepEval

**TruLens** (`trulens-eval`): wraps your RAG pipeline with instrumentation. Records each step (retrieval, generation) as a trace. Provides a web dashboard. Computes RAGAS-equivalent metrics (called "TruLens Feedback Functions") using LLM judges. Best when you want trace-level visibility into individual calls.

**DeepEval** (`deepeval`): similar scope to RAGAS. More test-framework oriented — integrates with pytest. Provides `assert_test()` for CI integration. Has more metric implementations (G-Eval, hallucination detection, toxicity). Good choice if your team already uses pytest.

Both automate the LLM judge calls and score aggregation. Neither eliminates the need for a golden dataset — they still need ground-truth examples to compute recall metrics.

---

## Goodhart's Law Warning

> When a metric becomes a target, it ceases to be a good metric.

In RAG evaluation this plays out as:
- Optimizing faithfulness → model learns to give short, hedged answers that copy chunks verbatim (high faithfulness, low answer quality)
- Optimizing thumbs-up rate → UI placement of feedback button matters more than answer quality
- Optimizing retrieval recall@5 → increasing top_k improves recall trivially but hurts precision and increases latency

**Safeguard**: track a portfolio of metrics, not a single number. When one metric shoots up dramatically, investigate whether the system actually improved or whether you've found a way to game it. Use human evaluation as a sanity check on automated metrics periodically (monthly or on major changes).

---

## Interview Q&A

**Q: What's the difference between context precision and context recall?**

Context precision measures whether the right chunks are ranked at the top of what you retrieved. Context recall measures whether you retrieved the right chunks at all. You can have high precision (top-3 results are all relevant) but low recall (relevant chunk #4 was never retrieved). Fix for low precision: reranking. Fix for low recall: expand top_k, improve embeddings, add hybrid search.

**Q: How do you evaluate a RAG system if you have no labeled data?**

You can generate synthetic labeled data: for each chunk in your corpus, use an LLM to generate a question that the chunk answers and a reference answer. Human spot-check 20% of examples. This gives you a labeled dataset in hours vs. weeks for manual annotation. Quality is lower but sufficient for detecting regressions on model/infrastructure changes.

**Q: What does a faithfulness score of 0.6 tell you?**

40% of statements in generated answers are not supported by the retrieved context — the model is hallucinating or extrapolating beyond what was retrieved. The fix is usually prompt engineering (stronger grounding instruction: "Answer only based on the provided context. If the answer is not in the context, say so.") or reducing max_tokens to limit elaboration.

**Q: How do you handle evaluation when your corpus changes frequently?**

Decouple your golden dataset from the corpus content. Questions should be timeless ("What is the refund policy?") rather than content-specific ("What is the refund policy as of March 2024?"). When the corpus changes and a reference answer becomes stale, flag that example for re-annotation rather than re-annotating the entire set. Track which examples are stale using `corpus_version` metadata.

**Q: What's a practical minimum for a golden evaluation set?**

50 examples is the floor. Below that, a single example's score change can swing the mean enough to look like a real signal. 100–200 examples provides meaningful statistical power for detecting 2–3% metric changes. For stratified evaluation (by query type, difficulty), you need 20+ examples per stratum.

---

## Related

- [../basic-rag/README.md](../basic-rag/README.md) — RAG pipeline to evaluate
- [../advanced-rag/README.md](../advanced-rag/README.md) — reranking and query rewriting that evaluation drives
- [../../04-ai-ml/evaluation/README.md](../../04-ai-ml/evaluation/README.md) — general ML evaluation concepts
