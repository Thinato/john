---
title: MusicXML import turns duration x 48 / divisions into ticks, finds the .mxl root in META-INF/container.xml, and closes a last ending with ||
tags: [musicxml, import, mxl, durations, ticks, repeats, endings, dependencies, web, windows]
---

# MusicXML import (importMusicXml)

## Context
Changing `importMusicXml()` in `src/musicxml.cpp` (#73), the `import_musicxml` MCP tool, or the pugixml/miniz dependencies in `CMakeLists.txt`.

## Rule
`<duration>` counts `<divisions>` per quarter note, so ticks = duration x 48 / divisions (48 = `WHOLE / 4`). When that isn't a whole number the length is rounded and a warning says so, never silently. `<divisions>` can change in any `<attributes>`, so it is read again each time. Tuplet notes already have their shortened duration: `<time-modification>` only sets `tuplet`/`tupletQ`. Only the first `<part>` and the voice of its first note are kept. A `<chord/>` note replaces the previous note's pitch when higher, and `<backup>` is ignored, since only one voice is kept. Only the first key and time signature count; later changes get a warning. An `.mxl` is a zip: `META-INF/container.xml`'s first `<rootfile full-path>` names the score (it may sit in a subfolder); without one the first `.xml`/`.musicxml` outside `META-INF/` is used. Before unzipping, an entry whose declared uncompressed size is over `MXL_MAX_ENTRY` (64 MB, `import.hpp`) is refused with "The .mxl file is too large.". This covers both `container.xml` and the score.

## Why
John has one key and meter per song and one melody line, like the ABC import (chords keep the top note). An ending that stops on a plain bar line (`<ending type="stop|discontinue">` with no repeat) is written as a `||` bar. `playOrder()` only leaves a 2nd ending at `||`, `|]`, `|:` or a repeat sign, so with a plain `|` a later `:|` in the B part would jump back to the start of the A part. MusicXML pitches already include `<alter>`, so the key signature is never applied to them, unlike in ABC.

## How to apply
Test MusicXML changes by writing the same tune in ABC and comparing `toJson(song)["events"]` and `playOrder()` (see the `musicxml` test in `tests/core/musicxml.cpp`). The test builds an `.mxl` in memory with miniz's writer, so no binary fixture is committed. The zip-bomb test zips 64 MB + 1 byte of spaces (a few KB compressed). Checking the declared size is enough because miniz inflates into a buffer of exactly that size, so an entry that lies about its size fails instead of growing. pugixml (MIT, its own CMake target, linked PRIVATE) and miniz (MIT, the release zip's amalgamated `miniz.c` compiled into `john_core`, include dir PUBLIC so the test can zip) come from release archives through FetchContent. They build without changes on macOS, Linux, MSVC and Emscripten. The W3C repo (`w3c/musicxml`, `docs/src/data/examples/musicxml/` and `tests/files/`) has real scores for manual checks. Don't commit them, and only commit public-domain music in fixtures.
