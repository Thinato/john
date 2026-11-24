# Development

How to build is in the [README](../README.md#build). How to work on John (issues, branches, definition of done) is in [CONTRIBUTING.md](../CONTRIBUTING.md), and the code rules are in [CLAUDE.md](../CLAUDE.md).

## Build details

Needs CMake ≥ 3.24 and a C++20 compiler. These are downloaded at configure time:

- GLFW, Dear ImGui, nlohmann/json and miniaudio;
- the Bravura music font and the Noto Sans text font (both SIL Open Font License).

`ctest --test-dir build` runs the core checks and the MCP smoke test.

The web build: see [Web version](web.md#build-it).

## How it works

| Piece | File |
|---|---|
| Model, storage, agent listing | `src/song.cpp`, `src/song.hpp` (`src/john.hpp` includes every area's header) |
| Theory (keys, spelling, fingerings), transposition, bars, score layout | `src/theory.cpp` |
| Data folder, settings, bundled lessons and demo songs | `src/settings.cpp` |
| ABC importer (thesession.org bodies, grace-note cuts/taps, `!decorations!`, tuplets, voltas, `w:` lyrics) | `src/abc.cpp` |
| Hints: range/transposition ranking, cut/roll/cran/slide/breath/tongue candidates | `src/analyze.cpp` |
| Play order (repeats/endings), backing-whistle synth, YIN pitch detection | `src/sound.cpp` |
| Audio transcription (pitch track, beats, meter, key) and Import audio | `src/transcribe.cpp`, `src/import_audio.cpp` |
| Speaker/microphone devices (miniaudio) | `src/audio.cpp` |
| MCP server (stdio JSON-RPC, tools + agent guide in `instructions`) | `src/mcp_main.cpp`, tools in `src/mcp_*.cpp` |
| Viewer (Bravura SMuFL glyphs drawn with ImDrawList) | `src/gui_main.cpp`, `src/gui_*.cpp` (score, panels, practice, settings, import) |
| Viewer translations (English text -> translation) | `i18n/<language>/*.json`, `tr()` in `src/i18n.cpp` |
| Instrument profiles (range, note difficulty, techniques, sound), technique catalog | `instruments/*.json`, `src/catalog.cpp` |
| Web build: IndexedDB storage and page | `web/pre.js`, `web/shell.html` |
| Web image import: the reader (Web Worker) and its copisteria port, the first-line key check | `web/omr/`, `checkFirstLineKey()` in `src/musicxml.cpp` |

Which file a new piece of code, test or translation goes in: [where-things-go.md](../kb/notes/where-things-go.md).

Notes are stored at written pitch: whistle music is written an octave below how it sounds, so a D whistle's bottom note is D4.
Durations are in ticks (192 per whole note).

## Dev aids

`JOHN_SNAPSHOT=out.tga ./build/john` renders a few frames, writes the window to a TGA file and exits.

Lessons learned about the code are in the searchable knowledge base, [`kb/`](../kb/README.md).

## Docs

User docs are in `docs/`, one file per area, listed in the [README](../README.md#more). A feature PR edits its area's doc, not the README.
