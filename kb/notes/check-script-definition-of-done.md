---
title: scripts/check.sh runs the definition of done (build, ctest, kb check) and reuses build/ as already configured
tags: [ci, build, tests, workflow]
---

# scripts/check.sh

## Context
Checking a change before a PR, or changing how CI builds and tests (`.github/workflows/ci.yml`, #101 phase 2).

## Rule
`scripts/check.sh` configures `build/` only if it has no `CMakeCache.txt` (Release, default generator), then runs `cmake --build build --parallel`, `ctest --test-dir build --output-on-failure` and `python3 kb/kb.py check`, stops at the first failure with exit 1, and prints an ok/FAIL/skip summary. `--web` also configures and builds `build-web/` when `emcmake` is on the PATH, and reports it skipped otherwise. The `test (ubuntu-latest)` and `test (macos-latest)` jobs run its Configure step first (with `-DGLFW_BUILD_WAYLAND=OFF`) and then the script; the `windows` job (multi-config MSVC, needs `--config`/`-C`) and the `web` job (extra bundle checks) keep their own steps.

## Why
One command means local checks and CI run the same thing. Reusing the existing `build/` keeps whatever generator and flags it was configured with, which is what lets CI pass its own configure flags.

## How to apply
Run `scripts/check.sh` before opening a PR. To change configure flags, delete `build/` (or reconfigure it yourself) first: the script won't. A new CI configure flag goes in the Configure step of `ci.yml`, not in the script. A new check that belongs in the definition of done is one `step name command...` line in the script, so CI picks it up too.
