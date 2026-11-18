# Offline checks for kb.py (no API key): chunking and the note format check. Run: python3 kb/test_kb.py
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import kb  # noqa: E402

GOOD = """---
title: A rule
tags: [abc, import]
---

# A rule

## Context
When importing.

## Rule
Do this.

## Why
Because.

## How to apply
Always.
"""

# code: 60-line windows overlapping by 15, 1-based line numbers, path in the embedded text, blank windows skipped
code = "\n".join(f"int line{i} = {i};" for i in range(1, 101))
cs = kb.chunk_code("src/x.cpp", code)
assert [(c["start"], c["end"]) for c in cs] == [(1, 60), (46, 100)], cs
assert cs[0]["text"].startswith("src/x.cpp\nint line1 = 1;") and "line60" in cs[0]["text"] and "line61" not in cs[0]["text"]
assert kb.chunk_code("src/empty.cpp", "\n\n  \n") == []

# notes: one chunk per ## section, each carrying the title and tags
ns = kb.chunk_note(GOOD)
assert [c["heading"] for c in ns] == ["Context", "Rule", "Why", "How to apply"]
assert all(c["text"].startswith("A rule\ntags: [abc, import]\n\n## ") for c in ns)

# format check
assert kb.check_note("a-rule.md", GOOD) == []
assert kb.check_note("A Rule.md", GOOD) == ["file name must be kebab-case, at most 60 characters + .md"]
assert kb.check_note("x.md", "# no frontmatter") == ["missing --- frontmatter --- block"]
bad = kb.check_note("x.md", GOOD.replace("tags: [abc, import]", "tags: abc").replace("## Why\nBecause.\n\n", ""))
assert bad == ["frontmatter needs tags: [tag-one, tag-two]", "missing section '## Why'"], bad
assert kb.check_note("x" * 61 + ".md", GOOD)  # too long
print("kb tests passed")
