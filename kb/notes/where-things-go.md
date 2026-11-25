---
title: Where things go - each core area, GUI window and MCP tool group has its own file (and test and pt-BR strings file)
tags: [layout, core, gui, mcp, tests, i18n, parallel-work]
---

# Where things go

## Context
Adding or moving code, a test or a translation in the core (`john_core`), the viewer (`john`) or the MCP server (`john-mcp`), and choosing which file to touch. The layout comes from #101: phase 1a split `song.cpp`, `john.hpp`, `test_core.cpp` and `pt-BR.json`; phase 1b split `gui_main.cpp` and `mcp_main.cpp`.

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

The viewer and the MCP server (not in `john_core`, no test file of their own; `ctest`'s `mcp` test drives the server):

| Area | File |
|---|---|
| `App` (all viewer state), `Score` (glyph and text drawing), declarations of every function more than one GUI file uses | `src/gui.hpp` |
| `main()`: window, fonts, the frame loop, `JOHN_SNAPSHOT`; text helpers `L()`, `utf8()`, `keyLabel()`, `instName()`, `myInstrument()` | `src/gui_main.cpp` |
| Main window: song list (`songList()`), `songArea()`, details panel and legend, the open song (`open()`, `poll()`, `editSong()`) | `src/gui_panels.cpp` |
| The score: `drawScore()` and its steps (`Page`: layout, staves, bars, rests, notes, marks, charts, lyrics, pitch track, beams, note tooltip) | `src/gui_score.cpp` |
| Playback, mic, wait-for-me, play-along, the transport bar | `src/gui_practice.cpp` |
| Settings window, custom instrument editor, Find my range | `src/gui_settings.cpp` |
| Import song (ABC, MusicXML, images on the web), Import audio | `src/gui_import.cpp` |
| MCP: JSON-RPC loop, `initialize`, the agent guide (`GUIDE`), tool lookup | `src/mcp_main.cpp` |
| MCP: `Tool`, `tool()`, schema pieces shared by areas, the list of areas | `src/mcp.hpp` |
| MCP tools: songs (list, import, get, analyze, techniques on notes, update, delete bars) / transcription / technique catalog / instruments | `src/mcp_songs.cpp` / `mcp_audio.cpp` / `mcp_techniques.cpp` / `mcp_instruments.cpp` |

## Why
Every PR used to append to the same `test_core.cpp`, `pt-BR.json`, `john.hpp` and `song.cpp`, so parallel branches conflicted on the same lines (#101 counted 13 to 25 of the last 30 commits on each).

## How to apply
- A new test is a new `TEST(name) { ... }` in its area's `tests/core/<area>.cpp`; it registers itself and gets a new, empty data folder (`home`, `$JOHN_HOME`), so it must not rely on files another test left. In-memory state (language, solfege, technique cache) is not reset: a test that changes it puts it back. `build/john-tests <name>` runs one test; a new area file needs no CMake line (glob).
- A new string is one line in the `i18n/pt-BR/<area>.json` that matches the code it comes from. Files merge at load (`loadStrings()`), and the `viewer_language` test fails on a key defined twice, a missing key from any `src/*.cpp`, or a bad value.
- A new core function goes in its area's `.cpp` and header. A new area gets its own pair, added to `john.hpp` and the `john_core` sources in `CMakeLists.txt`.
- A new viewer window: a `fooWindow(App&)` in the `gui_*.cpp` of its subject, declared in `gui.hpp`, called once from the frame loop in `main()`; its state is a block of `App` fields. A new subject gets its own `gui_<area>.cpp`, added to the `john` sources in `CMakeLists.txt`. A helper two GUI files need is declared in `gui.hpp` and defined once, never copied.
- A setting's UI is one line in `settingsWindow()` (`gui_settings.cpp`), next to its field (settings-add-a-setting).
- A new MCP tool: a handler named after the tool (`static std::string get_song(const json& a)`) and one `tool("name", "description", schema, handler)` entry in its area's table (`songTools()`, ...) in `mcp_<area>.cpp`. `tools/list` shows the areas in the order `tools()` in `mcp_main.cpp` joins them, and each table in its order. A new area: `mcp_<area>.cpp`, its `...Tools()` in `mcp.hpp` and `tools()`, and the `john-mcp` sources. Tool replies stay English and nothing else may reach stdout. Its smoke checks are a new `tests/mcp/<area>/<tool>.sh` (mcp-smoke-per-tool-checks).
- User docs: the feature's `docs/<area>.md`. The README only changes for install, build or a new doc.

## The shared lines that remain
Two branches still touch the same line only here; keep these edits to one line so a conflict is trivial:
- a new setting: its field in `Settings` (`settings.hpp`) and its name in the JSON list in `settings.cpp`;
- a new MCP tool: its `tool(...)` entry in the area's table (a new area: `tools()` in `mcp_main.cpp`);
- a new viewer window: its call in the frame loop in `gui_main.cpp`;
- a new source file of an existing area target: one line in `CMakeLists.txt` (tests and smoke scenarios are globbed: none).
