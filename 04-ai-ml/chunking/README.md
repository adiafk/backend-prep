# Chunking

Chunking strategies are covered in depth in the RAG section. See [Chunking Strategies](../../06-rag/chunking-strategies/README.md) for complete coverage including fixed-size, sentence-based, recursive, semantic, and overlap strategies with examples and trade-off analysis.

## Quick Reference

Chunking is the process of splitting source documents into smaller units before embedding and indexing them in a vector store. The size and method of chunking directly controls retrieval quality: chunks that are too large dilute the embedding signal and consume excessive context window tokens; chunks that are too small lose surrounding context needed to answer questions.

The three main approaches are: **fixed-size chunking** (split every N tokens with M tokens of overlap — simple, fast, language-agnostic, but splits mid-sentence), **sentence or paragraph chunking** (split at natural linguistic boundaries — preserves coherence but produces variable-length chunks), and **semantic chunking** (split when the embedding similarity between adjacent sentences drops below a threshold — produces semantically coherent units at the cost of computational overhead during ingestion).

In practice, **recursive character splitting** (the LangChain default) balances these concerns: it tries to split on paragraphs, then sentences, then words, then characters, never exceeding the target chunk size. Overlap of 10–20% of the chunk size is standard to prevent answers that span chunk boundaries from being missed. Always measure chunk size in **tokens, not characters** — use `tiktoken` or the equivalent for your model so chunks reliably fit within the context window budget available for retrieval.

See also:
- [Tokenization](../tokenization/README.md) — why token-based chunking is required
- [Chunking Strategies](../../06-rag/chunking-strategies/README.md) — full implementation guide
