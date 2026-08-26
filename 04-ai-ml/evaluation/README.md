# Evaluation

## Why Evaluation Is Non-Negotiable

Without measurement you cannot distinguish a good model from a lucky one, a regression from noise, or an improvement from overfitting to your test set. Evaluation is the feedback loop that makes all other ML work meaningful.

The core discipline: **define your metric before you train or fine-tune anything**. Metrics chosen after the fact will be unconsciously selected to make your current model look good.

---

## Classification Metrics

### The Confusion Matrix

For binary classification with positive class P and negative class N:

|  | Predicted Positive | Predicted Negative |
|--|-------------------|--------------------|
| **Actually Positive** | TP (True Positive) | FN (False Negative) |
| **Actually Negative** | FP (False Positive) | TN (True Negative) |

Everything else derives from this four-cell table.

### Precision and Recall

```
Precision = TP / (TP + FP)   — of predictions that are positive, how many are correct?
Recall    = TP / (TP + FN)   — of actual positives, how many did we find?
F1        = 2 × (P × R) / (P + R)   — harmonic mean; penalizes extreme imbalance
```

**When precision matters more**: spam detection, fraud alerts, medical test ordering. A false positive wastes the user's time or triggers unnecessary action. You accept missing some true positives to avoid crying wolf.

**When recall matters more**: cancer screening, critical system failure detection, content moderation for illegal material. A false negative is catastrophic. You accept more false positives to ensure nothing bad is missed.

**When F1 is appropriate**: imbalanced classes where you care about both, and neither precision nor recall clearly dominates. Note that F1 treats precision and recall equally — if your cost structure is asymmetric, use F_beta with the appropriate beta.

```python
from sklearn.metrics import precision_score, recall_score, f1_score, confusion_matrix

y_true = [1, 0, 1, 1, 0, 1, 0, 0]
y_pred = [1, 0, 1, 0, 0, 1, 1, 0]

print(f"Precision: {precision_score(y_true, y_pred):.3f}")  # 0.667
print(f"Recall:    {recall_score(y_true, y_pred):.3f}")     # 0.750
print(f"F1:        {f1_score(y_true, y_pred):.3f}")         # 0.706
print(confusion_matrix(y_true, y_pred))
```

### ROC-AUC

ROC (Receiver Operating Characteristic) plots TPR (recall) vs. FPR (false positive rate) as the decision threshold varies. AUC (area under curve) summarizes this into a single number.

- **AUC = 1.0**: perfect classifier
- **AUC = 0.5**: no better than random
- **AUC = 0.8–0.9**: good; **AUC > 0.95**: excellent

ROC-AUC is **threshold-independent** — it measures discrimination ability across all possible operating points. Use it for comparing models.

However, ROC-AUC is optimistic on highly imbalanced datasets. With 1% positive class, a classifier that predicts all negatives has AUC ≈ 0.5 but 99% accuracy. Use **Precision-Recall AUC (PR-AUC)** for imbalanced problems — it is more informative when the positive class is rare.

### Multiclass Extension

For K classes, compute per-class precision/recall and aggregate:
- **Macro average**: mean across classes, unweighted — treats all classes equally regardless of size
- **Weighted average**: mean weighted by class frequency — useful when class sizes differ and overall accuracy matters

---

## Regression Metrics

### MAE vs. RMSE

```
MAE  = (1/n) Σ |y_i - ŷ_i|
RMSE = √((1/n) Σ (y_i - ŷ_i)²)
```

**MAE** (Mean Absolute Error): linear penalty. A 10-unit error is 2× worse than a 5-unit error. Robust to outliers. Easy to interpret ("average error is X units").

**RMSE** (Root Mean Squared Error): quadratic penalty. A 10-unit error is 4× worse than a 5-unit error. Sensitive to outliers. Use when large errors are disproportionately costly (e.g., price prediction where catastrophic over/under-estimates matter more than minor deviations).

**R² (coefficient of determination)**: proportion of variance in y explained by the model. R² = 1 is perfect; R² = 0 means the model does no better than predicting the mean; R² < 0 means the model is worse than predicting the mean (possible).

```python
from sklearn.metrics import mean_absolute_error, mean_squared_error, r2_score
import numpy as np

rmse = np.sqrt(mean_squared_error(y_true, y_pred))
```

---

## The Precision-Recall Tradeoff

For a classifier that outputs a probability score, you choose a **decision threshold** T: predict positive if score ≥ T.

```
As T decreases (predict positive more freely):
  → Recall increases (catch more true positives)
  → Precision decreases (accept more false positives)

As T increases (predict positive more strictly):
  → Precision increases (fewer false positives)
  → Recall decreases (miss more true positives)
```

The threshold is a business decision, not a modeling decision. A fraud detection model's optimal threshold depends on: the cost of a fraudulent transaction, the cost of a declined legitimate transaction, and your customer satisfaction SLA. Set it based on those numbers, not on "F1 is maximized."

```python
from sklearn.metrics import precision_recall_curve
import matplotlib.pyplot as plt

precisions, recalls, thresholds = precision_recall_curve(y_true, y_scores)

# Find threshold that achieves target recall of 0.90
target_recall = 0.90
idx = next(i for i, r in enumerate(recalls) if r <= target_recall)
operating_threshold = thresholds[idx]
operating_precision = precisions[idx]
print(f"At recall≥0.90: threshold={operating_threshold:.3f}, precision={operating_precision:.3f}")
```

---

## Cross-Validation

Never report a single train/test split as your final result — it has high variance depending on which examples land in each split.

### k-Fold Cross-Validation

Split the data into k folds. Train on k-1 folds, evaluate on the held-out fold. Rotate k times. Report mean ± std across folds.

```python
from sklearn.model_selection import cross_val_score

scores = cross_val_score(model, X, y, cv=5, scoring='f1')
print(f"F1: {scores.mean():.3f} ± {scores.std():.3f}")
```

**k = 5 or 10** is standard. Smaller k = faster, higher bias; larger k = slower, lower bias.

### Stratified k-Fold

For imbalanced classes, plain k-fold can produce folds with no positive examples. Stratified k-fold maintains the class ratio in each fold.

```python
from sklearn.model_selection import StratifiedKFold

skf = StratifiedKFold(n_splits=5, shuffle=True, random_state=42)
for train_idx, val_idx in skf.split(X, y):
    # each fold has same class ratio as full dataset
    pass
```

Always use stratified splits for classification unless classes are perfectly balanced.

---

## Train / Validation / Test Split Discipline

This is the most commonly violated rule in ML, including by experienced practitioners:

**The contract:**
- **Train set**: the model sees this data during training
- **Validation set**: used to tune hyperparameters, select architecture, choose when to stop training — the model does not train on it, but your *decisions* are influenced by it
- **Test set**: touched exactly once, at the end, to report final performance — every time you look at it, you are implicitly leaking information

**The violation**: running a model on the test set, observing it performs poorly, tweaking the model, running it again. You have now implicitly trained on the test set. Your reported test performance is an overestimate.

Typical splits:
- 80/10/10 for large datasets (>100K examples)
- 70/15/15 for medium datasets
- For small datasets (<5K): use cross-validation; hold out a strict final test set and do not touch it during development

---

## Overfitting Signals

Overfitting means the model has memorized training data rather than learned generalizable patterns.

**Diagnostic:**

```
epoch | train_loss | val_loss | train_acc | val_acc
----------------------------------------------------
  10  |   0.42     |  0.45    |   83%     |  82%    ← healthy gap
  20  |   0.28     |  0.38    |   89%     |  84%    ← slight divergence
  30  |   0.15     |  0.52    |   94%     |  80%    ← overfitting: val loss rising
  40  |   0.08     |  0.71    |   97%     |  77%    ← severe overfit
```

Signals:
- Validation loss increasing while training loss continues to decrease
- Large gap between training accuracy and validation accuracy
- Model performs perfectly on training set but poorly on held-out data

Mitigations: early stopping (stop when val loss stops improving), dropout, weight decay (L2 regularization), data augmentation, reduce model capacity, collect more training data.

---

## LLM-Specific Evaluation

Traditional ML metrics (accuracy, F1) often do not apply to LLM outputs, which are free-form text. LLM evaluation has its own framework.

### Four Core RAG Metrics

**Faithfulness**: does the generated answer assert only things supported by the retrieved context? An answer that introduces facts not present in the context is hallucinating.

**Answer Relevance**: does the answer address what the question is asking? A retrieved context can be correct and relevant, but the answer might wander off-topic or answer a different question.

**Context Recall**: did the retriever surface all the chunks needed to answer the question? Measures retrieval completeness.

**Context Precision**: are the retrieved chunks actually useful? A retriever that returns 10 chunks, 9 of which are irrelevant, wastes context window space and confuses the LLM.

```
                High Context Precision    Low Context Precision
High Context    Ideal: retriever finds    Retriever finds needed
Recall          exactly what's needed     chunks plus lots of noise

Low Context     Retriever returns clean   Bad all around:
Recall          but incomplete results    missing and noisy
```

### RAGAS: Automated RAG Evaluation

RAGAS uses an LLM-as-judge to score these four metrics without requiring manual annotation.

```python
from ragas import evaluate
from ragas.metrics import faithfulness, answer_relevancy, context_recall, context_precision
from datasets import Dataset

# Your RAG pipeline output
data = {
    "question": ["What is the capital of France?"],
    "answer": ["The capital of France is Paris."],
    "contexts": [["France is a country in Western Europe. Its capital city is Paris."]],
    "ground_truth": ["Paris"]
}

dataset = Dataset.from_dict(data)
results = evaluate(dataset, metrics=[faithfulness, answer_relevancy, context_recall, context_precision])
print(results)
# {'faithfulness': 1.0, 'answer_relevancy': 0.97, 'context_recall': 1.0, 'context_precision': 1.0}
```

---

## Human Evaluation vs. LLM-as-Judge

**LLM-as-judge** uses a capable LLM (often GPT-4o or Claude) to score model outputs on dimensions like quality, helpfulness, and correctness. It is scalable and cheap.

**Known biases of LLM-as-judge:**

| Bias | Description | Mitigation |
|------|-------------|------------|
| Verbosity preference | Longer answers rated higher regardless of quality | Include length in rubric; penalize padding |
| Position bias | First option in a pairwise comparison rated higher | Swap order and average scores |
| Self-preference | Model rates its own outputs higher | Use a different model as judge |
| Style bias | Answers matching judge's writing style rated higher | Use rubric with explicit criteria |

**When to use LLM-as-judge**: regression testing (did a code change hurt quality?), large-scale offline evaluation, development-time iteration.

**When to use human evaluation**: before any production launch, for safety-critical decisions, when calibrating whether LLM-as-judge correlates with human preference in your specific domain.

**Gold standard**: run both. Compute the correlation between LLM-as-judge scores and human scores on a sample. If correlation is high (Spearman ρ > 0.7), LLM-as-judge is a valid proxy for your use case.

---

## A/B Testing for AI Features

Offline evaluation tells you the model is better on your benchmark. A/B testing tells you users prefer it.

**What to measure beyond task accuracy:**
- **Task completion rate**: did the user accomplish what they came to do?
- **User satisfaction**: explicit thumbs-up/down, session abandonment, return visits
- **Latency impact**: a more accurate model that is 2× slower may lose on user satisfaction
- **Edge case frequency**: how often does the new model fail in qualitatively bad ways?

**Goodhart's Law in AI systems**: "When a measure becomes a target, it ceases to be a good measure." If you optimize purely for thumbs-up rate, models learn to give agreeable answers rather than correct ones. Always pair quantitative metrics with qualitative review of a sample of outputs.

**Sample size**: use a power analysis to determine how many users you need in each arm before starting. Running the test until you see significance is p-hacking; decide the sample size upfront.

---

## Hallucination Detection

Hallucination (generating false but plausible-sounding information) is the failure mode with the highest reputational risk.

**Approaches:**

1. **Faithfulness scoring (RAGAS/TruLens)**: score each factual claim in the answer against the retrieved context. Claims not supported by context are potential hallucinations.

2. **Self-consistency**: generate the same response N times with temperature > 0. Facts that appear in most responses are more likely true; inconsistent facts are suspect.

3. **Citation grounding**: require the model to cite the source chunk for each claim. Check that the claim is supported by the cited chunk programmatically or via a secondary LLM.

4. **Entailment model**: use a natural language inference (NLI) model to check if the context entails each claim in the answer.

---

## Benchmark Contamination

A benchmark is only valid if the model was not trained on it. As models are trained on increasingly large internet crawls, the risk that evaluation benchmarks appear in training data is significant.

Signs of contamination:
- Model performance on a benchmark far exceeds performance on similar real-world tasks
- Model can complete evaluation examples verbatim without being prompted to
- Unusual improvement on a benchmark after a model update that didn't change relevant capabilities

For production evaluation, use **held-out private test sets** generated from your own data. Public benchmarks are useful for understanding a model's general capabilities but should not be used as the sole evaluation signal for your specific use case.

---

## Interview Q&A

**Q: What is the difference between precision and recall, and when does each matter more?**
A: Precision is the fraction of positive predictions that are actually positive (avoid false alarms). Recall is the fraction of actual positives that were predicted positive (avoid missing anything). Precision matters more when false positives are costly — spam filters, fraud alerts. Recall matters more when false negatives are costly — cancer screening, security threat detection. F1 is used when both matter and neither clearly dominates.

**Q: What is the difference between validation and test sets?**
A: The validation set is used to tune hyperparameters and make modeling decisions — the model does not train on it, but the human does. The test set is touched exactly once to report final performance. Every time you look at test set results and change anything, you have effectively trained on it. Test set performance becomes an overestimate.

**Q: Explain faithfulness in the context of RAG evaluation.**
A: Faithfulness measures whether the generated answer contains only claims that are supported by the retrieved context. An unfaithful answer introduces facts not present in the context — this is hallucination. A faithful answer that says "I don't know" when the context doesn't contain the answer is better than an unfaithful answer that invents plausible-sounding information.

**Q: What are the known biases of LLM-as-judge?**
A: Verbosity bias (prefers longer answers), position bias (favors answers presented first in pairwise comparison), self-preference (rates its own style higher), and style bias (prefers answers that match the judge's writing conventions). Mitigations: randomize presentation order, use a different model than the one being evaluated, write explicit scoring rubrics, calibrate against human judgments.

**Q: What is Goodhart's Law and why does it matter for AI systems?**
A: When a measure becomes a target, it ceases to be a good measure. For AI: if you optimize purely for a proxy metric (thumbs-up rate, BLEU score, accuracy on a benchmark), the model learns to game the proxy rather than actually improve at the underlying task. Always pair quantitative metrics with qualitative review and A/B tests measuring user behavior, not just model scores.

---

## Related Topics

- [AI/ML Fundamentals](../fundamentals/README.md) — model training basics
- [RAG Evaluation](../../06-rag/evaluation/README.md) — end-to-end evaluation for retrieval systems
