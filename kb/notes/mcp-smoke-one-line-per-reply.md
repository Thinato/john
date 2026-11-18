---
title: In tests/mcp_smoke.sh each reply is one line; grep by "id":N, to check a single reply
tags: [testing, mcp, smoke, shell]
---

# Checking one reply in the MCP smoke test

## Context
Adding calls or checks to `tests/mcp_smoke.sh`, especially negative checks ("this reply must not contain X") or checks tied to one call.

## Rule
The server writes each JSON-RPC reply on one line (newlines inside text are escaped as `\n`), so `printf "%s\n" "$OUT" | grep '"id":19,'` selects exactly reply 19; the script's `reply N` helper does this. `check` greps the whole output, so it can't tell which call matched and can't check for something missing. The final line count must equal the number of requests that get a reply (notifications get none).

## Why
On failure the script runs `echo "$OUT"`, and macOS `/bin/sh` echo expands `\n`, so the dump looks multi-line. That's only how it's displayed: grep still sees one line per reply.

## How to apply
For a filter or an error from a specific call, use `reply N | grep -q ...` (and `! reply N | grep -q ...` for absence). Bump the expected reply count when you add requests.
