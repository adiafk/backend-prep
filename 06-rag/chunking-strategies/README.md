# Chunking Strategies

Chunking is the process of splitting source documents into smaller units for indexing. The choice of strategy and parameters has a larger effect on RAG quality than most practitioners expect. Poor chunking undermines even the best embedding models and vector stores.

The fundamental tension: **smaller chunks retrieve more precisely but provide less context; larger chunks provide more context but retrieve less precisely.** Every strategy is a navigation of this tradeoff.

---

## 1. Fixed-Size Chunking

Split the document into segments of exactly N characters (or tokens), with an optional overlap of M characters.

```python
def fixed_size_chunks(text: str, chunk_size: int = 500, overlap: int = 50) -> list[str]:
    chunks = []
    start = 0
    while start < len(text):
        end = start + chunk_size
        chunks.append(text[start:end])
        start += chunk_size - overlap  # step forward by (size - overlap)
    return chunks
```

### How it works

The text is treated as a flat stream of characters. Every chunk is `chunk_size` characters long (except possibly the last). The `overlap` parameter ensures that the boundary between two consecutive chunks is present in both, which reduces the chance that a sentence spanning a boundary ends up half in one chunk and half in the next.

With `chunk_size=500` and `overlap=50`, chunk boundaries occur every 450 characters. Characters 0–500 form chunk 1, characters 450–950 form chunk 2, and so on.

### When it is appropriate

Fixed-size chunking is appropriate when:

- The document is homogeneous and unstructured (a long narrative, a transcript with no headings)
- You need a simple baseline to establish before trying more sophisticated strategies
- The source material has already been cleaned and normalized so character count approximates token count reasonably well
- You are chunking at a preprocessing stage and will apply a smarter splitter at a later stage

### When to avoid it

Fixed-size chunking ignores all document structure. A 500-character boundary might fall:
- In the middle of a sentence
- Separating a table header from its rows
- Between a code comment and the function it documents
- Halfway through a list item

These boundary artifacts degrade retrieval quality because the resulting chunk is semantically incomplete. For structured documents, use recursive character splitting or semantic chunking instead.

---

## 2. Recursive Character Splitting

Split on a priority list of separators, falling back to the next separator when a split would produce a chunk larger than the target size.

```python
from langchain.text_splitter import RecursiveCharacterTextSplitter

splitter = RecursiveCharacterTextSplitter(
    chunk_size=500,
    chunk_overlap=50,
    separators=["\n\n", "\n", ". ", " ", ""],
)

chunks = splitter.split_text(document_text)
```

### How it works

The splitter attempts splits in priority order:

1. `"\n\n"` — paragraph boundaries. A chunk that fits within 500 characters is returned as-is.
2. If no paragraph break produces a correctly-sized chunk, try `"\n"` — line breaks.
3. If that fails, try `". "` — sentence boundaries.
4. If that fails, try `" "` — word boundaries.
5. If that fails, split at raw characters.

This means the splitter respects document structure as much as possible while still enforcing a size limit. Most chunks end at paragraph or sentence boundaries rather than mid-word.

### Good defaults

For general prose:
- `chunk_size=500` characters (approximately 125–150 tokens for English text)
- `chunk_overlap=50` characters (approximately 10% of chunk size)

For technical documentation with code:
- `chunk_size=1000` characters
- `chunk_overlap=100` characters
- Add `"```"` as a high-priority separator so code blocks are not split

The 50-character overlap is the standard starting point. It ensures that sentences near a chunk boundary are fully captured in at least one chunk. Too small an overlap and boundary sentences get cut; too large and you are duplicating significant context, inflating your index and adding noise.

### Why it outperforms fixed-size chunking

Given a document with paragraphs and sentences, recursive character splitting produces chunks that correspond to coherent units of thought. A retrieved chunk about "the refund policy for digital goods" will contain the full policy statement rather than trailing off mid-sentence. This makes the chunk more useful when injected into the LLM prompt.

### Separator list customization

The default separator list works for plain prose. Adjust it for your content:

- Markdown: prepend `"## "`, `"# "`, `"### "` to split on heading boundaries before paragraphs
- HTML (after stripping tags): the default works reasonably well
- CSV: use `"\n"` only, with `chunk_size` set to a multiple of the expected row length
- Legal documents: add `"SECTION "`, `"ARTICLE "` as high-priority separators

---

## 3. Semantic Chunking

Split based on changes in meaning rather than character count or punctuation.

```python
from langchain_experimental.text_splitter import SemanticChunker
from langchain_openai import OpenAIEmbeddings

embeddings = OpenAIEmbeddings()

splitter = SemanticChunker(
    embeddings=embeddings,
    breakpoint_threshold_type="percentile",   # or "standard_deviation", "interquartile"
    breakpoint_threshold_amount=95,            # split where similarity drops below 95th percentile
)

chunks = splitter.split_text(document_text)
```

### How it works

1. Split the document into sentences.
2. Embed each sentence (or a sliding window of N sentences for context).
3. Compute the cosine similarity between adjacent sentence embeddings.
4. Where similarity drops sharply — where the topic shifts — place a chunk boundary.
5. Return chunks defined by those boundaries.

The result: each chunk corresponds to a topically coherent section of text. A chunk about refund policies will not bleed into a chunk about shipping times just because they appear in adjacent paragraphs.

### When to use it

Semantic chunking is most valuable when:
- The document covers multiple distinct topics in sequence (an FAQ, a knowledge base article)
- Documents are long and heterogeneous
- Retrieval precision is more important than indexing speed

### Cost and speed

Semantic chunking requires embedding every sentence during indexing, which is expensive for large corpora. A 10,000-document corpus indexed with semantic chunking might take 10–50x longer than fixed-size chunking and cost proportionally more in embedding API calls.

For many production use cases, recursive character splitting with tuned parameters achieves 80–90% of the quality improvement of semantic chunking at a fraction of the cost.

### When it underperforms

Semantic chunking struggles with documents that are coherent but dense — academic papers, technical manuals, legal contracts. The similarity between adjacent sentences in these documents is uniformly high, and the splitter may produce very large chunks or fail to find meaningful boundaries.

---

## 4. Chunking for Code

Code requires a fundamentally different strategy. Prose splitters applied to code produce broken chunks that:
- Cut functions in half
- Separate a function signature from its body
- Remove the context needed to understand what a snippet does (the class it belongs to, the imports it uses)

### Strategy: AST-based splitting

Parse the code into an Abstract Syntax Tree and split at syntactic boundaries: function definitions, class definitions, method blocks.

```python
from langchain.text_splitter import Language, RecursiveCharacterTextSplitter

python_splitter = RecursiveCharacterTextSplitter.from_language(
    language=Language.PYTHON,
    chunk_size=2000,      # larger chunks for code — functions can be long
    chunk_overlap=200,
)

chunks = python_splitter.split_text(python_source_code)
```

LangChain's `Language` enum supports Python, JavaScript, TypeScript, Go, Ruby, Rust, C, C++, Java, Markdown, HTML, LaTeX, Solidity, and more. Each language has a custom separator list tuned to its syntax.

### Metadata enrichment for code

Code chunks benefit from rich metadata that prose chunks do not need:

```python
{
    "chunk_text": "def calculate_tax(income: float, rate: float) -> float:\n    ...",
    "file_path": "src/billing/tax.py",
    "function_name": "calculate_tax",
    "class_name": None,
    "language": "python",
    "line_start": 42,
    "line_end": 58,
    "imports": ["from decimal import Decimal"]
}
```

Including the function name, class name, and file path in the text that gets embedded (not just stored as metadata) substantially improves retrieval. A query for "how does calculate_tax work" will match the embedding of a chunk that starts with "def calculate_tax" far better than a chunk that starts with an arbitrary line number offset.

### Prepend context

For short functions, prepend the file path and class name to give context:

```
# File: src/billing/tax.py
# Class: BillingCalculator

def calculate_tax(income: float, rate: float) -> float:
    """
    Calculate federal tax liability.
    ...
    """
```

This technique — adding context to the chunk text before embedding — is a simplified version of Anthropic's Contextual Retrieval approach (described in advanced-rag).

### Chunk size for code

Use larger chunks for code than for prose. A `chunk_size=2000` is a reasonable default. Functions and methods are the natural unit of retrieval, and most functions fit within 2000 characters. Forcing smaller chunks splits function bodies across chunk boundaries, which defeats the purpose.

---

## 5. How Chunk Size Affects Retrieval Quality

Chunk size is the single parameter with the most impact on RAG quality. The tradeoffs are real and pull in opposite directions.

### Small chunks (100–300 characters)

**Advantages:**
- Embedding is dense and specific — a small chunk about one thing embeds clearly
- High retrieval precision — when you retrieve a chunk, it is highly likely to be directly relevant
- Lower cost per token when the retrieved context is assembled into the prompt

**Disadvantages:**
- Context fragmentation — a small chunk may not contain enough information to answer the question without adjacent chunks
- More embedding API calls during indexing
- Increased risk of boundary artifacts cutting sentences in half
- The model receives thin slices of context that require stitching across multiple retrieved chunks

### Large chunks (1000–2000 characters)

**Advantages:**
- Self-contained — a retrieved chunk is more likely to contain the full answer
- Fewer boundary artifacts
- Better for questions that require understanding relationships between ideas that appear in adjacent sentences

**Disadvantages:**
- Embedding is diluted — a large chunk covering multiple sub-topics embeds as the average of all of them, reducing retrieval precision
- Lower recall for specific sub-topics buried mid-chunk
- Larger prompt when assembled — you are paying for tokens the model will largely ignore
- The "lost in the middle" effect — information buried in a large chunk is less reliably used by the model

### The standard recommendation

For general prose: **500 characters with 50 characters of overlap** is the widely-cited default. This corresponds to roughly 100–125 tokens per chunk in English text — small enough to be specific, large enough to be self-contained for most factual questions.

For technical documentation or knowledge bases with section-level questions: **1000–1500 characters**.

For code: **2000 characters, split at function/class boundaries**.

### Tuning chunk size empirically

The right chunk size depends on your specific corpus and query distribution. The process:

1. Build a small golden evaluation set: 50–100 query/expected-answer pairs
2. Run retrieval with multiple chunk sizes (300, 500, 1000, 2000)
3. Measure recall@K: what fraction of expected answers are in the top-K retrieved chunks?
4. Pick the chunk size that maximizes recall@K on your evaluation set

Do not optimize for a single K. Test retrieval at K=3, K=5, and K=10 to understand how the tradeoff shifts.

### The interaction between chunk size and K

Small chunks with high K can approximate large chunks: retrieving 10 small chunks covering adjacent content gives the model similar material to 3 large chunks. But the similarity score distribution is noisier with small chunks, so some of those 10 retrieved small chunks may be from unrelated sections of the document.

Parent-child chunking (described in advanced-rag) resolves this tension: use small chunks for retrieval precision, then expand to the parent chunk for generation context.
