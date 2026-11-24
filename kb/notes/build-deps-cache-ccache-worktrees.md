---
title: Local builds share dependency sources and downloads in JOHN_DEPS_CACHE (~/.cache/john) and use ccache; scripts/worktree-setup.sh and worktree-clean.sh
tags: [build, cmake, ccache, worktrees, dependencies, ci, web]
---

# Shared dependency cache, ccache and worktree scripts

## Context
Changing how `CMakeLists.txt` fetches dependencies (`john_declare`, `john_download`, `omr_download`), adding a dependency or a downloaded file, or setting up and cleaning agent worktrees (`.claude/worktrees/agent-*`) (#101, phase 2).

## Rule
`JOHN_DEPS_CACHE` (cache var, else the env var, else `~/.cache/john`; empty when the `CI` env var is set, so CI downloads into the build dir as before) holds `src/<name>-<12 hex of the declare options>/` (the FetchContent sources, used through `FETCHCONTENT_SOURCE_DIR_<NAME>`) and `downloads/<sha256>` or `downloads/url-<sha256 of the URL>` (fonts, the web's image reader). Each entry is fetched into a `.part` path under a `file(LOCK)` on `<entry>.lock`, then renamed, so worktrees that configure at the same time wait for each other and never see half an entry. ccache, when found, is the compiler launcher for C and C++ (also em++; not Visual Studio), run with `CCACHE_BASEDIR=<checkout>` and `CCACHE_NOHASHDIR=1`. The checkout's absolute paths (`JOHN_LESSONS`, `JOHN_I18N`, `JOHN_INSTRUMENTS`, `JOHN_FONT`, `JOHN_TEXT_FONT`) are set only on the source files that use them.

## Why
Every worktree used to clone six dependencies, download the fonts (and on the web about 50 MB of model and runtimes) and compile everything cold. Measured on the owner's Mac (10 cores, `-j 8`, other agents running): a fresh tree took 15 s to configure and 41 s to build; with a warm cache 0.8 s and 3 s, without network, and its build dir is about half the size (62 MB). ccache keys on the command line: an absolute path in a `-D` value differs per worktree and made every `john_core` object miss, and `base_dir` only rewrites path arguments, not `-D` values.

## How to apply
Add a dependency with `john_declare(name ...)` (same options as `FetchContent_Declare`): a new version changes the options, so it gets a new cache folder. Download files with `john_download(url [sha256])`, which sets `DOWNLOADED`; give a sha256 when the file is pinned. Don't extract or write into the shared cache from the build (extract into `${CMAKE_BINARY_DIR}`, as `omr_npm` does). A new path define goes on its source file (`set_property(SOURCE ... APPEND PROPERTY COMPILE_DEFINITIONS ...)`), not the target. `-DJOHN_DEPS_CACHE=` turns the cache off; deleting `~/.cache/john` is always safe. In a new worktree run `scripts/worktree-setup.sh` (configure plus build). `scripts/worktree-clean.sh` lists agent worktrees whose HEAD is in origin/main, whose squash-merged PR (gh) contains it, or whose remote branch is gone; `--yes` removes them with `git worktree remove` (the branch stays). It skips the main checkout, the current worktree, locked ones (Claude Code locks a running agent's worktree) and any with uncommitted or untracked files. Only the owner runs it with `--yes`.
