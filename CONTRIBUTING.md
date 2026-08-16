# Contributing Guide

This document explains how to contribute new topics, exercises, solutions, and notes to this repository. Read it before opening a pull request.

---

## Table of Contents

1. [Adding New Topics](#adding-new-topics)
2. [File and Folder Naming Conventions](#file-and-folder-naming-conventions)
3. [Code Style Requirements](#code-style-requirements)
4. [Documentation Standards](#documentation-standards)
5. [Pull Request Process](#pull-request-process)
6. [Review Criteria](#review-criteria)

---

## Adding New Topics

### Deciding Where a Topic Belongs

Before adding content, identify the correct section:

- Content that belongs in an existing section (e.g., a new PostgreSQL exercise) goes inside the relevant numbered folder.
- Content that spans multiple sections (e.g., a project combining auth and databases) goes in `14-projects/`.
- A genuinely new topic area that does not fit any existing section requires proposing a new section — open an issue first before creating a new top-level folder.

### Adding to an Existing Section

1. Check the section's `checklist.md` to confirm the topic is not already covered.
2. Add your notes to the section's `notes.md` under a clearly titled heading. Do not replace existing content — append under a new `###` subheading.
3. Place new exercises in `exercises/` with a descriptive filename (see naming conventions below).
4. If you are providing a reference solution, place it in `solutions/` with a matching filename.
5. Update `checklist.md` if your contribution adds a new verifiable milestone.
6. Update `resources.md` if your contribution references external material.

### Proposing a New Section

Open a GitHub issue with the title `[New Section] <topic-name>` and include:

- A one-paragraph justification for why this topic does not fit existing sections
- A proposed folder name following the `XX-topic-name` convention
- A draft outline of the `notes.md` structure
- At least three concrete exercise ideas

A maintainer will respond within 5 business days. Do not create the folder until the issue is approved.

---

## File and Folder Naming Conventions

### Section Folders

Top-level section folders use a two-digit prefix and lowercase kebab-case:

```
00-foundations/
01-http/
02-databases/
...
15-interview-prep/
```

Do not use spaces, uppercase letters, or underscores in folder names.

### Files Within a Section

Every section folder must contain these core files:

| File | Purpose |
|------|---------|
| `notes.md` | Concept explanations, diagrams, key takeaways |
| `exercises/` | Subdirectory of exercise starter files |
| `solutions/` | Subdirectory of reference solutions |
| `resources.md` | Curated reading/watching list for this section |
| `checklist.md` | Verifiable milestones for this section |

### Exercise Files

Exercise filenames follow this pattern: `<topic>-<descriptor>.<ext>`

Examples:
```
sliding-window-maximum.ts
postgres-window-functions.sql
raft-leader-election.py
jwt-refresh-rotation.ts
kafka-exactly-once.py
```

Rules:
- All lowercase, hyphen-separated
- No generic names like `exercise1.ts` or `practice.py`
- The filename should be descriptive enough that someone browsing the directory understands what the exercise covers without opening it
- Use the correct extension for the language (`.ts` for TypeScript, `.py` for Python, `.cpp` for C++, `.sql` for SQL)

### Solution Files

Solution files mirror the exercise filename exactly and live in the `solutions/` subdirectory:

```
exercises/sliding-window-maximum.ts    ->    solutions/sliding-window-maximum.ts
exercises/postgres-window-functions.sql ->   solutions/postgres-window-functions.sql
```

### Notes and Documentation Files

Notes files are always `notes.md` at the section root. Do not create multiple notes files per section. If a section grows large, use `###` subheadings within the single file rather than splitting into multiple files.

---

## Code Style Requirements

### TypeScript

- Use TypeScript strict mode (`"strict": true` in tsconfig)
- All functions must have explicit return type annotations
- No use of `any` — use `unknown` and narrow explicitly where necessary
- Use `const` by default; only use `let` when reassignment is genuinely needed
- Prefer named function declarations over anonymous arrow functions for top-level exports
- All exercises must export the primary function or class so Jest can import and test it
- Format with Prettier using the project config (run `npm run format` before committing)

Example of expected style:
```typescript
export function twoSum(nums: number[], target: number): number[] {
  const seen = new Map<number, number>();
  for (let i = 0; i < nums.length; i++) {
    const complement = target - nums[i];
    if (seen.has(complement)) {
      return [seen.get(complement)!, i];
    }
    seen.set(nums[i], i);
  }
  return [];
}
```

### Python

- Target Python 3.11+
- All functions must have type annotations (PEP 484)
- Use dataclasses or Pydantic models instead of plain dicts for structured data
- Follow PEP 8 for naming: `snake_case` for functions and variables, `PascalCase` for classes
- Use f-strings for string interpolation; do not use `.format()` or `%` formatting
- All exercises must include a `if __name__ == "__main__":` block demonstrating basic usage
- Format with `black` and lint with `ruff` before committing (run `make lint` from repo root)

Example of expected style:
```python
def max_sliding_window(nums: list[int], k: int) -> list[int]:
    from collections import deque
    result: list[int] = []
    dq: deque[int] = deque()
    for i, num in enumerate(nums):
        while dq and dq[0] < i - k + 1:
            dq.popleft()
        while dq and nums[dq[-1]] < num:
            dq.pop()
        dq.append(i)
        if i >= k - 1:
            result.append(nums[dq[0]])
    return result
```

### C++

- Target C++17 or later
- Use RAII — no raw `new`/`delete`; prefer `std::unique_ptr` and `std::shared_ptr`
- Use `const` and `constexpr` wherever applicable
- Prefer range-based for loops over index-based where semantics allow
- All public functions must have a doc comment explaining parameters and return value
- Compile with `-Wall -Wextra -Wno-unused-parameter -std=c++17` with zero warnings
- Format with `clang-format` using the project `.clang-format` config

### SQL

- All SQL must be valid PostgreSQL syntax
- Use uppercase for SQL keywords (`SELECT`, `FROM`, `WHERE`, `JOIN`)
- Use lowercase for identifiers (table names, column names)
- Qualify all column names with the table alias in multi-table queries
- Include a comment block at the top of each file explaining what the query demonstrates

Example:
```sql
-- Demonstrates: window functions, running totals, rank within partition
-- Table: orders(id, user_id, amount, created_at)

SELECT
  o.user_id,
  o.created_at,
  o.amount,
  SUM(o.amount) OVER (PARTITION BY o.user_id ORDER BY o.created_at) AS running_total,
  RANK() OVER (PARTITION BY o.user_id ORDER BY o.amount DESC) AS amount_rank
FROM orders o
ORDER BY o.user_id, o.created_at;
```

---

## Documentation Standards

### notes.md Structure

Every `notes.md` entry for a concept must follow this structure:

```markdown
### Concept Name

**What it is:** One or two sentences defining the concept precisely.

**Why it matters:** Why an engineer needs to understand this. What breaks if you get it wrong.

**How it works:** Mechanism explanation. Use bullet points for multi-step processes.
Diagrams are encouraged — use Mermaid for inline diagrams.

**Common misconceptions:**
- Misconception 1 and why it is wrong
- Misconception 2 and why it is wrong

**Worked example:** A concrete, minimal code snippet or scenario that demonstrates the concept.
```

Do not write "this section covers X" in notes. Get to the definition immediately.

### checklist.md Structure

Every checklist item must be:
- Verifiable by a third party without you explaining yourself
- Specific enough that "yes/no" can be determined by observation or demonstration
- An action ("Can explain...", "Can implement...", "Has completed...") not a state ("Understands X")

Acceptable:
```markdown
- [ ] Can implement JWT refresh token rotation with a Redis-backed revocation list
```

Not acceptable:
```markdown
- [ ] Understands JWT
- [ ] Knows about caching
```

### resources.md Structure

Each resource entry must include:
- Title and author/source
- One sentence explaining what it covers and why it is on the list
- Format tag: `[book]`, `[article]`, `[course]`, `[video]`, `[docs]`, `[paper]`
- A URL if publicly accessible

Example:
```markdown
- [book] **Designing Data-Intensive Applications** — Martin Kleppmann.
  The definitive reference for storage engines, replication, distributed transactions,
  and consistency models. Chapters 5-9 are required reading for system design interviews.

- [video] **CMU 15-445 Database Systems** — Andy Pavlo (YouTube).
  University-level lecture series covering query planning, buffer pools, B-trees,
  and concurrency control. Free and more rigorous than most courses.
  https://www.youtube.com/playlist?list=PLSE8ODhjZXjbohkNBWQs_otTrBTrjyohi
```

---

## Pull Request Process

### Before Opening a PR

1. Run the full test suite and confirm all tests pass:
   ```bash
   npm test
   pytest
   cd 14-dsa/cpp/build && ctest
   ```
2. Run formatters and linters:
   ```bash
   npm run format
   make lint
   ```
3. Self-review your diff. Verify that:
   - No solution files are accidentally committed to `exercises/`
   - No `.env` files, API keys, or credentials are included
   - All new files follow the naming conventions in this guide
   - New exercises have corresponding test files

### PR Title Format

Use the format: `[section-number] brief description`

Examples:
- `[02-databases] add PostgreSQL window function exercises`
- `[11-rag-systems] add HyDE retrieval implementation and notes`
- `[14-dsa] add monotonic stack problems with solutions`

Do not use vague titles like "updates", "fixes", or "added stuff".

### PR Description Template

```markdown
## What this adds

<!-- One paragraph describing what content is added and why -->

## Section(s) changed

<!-- List the folders modified -->

## Checklist

- [ ] All tests pass locally
- [ ] Formatters and linters run with no errors
- [ ] No credentials or API keys in the diff
- [ ] notes.md follows the documentation structure in CONTRIBUTING.md
- [ ] New exercises have test files
- [ ] checklist.md updated if new milestones are added
- [ ] resources.md updated if new external material is referenced
```

### Draft PRs

Open a draft PR if you want early feedback on direction before completing the work. Mark it ready for review only when all checklist items above are satisfied.

### Commit Message Format

```
[section] short description of what was added or changed

Optional longer body explaining why, if the title is not self-explanatory.
```

Examples:
```
[02-databases] add MVCC explanation and isolation level comparison table
[10-llm-engineering] add LoRA fine-tuning exercise with PEFT
[14-dsa] add union-find implementation with path compression and rank
```

One logical change per commit. Do not bundle unrelated changes into a single commit.

---

## Review Criteria

PRs are reviewed against these criteria. A PR will be requested for changes if any of these are not met:

**Correctness**
- Code in exercises compiles and runs without errors
- Reference solutions in `solutions/` produce correct output verified by tests
- Technical claims in `notes.md` are accurate — include a citation if in doubt

**Completeness**
- Notes cover the concept fully enough to be standalone (a reader should not need to Google the basics)
- Exercises are solvable with only the knowledge covered in `notes.md` for that section and earlier sections
- Checklist items are specific and verifiable

**Style and Consistency**
- Code follows the language-specific style rules in this guide
- Notes follow the `notes.md` structure defined above
- Filenames follow the naming conventions defined above

**Scope**
- A single PR does not span more than two sections
- Changes are focused — a PR adding exercises does not also rewrite unrelated notes

If your PR is declined, the reviewer will explain which criteria were not met. Address each point specifically and re-request review. Do not open a new PR to replace a declined one.

---

## Questions

If you are unsure whether a topic fits an existing section, open a GitHub issue before writing any content. This saves time for both you and the reviewers.
