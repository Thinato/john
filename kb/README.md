# Knowledge base

Short notes about John's codebase that agents and people would otherwise have to rediscover (gotchas, rules, lessons learned), plus semantic search over the notes and the code.

```sh
python3 kb/kb.py query "how do breath marks attach to notes"   # search notes + code (-n 6, --notes, --code)
python3 kb/kb.py check                                          # validate the notes (also runs in ctest)
python3 kb/kb.py sync                                           # refresh the index (query does this itself)
```

Search needs a [Voyage AI](https://dash.voyageai.com/) key in `VOYAGE_API_KEY`. Python 3.10+, standard library only. **No key?** The notes are plain markdown: `grep -ril <word> kb/notes/` and read them.

The index is a local cache in `kb/.cache/` (gitignored), one per checkout: each query re-embeds only the files that changed, so a note is searchable as soon as it's written. A fresh checkout or worktree embeds the whole repo on its first query (about 100k tokens, a few seconds; a small fraction of Voyage's 200M free tokens). Voyage's normal rate limits need a payment method on the account; without one the limits may be too low for that first index (the free tokens apply either way).

The model is `voyage-4`; it found the right result first for 9 of 10 test questions, `voyage-code-4` for 3 of 10 (see #28). `KB_MODEL` switches to another voyage-4-family model; the "no strong match" threshold is tuned for `voyage-4`.

## Writing a note

Write one in `kb/notes/` when you learn something about the codebase that isn't obvious from the code or CLAUDE.md:

- a correction from the user that the next agent shouldn't have to relearn
- a hidden gotcha, constraint or surprising behaviour
- a non-obvious approach that was confirmed to work
- a pitfall: code that looks right but isn't

Not for: things the code or CLAUDE.md already say, personal facts or preferences about the user, task progress.

Before writing, search (`kb.py query --notes "..."`) and edit an existing note rather than adding a near-duplicate. File name: kebab-case, at most 60 characters. Format (`kb.py check` enforces it):

```markdown
---
title: One-line summary of the rule
tags: [area, topic]
---

# Short title

## Context
When this applies.

## Rule
The knowledge itself, in one or two sentences. Name the files and functions involved.

## Why
The reason: the bug, constraint or user decision behind it.

## How to apply
What to do differently because of it.
```

Each `##` section is searched separately, with the title and tags attached, so keep sections self-contained. Notes go in the same PR as the code they describe and are reviewed with it. If a note becomes wrong, fix or delete it in the PR that changes the behaviour.
