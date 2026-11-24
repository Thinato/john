---
title: Where things go - each core area has its own .cpp, header, test file and pt-BR strings file
tags: [layout, core, tests, i18n, parallel-work]
---

# Where things go

## Context
Adding or moving code, a test or a translation in the core (`john_core`), and choosing which file to touch. The layout comes from #101, phase 1a (split of `song.cpp`, `john.hpp`, `test_core.cpp`, `pt-BR.json`); the GUI and MCP split is phase 1b.

## Rule
One area, one set of files:

| Area | Code | Header | Tests | pt-BR strings |
|---|---|---|---|---|
| Song model, JSON, storage, `listing()` | `src/song.cpp` | `song.hpp` | `tests/core/song.cpp` | |
| Theory: keys, spelling, fingerings, durations, transpose, bars, positions, score layout | `src/theory.cpp` | `theory.hpp` | `tests/core/theory.cpp` | `theory.json` |
| Techniques, instrument profiles, `marks()` | `src/catalog.cpp` | `catalog.hpp` | `tests/core/catalog.cpp` | `techniques.json`, `instruments.json` |
| Range, playability, play in key, `analyze()` | `src/analyze.cpp` | `analyze.hpp` | `tests/core/analyze.cpp` | |
| Data folder, settings, bundled lessons/demo songs | `src/settings.cpp` | `settings.hpp` | `tests/core/settings.cpp` | `settings.json` |
| Viewer language: `tr()`, language files | `src/i18n.cpp` | `i18n.hpp` | `tests/core/i18n.cpp` | |
| Play order, synth, pitch detection, voices | `src/sound.cpp` | `sound.hpp` | `tests/core/sound.cpp` | `practice.json` |
| ABC / MusicXML import | `src/abc.cpp`, `src/musicxml.cpp` | `import.hpp` | `tests/core/abc.cpp`, `tests/core/musicxml.cpp` | `import.json` |
| Transcription, Import audio, Demucs | `src/transcribe.cpp`, `src/import_audio.cpp` | `transcribe.hpp` | `tests/core/transcribe.cpp` | `import.json` |

`src/john.hpp` only includes every area header, so callers keep `#include "john.hpp"`. Helpers the core files share (`readJson`, `writeJson`, `persist`, `mod12`, `gLocalized`) live in `src/internal.hpp`, never copied. Viewer text of the main window (song list, song panel, legend, folders) goes in `i18n/pt-BR/gui.json`.

## Why
Every PR used to append to the same `test_core.cpp`, `pt-BR.json`, `john.hpp` and `song.cpp`, so parallel branches conflicted on the same lines (#101 counted 13 to 25 of the last 30 commits on each).

## How to apply
- A new test is a new `TEST(name) { ... }` in its area's `tests/core/<area>.cpp`; it registers itself and gets a new, empty data folder (`home`, `$JOHN_HOME`), so it must not rely on files another test left. `build/john-tests <name>` runs one test; a new area file needs no CMake line (glob).
- A new string is one line in the `i18n/pt-BR/<area>.json` that matches the code it comes from. Files merge at load (`loadStrings()`), and the `viewer_language` test fails on a key defined twice, a missing key from any `src/*.cpp`, or a bad value.
- A new core function goes in its area's `.cpp` and header. A new area gets its own pair, added to `john.hpp` and the `john_core` sources in `CMakeLists.txt`.
