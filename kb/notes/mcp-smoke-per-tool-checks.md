---
title: MCP smoke test - one scenario file per tool in tests/mcp/<area>/, each check matched by its call id, no reply count to bump
tags: [testing, mcp, smoke, shell, parallel-work]
---

# Adding MCP smoke checks

## Context
Adding or changing an MCP tool and its checks in the `mcp` ctest (`tests/mcp_smoke.sh`). Since #101 phase 2 the runner holds no requests and no total reply count: every PR used to edit the same request list and the same "expected 53 replies" line.

## Rule
Each `tests/mcp/<area>/<name>.sh` is one scenario (areas mirror `src/mcp_<area>.cpp`; `protocol/` is `mcp_main.cpp`) and runs against its own fresh `john-mcp` and empty `JOHN_HOME`. A scenario is a list of `call <id> '<body>'` lines (the body is the request after `"id"`, e.g. `'"method":"tools/call","params":{...}'`), each followed by `expect '<pattern>'` / `refute '<pattern>'` (BRE, like `grep`) on that call's reply. The runner sources the file twice: first to send every request in one server run, then to check each reply, found by `"id":"<id>","jsonrpc":"2.0"` (replies are one line each; error replies put `"error"` before `"id"`). Every call must get exactly one reply; for each scenario the runner also asserts that the server exits 0, every stdout line is a JSON-RPC message, no line has `\r`, and the number of replies equals the number of calls.

## Why
Matching by id and counting per scenario means two branches that add tools never touch the same line, and a broken tool fails only its own checks: a failure prints `FAIL <area>/<name>.sh: <id>: reply lacks: <pattern>` with the reply, and the other scenarios still run.

## How to apply
- A new tool: a new `tests/mcp/<area>/<tool>.sh`. No runner or CMake edit (the runner globs `tests/mcp/*/*.sh`). A new check on an existing tool goes in that tool's file.
- A scenario starts from nothing: import the songs or define the instruments it needs at its top (a `call` without `expect` is fine; it still has to reply). The server needs no `initialize` first.
- Ids only need to be unique inside one file; pick a short name, because it appears in failure messages.
- Top-level code (setup files, `export` of env vars for the server, such as the Demucs stub in `audio/transcribe_audio.sh`) runs twice, so keep it repeatable; `$JOHN_HOME` is set before the file is read. Use `notify '<body>'` for a notification (no id, no reply).
- Run it alone with `sh tests/mcp_smoke.sh build/john-mcp`.
