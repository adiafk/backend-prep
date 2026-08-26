# Tokenization

## What Tokenization Is

Tokenization converts raw text into a sequence of integer IDs drawn from a fixed vocabulary. It is not splitting on words, and it is not splitting on characters — it occupies a middle ground called **subword tokenization**.

Given a vocabulary of size V (e.g., 100,000), each token is an integer ID in `[0, V-1]`. The model's embedding layer converts those IDs into dense vectors. Everything downstream operates on these vectors, never on strings.

```
"Hello world" → ["Hello", " world"] → [9906, 1917]
```

The vocabulary is **fixed at training time**. Any text you feed the model at inference time must be representable as a sequence of tokens from that vocabulary. Unknown characters are handled by decomposing into known subword units or byte fallbacks — there is no `<UNK>` in modern tokenizers.

---

## Why Tokenization Matters Operationally

**Cost = tokens.** OpenAI, Anthropic, and every major provider charge per token — both input and output. A 10,000-character document is not 10,000 tokens; it is typically 2,500–4,000 depending on language and content type.

**Context window = tokens.** A model with a 128K context window can process 128,000 tokens, not characters. If you miscalculate token counts when building RAG pipelines, you either truncate context silently or hit API errors.

**Latency = output tokens.** Time-to-completion scales linearly with output tokens. Telling a model to "be concise" is a cost and latency optimization.

The practical consequence: always measure inputs and outputs in tokens, not characters or words.

---

## BPE: Byte Pair Encoding

BPE is the algorithm behind GPT-2, GPT-3, GPT-4, and most modern LLMs.

**Algorithm:**

1. Start with a character-level vocabulary (every byte or unicode character).
2. Count all adjacent symbol pairs in the training corpus.
3. Merge the most frequent pair into a single new symbol.
4. Repeat until the vocabulary reaches the target size.

**Concrete example:**

```
Corpus: "low low low lower lowest"
Initial vocab: {l, o, w, e, r, s, t, ' '}

Step 1: Most frequent pair is (l, o) → merge to "lo"
Step 2: Most frequent pair is (lo, w) → merge to "low"
Step 3: Most frequent pair is (low, ' ') → merge to "low "
...
```

After training, "lower" might tokenize as `["low", "er"]` and "lowest" as `["low", "est"]`. The subword units capture morphological patterns without requiring the full word to appear in training data.

**Why subwords handle rare words:** The word "photosynthesis" may never appear in training, but "photo", "synth", "esis" do. BPE can represent it via those subwords. This is strictly better than word-level tokenizers (which need `<UNK>`) and strictly more efficient than character-level (which produces very long sequences).

---

## WordPiece

Used in BERT and its derivatives. Conceptually similar to BPE but the merge criterion differs: instead of merging the most frequent pair, WordPiece merges the pair that maximizes the likelihood of the training corpus given the vocabulary.

Key distinguishing feature: subword continuations are prefixed with `##`.

```
"playing" → ["play", "##ing"]
"unplayable" → ["un", "##play", "##able"]
```

WordPiece tokenization is **greedy longest-match**: given the vocabulary, find the longest token that matches the start of the remaining string, emit it, advance, repeat. This is deterministic and fast at inference time.

---

## SentencePiece

Developed at Google, used in LLaMA, Gemma, T5, and many multilingual models.

Key differences from BPE/WordPiece:
- **Language-agnostic**: trains directly on raw text without pre-tokenization. Handles Chinese, Japanese, Arabic, and languages with no whitespace boundaries.
- **Whitespace as a token**: represents the space before a word as a special `▁` prefix: `"Hello world"` → `["▁Hello", "▁world"]`. This makes tokenization reversible without needing to track word boundaries separately.
- **Supports both BPE and unigram language model** as the subword algorithm underneath.

This approach is critical for multilingual models because the space-before-word convention is embedded in the token itself, not assumed from splitting on whitespace.

---

## tiktoken: Counting Tokens for OpenAI Models

OpenAI's `tiktoken` library implements the BPE tokenizer used by GPT models. Always use it to count tokens — character estimates are wrong.

```python
import tiktoken

# Load the encoding for a specific model
enc = tiktoken.encoding_for_model("gpt-4o")

text = "Hello, world! This is a tokenization example."
tokens = enc.encode(text)

print(f"Token count: {len(tokens)}")   # 10
print(f"Token IDs:   {tokens}")        # [9906, 11, 1917, 0, 1115, 374, 264, ...]
print(f"Decoded:     {enc.decode(tokens)}")  # "Hello, world! ..."

# Count tokens for a chat completion (including message overhead)
def count_chat_tokens(messages: list[dict], model: str = "gpt-4o") -> int:
    enc = tiktoken.encoding_for_model(model)
    # Each message has 3 overhead tokens (role, content, separator)
    tokens_per_message = 3
    total = 0
    for msg in messages:
        total += tokens_per_message
        for value in msg.values():
            total += len(enc.encode(value))
    total += 3  # reply priming
    return total

messages = [
    {"role": "system", "content": "You are a helpful assistant."},
    {"role": "user", "content": "What is tokenization?"},
]
print(count_chat_tokens(messages))  # ~24
```

For Anthropic models, use the Anthropic SDK's token counting endpoint directly:

```python
import anthropic

client = anthropic.Anthropic()
response = client.messages.count_tokens(
    model="claude-opus-4-5",
    messages=[{"role": "user", "content": "What is tokenization?"}],
)
print(response.input_tokens)
```

---

## Tokenization Surprises Interviewers Love

These gotchas expose whether a candidate truly understands the tokenization layer.

### Numbers tokenize poorly

Numbers are not a single token per digit. Multi-digit numbers may split in arbitrary, non-intuitive ways.

```python
enc = tiktoken.encoding_for_model("gpt-4o")

enc.encode("1234567890")
# → [4513, 2131, 22, 16708]  — 4 tokens, not 10
```

This is why LLMs struggle with arithmetic: "1234 + 5678" requires the model to first reconstruct the numbers from token fragments before it can reason about them.

### Same word, different tokens based on context

Leading whitespace changes the token ID. `"hello"` and `" hello"` are different tokens.

```python
enc.encode("hello")   # [15339]
enc.encode(" hello")  # [24748]
```

This matters for few-shot prompts: misaligned whitespace changes what the model sees and can affect generations.

### Emoji and non-ASCII inflate token count

```python
enc.encode("hello")   # 1 token
enc.encode("👋")       # 3 tokens (multi-byte UTF-8 encoded as byte-level BPE)
enc.encode("こんにちは")  # 15 tokens — Japanese is expensive
```

Building a multilingual RAG system? Your token budget for Japanese or Chinese content is roughly 3-4x what English would consume for the same semantic content.

### Code tokenizes efficiently

Programming languages, especially Python and JavaScript, appear heavily in LLM training data. Code keywords and common patterns are often single tokens.

```python
enc.encode("def __init__(self):")  # ~5 tokens — very efficient
enc.encode("import numpy as np")  # 4 tokens
```

This is why LLMs perform well at code — the tokenization is well-suited to the domain.

---

## Vocabulary Size Tradeoffs

| Model | Vocabulary Size | Notes |
|-------|----------------|-------|
| GPT-2 | 50,257 | Original BPE, byte-level |
| GPT-3 / GPT-4 | 100,277 | `cl100k_base` encoding |
| LLaMA 2 | 32,000 | SentencePiece BPE |
| LLaMA 3 | 128,000 | Dramatically expanded |
| Gemma | 256,000 | Large multilingual coverage |

**Larger vocabulary** → better coverage of common words as single tokens, more efficient encoding of text, fewer tokens per document. Cost: the embedding matrix grows (vocab_size × embedding_dim), increasing memory and parameters.

**Smaller vocabulary** → more tokens per document, longer sequences, higher attention cost (attention is O(n²) in sequence length). Benefit: smaller model size.

The trend is toward larger vocabularies because the embedding matrix is a small fraction of total model parameters, while reducing sequence length has large throughput benefits.

---

## Special Tokens

Special tokens are reserved IDs that signal structure to the model. They were part of training, so the model has learned to attend to them.

| Token | Common Name | Purpose |
|-------|-------------|---------|
| `<\|endoftext\|>` | EOS / end-of-sequence | Signals end of document or generation |
| `<\|begin_of_text\|>` | BOS | Marks start of input (LLaMA 3) |
| `[PAD]` | PAD | Pads batches to equal length; masked in attention |
| `[MASK]` | MASK | Replaced token in masked LM training (BERT) |
| `[SEP]` | SEP | Separates two sequences in BERT (e.g., question/context) |
| `[CLS]` | CLS | BERT's classification token; its embedding represents the full sequence |
| `<\|im_start\|>` / `<\|im_end\|>` | IM | ChatML format role delimiters (OpenAI fine-tunes) |

When building prompts programmatically, never inject these strings as raw text unless you intend them to be treated as special tokens. The tokenizer will usually handle them correctly, but it is worth verifying.

---

## Implications for RAG Systems

**Always chunk by token count, not character count.**

Character counts are a rough approximation. A 1,000-character English chunk is ~250 tokens. A 1,000-character Japanese chunk is ~750 tokens. If you chunk naively by character count, your Japanese documents will silently overflow context windows.

```python
import tiktoken

enc = tiktoken.encoding_for_model("gpt-4o")

def split_by_tokens(text: str, max_tokens: int, overlap_tokens: int = 50) -> list[str]:
    tokens = enc.encode(text)
    chunks = []
    start = 0
    while start < len(tokens):
        end = min(start + max_tokens, len(tokens))
        chunk_tokens = tokens[start:end]
        chunks.append(enc.decode(chunk_tokens))
        start += max_tokens - overlap_tokens
    return chunks
```

**Reserve tokens for the prompt template and response.**

If your context window is 128K tokens, your retrieval budget is less than 128K. Subtract: system prompt, user query, prompt template structure, expected output length. A safe formula:

```
retrieval_budget = context_window - system_prompt_tokens - query_tokens - template_overhead - max_output_tokens
```

---

## Interview Q&A

**Q: What is a token?**
A: An integer ID mapped to a subword unit from a fixed vocabulary. Not a word, not a character. The vocabulary and mapping are determined by training the tokenizer on a large text corpus using an algorithm like BPE.

**Q: Why does "1234" sometimes tokenize into multiple tokens?**
A: Numbers appear in too many combinations for each to get its own vocabulary entry. BPE merges frequent pairs, and numeric strings are relatively rare compared to common words. The tokenizer encodes them as sequences of subword units that happen to correspond to digit groups, which is why arithmetic is hard for LLMs — the number representation is fragmented.

**Q: How would you count tokens before making an API call?**
A: Use the provider's official tokenizer library — `tiktoken` for OpenAI models, the Anthropic SDK's `count_tokens` endpoint for Claude. Never use `len(text.split())` or `len(text) / 4` in production; these estimates are wrong for non-English text and code.

**Q: Why do tokenizers treat `"hello"` and `" hello"` as different tokens?**
A: Because they are different subword units in the vocabulary. During BPE training, "hello" following a newline or at document start differs from "hello" following a space within a sentence. This captures real distributional differences but can cause surprising behavior when whitespace is misaligned in few-shot prompts.

**Q: What happens when the model encounters a character not in the vocabulary?**
A: Modern tokenizers use byte-level BPE as a fallback. Every possible byte (0–255) is in the vocabulary, so any UTF-8 encoded text can always be represented — at the cost of more tokens per character. There is no `<UNK>` token.

---

## Related Topics

- [AI/ML Fundamentals](../fundamentals/README.md) — embeddings, transformers, attention
- [Chunking Strategies](../../06-rag/chunking-strategies/README.md) — token-aware chunking for RAG
