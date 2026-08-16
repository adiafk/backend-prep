# Advanced RAG

## Limitations of Basic RAG

Naive chunk → embed → retrieve → generate fails when:
- User asks vague questions ("tell me more about that")
- The answer requires combining multiple chunks
- Chunks are too large/small to be retrieved reliably
- The retriever fetches irrelevant chunks and the LLM hallucinates

---

## Query Transformation

Transform the user's query before retrieval to improve recall.

### HyDE (Hypothetical Document Embeddings)
Generate a *hypothetical answer* to the query, embed that answer, then retrieve using the hypothetical answer's embedding. This works because the hypothetical answer is semantically closer to the documents than the raw question.

```typescript
import { ChatOpenAI } from "@langchain/openai";
import { OpenAIEmbeddings } from "@langchain/openai";
import { PromptTemplate } from "@langchain/core/prompts";

const hydePrompt = PromptTemplate.fromTemplate(
  `Write a short paragraph that would answer the following question.
   Do not use phrases like "based on..." — write as if you know the answer.
   Question: {question}`
);

const llm = new ChatOpenAI({ model: "gpt-4o-mini" });
const embeddings = new OpenAIEmbeddings();

async function hydeRetrieve(question: string, vectorStore: VectorStore) {
  // 1. Generate hypothetical answer
  const chain = hydePrompt.pipe(llm);
  const hypotheticalAnswer = await chain.invoke({ question });

  // 2. Embed the hypothetical answer
  const queryEmbedding = await embeddings.embedQuery(
    hypotheticalAnswer.content as string
  );

  // 3. Retrieve using hypothetical embedding
  return vectorStore.similaritySearchVectorWithScore(queryEmbedding, 4);
}
```

### Multi-Query Retrieval
Generate N variants of the query, retrieve for each, and deduplicate results.

```typescript
const multiQueryPrompt = PromptTemplate.fromTemplate(
  `Generate {n} different versions of the following question to improve retrieval.
   Output one question per line, no numbering.
   Original: {question}`
);

async function multiQueryRetrieve(question: string, vectorStore: VectorStore, n = 3) {
  const chain = multiQueryPrompt.pipe(llm);
  const result = await chain.invoke({ question, n: n.toString() });
  const queries = (result.content as string).trim().split("\n").filter(Boolean);

  const allResults = await Promise.all(
    queries.map(q => vectorStore.similaritySearch(q, 3))
  );

  // Deduplicate by content
  const seen = new Set<string>();
  return allResults.flat().filter(doc => {
    if (seen.has(doc.pageContent)) return false;
    seen.add(doc.pageContent);
    return true;
  });
}
```

---

## Re-ranking

Retrieval returns candidates ranked by embedding similarity, which isn't always the same as relevance. A reranker re-scores with a cross-encoder model (looks at query + document together).

```typescript
import { CohereRerank } from "@langchain/cohere";

const reranker = new CohereRerank({
  apiKey: process.env.COHERE_API_KEY,
  model: "rerank-english-v3.0",
  topN: 3,  // keep top 3 after reranking
});

async function retrieveAndRerank(question: string, vectorStore: VectorStore) {
  // Retrieve more candidates than we need
  const candidates = await vectorStore.similaritySearch(question, 10);
  
  // Rerank and keep top 3
  const reranked = await reranker.compressDocuments(candidates, question);
  return reranked;
}
```

---

## Contextual Compression

Strip irrelevant content from retrieved chunks before sending to the LLM.

```typescript
import { LLMChainExtractor } from "langchain/retrievers/document_compressors";
import { ContextualCompressionRetriever } from "langchain/retrievers/contextual_compression";

const compressor = LLMChainExtractor.fromLLM(llm);

const compressionRetriever = new ContextualCompressionRetriever({
  baseCompressor: compressor,
  baseRetriever: vectorStore.asRetriever(6),
});

// Retriever now returns only the relevant sentence(s) from each chunk
const relevantPassages = await compressionRetriever.getRelevantDocuments(question);
```

---

## Self-RAG / Adaptive RAG

The system decides *whether* to retrieve at all, then checks whether retrieved docs are relevant.

```typescript
type RagDecision = "generate" | "retrieve" | "rewrite_query";

async function adaptiveRag(question: string, history: string): Promise<string> {
  // Step 1: Decide if retrieval is needed
  const routingDecision = await routingChain.invoke({ question, history });
  
  if (routingDecision === "generate") {
    // Question answerable from LLM knowledge alone
    return directGenerate(question);
  }

  // Step 2: Retrieve
  let docs = await vectorStore.similaritySearch(question, 5);
  
  // Step 3: Grade retrieved docs for relevance
  const relevantDocs = await Promise.all(
    docs.map(async (doc) => {
      const grade = await relevanceGrader.invoke({ question, document: doc.pageContent });
      return grade === "relevant" ? doc : null;
    })
  );
  const filteredDocs = relevantDocs.filter(Boolean);
  
  // Step 4: If no relevant docs, rewrite query and try again
  if (filteredDocs.length === 0) {
    const rewrittenQuery = await queryRewriter.invoke({ question });
    filteredDocs.push(...await vectorStore.similaritySearch(rewrittenQuery, 5));
  }
  
  // Step 5: Generate answer, check for hallucination
  const answer = await generateChain.invoke({ question, context: filteredDocs });
  const isGrounded = await hallucinationChecker.invoke({ answer, docs: filteredDocs });
  
  return isGrounded ? answer : "I could not find a reliable answer in the provided documents.";
}
```

---

## Metadata Filtering

Store structured metadata alongside embeddings and pre-filter before vector search.

```typescript
import { Document } from "@langchain/core/documents";

// Store documents with metadata
await vectorStore.addDocuments([
  new Document({
    pageContent: "PostgreSQL supports JSONB columns...",
    metadata: { source: "postgres-guide", section: "data-types", year: 2024 }
  })
]);

// Filter by metadata at retrieval time (PGVector example)
const results = await vectorStore.similaritySearch(question, 4, {
  filter: { source: "postgres-guide", year: { $gte: 2023 } }
});
```

---

## Evaluation

Don't ship a RAG system without evaluation:

| Metric | What it measures | Tool |
|--------|-----------------|------|
| Context Precision | Are retrieved chunks actually relevant? | RAGAS |
| Context Recall | Did retrieval find all necessary context? | RAGAS |
| Faithfulness | Is the answer grounded in the retrieved context? | RAGAS, TruLens |
| Answer Relevancy | Does the answer address the question? | RAGAS |

```bash
pip install ragas
# RAGAS needs: question, answer, contexts, ground_truth
```
