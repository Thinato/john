---
title: On Windows, char strings and paths are UTF-8 through src/utf8.manifest; data lives in %USERPROFILE%\Documents\John
tags: [windows, build, ci, paths, filesystem, unicode, release]
---

# Windows build: UTF-8 paths, env vars, data folder

## Context
Writing code that turns paths into strings or back, reads environment variables, or uses a POSIX or GCC-only API (#50). The `windows` job in `.github/workflows/ci.yml` builds with MSVC and is not a required check, so a break there doesn't block merges but should be fixed.

## Rule
`src/utf8.manifest` (added to `john`, `john-mcp` and `john-tests` in `CMakeLists.txt`) sets the process code page to UTF-8, so on Windows `std::string` paths, `fs::path(std::string)`, `path.string()`, `fopen`, `getenv` and miniaudio's `ma_decoder_init_file` all use UTF-8, as on macOS/Linux. `fs::path::c_str()` is `const wchar_t*` on Windows: use `.string().c_str()` for `printf`/`fopen`/ImGui. `homeDir()` is `HOME`, or `USERPROFILE` on Windows, so `dataDir()` is `%USERPROFILE%\Documents\John`. `bundled()` finds the exe folder with `GetModuleFileNameW`. `systemLanguage()` falls back to the Windows display language when `LC_ALL`/`LANG` are empty.

## Why
Without the manifest, narrow strings use the ANSI code page (e.g. cp1252), so a path with an accent from JSON or a user folder like `C:\Users\João` fails or turns into garbage. Windows has no `HOME` unless Git Bash sets one, and the GUI and the MCP server must agree on the data folder however they are started. MSVC has no `unistd.h`/`getpid`/`setenv`/`unsetenv`, no `__attribute__`, no `M_PI` without `_USE_MATH_DEFINES` (set in `CMakeLists.txt`), reads sources as cp1252 without `/utf-8`, and its `gl.h` stops at OpenGL 1.1 (`GL_BGRA` is defined by hand in `gui_main.cpp`).

## How to apply
Use `homeDir()`, not `getenv("HOME")`. Keep paths that go to ImGui or JSON as UTF-8 `std::string`; never print `path.c_str()`. Wrap GCC/Clang-only syntax (attributes, builtins) in `#ifdef __GNUC__`. The Visual Studio generator is multi-config: build with `--config Release`, test with `ctest -C Release`, binaries are in `build/Release/*.exe` (the release Package step copies from there; the zip is made with `7z`). `tests/mcp_smoke.sh` runs through Git for Windows' `sh` (found by `find_program`), and `.gitattributes` keeps `*.sh` LF so it parses. `john.exe` is a GUI-subsystem program (no console window), so its stdout/stderr go nowhere. `john-mcp.exe` sets stdin/stdout to `_O_BINARY` in `main()`: in text mode every reply would end in `\r\n`, and `mcp_smoke.sh` fails on any `\r`. `fs::rename` over an existing file works on MSVC, but fails if another process has the target open at that moment. Iterate on CI with `gh api --allow-escape-sequences repos/Thinato/john/actions/jobs/<id>/logs`, which works while other jobs of the run are still going (`gh run view --log-failed` waits for the whole run).
